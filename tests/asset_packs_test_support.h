// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "asset_packs.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace asset_fixture {
namespace fs=std::filesystem;
inline std::string utf8(const fs::path& path){const auto v=path.u8string();return {reinterpret_cast<const char*>(v.data()),v.size()};}
inline void write(const fs::path& file,const std::string& text){fs::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);out<<text;assert(out.good());}
inline std::string read(const fs::path& file){std::ifstream in(file,std::ios::binary);assert(in);return {std::istreambuf_iterator<char>(in),{}};}
inline fs::path path(const char* value){return fs::path(reinterpret_cast<const char8_t*>(value));}
inline fs::path directory(const char* base){assert(base&&*base);auto result=fs::absolute(path(base))/("assets-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));assert(fs::create_directories(result));return result;}
// Original synthetic 1x1 RGBA image, generated from PNG chunks and zlib data.
// No game textures, replacement art, or copyrighted fixture bytes.
inline void png(const fs::path& file) {
    static constexpr unsigned char bytes[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,248,207,192,240,31,0,5,0,1,255,137,153,61,29,0,0,0,0,73,69,78,68,174,66,96,130};
    fs::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes),sizeof bytes);assert(out.good());
}
inline void dds(const fs::path& file,uint32_t width=1,uint32_t height=1) {
    // Synthetic 1x1 uncompressed RGBA DDS using the same standard layout as
    // Aurora's encode_rgba8_dds, never a copied game/replacement asset.
    std::string bytes(128+width*height*4,'\0');bytes.replace(0,4,"DDS ");
    const auto put=[&](size_t at,uint32_t value){for(unsigned i=0;i<4;++i)bytes[at+i]=static_cast<char>(value>>(i*8));};
    put(4,124);put(8,0x100F);put(12,height);put(16,width);put(20,width*4);put(28,1);
    put(76,32);put(80,0x41);put(88,32);put(92,0xFF);put(96,0xFF00);put(100,0xFF0000);put(104,0xFF000000);put(108,0x1000);
    for(size_t at=128;at<bytes.size();at+=4){bytes[at]=static_cast<char>(255);bytes[at+3]=static_cast<char>(255);}write(file,bytes);
}
inline fs::path pack(const fs::path& data,const std::string& folder,const std::string& id,const std::string& name) {
    const auto path=data/"AssetPacks"/folder;
    write(path/"pack.ini","[pack]\nversion=1\nid="+id+"\nname="+name+"\nkind=textures\ngame=GZLE01\nroot=textures\n");
    png(path/"textures"/"tex1_8x8_0123456789abcdef_0.png");return path;
}
inline BluewakeAssetPackInfo info(BluewakeAssetPacks* manager,const char* id) {
    BluewakeAssetPackInfo value{};for(size_t i=0;i<bluewake_asset_packs_count(manager);++i){assert(bluewake_asset_packs_get(manager,i,&value));if(std::string(value.id)==id)return value;}assert(false);return value;
}
}
