// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_PROGRESSION_SYNC_H
#define BLUEWAKE_PROGRESSION_SYNC_H
#include "game_events.h"
#ifdef __cplusplus
extern "C" {
#endif
#define BW_PROGRESSION_SCHEMA 1u
#define BW_PROGRESSION_KEYS 256u
/* A key is a schema identifier, never a guest address or generic save offset.
 * Equipment0..20; capacity32..35; passive collected67/68, songs73/shards74/pearls75;
 * dungeon192..207 excludes STAGE_LIFE; placed-pearl244; Grandma255.
 * Collected sword/shield/bracelets and ALL chest markers are excluded pending
 * native derived equipment/item-aware reward reconciliation qualification. */
typedef struct BwProgressionDelta { uint16_t key; uint32_t value; } BwProgressionDelta;
typedef struct BwProgressionState { uint32_t values[BW_PROGRESSION_KEYS]; } BwProgressionState;
typedef enum BwProgressionApply {
    BW_PROGRESS_INVALID, BW_PROGRESS_DEFERRED, BW_PROGRESS_UNCHANGED, BW_PROGRESS_APPLIED
} BwProgressionApply;
void bw_progression_clear(BwProgressionState* state);
bool bw_progression_valid(BwProgressionDelta delta);
bool bw_progression_merge(BwProgressionState* state, BwProgressionDelta delta);
const char* bw_progression_key_name(uint16_t key);
/* Game thread only. Normalize genuine copied GameEvents facts against the
 * current native RAM. Updates known only for a new monotonic local fact.
 * State loads/resets never authorize exporting personal saves into a room. */
bool bw_progression_capture_event(BwProgressionState* known, const CPUState* cpu,
                                 const BwGameEvent* event, BwProgressionDelta* out);
/* Explicit caller authorization: use only after a room CARD has loaded.
 * Reads whitelist entries only, including eligible current-stage dungeon bits.
 * Native recollection loadouts reject capture/snapshots without modifying out. */
bool bw_progression_snapshot(const CPUState* cpu, BwProgressionState* out);
/* Applies audited equivalents of native ownership/passive collect/dungeon operations.
 * Never calls execItemGet (which may refill resources or run rewards twice),
 * never writes equipment selections, counts, actor/CPU/camera/temporary state.
 * The live native scene must match the supplied ready scene. */
BwProgressionApply bw_progression_apply(CPUState* cpu, const BwGameScene* scene,
                                       BwProgressionDelta delta);
int bw_progression_current_save_stage(const CPUState* cpu);
#ifdef __cplusplus
}
#endif
#endif
