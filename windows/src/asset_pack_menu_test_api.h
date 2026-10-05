// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "asset_packs.h"
#if !defined(BLUEWAKE_ASSET_PACK_MENU_TEST)
#error Fixture-only API
#endif
#ifdef __cplusplus
extern "C" {
#endif
void bw_asset_pack_menu_test_paths(const char* import_path,const char* export_path);
bool bw_asset_pack_menu_test_busy(void);
size_t bw_asset_pack_menu_test_rows(void);
bool bw_asset_pack_menu_test_row(size_t index,BluewakeAssetPackInfo* info);
// Counts cache pulls, not per-frame drawing. A busy/idle draw must not rescan.
unsigned bw_asset_pack_menu_test_cache_reads(void);
void bw_asset_pack_menu_test_pause_work(bool paused);
#ifdef __cplusplus
}
#endif
