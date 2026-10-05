// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_ASSET_PACKS_H
#define BLUEWAKE_ASSET_PACKS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct BluewakeAssetPacks BluewakeAssetPacks;
typedef struct BluewakeAssetPackRuntime BluewakeAssetPackRuntime;
typedef struct BluewakeAssetPackSnapshot BluewakeAssetPackSnapshot;
typedef enum BluewakeAssetPackStatus {
    BW_ASSET_PACK_READY, BW_ASSET_PACK_MISSING, BW_ASSET_PACK_BROKEN,
    BW_ASSET_PACK_HASH_CHANGED, BW_ASSET_PACK_UNSUPPORTED,
    BW_ASSET_PACK_DUPLICATE, BW_ASSET_PACK_RUNTIME_BROKEN
} BluewakeAssetPackStatus;
typedef struct BluewakeAssetPackInfo {
    char id[96], name[192], kind[32], root[4096];
    char hash[33], expected_hash[33], loaded_hash[33], message[256];
    BluewakeAssetPackStatus status;
    bool enabled, loaded;
    uint32_t order, priority;
    uint64_t files, bytes, registrations;
} BluewakeAssetPackInfo;
typedef struct BluewakeAssetPackConflict {
    char key[160], winner[96], loser[96];
} BluewakeAssetPackConflict;
typedef struct BluewakeAssetPackResult { char message[256]; } BluewakeAssetPackResult;

// Filesystem-only catalog. Does not register textures or change gameplay mods.
// Managed roots: data_dir/AssetPacks/<folder>/pack.ini (game=GZLE01).
BluewakeAssetPacks* bluewake_asset_packs_create(const char* data_dir, BluewakeAssetPackResult* result);
void bluewake_asset_packs_destroy(BluewakeAssetPacks* packs);
bool bluewake_asset_packs_refresh(BluewakeAssetPacks* packs, BluewakeAssetPackResult* result);
size_t bluewake_asset_packs_count(BluewakeAssetPacks* packs);
bool bluewake_asset_packs_get(BluewakeAssetPacks* packs, size_t index, BluewakeAssetPackInfo* info);
bool bluewake_asset_packs_enable(BluewakeAssetPacks* packs, const char* id, bool enabled, BluewakeAssetPackResult* result);
// Lowest priority first; later entries win exact-key conflicts.
bool bluewake_asset_packs_move(BluewakeAssetPacks* packs, const char* id, size_t index, BluewakeAssetPackResult* result);
bool bluewake_asset_packs_dirty(BluewakeAssetPacks* packs);
bool bluewake_asset_packs_restart_needed(BluewakeAssetPacks* packs);
bool bluewake_asset_packs_save(BluewakeAssetPacks* packs, BluewakeAssetPackResult* result);
bool bluewake_asset_packs_reload(BluewakeAssetPacks* packs, BluewakeAssetPackResult* result);
// Transaction rollback for UI persistence failure: metadata/selections only,
// no filesystem reads/writes, registration changes, or renderer ownership.
BluewakeAssetPackSnapshot* bluewake_asset_packs_snapshot(BluewakeAssetPacks* packs);
bool bluewake_asset_packs_restore(BluewakeAssetPacks* packs, const BluewakeAssetPackSnapshot* snapshot, BluewakeAssetPackResult* result);
void bluewake_asset_packs_free_snapshot(BluewakeAssetPackSnapshot* snapshot);
// Presets carry only pack IDs, order, enable state and XXH3-128 fingerprints.
// They contain no local paths, textures, game code, saves, or control bindings.
bool bluewake_asset_packs_export(BluewakeAssetPacks* packs, const char* path, const char* name, BluewakeAssetPackResult* result);
bool bluewake_asset_packs_import(BluewakeAssetPacks* packs, const char* path, BluewakeAssetPackResult* result);
size_t bluewake_asset_packs_conflicts(BluewakeAssetPacks* packs);
bool bluewake_asset_packs_conflict(BluewakeAssetPacks* packs, size_t index, BluewakeAssetPackConflict* conflict);

// Real Aurora adapter: call once after renderer initialization, BEFORE guest
// execution; destroy BEFORE renderer shutdown. Never reload while game runs.
// Existing explicit DOL_AURORA_TEXTURE_PACK registrations have priority zero.
// Managed groups alone are owned/unregistered by this adapter.
BluewakeAssetPackRuntime* bluewake_asset_packs_load_runtime(BluewakeAssetPacks* packs, bool textures_enabled, BluewakeAssetPackResult* result);
void bluewake_asset_packs_unload_runtime(BluewakeAssetPackRuntime* runtime);

#ifdef __cplusplus
}
#endif
#endif
