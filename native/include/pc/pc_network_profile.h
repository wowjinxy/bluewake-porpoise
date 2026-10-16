// SPDX-License-Identifier: GPL-3.0-or-later
// Native directory/GCI isolation; configuration principles adapted from the
// preserved BlueWake network_preferences.cpp, not its guest .card representation.
#ifndef PC_NETWORK_PROFILE_H
#define PC_NETWORK_PROFILE_H
#include "pc/network/network_session.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct PcNetworkProfile {
    bool enabled; /* room storage is selected only at process startup */
    bool host;
    char bind[256];
    PcNetworkConfig config;
} PcNetworkProfile;

void pc_network_profile_defaults(PcNetworkProfile*);
bool pc_network_profile_valid(const PcNetworkProfile*, char* error, unsigned capacity);
/* A missing optional default file means personal mode; a missing explicit file
 * or any malformed/unsafe file is an error. These functions never log values. */
bool pc_network_profile_load(const char* file, bool required, PcNetworkProfile*, char* error, unsigned capacity);
bool pc_network_profile_save(const char* file, const PcNetworkProfile*, char* error, unsigned capacity);
/* Bounded route calculation only. Credentials/display names never enter it.
 * The caller must refuse an explicit COS_CARD_DIR together with room mode. */
bool pc_network_profile_route(const PcNetworkProfile*, const char* room_root,
    const char* personal_base, char* card_base, unsigned card_capacity,
    char* identity, unsigned identity_capacity, char* error, unsigned error_capacity);

typedef struct PcNetworkProfileMount PcNetworkProfileMount;
/* Prepare/lease a directory without opening CARD or reading/copying a GCI.
 * The instance API permits isolated filesystem qualification without a game. */
PcNetworkProfileMount* pc_network_profile_mount_create(const PcNetworkProfile*,
    const char* room_root, const char* personal_base, char* error, unsigned capacity);
void pc_network_profile_mount_destroy(PcNetworkProfileMount*);
const char* pc_network_profile_mount_card_base(const PcNetworkProfileMount*);
const char* pc_network_profile_mount_identity(const PcNetworkProfileMount*);

/* Production startup freezes exactly one successful mount. No remount API.
 * Null getters before successful preparation must never mean personal fallback. */
bool pc_network_profile_prepare(const PcNetworkProfile*, const char* room_root,
    const char* personal_base, char* error, unsigned capacity);
const char* pc_network_profile_mounted_card_base(void);
const char* pc_network_profile_mounted_identity(void);
bool pc_network_profile_mounted(PcNetworkProfile* copy);

#ifdef __cplusplus
}
#endif
#endif
