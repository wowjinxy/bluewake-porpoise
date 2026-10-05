// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_NETWORK_GAME_H
#define BLUEWAKE_NETWORK_GAME_H
#include "network_session.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BwNetworkGameSnapshot {
    bool mounted_room_mode, native_load_authorized, clean_boot_authorized;
    bool pending_command, exporting, local_host;
    bool persistent_host_configured, persistent_host;
    unsigned deferred_updates;
    uint64_t captured, applied, unchanged, invalid, queue_failures;
    BwNetworkConfig mounted;
    BwNetworkSnapshot session;
    char message[160];
} BwNetworkGameSnapshot;
/* Call once at startup BEFORE any CARD backend opens. A selected room config
 * is immutable until process restart; false room mode never permits joining.
 * Root computes/validates the real compatibility manifest before this call. */
bool bw_network_game_prepare(const BwNetworkPreferences* mounted);
/* Production startup: fixes a copied <data_dir>/Network/Server route before
 * CARD opens. Directory/lease creation happens only when actually hosting.
 * Personal mode needs no storage path. Legacy prepare remains ephemeral for
 * explicit source-only fixtures; production must use this durable entry. */
bool bw_network_game_prepare_with_store(const BwNetworkPreferences* mounted,const char* data_dir);
/* Game thread only, after GameEvents attach; call retrace after its retrace.
 * Detach cancels authorization/queues and stops workers. A second attach has
 * no clean-boot token: genuine native CARD load must authorize it again. */
void bw_network_game_attach(CPUState* cpu);
void bw_network_game_retrace(CPUState* cpu);
void bw_network_game_detach(void);
/* UI-safe copied commands. Success means queued, not connected. Every config
 * must match the mounted room/card manifest/player/endpoint. Leave pauses
 * capture and cannot change the mounted CARD route. Autojoin occurs on the
 * initial room-mode attach; local create_room uses a loopback host by default.
 * Request_host's bind_address is numeric IPv4; LAN binding is explicit. */
bool bw_network_game_request_join(const BwNetworkConfig* config);
bool bw_network_game_request_host(const BwNetworkConfig* config,const char* bind_address);
void bw_network_game_request_leave(void);
void bw_network_game_snapshot(BwNetworkGameSnapshot* snapshot);
#ifdef __cplusplus
}
#endif
#endif
