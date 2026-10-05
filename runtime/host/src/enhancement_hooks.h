// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_ENHANCEMENT_HOOKS_H
#define BLUEWAKE_ENHANCEMENT_HOOKS_H

#include "core/cpu.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GZLE01 native boundaries, canonical PCs (also accept the uncached mirror).
 * Add all four to the optimized module watch table. Wire observes into the
 * direct-call host predicate and dispatch at both the first PC and edges.
 * A watch list alone cannot intercept an optimized intrachunk label. */
#define BLUEWAKE_ENHANCEMENT_WIND_COMMIT 0x8008A870u
#define BLUEWAKE_ENHANCEMENT_WIND_RETURN 0x81F10624u
#define BLUEWAKE_ENHANCEMENT_BOOTS_ANIMATION 0x8012821Cu
#define BLUEWAKE_ENHANCEMENT_BOOTS_RETURN 0x801198BCu

/* Independently published UI-thread settings; both default to false. Neither
 * setter touches game-thread state. Game dispatch accepts configuration changes
 * and cancels pending calls. These hooks do not change BetterWW options. */
void bluewake_enhancement_faster_wind(bool enabled);
void bluewake_enhancement_faster_boots(bool enabled);
bool bluewake_enhancement_faster_wind_enabled(void);
bool bluewake_enhancement_faster_boots_enabled(void);

/* All remaining APIs belong to the game thread. Reset after state restore,
 * machine/module reset, including when CPU/RAM addresses have not changed.
 * Retrace expires abandoned calls; it never changes guest timers or frames. */
void bluewake_enhancement_hooks_attach(CPUState* cpu);
void bluewake_enhancement_hooks_reset(CPUState* cpu);
void bluewake_enhancement_hooks_retrace(CPUState* cpu);
bool bluewake_enhancement_hooks_observes(uint32_t address);
/* Read-only admission for the supplied state, including private native-call
 * probes. This reads only LR/configuration and never dereferences guest RAM.
 * Pending returns remain observed regardless of the probe's current LR. */
bool bluewake_enhancement_hooks_observes_context(const CPUState* cpu, uint32_t address);
void bluewake_enhancement_hooks_dispatch(CPUState* cpu, uint32_t address);

typedef struct BwEnhancementHooksStats {
    uint64_t wind_entries, wind_completed, wind_shortened;
    uint64_t boots_entries, boots_completed, boots_scaled;
    uint64_t cancelled;
} BwEnhancementHooksStats;
void bluewake_enhancement_hooks_stats(BwEnhancementHooksStats* result);

#ifdef __cplusplus
}
#endif
#endif
