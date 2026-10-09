#ifndef BLUEWAKE_INLINE_GPR_H
#define BLUEWAKE_INLINE_GPR_H

#include "direct_calls.h"

/* Bulk charging cannot expose a different PC/downcount to an MMIO or journal
 * callback. A possible partial alias requires checking each word rather than
 * resolving the whole frame as one range. Actual stores still use the
 * maintained memory helpers, including reservation invalidation. */
static inline bool bw_inline_gpr_memory_ready(CPUState* cpu, unsigned first) {
    if (first < 14 || first > 31 || cpu->ram == NULL || g_mem_write_journal != NULL)
        return false;
#if defined(BLUEWAKE_EXPERIMENTAL_GPR_FRAME_PREFLIGHT) && BLUEWAKE_EXPERIMENTAL_GPR_FRAME_PREFLIGHT
    /* Experimental: controlled title measurements did not establish a paired
     * CPU/wall gain. Keep the existing per-word predicate as the default. */
    /* With no aliases over cached MEM1, every word in an aligned, bounded
     * frame has the same RAM owner. Prove that once instead of resolving all
     * 18 words. A fixed-memory build must also have adopted its global RAM:
     * otherwise its early get_ram_ptr path could return a different owner.
     * Mirrors, partial aliases and every other case retain the word checks. */
    const u32 start = cpu->gpr[11] + (u32)(4 * first - 128);
    const u32 bytes = 4u * (32u - first);
    const u32 offset = start - GC_RAM_BASE;
    if (!g_ppc_guest_aliases_overlap_mem1 && (start & 3u) == 0u &&
#if defined(BW_GUEST_MEM1)
        cpu->ram == (u8*)BW_GUEST_MEM1 &&
#endif
        bytes <= cpu->ram_size && offset <= cpu->ram_size - bytes &&
        offset <= GC_MEM1_ADDRESS_END - GC_RAM_BASE - bytes)
        return true;
#endif
    for (unsigned reg = first; reg < 32; ++reg) {
        const u32 address = cpu->gpr[11] + (u32)(4 * reg - 128);
        const u32 offset = (address & ~0x40000000u) - GC_RAM_BASE;
        if ((address & 3u) != 0u || cpu->ram_size < 4u || offset > cpu->ram_size - 4u ||
            get_ram_ptr(cpu, address, 4u, NULL) != cpu->ram + offset)
            return false;
    }
    return true;
}

#endif
