#ifndef BLUEWAKE_FP_CONTEXT_PAIR_V1_H
#define BLUEWAKE_FP_CONTEXT_PAIR_V1_H
#if !defined(BLUEWAKE_COMPOSITE_INLINE_FP_H) || !defined(RECOMP_COMPOSITE_H)
#error "context pair requires the exact production generated/inline_fp order"
#endif
/* Private paid-fast-copy consumer only. False is read-only: execute both
 * original instructions. A finite first stage cannot change MSR or reenter.
 * No callback, suffix refund, precise charge, Rc or observer may separate it
 * from the following multiply. No cross-instruction charge is moved. */
static inline bool bw_fp_try_fsubs_fmuls_context_v1(
    CPUState* cpu, u8 dsub, u8 asub, u8 bsub,
    u8 dmul, u8 amul, u8 cmul, u32 second_pc) {
    const f64 a = cpu->fpr[asub], b = cpu->fpr[bsub];
    if ((cpu->msr & PPC_MSR_FP) == 0 ||
        !(bw_fp_finite(a) && bw_fp_finite(b)))
        return false;
    const f32 rounded = bw_fp_single(cpu, a - b);
    cpu->fpr[dsub] = (f64)rounded;
    cpu->ps1[dsub] = (f64)rounded;
    cpu->pc = second_pc;
    const f64 x = cpu->fpr[amul], y = cpu->fpr[cmul];
    if (bw_fp_finite(x) && bw_fp_finite(y)) {
        /* Only the actual widened finite result has this identity. */
        const f64 multiplier = cmul == dsub ? y : bw_fp_25bit(y);
        const f64 product = x * multiplier;
        if (product == product) {
            bw_fp_write_single(cpu, dmul, bw_fp_single(cpu, product));
            cpu->fpscr &= ~(BW_FP_FPSCR_FI | BW_FP_FPSCR_FR);
            return true;
        }
    }
    /* This must precede canonical VE/invalid-gated fallback. Classify the
     * saved f32, never a DAZ-widened FPR. First fsubs preserves FI and FR. */
    bw_fp_fprf(cpu, bw_fp_class32(rounded));
    bw_fp_fmuls(cpu, dmul, amul, cmul);
    return true;
}
#endif
