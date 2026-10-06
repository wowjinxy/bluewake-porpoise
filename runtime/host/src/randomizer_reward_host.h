// SPDX-License-Identifier: GPL-3.0-or-later
// Experimental single LinkUG reward owner. Unlinked until main's real lifetime
// and PC seams are integrated and qualified. No UI or native helper invocation.
#ifndef BLUEWAKE_RANDOMIZER_REWARD_HOST_H
#define BLUEWAKE_RANDOMIZER_REWARD_HOST_H
#include "core/cpu.h"
#include "loaded_code_admission.h"
#include "randomizer_rel_owner.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
#define BW_REWARD_HOST_NOEXCEPT noexcept
extern "C" {
#else
#define BW_REWARD_HOST_NOEXCEPT
#endif

typedef struct BwRandomizerRewardHost BwRandomizerRewardHost;
typedef enum BwRewardHostPhase {
    BW_REWARD_HOST_AWAITING_CARD, BW_REWARD_HOST_AWAITING_NATIVE_LOAD,
    BW_REWARD_HOST_READY, BW_REWARD_HOST_CREATING, BW_REWARD_HOST_AWAITING_DELETE,
    BW_REWARD_HOST_AWARDING, BW_REWARD_HOST_UNSAVED, BW_REWARD_HOST_SAVE_PENDING,
    BW_REWARD_HOST_CHECKPOINT_PENDING, BW_REWARD_HOST_STOP_REQUIRED,
    BW_REWARD_HOST_STOPPED, BW_REWARD_HOST_DRAIN_UNAVAILABLE
} BwRewardHostPhase;
typedef struct BwRewardHostStartup {
    const char* dedicated_seed_directory_utf8;
    const uint8_t* canonical_profile;
    size_t canonical_profile_size;
    uint8_t origin_card_sha256[32];
    uint8_t quest;
    // Explicit immutable input for a NEW pair only. No source/personal path.
    const uint8_t* explicit_initial_card;
    size_t explicit_initial_card_size;
    // Already admitted by the one actual module owner. Never a second prepare.
    BwIcLoadedCode* admitted_code;
    uint64_t code_generation;
    const StaticRecompModuleDesc* admitted_descriptor;
} BwRewardHostStartup;
// The old main/HOSTVARS slot stride stays12 bytes. These snapshots never mint
// materialization authority; the actual copy/association hooks below do that.
typedef struct BwRewardHostRelSlot { uint32_t owner, address, capacity; } BwRewardHostRelSlot;
typedef struct BwRewardHostBacking {
    uint32_t linked_start, size;
    uint8_t* storage; // Exact storage used by BOTH actual alias tables at commit.
} BwRewardHostBacking;
typedef enum BwRewardHostChange {
    BW_REWARD_HOST_SHARED_ALIAS_CHANGE, BW_REWARD_HOST_CPU_REPLACED,
    BW_REWARD_HOST_RAM_REPLACED, BW_REWARD_HOST_MODULE_REPLACED,
    BW_REWARD_HOST_STATE_LOAD, BW_REWARD_HOST_MACHINE_RESET,
    BW_REWARD_HOST_STATE_CAPTURE, BW_REWARD_HOST_SHUTDOWN
} BwRewardHostChange;
typedef struct BwRewardHostStatus {
    BwRewardHostPhase phase;
    bool stop_required, native_load_authorized, hold_seed_selections;
    bool drain_native_save, hold_guest_mutators;
    uint8_t quest, reward_item;
    uint64_t confirmed_generation, ledger_revision, native_card_load;
    uint64_t substitutions, completed_awards, published_saves;
    uint8_t confirmed_card_sha256[32];
    char reason[192]; // Copied host history only, no CPU/RAM reads.
} BwRewardHostStatus;

// All operations belong to the creator/game thread; C++ exceptions are contained.
// OFF means main creates no object, installs no hooks, and calls none of these.
// BEFORE backend open: decode exact one-placement profile, verify admitted code
// digest, open/restore Session, inspect compatible confirmed native CARD baseline.
BwRandomizerRewardHost* bw_randomizer_reward_host_create(const BwRewardHostStartup*) BW_REWARD_HOST_NOEXCEPT;
bool bw_randomizer_reward_host_card_path(BwRandomizerRewardHost*, char*, size_t) BW_REWARD_HOST_NOEXCEPT;
// Root opens only the copied working path. Refuse an already-mounted other CARD.
// Verify exact active path and a locked copy of the last confirmed generation.
bool bw_randomizer_reward_host_backend_opened(BwRandomizerRewardHost*) BW_REWARD_HOST_NOEXCEPT;
// AFTER CPU init/32MiB RAM expansion/DOL load and both shared alias commits.
// Supply exactly TBOX and Demo_Item .data registrations from actual commits.
bool bw_randomizer_reward_host_cpu_ready(BwRandomizerRewardHost*, CPUState*,
    const BwRewardHostBacking*, size_t) BW_REWARD_HOST_NOEXCEPT;
// BEFORE any borrowed storage changes. STATE recovery is unsupported. Capture
// during a substituted transaction retires it; no proof is serialized/restored.
// False means the genuine manual/owned autosave pipeline must drain first;
// caller MUST NOT perform the proposed replacement/capture/teardown yet.
bool bw_randomizer_reward_host_before_owner_change(BwRandomizerRewardHost*, BwRewardHostChange) BW_REWARD_HOST_NOEXCEPT;
bool bw_randomizer_reward_host_shared_backing_committed(BwRandomizerRewardHost*, CPUState*,
    const BwRewardHostBacking*, size_t) BW_REWARD_HOST_NOEXCEPT;
// Actual materialization/reuse, not idempotent cache hits. Revoke BEFORE raw copy
// or alias removal; mint only AFTER real copy. Association reads owner+10 itself.
void bw_randomizer_reward_host_rel_will_change(BwRandomizerRewardHost*, uint32_t) BW_REWARD_HOST_NOEXCEPT;
bool bw_randomizer_reward_host_rel_materialized(BwRandomizerRewardHost*, uint32_t,
    uint32_t, uint32_t) BW_REWARD_HOST_NOEXCEPT;
bool bw_randomizer_reward_host_rel_loader_associated(BwRandomizerRewardHost*, uint32_t,
    uint32_t) BW_REWARD_HOST_NOEXCEPT;
// Copy the CURRENT main arrays before a relevant observation; no retained source
// pointers or caller-supplied tokens. Metadata comes from the admitted descriptor.
bool bw_randomizer_reward_host_rel_tables(BwRandomizerRewardHost*,
    const BwRandomizerRelAlias*, size_t, const BwRewardHostRelSlot*, size_t) BW_REWARD_HOST_NOEXCEPT;

// Before autosave/GameEvents at BOTH serviced edges and first-PC path. Address
// rejection precedes CPU dereference. An armed real caller return cannot be skipped.
bool bw_randomizer_reward_host_observes(const BwRandomizerRewardHost*, const CPUState*, uint32_t) BW_REWARD_HOST_NOEXCEPT;
// Only an Accepted creation changes r4. No PC, budget, RAM, timer or return edits.
// False requests orderly stop, after an active genuine autosave pipeline drains.
bool bw_randomizer_reward_host_dispatch(BwRandomizerRewardHost*, CPUState*, uint32_t) BW_REWARD_HOST_NOEXCEPT;
// At an owned boundary BEFORE guest resume; no recursive GameEvents operation.
// Correlated real save -> locked same working CARD -> payload/preservation checks
// -> Session publication. No callback does disk publication or infers success.
bool bw_randomizer_reward_host_maintenance(BwRandomizerRewardHost*) BW_REWARD_HOST_NOEXCEPT;
void bw_randomizer_reward_host_status(const BwRandomizerRewardHost*, BwRewardHostStatus*) BW_REWARD_HOST_NOEXCEPT;
// BEFORE backend close: revoke/unsubscribe without guest restoration/helper calls.
// Refuses while a genuine native save is draining; caller services the real
// scheduler/VI/CARD/callbacks and retries, without starting other host mutations.
bool bw_randomizer_reward_host_stop(BwRandomizerRewardHost*) BW_REWARD_HOST_NOEXCEPT;
// ONLY after actual main determines native continuation is impossible (fatal
// exception/unmapped dispatch/storage loss). Revoke before freeing borrowed
// storage, preserve the last confirmed pair, and report no completed drain.
// This makes close/destroy possible; it invokes no autosave/native cancellation,
// does not roll back guest state, and is never a timeout/budget shortcut.
void bw_randomizer_reward_host_abandon_unavailable(BwRandomizerRewardHost*) BW_REWARD_HOST_NOEXCEPT;
// AFTER native backend close and BEFORE admitted-code revoke/unload. Refuses while
// its backend remains open; wrong-thread calls never destroy the owner.
bool bw_randomizer_reward_host_destroy(BwRandomizerRewardHost*) BW_REWARD_HOST_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#endif
