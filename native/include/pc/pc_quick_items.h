// Native held-modifier equipment shortcuts. No guest CPU or item-slot overlays.
#ifndef PC_QUICK_ITEMS_H
#define PC_QUICK_ITEMS_H

#include <stdint.h>
#include <dolphin/pad.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PcQuickItemAction {
    PC_QUICK_ITEM_NONE = 0,
    PC_QUICK_ITEM_TACT = 1,
    PC_QUICK_ITEM_CANNON = 2,
    PC_QUICK_ITEM_CRANE = 3
} PcQuickItemAction;

// LB shortcuts require canonical D-pad mappings, an otherwise unused LB, and
// no LB/D-pad bindings to native axes or triggers.
// Tab/arrow shortcuts require those keys unbound in native keyboard profiles.
// Conflicting custom profiles retain their native effects.
// Called once immediately after PADRead, before JUT computes button edges.
void pc_quick_items_filter(PADStatus* status);
// Same-frame intentions only. Each read rechecks the native actor/context token.
int pc_quick_items_pressed(PcQuickItemAction action);
int pc_quick_items_held(PcQuickItemAction action);
int pc_quick_items_enabled(void);
void pc_quick_items_set_enabled(int enabled);
// The actor adapter supplies zero while unsafe, otherwise a player/scene token.
uint64_t pc_quick_items_context(void);

#ifdef __cplusplus
}
#endif
#endif
