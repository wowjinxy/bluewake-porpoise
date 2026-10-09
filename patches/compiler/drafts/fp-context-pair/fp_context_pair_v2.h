#ifndef BLUEWAKE_FP_CONTEXT_PAIR_V2_H
#define BLUEWAKE_FP_CONTEXT_PAIR_V2_H
#if !defined(BLUEWAKE_COMPOSITE_INLINE_FP_H) || !defined(RECOMP_COMPOSITE_H)
#error "context pair requires the exact production generated/inline_fp order"
#endif
/* Private paid-fast-copy consumer only. False is read-only. The finite first
 * stage has no callback, observation, guest exception or MSR writer. Resolve
 * second-source aliases from the saved first value instead of publishing an
 * intermediate FPR/PS1 pair that a successful same-destination multiply kills.
 * Before any canonical fallback, publish the complete original first result.
 * Precise labels, charges and unavailable/nonfinite-first paths stay original. */
static inline bool bw_fp_try_fsubs_fmuls_context_v2(
    CPUState* cpu, u8 dsub, u8 asub, u8 bsub,
    u8 dmul, u8 amul, u8 cmul, u32 second_pc) {
    const f64 a = cpu->fpr[asub], b = cpu->fpr[bsub];
    if ((cpu->msr & PPC_MSR_FP) == 0 ||
        !(bw_fp_finite(a) && bw_fp_finite(b)))
        return false;
    const f32 rounded = bw_fp_single(cpu, a - b);
    const f64 first = (f64)rounded;
    cpu->pc = second_pc;
    const f64 x = amul == dsub ? first : cpu->fpr[amul];
    const f64 y = cmul == dsub ? first : cpu->fpr[cmul];
    if (bw_fp_finite(x) && bw_fp_finite(y)) {
        const f64 multiplier = cmul == dsub ? y : bw_fp_25bit(y);
        const f64 product = x * multiplier;
        if (product == product) {
            if (dsub != dmul) {
                cpu->fpr[dsub] = first;
                cpu->ps1[dsub] = first;
            }
            bw_fp_write_single(cpu, dmul, bw_fp_single(cpu, product));
            cpu->fpscr &= ~(BW_FP_FPSCR_FI | BW_FP_FPSCR_FR);
            return true;
        }
    }
    cpu->fpr[dsub] = first;
    cpu->ps1[dsub] = first;
    bw_fp_fprf(cpu, bw_fp_class32(rounded));
    bw_fp_fmuls(cpu, dmul, amul, cmul);
    return true;
}
#endif
