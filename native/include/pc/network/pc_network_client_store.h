// SPDX-License-Identifier: GPL-3.0-or-later
// Native copied-value journal; uses the credited network atomic/digest helpers.
#ifndef PC_NETWORK_CLIENT_STORE_H
#define PC_NETWORK_CLIENT_STORE_H
#include "pc/network/network_session.h"
#ifdef __cplusplus
extern "C" {
#endif
#define PC_NETWORK_CLIENT_SLOTS 3u
#define PC_NETWORK_CLIENT_MAX_FILE (512u*1024u)
typedef struct PcNetworkClientIntent {
    bool active, provisional;
    uint64_t token, revision;
    PcNetworkFacts facts;
    unsigned pending_count;
    PcNetworkReward pending[PC_NETWORK_QUEUE];
} PcNetworkClientIntent;
typedef struct PcNetworkClientReceipt {
    bool valid, recovered, provisional, reconciled;
    uint32_t quest_slot;
    uint64_t token, revision;
    char gci_sha256[65];
    PcNetworkFacts facts;
    unsigned pending_count;
    PcNetworkReward pending[PC_NETWORK_QUEUE];
} PcNetworkClientReceipt;
typedef struct PcNetworkClientState {
    PcNetworkCompatibility compatibility;
    char profile_identity[65], room_identity[33];
    uint64_t serial, revision;
    bool canonical_present;
    PcNetworkFacts facts;
    unsigned reward_count, outgoing_count;
    PcNetworkReward rewards[PC_NETWORK_REWARDS]; /* strictly increasing IDs */
    PcNetworkReward outgoing[PC_NETWORK_QUEUE]; /* completed, not native observations */
    PcNetworkClientIntent intents[PC_NETWORK_CLIENT_SLOTS];
    PcNetworkClientReceipt receipts[PC_NETWORK_CLIENT_SLOTS];
} PcNetworkClientState;
typedef struct PcNetworkClientStore PcNetworkClientStore;
typedef struct PcNetworkClientProjection {
    PcNetworkFacts facts;
    uint64_t serial, revision;
    bool provisional;
    char room_identity[33];
} PcNetworkClientProjection;
typedef enum PcNetworkClientLoadDisposition {
    PC_NETWORK_CLIENT_LOAD_NONE=0,
    PC_NETWORK_CLIENT_LOAD_PERSISTED=1,
    PC_NETWORK_CLIENT_LOAD_NOT_PRESENT=2
} PcNetworkClientLoadDisposition;

/* Explicit isolated mounted-profile directory; own exclusive client.lock.
 * Existing incompatible/corrupt/link state fails closed without replacement.
 * All calls belong to one game-thread owner; there is no background game I/O. */
PcNetworkClientStore* pc_network_client_open(const char* directory,
    const PcNetworkCompatibility*,const char* profile_identity,char* error,unsigned capacity);
void pc_network_client_close(PcNetworkClientStore*);
bool pc_network_client_copy(PcNetworkClientStore*,PcNetworkClientState*);
/* Nonmutating offline replay target from ONE validated durable canonical state
 * plus each deduplicated pending authored operation once. No inventory polls,
 * ACKs, queue pruning or revision advance. Absent ammo capacity is the original
 *30; wallet zero remains base. Caller binds fresh native quest identity/epoch,
 * uses safe native application and rechecks serial before saving/replaying. */
bool pc_network_client_projection(PcNetworkClientStore*,PcNetworkClientProjection*,char* error,unsigned capacity);
/* Must succeed BEFORE transport submission. Exact duplicate is idempotent;
 * conflicting producer operations and the128-entry bound refuse new work. */
bool pc_network_client_enqueue(PcNetworkClientStore*,PcNetworkReward,char* error,unsigned capacity);
/* Must succeed BEFORE applying received native effects. Binds first owned
 * roomId once; later room identity changes refuse, even at the same endpoint.
 * Atomically stages canonical facts+full ledger and removes only matching
 * acknowledged outbox claims. Revision/known producers never regress. */
bool pc_network_client_stage(PcNetworkClientStore*,const char* room_identity,
    const PcNetworkFacts*,const PcNetworkReward*,unsigned count,uint64_t revision,
    char* error,unsigned capacity);
/* Native save is a separate transaction. Persist intent before CARD enqueue,
 * then commit only AFTER a real successful native write of these frozen facts.
 * No unacknowledged completed claims may cross the save gate. */
bool pc_network_client_prepare_save(PcNetworkClientStore*,unsigned quest_slot,
    const PcNetworkFacts* frozen,uint64_t canonical_revision,uint64_t* token,char* error,unsigned capacity);
/* Explicit offline mode: frozen STEP capacities must equal canonical plus the
 * deduplicated durable outbox projection. The copied pending operations stay
 * with the provisional intent/receipt even if the live outbox later drains.
 * Its revision identifies the known baseline, NEVER confirmed projected state. */
bool pc_network_client_prepare_offline_save(PcNetworkClientStore*,unsigned quest_slot,
    const PcNetworkFacts* frozen,uint64_t canonical_revision,uint64_t* token,char* error,unsigned capacity);
bool pc_network_client_cancel_save(PcNetworkClientStore*,uint64_t token,char* error,unsigned capacity);
bool pc_network_client_commit_save(PcNetworkClientStore*,uint64_t token,
    const char* actual_gci_path,char* error,unsigned capacity);
/* Hashes the actual bounded GZLE01 GCI under a stable read handle; no CARD bytes
 * enter the journal. This structural hash is not proof of native save/load. */
bool pc_network_client_hash_gci(const char* actual_gci_path,char sha256[65],char* error,unsigned capacity);
/* Recovery requires a real fresh native load, with source hash measured before
 * that read. Rehash must still match; loaded facts must cover the saved intent
 * (STEP exact). A recovered receipt proves persisted permanent facts, never
 * invents a native-save event or claims a two-file atomic transaction. */
bool pc_network_client_recover_loaded(PcNetworkClientStore*,unsigned quest_slot,
    const char* actual_gci_path,const char* source_sha256,const PcNetworkFacts* freshly_loaded,
    bool* recovered,char* error,unsigned capacity);
/* Explicit crash-before-write disposition, requiring the same real fresh-load
 * proof as recovery. A deficient snapshot retires ONLY this slot's intent and
 * preserves its old receipt+all completed outbox claims; it never emits a
 * native save/failure event. Ambiguous higher STEP ranks, invalid facts and
 * I/O/hash failures retain the intent. Other-slot intents are untouched. */
bool pc_network_client_reconcile_loaded(PcNetworkClientStore*,unsigned quest_slot,
    const char* actual_gci_path,const char* source_sha256,const PcNetworkFacts* freshly_loaded,
    PcNetworkClientLoadDisposition*,char* error,unsigned capacity);
/* A provisional receipt becomes confirmed only after every retained producer
 * has a matching durable room receipt AND a fresh unchanged native GCI load
 * contains current canonical facts. No synthetic native-save event is emitted. */
bool pc_network_client_confirm_loaded(PcNetworkClientStore*,unsigned quest_slot,
    const char* actual_gci_path,const char* source_sha256,const PcNetworkFacts* freshly_loaded,
    bool* confirmed,char* error,unsigned capacity);
#ifdef __cplusplus
}
#endif
#endif
