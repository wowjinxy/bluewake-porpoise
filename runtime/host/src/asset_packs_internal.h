// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_ASSET_PACKS_INTERNAL_H
#define BLUEWAKE_ASSET_PACKS_INTERNAL_H
#include "asset_packs.h"
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace bw_assets {
struct Selection { std::string id, hash; bool enabled=false; };
struct Pack { BluewakeAssetPackInfo info{}; std::filesystem::path manifest, root; };
bool fingerprint(const Pack& pack, std::string& hash, uint64_t& files, uint64_t& bytes, std::string& reason);
void text(char* target, size_t capacity, const std::string& source);
bool result(BluewakeAssetPackResult* out, bool ok, const std::string& message);
}
struct BluewakeAssetPacks {
    std::mutex mutex;
    std::filesystem::path data, directory, state;
    std::vector<bw_assets::Pack> packs;
    std::vector<bw_assets::Selection> selections;
    std::vector<BluewakeAssetPackConflict> conflicts;
    bool dirty=false, changed=false, runtime_attached=false;
};
#endif
