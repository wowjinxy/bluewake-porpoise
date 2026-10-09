#ifndef BLUEWAKE_COMPOSITE_DISPATCH_LOOP_H
#define BLUEWAKE_COMPOSITE_DISPATCH_LOOP_H

#include "edge_intercept_abi.h"
#if defined(BLUEWAKE_DIRECT_CALLS)
#include "direct_calls.h"
#include "observation_facts.h"
#endif
#if defined(BLUEWAKE_GATHER_PIPE)
#include "gather_pipe_batch.h"
#endif

static inline int bluewake_chassis_return(int dispatched) {
#if defined(BLUEWAKE_GATHER_PIPE)
    bw_gather_pipe_drain();
#endif
    return dispatched;
}

typedef int (*BluewakeCompositeDispatchFn)(CPUState* ctx, u32 address);

#if defined(BLUEWAKE_DIRECT_CALLS)
/* Called only after this address has missed bw_edge_watch_table. Handshake
 * validation proves that set contains every static host intercept. Quiet is
 * checked here at the same point as the original complete ready query. */
static inline bool bw_chassis_ready_after_unwatched(const CPUState* cpu, u32 address) {
    if (bw_host_observation_facts == NULL)
        return bw_direct_call_ready(cpu, address);
    if (!bw_direct_enabled || cpu == NULL || bw_host_can_skip == NULL ||
        bw_direct_depth >= BW_DIRECT_DEPTH_MAX)
        return false;
    if (cpu->exception != 0u || (cpu->cycle_budget > 0 && cpu->downcount <= -cpu->cycle_budget))
        return false;
    return bw_host_quiet(cpu) && bw_host_observation_facts(
        bw_host_observation_facts_user, cpu, address, BW_OBSERVATION_FACTS_ALL);
}
#endif

/* The boundary loop, in the header and always inlined.
 *
 * Why it is not only the out-of-line function below. The composite dispatches
 * one guest block per iteration of this loop, and the dispatch arrives as a
 * function pointer, so passing it through the entry point of another
 * translation unit left the compiler with an indirect call it could not see
 * through: the pointer was reloaded from its stack slot at every guest edge and
 * neither selected_dispatch nor the chunk lookup beneath it could be inlined
 * into the loop. Called with a static function from the same translation unit
 * the call devirtualises, the lookup folds into the loop, and the loop's own
 * state survives an edge in registers instead of being rebuilt.
 *
 * The exported form and the unit test keep the out-of-line entry point; it is
 * this body, so the two cannot drift.
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((always_inline))
#endif
static inline int bluewake_chassis_dispatch_loop(
    CPUState* ctx, u32 address, BluewakeCompositeDispatchFn dispatch,
    BluewakeEdgeServiceFn edge_service, void* service_user) {
    if (ctx == NULL || dispatch == NULL)
        return bluewake_chassis_return(0);

    s64 prior_downcount = ctx->downcount;
    int dispatched = dispatch(ctx, address);
    if (!dispatched || edge_service == NULL)
        return bluewake_chassis_return(dispatched);
    if (ctx->downcount >= prior_downcount)
        return bluewake_chassis_return(1);

    unsigned zero_charge_run = 0u;
    for (;;) {
        if (ctx->exception != 0u ||
            (ctx->cycle_budget > 0 &&
             ctx->downcount <= -ctx->cycle_budget))
            return bluewake_chassis_return(1);

        address = ctx->pc;
        bool skip_edge = false;
#if defined(BLUEWAKE_DIRECT_CALLS)
        skip_edge = bw_edge_filter_enabled && bw_edge_watch_ready &&
                    bw_edge_unwatched(address) && bw_chassis_ready_after_unwatched(ctx, address);
#endif
        if (!skip_edge) {
#if defined(BLUEWAKE_GATHER_PIPE)
            /* A consulted host edge must observe all preceding FIFO writes.
             * Certified successors with no host work can retain their batch. */
            bw_gather_pipe_drain();
#endif
            if (edge_service(service_user, ctx, address))
                return bluewake_chassis_return(1);
        }

        prior_downcount = ctx->downcount;
        dispatched = dispatch(ctx, address);
        if (!dispatched)
            return bluewake_chassis_return(1);
        if (ctx->downcount >= prior_downcount) {
            /* Eight non-advancing successors are allowed; the ninth yields
             * so a stuck guest always returns to the host. Progress resets
             * the run, and a new turn or CPU gets an independent allowance. */
            if (++zero_charge_run <= 8u)
                continue;
            return bluewake_chassis_return(1);
        }
        zero_charge_run = 0u;
    }
}

int bluewake_composite_dispatch_until_boundary(
    CPUState* ctx, u32 address, BluewakeCompositeDispatchFn dispatch,
    BluewakeEdgeServiceFn edge_service, void* service_user);

#endif
