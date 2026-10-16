// SPDX-License-Identifier: GPL-3.0-or-later
// Copied native gameplay events. Game-thread API; no game pointer crosses it.
// These DTOs are an in-process interface, never a raw wire/save representation.
#ifndef PC_PROGRESSION_H
#define PC_PROGRESSION_H
#include <stdint.h>
#include "pc/permanent_facts.h"
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
    PC_NATIVE_SAVE_PROGRESS_PENDING = 14, // refused before CARD enqueue; never an I/O failure
    PC_NATIVE_QUEST_LOAD_FAILED = 15, // activation failed; no authorized quest baseline
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
    uint64_t quest_generation; // init/load/profile activation, independent of scene epoch
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
    uint64_t client_save_token; // local durable journal intent; never a host ACK
    uint64_t source_read_invocation; // exact accepted native read; no global hash reuse
    char read_source_sha256[65];
    PcProgressIdentity identity;
    char stage[9];
    int32_t room;
    uint64_t authored_reward_id; // stable authored source; never a runtime actor/process ID
    uint32_t source_kind, source_slot, item_actor_id, item_no;
    uint32_t operation, operation_value; // MAX outcome or one authored STEP
    uint32_t saved_facts_valid;
    // Frozen prepared-save buffer, or actual activated quest init/load baseline.
    // A live observation is never an authorized baseline or a locally earned award.
    uint32_t saved_facts[PC_PERMANENT_FACT_COUNT];
} PcNativeEvent;

typedef struct PcProgressSnapshot {
    PcProgressIdentity identity;
    uint32_t telescope_owned;
    uint32_t safe_to_apply;
    uint32_t event_overflow;
    uint32_t facts_valid;
    uint32_t facts[PC_PERMANENT_FACT_COUNT];
    uint32_t pending_claims, recovery_required;
} PcProgressSnapshot;

enum PcProgressClaimState {
    PC_PROGRESS_CLAIM_ARMED = 0, PC_PROGRESS_CLAIM_COMPLETED = 1,
    PC_PROGRESS_CLAIM_CANCELLED_EFFECT = 2, PC_PROGRESS_CLAIM_STALE = 3,
};
typedef struct PcProgressClaim {
    uint64_t invocation_id, authored_reward_id;
    uint32_t key, operation, value, observed_value, state;
    PcProgressIdentity identity;
    uint32_t durably_queued; // local outbox persisted; does NOT mean room confirmation
    uint32_t completed_proof; // set only by genuine native completion; survives STALE
} PcProgressClaim;
#define PC_PROGRESS_MAX_PENDING_CLAIMS 32u

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
// Copies pending operation claims; no transport pointers or mutable references.
unsigned pc_progress_copy_claims(PcProgressClaim* out, unsigned capacity);
int pc_progress_claim_durable(uint64_t invocation_id, uint64_t authored_reward_id,
    uint64_t expected_epoch);
// Caller must FIRST durably enqueue this exact immutable operation. This can
// retire an old completed correlation after quest activation; it cannot grant
// an item, acknowledge a host, or turn an armed/cancelled presentation into one.
int pc_progress_claim_journaled(uint64_t invocation_id, uint64_t authored_reward_id,
    uint32_t key, uint32_t operation, uint32_t value);
// Receipt-backed canonical reconciliation. STEP keys may correct local capacity
// downward; current consumable counts and saved X/Y/Z assignments stay local.
int pc_progress_resolve_claim(uint64_t invocation_id, uint64_t authored_reward_id,
    uint32_t key, uint32_t canonical_value, uint64_t expected_epoch);
int pc_progress_save_allowed(void);
const char* pc_progress_save_block_reason(void);
void pc_progress_save_refused(void);
// Actual quest-save producer tags the immediate native STORE, after packing and
// checksumming. A diagnostic memory_to_card or unarmed metadata STORE is not a
// quest commit. The controller clears this tag even if its mutex could not lock.
void pc_progress_arm_quest_save(int32_t slot);
void pc_progress_cancel_card_save(void);
// Called under the native CARD mutex before copying/signalling a STORE.
// Room mode requires a durable local journal intent, never a fictitious host ACK.
int pc_progress_prepare_card_save(void);
// Called under the same mutex before signalling a native RESTORE. Hash failure
// is observational and must not replace the native read/result with a fake one.
void pc_progress_prepare_card_read(void);

// Native producer/lifecycle hooks; not an API for creating a reward.
void pc_progress_aryll_begin(uint32_t producer_id, int32_t native_event_id);
void pc_progress_aryll_end(uint32_t producer_id);
void pc_progress_aryll_cancel(uint32_t producer_id);
void pc_progress_item_message_completed(uint32_t item_actor_id, uint32_t item_no);
void pc_progress_item_effect(uint32_t item_actor_id, uint32_t item_no);
void pc_progress_item_presented(uint32_t item_actor_id, uint32_t item_no);
void pc_progress_chest_item(uint32_t producer_id, uint32_t item_actor_id,
    uint32_t item_no, uint32_t treasure_no, int32_t room);
void pc_progress_native_event_completed(int32_t native_event_id);
void pc_progress_song_begin(uint32_t player_id, uint32_t melody);
void pc_progress_song_effect(uint32_t player_id, uint32_t melody);
void pc_progress_song_message_completed(uint32_t player_id, uint32_t melody);
uint64_t pc_progress_script_item_begin(uint32_t producer_id, uint32_t item_no, uint32_t authored_step);
void pc_progress_script_item_effect(uint64_t invocation_id);
void pc_progress_quest_initialized(void);
void pc_progress_quest_loaded(int32_t slot, int32_t success);
void pc_progress_save_prepared(int32_t slot);
void pc_progress_card_request(int32_t save);
void pc_progress_card_result(int32_t save, int32_t native_result);

#ifdef __cplusplus
}
#endif
#endif
