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
bool pc_network_game_start(bool host);
void pc_network_game_leave(void);
void pc_network_game_frame(void);
void pc_network_game_snapshot(PcNetworkGameSnapshot*);
#ifdef __cplusplus
}
#endif
#endif
