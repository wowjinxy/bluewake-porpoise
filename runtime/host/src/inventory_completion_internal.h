// SPDX-License-Identifier: GPL-3.0-or-later
// Internal producer-to-collector seam. No external feeder or current-context API.
#ifndef BW_INVENTORY_COLLECTOR_COMPLETION_H
#define BW_INVENTORY_COLLECTOR_COMPLETION_H
#include "game_events.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
#define BW_IC_NOEXCEPT noexcept
extern "C" {
#else
#define BW_IC_NOEXCEPT
#endif
typedef struct BwInventoryCollectorCallProof {
    bool valid;
    uint64_t cpu_lifetime, memory_lifetime, module_generation, alias_binding;
    uint64_t native_call, epoch, scene_generation, guard_revision;
    uint32_t raw_alias_generation, stack, player, actor_id;
    uint32_t entry_frame;
    uint8_t module[32];
    char stage[9];
    uint16_t spawn;
    int8_t room, layer, stay_room;
} BwInventoryCollectorCallProof;
bool bw_inventory_collector_completion_enabled(void) BW_IC_NOEXCEPT;
bool bw_inventory_collector_call_begin(const CPUState*, uint64_t native_call,
    uint64_t epoch, uint64_t scene_generation, uint32_t stack, uint32_t player,
    BwInventoryCollectorCallProof*) BW_IC_NOEXCEPT;
bool bw_inventory_collector_call_replay(const CPUState*,
    const BwInventoryCollectorCallProof*) BW_IC_NOEXCEPT;
// Defined in the guarded actual producer. Clears only private proof bytes.
void bw_inventory_collector_retire_pending_proofs(void) BW_IC_NOEXCEPT;
void bw_inventory_collector_emit_begin(const CPUState*,
    const BwInventoryCollectorCallProof*) BW_IC_NOEXCEPT;
void bw_inventory_collector_event_scope_bind(const BwGameEvent*) BW_IC_NOEXCEPT;
bool bw_inventory_collector_copy_emitted_lease(const BwGameEvent*,
    BwInventoryCollectorCallProof*) BW_IC_NOEXCEPT;
void bw_inventory_collector_emit_end(void) BW_IC_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#endif
