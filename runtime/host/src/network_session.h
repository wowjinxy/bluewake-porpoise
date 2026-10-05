// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_NETWORK_SESSION_H
#define BLUEWAKE_NETWORK_SESSION_H
#include "progression_sync.h"
#ifdef __cplusplus
extern "C" {
#endif
#define BW_NETWORK_PROTOCOL 1u
#define BW_NETWORK_NAMESPACE "BlueWake.WindWaker.GZLE01.Progression"
#define BW_NETWORK_PEERS 8u
#define BW_NETWORK_QUEUE 128u
typedef struct BwNetworkCompatibility {
    char game_id[7];         /* GZLE01 only for schema1. */
    char build_id[65];       /* A stable client/build compatibility digest. */
    char module_digest[65];  /* Exact own-disc translated module digest. */
    char options_digest[65];/* Sorted progression-affecting options only. */
    uint32_t progression_schema;
} BwNetworkCompatibility;
typedef struct BwNetworkConfig {
    BwNetworkCompatibility compatibility;
    char server[256]; uint16_t port; /* Numeric IPv4/IPv6 or localhost; bounded resolver. */
    char room[33];           /* ASCII letters/digits/_/-; not a path. */
    char player_id[33];      /* Stable32lowercase hex; distinct per room player. */
    char player_name[33];
    char room_password[65];  /* Optional room credential; never in a path/log. */
    bool create_room;
} BwNetworkConfig;
typedef enum BwNetworkStatus {
    BW_NETWORK_OFFLINE, BW_NETWORK_CONNECTING, BW_NETWORK_CONNECTED,
    BW_NETWORK_RECONNECTING, BW_NETWORK_REJECTED, BW_NETWORK_FAILED
} BwNetworkStatus;
typedef struct BwNetworkPeer {
    char player_id[33], player_name[33], stage[9];
    int8_t room; float position[3]; bool online;
} BwNetworkPeer;
typedef struct BwNetworkSnapshot {
    BwNetworkStatus status;
    char message[128], room_identity[33];
    uint64_t server_revision, generation, sent, received, reconnects;
    uint16_t local_server_port;
    unsigned pending_outgoing, pending_incoming, peer_count;
    BwNetworkPeer peers[BW_NETWORK_PEERS];
} BwNetworkSnapshot;
typedef struct BwNetworkUpdate {
    BwProgressionDelta delta;
    uint64_t server_revision, generation;
    bool snapshot;           /* Joining/rejoining snapshot also uses whitelist deltas. */
} BwNetworkUpdate;
typedef struct BwNetworkSession BwNetworkSession;
typedef struct BwNetworkServer BwNetworkServer;
/* Thread-safe session controls; all strings are copied and validated. The
 * worker NEVER gets a CPU/RAM pointer. Poll and progression apply belong to the
 * game thread. Saturation fails closed/reconnects; no silent progress drops.
 * Reconnect retains unacknowledged local sequences and receives a fresh full
 * room snapshot. Personal-state export is never implicit. */
bool bw_network_config_valid(const BwNetworkConfig* config,char* error,unsigned error_size);
BwNetworkSession* bw_network_create(void);
void bw_network_destroy(BwNetworkSession* session);
bool bw_network_join(BwNetworkSession* session,const BwNetworkConfig* config);
void bw_network_leave(BwNetworkSession* session);
bool bw_network_submit(BwNetworkSession* session,BwProgressionDelta delta);
bool bw_network_presence(BwNetworkSession* session,const BwGameScene* scene);
bool bw_network_poll(BwNetworkSession* session,BwNetworkUpdate* update);
void bw_network_status(BwNetworkSession* session,BwNetworkSnapshot* snapshot);
/* Standalone/in-process dedicated server. Bounded8clients/16rooms. Default UI
 * host may use127.0.0.1; LAN hosting selects an explicit bind address. */
BwNetworkServer* bw_network_server_start(const char* bind_address,uint16_t port);
/* Durable dedicated host. State is bounded, compatibility/room isolated and
 * acknowledged only after flushed atomic publication. An invalid/locked store
 * fails startup; write failures never fall back to ephemeral acknowledgements.
 * Caller supplies UTF8 directory, which is copied before the worker starts. */
BwNetworkServer* bw_network_server_start_persistent(const char* bind_address,uint16_t port,
    const char* state_directory,char* error,unsigned error_size);
uint16_t bw_network_server_port(BwNetworkServer* server);
void bw_network_server_stop(BwNetworkServer* server);
/* Atomic preferences and safe room CARD route. Routing is selected BEFORE any
 * CARD backend opens. Enabling/changing room while a personal/other-room CARD
 * is mounted requires restart. No personal-card copy/import/fallback occurs.
 * A deterministic namespace contains compatibility + room + player identity;
 * server addresses/passwords are never path components. */
typedef struct BwNetworkPreferences { bool room_mode; BwNetworkConfig config; } BwNetworkPreferences;
bool bw_network_preferences_load(const char* data_dir,BwNetworkPreferences* prefs,char* error,unsigned error_size);
bool bw_network_preferences_save(const char* data_dir,const BwNetworkPreferences* prefs,char* error,unsigned error_size);
bool bw_network_room_card_path(const char* data_dir,const BwNetworkConfig* config,char* path,unsigned path_size,char* error,unsigned error_size);
bool bw_network_same_room(const BwNetworkConfig* mounted,const BwNetworkConfig* requested);
#ifdef __cplusplus
}
#endif
#endif
