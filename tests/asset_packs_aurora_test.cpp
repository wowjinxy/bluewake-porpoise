// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "asset_packs_test_support.h"
#include <aurora/texture.hpp>
#include <aurora/aurora.h>
#include <iostream>
#include <memory>
// Actual Aurora configuration storage. Set a private cache path without
// initializing Aurora, SDL video, a window, a GPU, or native input.
namespace aurora {extern AuroraConfig g_config;}
using namespace asset_fixture;
int main(int argc,char** argv) {
    assert(argc==2);const auto root=directory(argv[1]),data=root/"data";
    const auto cache=utf8(root/"cache");aurora::g_config.cachePath=cache.c_str();aurora::g_config.allowTextureDumps=false;
    const auto alpha=pack(data,"alpha-folder","alpha","Synthetic Alpha");
    pack(data,"beta-folder","beta","Synthetic Beta");
    const auto legacy_path=root/"explicit-legacy";png(legacy_path/"tex1_8x8_0123456789abcdef_0.png");
    dds(legacy_path/"tex1_8x8_3333333333333333_0.dds");
    auto legacy=aurora::texture::load_replacement_directory(legacy_path,{0});assert(legacy.registrations.size()==2);
    const aurora::texture::ReplacementKey key=aurora::texture::TextureSourceKey{.textureHash=0x0123456789abcdef,.width=8,.height=8,.format=0};
    for(const auto& registration:legacy.registrations)assert(aurora::texture::validate_replacement(registration));
    assert(aurora::texture::selected_replacement_id(key)==legacy.registrations.front().id);
    auto invalid=legacy.registrations.front();invalid.id+=9999;assert(!aurora::texture::validate_replacement(invalid));
    BluewakeAssetPackResult result{};
    using Manager=std::unique_ptr<BluewakeAssetPacks,decltype(&bluewake_asset_packs_destroy)>;
    using Runtime=std::unique_ptr<BluewakeAssetPackRuntime,decltype(&bluewake_asset_packs_unload_runtime)>;
    Manager manager(bluewake_asset_packs_create(utf8(data).c_str(),&result),bluewake_asset_packs_destroy);assert(manager);
    assert(bluewake_asset_packs_enable(manager.get(),"alpha",true,&result)&&bluewake_asset_packs_enable(manager.get(),"beta",true,&result));
    assert(bluewake_asset_packs_move(manager.get(),"alpha",1,&result)&&bluewake_asset_packs_save(manager.get(),&result));
    Runtime runtime(bluewake_asset_packs_load_runtime(manager.get(),true,&result),bluewake_asset_packs_unload_runtime);assert(runtime);
    assert(info(manager.get(),"alpha").loaded&&info(manager.get(),"beta").loaded&&info(manager.get(),"alpha").registrations==1);
    assert(bluewake_asset_packs_conflicts(manager.get())==1);
    BluewakeAssetPackConflict conflict{};assert(bluewake_asset_packs_conflict(manager.get(),0,&conflict));
    assert(std::string(conflict.winner)=="alpha"&&std::string(conflict.loser)=="beta");
    const auto selected=aurora::texture::selected_replacement_id(key);assert(selected&&selected!=legacy.registrations.front().id);
    assert(!bluewake_asset_packs_load_runtime(manager.get(),true,&result)); // no live re-register/reload
    assert(bluewake_asset_packs_enable(manager.get(),"alpha",false,&result)&&bluewake_asset_packs_restart_needed(manager.get()));
    assert(aurora::texture::selected_replacement_id(key)==selected); // UI edits leave actual registry unchanged
    assert(bluewake_asset_packs_refresh(manager.get(),&result)&&info(manager.get(),"alpha").loaded);
    runtime.reset();assert(aurora::texture::selected_replacement_id(key)==legacy.registrations.front().id);
    // A corrupt high-priority pack is completely unregistered, including valid
    // registrations already seen before its failed image. Lower pack survives.
    write(alpha/"textures"/"tex1_8x8_1111111111111111_0.png","deliberately corrupt synthetic PNG");
    manager.reset(bluewake_asset_packs_create(utf8(data).c_str(),&result));assert(manager);
    assert(info(manager.get(),"alpha").status==BW_ASSET_PACK_HASH_CHANGED);
    assert(bluewake_asset_packs_enable(manager.get(),"alpha",true,&result));
    runtime.reset(bluewake_asset_packs_load_runtime(manager.get(),true,&result));assert(runtime);
    assert(!info(manager.get(),"alpha").loaded&&info(manager.get(),"alpha").status==BW_ASSET_PACK_RUNTIME_BROKEN);
    assert(info(manager.get(),"beta").loaded&&bluewake_asset_packs_conflicts(manager.get())==0);
    assert(aurora::texture::selected_replacement_id(key)!=legacy.registrations.front().id);
    const aurora::texture::ReplacementKey rejected=aurora::texture::TextureSourceKey{.textureHash=0x1111111111111111,.width=8,.height=8,.format=0};
    assert(aurora::texture::selected_replacement_id(rejected)==0);
    runtime.reset();assert(aurora::texture::selected_replacement_id(key)==legacy.registrations.front().id);
    // Missing files after registration fail actual decoder inspection, without
    // caching failure or silently removing someone else's registration.
    const auto missing_path=root/"missing";const auto missing_file=missing_path/"tex1_8x8_2222222222222222_0.png";png(missing_file);
    auto missing=aurora::texture::load_replacement_directory(missing_path);assert(missing.registrations.size()==1);
    const auto moved=missing_path/"retained.png";fs::rename(missing_file,moved);
    assert(!aurora::texture::validate_replacement(missing.registrations.front()));
    assert(aurora::texture::selected_replacement_id(missing.registrations.front().key)==missing.registrations.front().id);
    aurora::texture::unregister_replacements(missing);
    const auto surplus=root/"surplus";png(surplus/"tex1_8x8_4444444444444444_0.png");
    png(surplus/"tex1_8x8_4444444444444444_0_mip1.png"); // 1x1 base already has its complete chain
    auto mip_group=aurora::texture::load_replacement_directory(surplus);assert(mip_group.registrations.size()==1);
    assert(!aurora::texture::validate_replacement(mip_group.registrations.front()));
    aurora::texture::unregister_replacements(mip_group);
    const auto malformed=root/"malformed-mip";dds(malformed/"tex1_8x8_5555555555555555_0.dds",2,2);
    dds(malformed/"tex1_8x8_5555555555555555_0_mip1.dds",2,2); // native mip1 must be 1x1
    mip_group=aurora::texture::load_replacement_directory(malformed);assert(mip_group.registrations.size()==1);
    assert(!aurora::texture::validate_replacement(mip_group.registrations.front()));
    aurora::texture::unregister_replacements(mip_group);
    const auto ignored=root/"ignored-gap";png(ignored/"tex1_8x8_6666666666666666_0.png");
    write(ignored/"tex1_8x8_6666666666666666_0_mip33.png","ignored beyond a gap, never decoded or shifted");
    mip_group=aurora::texture::load_replacement_directory(ignored);assert(mip_group.registrations.size()==1);
    assert(aurora::texture::validate_replacement(mip_group.registrations.front()));
    aurora::texture::unregister_replacements(mip_group);
    manager.reset(bluewake_asset_packs_create(utf8(data).c_str(),&result));assert(manager);
    runtime.reset(bluewake_asset_packs_load_runtime(manager.get(),false,&result));assert(runtime);
    assert(!info(manager.get(),"beta").loaded&&aurora::texture::selected_replacement_id(key)==legacy.registrations.front().id);
    runtime.reset();aurora::texture::unregister_replacements(legacy);assert(aurora::texture::selected_replacement_id(key)==0);
    std::cout<<"Actual Aurora directory keys, priority selection, decoder validation, broken-pack fallback and scoped teardown passed; no rendering/window evidence claimed: "<<utf8(root)<<'\n';
}
