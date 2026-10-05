// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_AUTOSAVE_H
#define BLUEWAKE_AUTOSAVE_H
#include "core/cpu.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

#define BLUEWAKE_AUTOSAVE_FRAME 0x800231E4u
#define BLUEWAKE_AUTOSAVE_FRAME_CALLER 0x8000645Cu
// Host-only return token, never guest storage. Reserve in first-PC/edge service;
// it is distinct from callback_delivery's 8180FFF0. No translated code here.
#define BLUEWAKE_AUTOSAVE_RETURN 0x8180FFE0u

typedef enum BluewakeAutosavePhase {
    BW_AUTOSAVE_IDLE, BW_AUTOSAVE_HEAP_LOCK, BW_AUTOSAVE_HEAP_CAPACITY,
    BW_AUTOSAVE_ALLOCATE, BW_AUTOSAVE_HEAP_UNLOCK, BW_AUTOSAVE_CARD_LOCK,
    BW_AUTOSAVE_OLD_CHECKSUM, BW_AUTOSAVE_PUT_STAGE, BW_AUTOSAVE_START_STAGE,
    BW_AUTOSAVE_SERIALIZE, BW_AUTOSAVE_CHECKSUM, BW_AUTOSAVE_STORE,
    BW_AUTOSAVE_CARD_UNLOCK, BW_AUTOSAVE_POLL, BW_AUTOSAVE_WAIT,
    BW_AUTOSAVE_FREE, BW_AUTOSAVE_FPU_REOWN, BW_AUTOSAVE_QUARANTINED
} BluewakeAutosavePhase;
typedef enum BluewakeAutosaveReason {
    BW_AUTOSAVE_OK, BW_AUTOSAVE_DISABLED, BW_AUTOSAVE_NEEDS_NATIVE_QUEST,
    BW_AUTOSAVE_HOST_BUSY, BW_AUTOSAVE_UNSAFE_GAME, BW_AUTOSAVE_CARD_BUSY,
    BW_AUTOSAVE_WRONG_CARD, BW_AUTOSAVE_INVALID_SLOT,
    BW_AUTOSAVE_NO_SCRATCH, BW_AUTOSAVE_NATIVE_ERROR,
    BW_AUTOSAVE_STALE_OWNER, BW_AUTOSAVE_SLOW_TRANSACTION, BW_AUTOSAVE_FPU_UNAVAILABLE
} BluewakeAutosaveReason;
typedef struct BluewakeAutosaveStatus {
    BluewakeAutosavePhase phase;
    BluewakeAutosaveReason reason;
    u64 completed, failed, deferred, last_success_tick;
    bool desired, active;
    unsigned interval_seconds;
} BluewakeAutosaveStatus;
// Game-thread-only diagnostic snapshot. A boundary owner is valid only at the
// owned native entry/return SP, not while a callee/worker has its own frame.
typedef struct BluewakeAutosaveAudit {
    BluewakeAutosavePhase phase;
    bool boundary_owner_valid, heap_locked, card_locked, store_queued;
    u32 native_entry, caller_pc, caller_lr, caller_stack, current_pc, current_stack;
    u32 expected_thread, current_thread, current_context, fpu_context;
    u32 player, heap, scratch, native_frame, slot, stage_table;
    u32 serial_high, serial_low, card_command, card_state;
    u32 heap_mutex_owner, heap_mutex_count, card_mutex_owner, card_mutex_count;
    u64 epoch, scene_generation, start_tick, current_tick, due_tick;
} BluewakeAutosaveAudit;

// UI-safe atomic configuration. Default disabled; interval clamped 60..3600s.
void bluewake_autosave_configure(bool enabled, unsigned interval_seconds);
bool bluewake_autosave_desired(void);
void bluewake_autosave_status(BluewakeAutosaveStatus* out);
const char* bluewake_autosave_reason_text(BluewakeAutosaveReason reason);

// Game thread only. Native GAME_LOAD authorizes reset(...,true); machine
// states/attach do not. A genuine manual GuestSaveCompleted authorizes note.
// Reset/detach are AFTER memory replacement or teardown, never an active-call
// cancellation: they drop stale metadata without writing/restoring old RAM.
void bluewake_autosave_attach(CPUState* cpu);
void bluewake_autosave_reset(CPUState* cpu, bool native_quest_loaded);
void bluewake_autosave_note_native_save(CPUState* cpu);
void bluewake_autosave_detach(void);
// Once per VI. host_safe excludes pending state operations, REL prologs,
// callbacks, quick doors/items. This only schedules; never calls the module.
void bluewake_autosave_retrace(CPUState* cpu, u64 tick, bool host_safe,
                              bool input_blocked);
bool bluewake_autosave_observes(u32 address);
// True means consumed/changed PC; return immediately before other hooks.
// Native calls execute in the ordinary scheduler; do not call recursively.
bool bluewake_autosave_dispatch(CPUState* cpu, u32 address);
// While active, hold actor/menu work and ALL host guest-RAM mutation helpers,
// state save/load/reset and relaunch; retain VI/audio/CARD/interrupt servicing.
// Disabling waits for the owned native transaction; it cannot cancel mid-call.
bool bluewake_autosave_active(void);
void bluewake_autosave_audit(CPUState* cpu, BluewakeAutosaveAudit* out);
// Root authorizes a narrow game_events host-save observer only when this is
// true. Permits SERIALIZE/POLL native entry, or their private return token.
// Entry additionally verifies native arguments; return leaves raw R3 intact.
bool bluewake_autosave_owns_native_call(CPUState* cpu, u32 address);
#ifdef __cplusplus
}
#endif
#endif
