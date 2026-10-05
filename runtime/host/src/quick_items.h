// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_QUICK_ITEMS_H
#define BLUEWAKE_QUICK_ITEMS_H
#include "core/cpu.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
#define BLUEWAKE_QUICK_ITEMS_FRAME 0x800231E4u
#define BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY 0x8003EF38u
#define BLUEWAKE_QUICK_ITEMS_SHIP_RETURN 0x80023960u

// Fixed LB/Tab + Up Wind Waker; Left cannon at sea; Right crane at sea.
// UI thread: atomic publication. Native XYZ assignments are never changed.
void bluewake_quick_items_configure(bool enabled);
bool bluewake_quick_items_enabled(void);
// Game thread: reset AFTER lifecycle replacement, even reused RAM/player.
// Reset drops stale overlay metadata without writing old bytes into a new state.
void bluewake_quick_items_attach(CPUState* cpu);
void bluewake_quick_items_reset(CPUState* cpu);
// Once per VI after the Controls action snapshot. Native modifier bits identify
// GameCube buttons sharing that held physical source.
void bluewake_quick_items_input(bool modifier_down,bool blocked,u64 generation,u16 native_modifier_buttons);
bool bluewake_quick_items_observes(u32 address);
void bluewake_quick_items_dispatch(CPUState* cpu,u32 address);
// Defer host state save while the ship-only live/input overlay is active. Its
// exact caller return restores it before ordinary scene/Link/UI work resumes.
bool bluewake_quick_items_busy(void);
typedef struct BluewakeQuickItemsStats {
    u64 accepted,native_handoffs,cannon_operations,crane_operations,overlay_restores,cancelled;
} BluewakeQuickItemsStats;
void bluewake_quick_items_stats(BluewakeQuickItemsStats* out);
#ifdef __cplusplus
}
#endif
#endif
