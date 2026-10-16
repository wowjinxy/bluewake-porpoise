// Native game-thread integration. Workers receive copied transport facts only.
#ifndef PC_NETWORK_GAME_H
#define PC_NETWORK_GAME_H
#include "pc/pc_network_profile.h"
#include "pc/pc_progression.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PcNetworkGameSnapshot {
    bool prepared, room_mode, local_host, restart_pending, authorized;
    bool pending_local, pending_remote, unsaved_remote, failed;
    bool provisional_save;
    uint64_t local_awards, remote_applies, native_saves, native_loads;
    PcNetworkSnapshot session;
    PcProgressSnapshot progress;
    char message[192];
} PcNetworkGameSnapshot;
// Called exactly once before CARDInit. Invalid explicit configuration refuses boot.
bool pc_network_game_prepare(const char* user_directory, char* error, unsigned capacity);
const char* pc_network_game_card_base(void);
bool pc_network_game_room_mode(void);
bool pc_network_game_profile(PcNetworkProfile* copy);
// Saves next-start configuration only; never changes the mounted profile.
bool pc_network_game_save_profile(const PcNetworkProfile*, char* error, unsigned capacity);
// Actual armed native quest save only, before CARD enqueue. A nonzero token
// means flushed LOCAL intent/outbox, never a host acknowledgement or disk save.
int pc_network_game_native_save_prepare(const PcProgressIdentity*,
    const uint32_t frozen[PC_PERMANENT_FACT_COUNT], uint64_t* token);
// Observes the actual GCI before a native CARD read; failure never fabricates
// an I/O result. Fresh-load recovery still needs native loaded-state evidence.
void pc_network_game_native_read_prepare(char sha256[65]);
bool pc_network_game_start(bool host);
void pc_network_game_leave(void);
void pc_network_game_frame(void);
void pc_network_game_snapshot(PcNetworkGameSnapshot*);
// Game-thread copied presence only; no session pointer or native object escapes.
bool pc_network_game_publish_pose(const PcNetworkPose*);
bool pc_network_game_copy_presence(PcNetworkPresenceSnapshot*);
#ifdef __cplusplus
}
#endif
#endif
