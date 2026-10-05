// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_CARD_MANAGER_H
#define BLUEWAKE_CARD_MANAGER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define BW_CARD_MANAGER_PATH_CAPACITY 4096u
typedef struct BwCardManager BwCardManager;
typedef enum BwCardManagerCode {
    BW_CARD_MANAGER_OK,
    BW_CARD_MANAGER_NO_PENDING,
    BW_CARD_MANAGER_BUSY,
    BW_CARD_MANAGER_INVALID,
    BW_CARD_MANAGER_IO_ERROR,
    BW_CARD_MANAGER_INCOMPLETE
} BwCardManagerCode;
typedef struct BwCardManagerOutcome {
    BwCardManagerCode code;
    bool startup_ready; // Only OK/NO_PENDING after a fully committed startup apply.
    bool card_changed;  // Publication happened, even if later durability/cleanup failed.
    bool backup_valid;
    char backup_path[BW_CARD_MANAGER_PATH_CAPACITY];
    char recovery_path[BW_CARD_MANAGER_PATH_CAPACITY];
    char message[320];
} BwCardManagerOutcome;
typedef struct BwCardManagerConfig {
    const char* card_path;
    const char* backup_directory;
    // Optional live-backup bridge, called on the UI/input thread. begin must
    // confirm the expected card is this process's active persistent card and
    // wait for an in-flight dispatch, then hold the runtime card mutex for the
    // entire copy so other dispatches wait. end releases it on EVERY return
    // path. This snapshots persistent bytes; it does not establish that the
    // guest's multi-write save transaction/SaveSync has completed. The manager
    // never closes/reopens HLE or clears callbacks.
    // Without this pair, Backup acquires GXRuntime's card lock and therefore
    // refuses to snapshot any running backend. Stage never needs suspension.
    bool (*begin_snapshot)(const char* expected_card_path, void* user);
    void (*end_snapshot)(void* user);
    void* user;
} BwCardManagerConfig;
typedef struct BwCardManagerPending {
    bool present, valid;
    uint64_t bytes;
    char path[BW_CARD_MANAGER_PATH_CAPACITY];
} BwCardManagerPending;
typedef struct BwCardManagerBackup {
    bool valid;
    uint64_t bytes;
    int64_t modified_unix_seconds;
    char path[BW_CARD_MANAGER_PATH_CAPACITY];
} BwCardManagerBackup;

BwCardManager* bluewake_card_manager_create(const BwCardManagerConfig* config, BwCardManagerOutcome* outcome);
void bluewake_card_manager_destroy(BwCardManager* manager);
bool bluewake_card_manager_backup(BwCardManager* manager, BwCardManagerOutcome* outcome);
// Accepts GXRuntime DOLCARD1 or validated GZLE01 gczelda GCI/raw saves. Original
// imports preserve the whole Wind Waker game file (all three logs/pictures),
// never repair progress/checksums, and must pass actual backend validation.
// Validates the copied bytes, then atomically stages <card_path>.pending. Live
// card bytes remain unchanged, and a previous pending request survives failure.
bool bluewake_card_manager_stage(BwCardManager* manager, const char* source, BwCardManagerOutcome* outcome);
bool bluewake_card_manager_pending(BwCardManager* manager, BwCardManagerPending* pending, BwCardManagerOutcome* outcome);
// Dequeues by retaining an immutable .cancelled-* file rather than deleting it.
bool bluewake_card_manager_cancel(BwCardManager* manager, BwCardManagerOutcome* outcome);
bool bluewake_card_manager_list_backups(BwCardManager* manager, BwCardManagerBackup* backups,
                                      size_t capacity, size_t* total, BwCardManagerOutcome* outcome);
// MUST run before bluewake_card_runtime_open/dol_hle_card_open. Acquires the
// same <card>.lock as GXRuntime; refusal leaves live backend and queue intact.
// Current bytes, including a damaged card, are preserved to a unique backup
// before replacement. Do not open the backend unless startup_ready is true.
// Failure retains the staged candidate/backup for recovery. Success consumes
// the queue into .applied-* so it can never overwrite a later gameplay save.
BwCardManagerCode bluewake_card_manager_apply_at_startup(BwCardManager* manager, BwCardManagerOutcome* outcome);

#ifdef BLUEWAKE_CARD_MANAGER_TEST
typedef enum BwCardManagerTestFailure {
    BW_CARD_TEST_NONE,
    BW_CARD_TEST_BACKUP_WRITE,
    BW_CARD_TEST_STAGE_FLUSH,
    BW_CARD_TEST_STAGE_PUBLISH,
    BW_CARD_TEST_CURRENT_PUBLISH,
    BW_CARD_TEST_AFTER_CURRENT_SYNC,
    BW_CARD_TEST_CONSUME_PENDING,
    BW_CARD_TEST_CONVERSION_FLUSH
} BwCardManagerTestFailure;
void bluewake_card_manager_test_fail_once(BwCardManagerTestFailure failure);
#endif
#ifdef __cplusplus
}
#endif
#endif
