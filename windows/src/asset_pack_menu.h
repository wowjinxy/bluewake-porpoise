// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_WINDOWS_ASSET_PACK_MENU_H
#define BLUEWAKE_WINDOWS_ASSET_PACK_MENU_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// After renderer initialization, before guest execution. Failure leaves native
// and legacy textures available; report error, rather than blocking gameplay.
bool bw_asset_pack_menu_start(const char* data_dir,bool textures_enabled);
void bw_asset_pack_menu_draw(void);
bool bw_asset_pack_menu_restart_needed(void);
const char* bw_asset_pack_menu_error(void);
// After game/UI workers stop, before renderer shutdown. Joins filesystem work
// before destroying the catalog and its owned runtime registration groups.
void bw_asset_pack_menu_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
