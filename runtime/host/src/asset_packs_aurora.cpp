// SPDX-License-Identifier: GPL-3.0-or-later
#include "asset_packs.h"
#ifdef rename
#undef rename
#endif
#include "asset_packs_internal.h"
#include <aurora/texture.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>

struct OwnedGroup {
    aurora::texture::ReplacementGroup group;
    OwnedGroup()=default;
    OwnedGroup(const OwnedGroup&)=delete;
    OwnedGroup& operator=(const OwnedGroup&)=delete;
    OwnedGroup(OwnedGroup&& other) noexcept:group(std::move(other.group)){other.group.registrations.clear();}
    ~OwnedGroup(){aurora::texture::unregister_replacements(group);}
};
struct BluewakeAssetPackRuntime {
    std::vector<OwnedGroup> groups;
};
namespace {
std::string key_name(const aurora::texture::TextureSourceKey& key) {
    char buffer[160];std::snprintf(buffer,sizeof buffer,"%ux%u/%u/%016llx/%s%016llx",key.width,key.height,key.format,
        static_cast<unsigned long long>(key.textureHash),key.hasTlut?"tlut:":"none:",static_cast<unsigned long long>(key.tlutHash));return buffer;
}
}
extern "C" {
BluewakeAssetPackRuntime* bluewake_asset_packs_load_runtime(BluewakeAssetPacks* manager,bool enabled,BluewakeAssetPackResult* result) {
    if(!manager){bw_assets::result(result,false,"Asset catalog unavailable; native textures remain available");return nullptr;}
    try {
        std::lock_guard lock(manager->mutex);
        if(manager->runtime_attached){bw_assets::result(result,false,"Pack registrations already belong to this running renderer; restart to reload");return nullptr;}
        auto runtime=std::make_unique<BluewakeAssetPackRuntime>();
        using Owner=std::pair<std::string,aurora::texture::ReplacementRegistration>;
        std::map<std::string,std::vector<Owner>> owners;
        size_t skipped=0;
        for(auto& pack:manager->packs) {
            pack.info.loaded=false;pack.info.registrations=0;pack.info.loaded_hash[0]=0;
            if(!enabled||!pack.info.enabled)continue;
            if(pack.info.status!=BW_ASSET_PACK_READY){++skipped;continue;}
            std::string hash,reason;uint64_t files=0,bytes=0;
            if(!bw_assets::fingerprint(pack,hash,files,bytes,reason)||hash!=pack.info.expected_hash) {
                pack.info.status=BW_ASSET_PACK_HASH_CHANGED;
                bw_assets::text(pack.info.message,sizeof pack.info.message,"Pack changed or became unreadable before renderer startup; skipped");++skipped;continue;
            }
            OwnedGroup owned;owned.group=aurora::texture::load_replacement_directory(pack.root,{static_cast<int32_t>(pack.info.priority)});
            auto& group=owned.group;
            bool valid=!group.registrations.empty();
            for(const auto& registration:group.registrations) {
                if(!aurora::texture::validate_replacement(registration)){valid=false;break;}
            }
            // Detect whole-pack revision changes, including sidecars and files
            // that Aurora intentionally ignores. Do not trust extensions alone.
            if(!bw_assets::fingerprint(pack,hash,files,bytes,reason)||hash!=pack.info.expected_hash)valid=false;
            if(!valid) {
                pack.info.status=BW_ASSET_PACK_RUNTIME_BROKEN;
                bw_assets::text(pack.info.message,sizeof pack.info.message,"No usable registrations, failed native texture decode, or changed files; pack skipped and lower/native textures retained");++skipped;continue;
            }
            for(const auto& registration:group.registrations)if(const auto* key=std::get_if<aurora::texture::TextureSourceKey>(&registration.key))owners[key_name(*key)].push_back({pack.info.id,registration});
            pack.info.loaded=true;pack.info.registrations=group.registrations.size();bw_assets::text(pack.info.loaded_hash,sizeof pack.info.loaded_hash,hash);
            bw_assets::text(pack.info.message,sizeof pack.info.message,"Loaded and decoded at renderer startup; saved gameplay/item assignments are untouched");
            runtime->groups.push_back(std::move(owned));
        }
        manager->conflicts.clear();
        for(const auto& [key,entries]:owners) {
            const auto selected=aurora::texture::selected_replacement_id(entries.front().second.key);
            const auto winner=std::find_if(entries.begin(),entries.end(),[&](const Owner& owner){return owner.second.id==selected;});
            if(winner==entries.end())continue;
            for(const auto& owner:entries)if(owner.second.id!=selected) {
                BluewakeAssetPackConflict conflict{};bw_assets::text(conflict.key,sizeof conflict.key,key);
                bw_assets::text(conflict.winner,sizeof conflict.winner,winner->first);bw_assets::text(conflict.loser,sizeof conflict.loser,owner.first);manager->conflicts.push_back(conflict);
            }
        }
        manager->runtime_attached=true;manager->changed=false;
        bw_assets::result(result,true,"Loaded "+std::to_string(runtime->groups.size())+" managed texture packs; skipped "+std::to_string(skipped)+". Edits apply after restart.");
        return runtime.release();
    } catch(...) {
        // OwnedGroup unregisters all adopted/current registrations during
        // unwinding; never leave a partially registered higher-priority pack.
        std::lock_guard lock(manager->mutex);
        for(auto& pack:manager->packs){
            pack.info.loaded=false;pack.info.loaded_hash[0]=0;pack.info.registrations=0;
            if(pack.info.enabled&&pack.info.status==BW_ASSET_PACK_READY){
                pack.info.status=BW_ASSET_PACK_RUNTIME_BROKEN;
                bw_assets::text(pack.info.message,sizeof pack.info.message,"Managed loader failed; its registrations were removed and legacy/native textures retained");
            }
        }
        manager->conflicts.clear();manager->runtime_attached=false;
        bw_assets::result(result,false,"Managed pack startup failed; use original textures and inspect the catalog");return nullptr;
    }
}
void bluewake_asset_packs_unload_runtime(BluewakeAssetPackRuntime* runtime) {
    if(!runtime)return;
    delete runtime;
}
}
