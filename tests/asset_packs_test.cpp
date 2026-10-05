// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "asset_packs_test_support.h"
#include <iostream>
#include <memory>

using namespace asset_fixture;
int main(int argc,char** argv) {
    assert(argc==2);const auto root=directory(argv[1]),data=root/path("data-\xE2\x98\x83");
    const auto alpha=pack(data,"folder-z","alpha","Synthetic Alpha"),beta=pack(data,"folder-a","beta","Synthetic Beta");
    write(data/"controls.ini","controls sentinel");write(data/"settings.ini","scalar sentinel");
    BluewakeAssetPackResult result{};
    using Manager=std::unique_ptr<BluewakeAssetPacks,decltype(&bluewake_asset_packs_destroy)>;
    Manager manager(bluewake_asset_packs_create(utf8(data).c_str(),&result),bluewake_asset_packs_destroy);assert(manager);
    assert(bluewake_asset_packs_count(manager.get())==2&&!bluewake_asset_packs_dirty(manager.get()));
    auto initial=info(manager.get(),"alpha");assert(!initial.enabled&&initial.status==BW_ASSET_PACK_READY&&initial.files==1&&std::string(initial.hash).size()==32);
    const std::string alpha_hash=initial.hash;
    assert(bluewake_asset_packs_refresh(manager.get(),&result)&&std::string(info(manager.get(),"alpha").hash)==alpha_hash);
    assert(bluewake_asset_packs_enable(manager.get(),"alpha",true,&result)&&bluewake_asset_packs_enable(manager.get(),"beta",true,&result));
    assert(bluewake_asset_packs_move(manager.get(),"alpha",1,&result));assert(info(manager.get(),"alpha").priority>info(manager.get(),"beta").priority);
    assert(bluewake_asset_packs_save(manager.get(),&result)&&!bluewake_asset_packs_dirty(manager.get()));
    const auto saved=read(data/"asset_packs.ini");assert(saved.find(utf8(data))==std::string::npos);
    manager.reset(bluewake_asset_packs_create(utf8(data).c_str(),&result));assert(manager&&info(manager.get(),"alpha").enabled&&info(manager.get(),"alpha").order==1);
    const auto preset=root/"share.bwpackpreset";
    assert(!bluewake_asset_packs_export(manager.get(),utf8(data/"controls.ini").c_str(),"Invalid target",&result));
    assert(bluewake_asset_packs_export(manager.get(),utf8(preset).c_str(),"Synthetic arrangement",&result));
    const auto portable=read(preset);assert(portable.find(utf8(data))==std::string::npos&&portable.find("textures/")==std::string::npos&&portable.find("sentinel")==std::string::npos);
    const auto other=root/"other-data";pack(other,"different-folder","alpha","Synthetic Alpha");
    Manager receiver(bluewake_asset_packs_create(utf8(other).c_str(),&result),bluewake_asset_packs_destroy);assert(receiver);
    assert(bluewake_asset_packs_import(receiver.get(),utf8(preset).c_str(),&result));
    assert(info(receiver.get(),"alpha").enabled&&info(receiver.get(),"alpha").status==BW_ASSET_PACK_READY);
    assert(info(receiver.get(),"beta").enabled&&info(receiver.get(),"beta").status==BW_ASSET_PACK_MISSING);
    assert(bluewake_asset_packs_save(receiver.get(),&result));
    // Traversal failure during import is transactional, not a partial selection.
    assert(bluewake_asset_packs_enable(receiver.get(),"alpha",false,&result));
    fs::rename(other/"AssetPacks",other/"RetainedPacks");write(other/"AssetPacks","blocks directory creation");
    assert(!bluewake_asset_packs_import(receiver.get(),utf8(preset).c_str(),&result));
    assert(!info(receiver.get(),"alpha").enabled);
    fs::rename(other/"AssetPacks",other/"RetainedBlocker");fs::rename(other/"RetainedPacks",other/"AssetPacks");
    assert(bluewake_asset_packs_import(receiver.get(),utf8(preset).c_str(),&result));
    auto malformed=portable+"entry=616c706861,1,"+alpha_hash+"\n";write(root/"bad.bwpackpreset",malformed);
    assert(!bluewake_asset_packs_import(receiver.get(),utf8(root/"bad.bwpackpreset").c_str(),&result));
    assert(info(receiver.get(),"alpha").enabled&&info(receiver.get(),"beta").order==0);
    write(data/"asset_packs.ini",saved+"unknown=1\n");assert(!bluewake_asset_packs_reload(manager.get(),&result));
    assert(info(manager.get(),"alpha").enabled);write(data/"asset_packs.ini",saved);
    write(alpha/"textures"/"notes.txt","revision changed");assert(bluewake_asset_packs_refresh(manager.get(),&result));
    assert(info(manager.get(),"alpha").status==BW_ASSET_PACK_HASH_CHANGED);
    assert(bluewake_asset_packs_enable(manager.get(),"alpha",true,&result));assert(info(manager.get(),"alpha").status==BW_ASSET_PACK_READY&&std::string(info(manager.get(),"alpha").expected_hash)!=alpha_hash);
    write(data/"AssetPacks"/"escape"/"pack.ini","[pack]\nversion=1\nid=escape\nname=Unsafe root\nkind=textures\ngame=GZLE01\nroot=../../outside\n");
    write(data/"AssetPacks"/"code"/"pack.ini","[pack]\nversion=1\nid=code\nname=Compiled code\nkind=gameplay\ngame=GZLE01\n");
    pack(data,"duplicate-one","duplicate","One");pack(data,"duplicate-two","duplicate","Two");
    assert(bluewake_asset_packs_refresh(manager.get(),&result));
    assert(info(manager.get(),"escape").status==BW_ASSET_PACK_BROKEN&&!bluewake_asset_packs_enable(manager.get(),"escape",true,&result));
    assert(info(manager.get(),"code").status==BW_ASSET_PACK_UNSUPPORTED&&!bluewake_asset_packs_enable(manager.get(),"code",true,&result));
    assert(info(manager.get(),"duplicate").status==BW_ASSET_PACK_DUPLICATE&&!bluewake_asset_packs_enable(manager.get(),"duplicate",true,&result));
    assert(bluewake_asset_packs_enable(manager.get(),"beta",false,&result)&&bluewake_asset_packs_save(manager.get(),&result));
    manager.reset(bluewake_asset_packs_create(utf8(data).c_str(),&result));assert(manager); // duplicates cannot corrupt saved selection schema
    assert(read(data/"controls.ini")=="controls sentinel"&&read(data/"settings.ini")=="scalar sentinel");
    std::cout<<"Asset catalog manifests, real content hashes, ordering, portable presets and rejection passed; evidence: "<<utf8(root)<<'\n';
}
