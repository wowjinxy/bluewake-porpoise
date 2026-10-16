// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from BlueWake runtime/host/src/network_session.h; see network/PROVENANCE.md.
#ifndef PC_NATIVE_NETWORK_SESSION_H
#define PC_NATIVE_NETWORK_SESSION_H
#include <stdbool.h>
#include <stdint.h>
#include "pc/permanent_facts.h"
#ifdef __cplusplus
extern "C" {
#endif
#define PC_NETWORK_PROTOCOL 3u
#define PC_NETWORK_NAMESPACE "BlueWake.Native.GZLE01.Permanent"
#define PC_NETWORK_SCHEMA 2u
#define PC_NETWORK_FACTS PC_PERMANENT_FACT_COUNT
#define PC_NETWORK_FACT_TELESCOPE PC_PERMANENT_TELESCOPE
#define PC_NETWORK_PEERS 8u
#define PC_NETWORK_QUEUE 128u
#define PC_NETWORK_REWARDS 4096u

/* Only copied permanent facts enter this module. Scene timing, positions,
 * consumables, selections, reward-suppressing flags and game pointers do not. */
typedef struct PcNetworkFact { uint16_t key; uint32_t value; } PcNetworkFact;
typedef struct PcNetworkFacts { uint32_t values[PC_NETWORK_FACTS]; } PcNetworkFacts;
bool pc_network_fact_valid(PcNetworkFact fact);
bool pc_network_fact_merge(PcNetworkFacts* state, PcNetworkFact fact);
void pc_network_fact_clear(PcNetworkFacts* state);
const char* pc_network_fact_name(uint16_t key);
/* One authored producer -> one immutable operation. IDs contain a producer
 * kind in the high byte and a deterministic authored identity in the low56;
 * never process IDs, memory addresses, local epochs or connection sequences. */
typedef struct PcNetworkReward {
    uint64_t reward_id;
    uint16_t key, operation;
    uint32_t value;
} PcNetworkReward;
bool pc_network_reward_valid(PcNetworkReward reward);

typedef struct PcNetworkCompatibility {
    char game_id[7];
    char build_id[65]; /* native semantic build/rules identity, not source commit */
    char semantic_digest[65], rules_digest[65];
    uint32_t progression_schema, disc_revision;
} PcNetworkCompatibility;
void pc_network_native_compatibility(PcNetworkCompatibility* out);
bool pc_network_normalize_endpoint(const char* address, char* out, unsigned capacity);

typedef struct PcNetworkConfig {
    PcNetworkCompatibility compatibility;
    char server[256]; uint16_t port;
    char room[33], player_id[33], player_name[33], room_password[65];
    bool create_room;
} PcNetworkConfig;
typedef enum PcNetworkStatus {
    PC_NETWORK_OFFLINE, PC_NETWORK_CONNECTING, PC_NETWORK_CONNECTED,
    PC_NETWORK_RECONNECTING, PC_NETWORK_REJECTED, PC_NETWORK_FAILED
} PcNetworkStatus;
typedef struct PcNetworkPeer { char player_id[33], player_name[33]; bool online; } PcNetworkPeer;
typedef struct PcNetworkSnapshot {
    PcNetworkStatus status;
    char message[128], room_identity[33];
    uint64_t server_revision, generation, sent, received, reconnects;
    uint16_t local_server_port;
    unsigned pending_outgoing, pending_incoming, peer_count;
    PcNetworkPeer peers[PC_NETWORK_PEERS];
} PcNetworkSnapshot;
typedef struct PcNetworkUpdate {
    PcNetworkFact delta;
    uint64_t server_revision, generation;
    bool snapshot;
    PcNetworkReward reward; /* zero for canonical-only reconciliation/snapshot */
} PcNetworkUpdate;
typedef struct PcNetworkSession PcNetworkSession;
typedef struct PcNetworkServer PcNetworkServer;

/* Explicit opt-in only. Join/submit/poll/status copy values under a mutex;
 * worker threads never call game code. Submit failure retains caller ownership.
 * Poll/application must be drained only by the game-thread native adapter.
 * Automatic reconnect retains unacknowledged sequences and replaces incoming
 * work with an authoritative full snapshot. Leave explicitly discards queues. */
bool pc_network_config_valid(const PcNetworkConfig*, char* error, unsigned error_size);
PcNetworkSession* pc_network_create(void);
void pc_network_destroy(PcNetworkSession*);
bool pc_network_join(PcNetworkSession*, const PcNetworkConfig*);
void pc_network_leave(PcNetworkSession*);
bool pc_network_submit(PcNetworkSession*, PcNetworkFact);
/* Raw canonical submissions reject server-authoritative STEP keys. Genuine
 * awards use submit_reward; failure retains caller ownership. Duplicate exact
 * producer operations are acknowledged without another step or revision. */
bool pc_network_submit_reward(PcNetworkSession*, PcNetworkReward);
bool pc_network_poll(PcNetworkSession*, PcNetworkUpdate*);
void pc_network_status(PcNetworkSession*, PcNetworkSnapshot*);
/* Copies one complete validated connected state at a single generation and
 * revision. On insufficient capacity returns false and reports needed count;
 * never publishes a partial facts/ledger snapshot. No game calls on workers. */
bool pc_network_copy_state(PcNetworkSession*, PcNetworkFacts*, PcNetworkReward*,
    unsigned capacity, unsigned* count, PcNetworkSnapshot*);
/* -1 unavailable,0 absent,1 exact known producer (out optional). */
int pc_network_reward_known(PcNetworkSession*, uint64_t reward_id, PcNetworkReward* out);

/* Explicit owned IPv4 bind. Port zero selects an ephemeral port. Durable
 * startup exclusively leases the supplied directory and ACKs only after an
 * atomic flushed room+receipt publication. Failure never enables ephemeral
 * fallback. The ephemeral entry is for standalone tests, not room gameplay. */
PcNetworkServer* pc_network_server_start(const char* bind_address, uint16_t port);
PcNetworkServer* pc_network_server_start_persistent(const char* bind_address, uint16_t port,
    const char* state_directory, char* error, unsigned error_size);
uint16_t pc_network_server_port(PcNetworkServer*);
void pc_network_server_stop(PcNetworkServer*);
#ifdef __cplusplus
}
#endif
#endif
