// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_DIALOGUE_SPEED_H
#define BLUEWAKE_DIALOGUE_SPEED_H

#include "core/cpu.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GZLE01 native dialogue enhancement for ordinary MSG and JMessage. 1 is native.
 * This does not alter BMG data, scripted waits, page stops, choices, native
 * fast spans or unskippable/script-directed drawing. Ordinary MSG uses the
 * native temporary character allowance; JMessage uses character delays.
 * BetterWW instant_text takes priority and
 * must be off from startup to qualify this feature against original BMG data.
 * Accept finite multipliers in [1,10]; invalid values leave the setting alone.
 * Configure/getter are UI-thread-safe: they atomically publish a setting and
 * generation. The game thread cancels pending invocations/fractional delays
 * when it accepts that generation. All other APIs belong to the game thread. */
bool bluewake_dialogue_speed_configure(float multiplier);
float bluewake_dialogue_speed_multiplier(void);
void bluewake_dialogue_speed_attach(CPUState* cpu);
/* Call on state load, machine reset, module reload and memory replacement.
 * Also tracks game_events epoch/generation and same-VI scene changes. */
void bluewake_dialogue_speed_reset(CPUState* cpu);

/* Consult observes in the optimized host skip predicate. Dispatch both the
 * initial PC and ordinary host edges, outside feature_dispatch's geometry
 * interval. Both paths may replay an entry/return; one invocation is consumed
 * exactly once. Guest PC and timing budgets are never changed. The ordinary
 * MSG path changes only stringSet's temporary r30 character allowance once;
 * the native epilogue restores its caller's r30. JMessage changes only its
 * verified character wait. No per-VI timer writes or invented guest calls. */
bool bluewake_dialogue_speed_observes(uint32_t address);
void bluewake_dialogue_speed_dispatch(CPUState* cpu, uint32_t address);

typedef struct BwDialogueSpeedStats {
    uint64_t entries;
    uint64_t completed;
    uint64_t scaled;
    uint64_t cancelled;
    uint64_t pending_overflow;
    uint64_t legacy_entries;
    uint64_t legacy_completed;
    uint64_t legacy_budget_scaled;
    uint64_t legacy_spacing_scaled;
    uint64_t legacy_cancelled;
} BwDialogueSpeedStats;
void bluewake_dialogue_speed_stats(BwDialogueSpeedStats* stats);

#ifdef __cplusplus
}
#endif
#endif
