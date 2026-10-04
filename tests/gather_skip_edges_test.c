#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gather_pipe.h"
#include "dispatch_loop.h"

unsigned bw_direct_depth;
bool bw_direct_enabled, bw_edge_filter_enabled, bw_edge_watch_ready;
u32 bw_edge_watch_table[BW_EDGE_WATCH_SLOTS];
static bool dirty, decrementer;
static u32 cause, mask;
const bool* bw_host_sources_dirty = &dirty;
const bool* bw_host_decrementer_pending = &decrementer;
const u32* bw_host_pi_cause = &cause;
const u32* bw_host_pi_mask = &mask;
void* bw_host_can_skip_user;
static unsigned dispatched, delivered, service_calls, largest_batch, mode;

static bool ready(void* user, const CPUState* cpu, u32 address) {
    assert(user == NULL && cpu != NULL && address == cpu->pc);
    return mode != 0 || dispatched < 4;
}
BwHostCanSkipFn bw_host_can_skip = ready;

static void bytes(const u8* data, u32 size) {
    if (size > largest_batch) largest_batch = size;
    for (u32 i = 0; i < size; ++i)
        assert(data[i] == ++delivered);
}
static void word(u64 value, u8 size) {
    (void)value; (void)size;
    assert(0 && "batch must be enabled");
}
static int dispatch(CPUState* cpu, u32 address) {
    ++dispatched;
    cpu->pc = address + 4;
    if (mode != 3 || dispatched == 1) --cpu->downcount;
    bw_gather_pipe_put(dispatched, 1);
    if (mode == 2 && dispatched == 2) cpu->exception = 1;
    return mode != 5 && !(mode == 4 && dispatched == 3);
}
static bool service(void* user, CPUState* cpu, u32 address) {
    assert(user == NULL && address == cpu->pc);
    assert(bw_gather_pipe_length == 0 && delivered == dispatched);
    ++service_calls;
    return true;
}
typedef int (*Run)(CPUState*, u32, BluewakeCompositeDispatchFn,
                   BluewakeEdgeServiceFn, void*);

static void reset(void) {
    dispatched = delivered = service_calls = largest_batch = 0;
    bw_direct_enabled = bw_edge_filter_enabled = bw_edge_watch_ready = true;
    bw_direct_depth = 0;
    dirty = decrementer = false;
    cause = mask = 0;
    bw_host_can_skip = ready;
    memset(bw_edge_watch_table, 0, sizeof bw_edge_watch_table);
}
static void check(Run run) {
    /* Host observation, budget, exception, non-advancing execution, misses. */
    const unsigned counts[] = {4, 3, 2, 10, 3, 1};
    for (mode = 0; mode < 6; ++mode) {
        reset();
        CPUState cpu = {0};
        cpu.cycle_budget = mode == 1 ? 3 : 100;
        assert(run(&cpu, 0x80004000u, dispatch, service, NULL) == (mode != 5));
        assert(dispatched == counts[mode] && delivered == dispatched);
        assert(bw_gather_pipe_length == 0 && largest_batch == counts[mode]);
        assert(service_calls == (mode == 0));
    }
    /* Each reason a boundary cannot be skipped drains before consulting it. */
    mode = 1;
    for (unsigned reject = 0; reject < 9; ++reject) {
        reset();
        CPUState cpu = {0};
        cpu.cycle_budget = 100;
        const u32 address = 0x80004004u;
        switch (reject) {
        case 0: bw_edge_filter_enabled = false; break;
        case 1: bw_edge_watch_ready = false; break;
        case 2: bw_direct_enabled = false; break;
        case 3: bw_host_can_skip = NULL; break;
        case 4: bw_direct_depth = BW_DIRECT_DEPTH_MAX; break;
        case 5: dirty = true; break;
        case 6: cpu.msr = PPC_MSR_EE; decrementer = true; break;
        case 7: cpu.msr = PPC_MSR_EE; cause = mask = 1; break;
        case 8: bw_edge_watch_table[(address * 0x9E3779B1u) >> 20] = address; break;
        }
        assert(run(&cpu, address - 4, dispatch, service, NULL) == 1);
        assert(dispatched == 1 && delivered == 1 && service_calls == 1);
    }
    reset();
    CPUState cpu = {0};
    cpu.cycle_budget = 3;
    decrementer = true; cause = mask = 1; /* masked by guest EE, so still quiet */
    assert(run(&cpu, 0x80004000u, dispatch, service, NULL) == 1);
    assert(delivered == 3 && largest_batch == 3 && service_calls == 0);
    reset();
    assert(run(&cpu, 0x80004000u, dispatch, NULL, NULL) == 1);
    assert(delivered == 1 && bw_gather_pipe_length == 0);
    bw_gather_pipe_put(2, 1);
    assert(run(NULL, 0, dispatch, service, NULL) == 0);
    assert(delivered == 2 && bw_gather_pipe_length == 0);
    bw_gather_pipe_put(3, 1);
    assert(run(&cpu, 0, NULL, service, NULL) == 0);
    assert(delivered == 3 && bw_gather_pipe_length == 0);
}
int main(void) {
    bw_gather_pipe_write = word;
    bw_gather_pipe_bytes = bytes;
    check(bluewake_chassis_dispatch_loop);
    check(bluewake_composite_dispatch_until_boundary);
    puts("gather retained across approved edges; observation and exits drained");
    return 0;
}
