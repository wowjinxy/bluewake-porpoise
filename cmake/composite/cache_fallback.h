#ifndef BLUEWAKE_COMPOSITE_CACHE_FALLBACK_H
#define BLUEWAKE_COMPOSITE_CACHE_FALLBACK_H

/* Cache-site dispatch adaptation of Elliott Tate's cache_ops.py, app
 * 944a1f3c2b130a8086295428a0847938a777a2d7. This deliberately does not use
 * its PC-only replacement: the FIFO and current callback are observable.
 * Only cache_callbacks.py's four exact emitter templates call this helper.
 * No identifier may be ctx (prepared chunks can define it as a macro). */
#include "gather_pipe.h"

static inline __attribute__((always_inline)) void bw_cache_fallback_instruction(
    CPUState* cpu, u32 raw, u32 cia) {
    /* A flush can itself replace the callback. Read it only afterwards. */
    bw_gather_pipe_drain();
    const PPCInstructionFallback fallback = cpu->instruction_fallback;
    if (fallback != NULL) {
        fallback(cpu, raw, cia);
        return;
    }
    /* Exact NULL branch from the pinned runtime trampoline. Do not re-enter
     * the gather wrapper: its first flush may have queued new FIFO bytes. */
    ppc_program_exception(cpu, PPC_PROGRAM_ILLEGAL, cia);
}

#endif
