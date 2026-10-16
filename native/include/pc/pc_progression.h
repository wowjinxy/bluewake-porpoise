// SPDX-License-Identifier: GPL-3.0-or-later
// Copied native gameplay events. Game-thread API; no game pointer crosses it.
// These DTOs are an in-process interface, never a raw wire/save representation.
#ifndef PC_PROGRESSION_H
#define PC_PROGRESSION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum PcProgressFact { PC_PROGRESS_FACT_TELESCOPE = 1 };
enum PcNativeOrigin { PC_NATIVE_SYSTEM = 0, PC_NATIVE_LOCAL = 1, PC_NATIVE_REMOTE = 2 };
enum PcNativeEventKind {
    PC_NATIVE_LIFECYCLE_CHANGED = 1,
    PC_NATIVE_AWARD_BEGAN = 2,
    PC_NATIVE_AWARD_COMPLETED = 3,
    PC_NATIVE_AWARD_CANCELLED = 4,
    PC_NATIVE_FACT_APPLIED = 5,
    PC_NATIVE_SAVE_REQUESTED = 6,
    PC_NATIVE_SAVE_COMMITTED = 7,
    PC_NATIVE_SAVE_FAILED = 8,
    PC_NATIVE_CARD_READ_REQUESTED = 9,
    PC_NATIVE_CARD_READ_COMPLETED = 10,
    PC_NATIVE_CARD_READ_FAILED = 11,
    PC_NATIVE_QUEST_LOADED = 12,
    PC_NATIVE_QUEST_INITIALIZED = 13,
};
enum PcNativeReward { PC_NATIVE_REWARD_ARYLL_TELESCOPE = 1 };
enum PcProgressApplyResult {
    PC_PROGRESS_REJECTED = -1, PC_PROGRESS_STALE = -2,
    PC_PROGRESS_DEFERRED = 0, PC_PROGRESS_APPLIED = 1, PC_PROGRESS_ALREADY = 2,
};

typedef struct PcProgressIdentity {
    uint64_t session_id;
    uint64_t epoch;
    uint64_t card_serial;
    uint64_t save_generation; // process-local successful native writes, not a durable receipt
    uint32_t scene_id;
    uint32_t player_id;
    int32_t quest_slot;
    char profile_id[65]; // copied immutable mounted identity, no path/credential
    char player_name[17];
} PcProgressIdentity;

typedef struct PcNativeEvent {
    uint32_t kind;
    uint32_t origin;
    uint32_t fact_key;
    uint32_t fact_value;
    uint32_t reward_id;
    uint32_t producer_id;
    uint64_t invocation_id;
    PcProgressIdentity identity;
    char stage[9];
    int32_t room;
} PcNativeEvent;

typedef struct PcProgressSnapshot {
    PcProgressIdentity identity;
    uint32_t telescope_owned;
    uint32_t safe_to_apply;
    uint32_t event_overflow;
} PcProgressSnapshot;

// Configure before native CARD startup, with mounted profile identity (64hex or
// "personal"). A different later identity invalidates every pending observation.
int pc_progress_bind_profile(const char* identity);
void pc_progress_frame(void);
void pc_progress_capture(PcProgressSnapshot* out);
// 1 copied event, 0 empty, -1 queue overflow (caller must resnapshot/reconcile).
int pc_progress_pop_event(PcNativeEvent* out);
// Canonical ownership capture is NOT a locally earned reward. Apply only a
// validated incoming fact to the current epoch on the game thread; no echo.
int pc_progress_apply(uint32_t key, uint32_t value, uint64_t expected_epoch);

// Native producer/lifecycle hooks; not an API for creating a reward.
void pc_progress_aryll_begin(uint32_t producer_id, int32_t native_event_id);
void pc_progress_aryll_end(uint32_t producer_id);
void pc_progress_aryll_cancel(uint32_t producer_id);
void pc_progress_item_message_completed(uint32_t item_actor_id, uint32_t item_no);
void pc_progress_item_effect(uint32_t item_actor_id, uint32_t item_no);
void pc_progress_quest_initialized(void);
void pc_progress_quest_loaded(int32_t slot, int32_t success);
void pc_progress_save_prepared(int32_t slot);
void pc_progress_card_request(int32_t save);
void pc_progress_card_result(int32_t save, int32_t native_result);

#ifdef __cplusplus
}
#endif
#endif
