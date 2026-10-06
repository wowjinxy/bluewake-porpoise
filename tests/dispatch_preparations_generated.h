#include "core/cpu.h"
static inline s64 dolrecomp_loop_cycle_budget(const CPUState* ctx) {
    return ctx->cycle_budget;
}
#define DOLRECOMP_C_LOOP_CYCLE_BUDGET dolrecomp_loop_cycle_budget(ctx)
static inline bool dolrecomp_block_can_precharge(const CPUState* ctx,u32 block_cycles) {
    if (ctx->cycle_deadline_budget <= 0) return true;
    const s64 remaining = ctx->cycle_deadline_budget + ctx->downcount;
    return remaining >= 0 && (u64)remaining >= (u64)block_cycles;
}
static inline bool dolrecomp_charge_precise(CPUState* ctx,u32 cycles,u32 resume) {
    if (ctx->downcount <= -(s64)DOLRECOMP_C_LOOP_CYCLE_BUDGET) {
        ctx->pc = resume;
        return false;
    }
    ctx->downcount -= (s64)cycles;
    return true;
}
