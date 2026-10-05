// SPDX-License-Identifier: GPL-3.0-or-later
#include "asset_packs.h"
#ifdef rename
#undef rename
#endif
#include "asset_packs_internal.h"
#include "atomic_file.h"
#include <xxhash.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>

namespace fs=std::filesystem;
struct BluewakeAssetPackSnapshot {
    const BluewakeAssetPacks* owner=nullptr;
    std::vector<bw_assets::Pack> packs;
    std::vector<bw_assets::Selection> selections;
    bool dirty=false,changed=false;
};
namespace bw_assets {
constexpr size_t max_packs=128, max_text=65536, max_files=100000;
std::string utf8(const fs::path& path) { const auto value=path.u8string();return {reinterpret_cast<const char*>(value.data()),value.size()}; }
std::string portable_path(const fs::path& path) {const auto value=path.generic_u8string();return {reinterpret_cast<const char*>(value.data()),value.size()};}
fs::path path(const char* value) { return fs::path(reinterpret_cast<const char8_t*>(value?value:"")); }
void text(char* target,size_t capacity,const std::string& source) { std::snprintf(target,capacity,"%s",source.c_str()); }
bool result(BluewakeAssetPackResult* out,bool ok,const std::string& message) {if(out)text(out->message,sizeof out->message,message);return ok;}
bool id_valid(const std::string& id) {
    return !id.empty()&&id.size()<=64&&std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_';});
}
bool hash_valid(const std::string& hash) {return hash.size()==32&&std::all_of(hash.begin(),hash.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
bool printable(const std::string& value,size_t limit) {
    if(value.empty()||value.size()>limit)return false;
    for(size_t i=0;i<value.size();) {
        const auto lead=static_cast<unsigned char>(value[i++]);
        if(lead<32||lead==127)return false;if(lead<128)continue;
        const unsigned count=lead>=0xC2&&lead<=0xDF?1:lead>=0xE0&&lead<=0xEF?2:lead>=0xF0&&lead<=0xF4?3:0;
        if(!count||i+count>value.size())return false;
        uint32_t code=lead&((1u<<(6u-count))-1u);
        for(unsigned j=0;j<count;++j){const auto next=static_cast<unsigned char>(value[i++]);if((next&0xC0u)!=0x80u)return false;code=(code<<6)|(next&0x3Fu);}
        if(code<(count==1?0x80u:count==2?0x800u:0x10000u)||code>0x10FFFFu||(code>=0xD800u&&code<=0xDFFFu))return false;
    }
    return true;
}
std::string hex(const std::string& value) {constexpr char digits[]="0123456789abcdef";std::string out;for(unsigned char c:value){out+=digits[c>>4];out+=digits[c&15];}return out;}
bool unhex(const std::string& value,std::string& out) {
    if(value.size()>512||value.size()%2)return false;out.clear();
    for(size_t i=0;i<value.size();i+=2){unsigned v;auto parsed=std::from_chars(value.data()+i,value.data()+i+2,v,16);if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+i+2||v==0)return false;out+=static_cast<char>(v);}return true;
}
bool read(const fs::path& file,std::string& out) {
    std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in)return false;
    const auto size=in.tellg();if(size<0||size>max_text)return false;out.resize(static_cast<size_t>(size));in.seekg(0);if(!out.empty())in.read(out.data(),size);return !in.bad()&&!in.fail();
}
bool atomic_write(const fs::path& file,const std::string& content) {
    const auto name=utf8(file);char* temporary=bw_atomic_path(name.c_str());if(!temporary)return false;
    FILE* stream=bw_atomic_open(temporary,"wb");if(!stream){std::free(temporary);return false;}
    const bool written=std::fwrite(content.data(),1,content.size(),stream)==content.size();
    const bool ok=bw_atomic_finish(stream,temporary,name.c_str(),written);std::free(temporary);return ok;
}
bool safe_relative(const fs::path& value) {
    if(value.empty()||value.is_absolute()||value.has_root_name()||value.has_root_directory())return false;
    for(const auto& part:value)if(part==".."||part==".")return false;return true;
}
bool within(const fs::path& file,const fs::path& root) {
    std::error_code ec;const auto resolved_root=fs::canonical(root,ec);if(ec)return false;
    const auto resolved_file=fs::canonical(file,ec);if(ec)return false;
    const auto relative=resolved_file.lexically_relative(resolved_root);return !relative.empty()&&safe_relative(relative);
}
bool fingerprint(const Pack& pack,std::string& hash,uint64_t& files,uint64_t& bytes,std::string& reason) {
    files=bytes=0;std::error_code ec;
    if(!fs::is_directory(pack.root,ec)||ec){reason="Texture root is missing or unreadable";return false;}
    if(fs::is_symlink(fs::symlink_status(pack.root,ec))||ec){reason="Linked texture roots are not managed packs";return false;}
    std::vector<fs::path> paths;
    for(fs::recursive_directory_iterator it(pack.root,fs::directory_options::none,ec),end;it!=end&&!ec;it.increment(ec)) {
        const auto status=it->symlink_status(ec);if(ec)break;
        if(fs::is_symlink(status)){reason="A pack contains a linked file or directory";return false;}
        if(fs::is_regular_file(status)) {if(paths.size()>=max_files){reason="Pack file limit exceeded";return false;}paths.push_back(it->path());}
    }
    if(ec){reason="Pack traversal failed; no partial scan was accepted";return false;}
    std::sort(paths.begin(),paths.end(),[&](const auto& a,const auto& b){return portable_path(a.lexically_relative(pack.root))<portable_path(b.lexically_relative(pack.root));});
    std::unique_ptr<XXH3_state_t,decltype(&XXH3_freeState)> state(XXH3_createState(),XXH3_freeState);
    if(!state||XXH3_128bits_reset(state.get())!=XXH_OK){reason="Content fingerprint could not be initialized";return false;}
    const auto feed=[&](const std::string& value){XXH3_128bits_update(state.get(),value.data(),value.size());const char zero=0;XXH3_128bits_update(state.get(),&zero,1);};
    feed("BlueWake asset-pack content v1");
    if(!pack.manifest.empty()){std::string manifest;if(!read(pack.manifest,manifest)){reason="Manifest could not be read";return false;}feed(manifest);}
    std::array<char,65536> buffer;
    for(const auto& file:paths) {
        feed(portable_path(file.lexically_relative(pack.root)));
        const auto before_size=fs::file_size(file,ec);if(ec){reason="A pack file is unreadable";return false;}
        const auto before_time=fs::last_write_time(file,ec);if(ec)return false;
        std::ifstream input(file,std::ios::binary);if(!input){reason="A pack file is unreadable";return false;}
        uint64_t count=0;
        while(input){input.read(buffer.data(),buffer.size());const auto n=input.gcount();if(n>0){XXH3_128bits_update(state.get(),buffer.data(),n);count+=n;}}
        if(input.bad()||count!=before_size||fs::file_size(file,ec)!=before_size||ec||fs::last_write_time(file,ec)!=before_time||ec){reason="A pack changed while it was being fingerprinted";return false;}
        feed(std::to_string(count));++files;bytes+=count;
    }
    const auto value=XXH3_128bits_digest(state.get());char value_text[33];
    std::snprintf(value_text,sizeof value_text,"%016llx%016llx",static_cast<unsigned long long>(value.high64),static_cast<unsigned long long>(value.low64));hash=value_text;return true;
}
bool manifest(Pack& pack,const fs::path& folder) {
    pack.manifest=folder/"pack.ini";std::string content;std::error_code ec;
    if(fs::is_symlink(fs::symlink_status(pack.manifest,ec))||ec)return false;
    if(!read(pack.manifest,content)){text(pack.info.message,sizeof pack.info.message,"Missing or unreadable pack.ini");return false;}
    std::map<std::string,std::string> fields;std::istringstream in(content);std::string line;bool section=false;
    while(std::getline(in,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#'||line[0]==';')continue;
        if(line=="[pack]"){if(section)return false;section=true;continue;}
        const auto equal=line.find('=');if(!section||equal==std::string::npos||line.size()>4096)return false;
        const auto key=line.substr(0,equal),value=line.substr(equal+1);
        if(key!="version"&&key!="id"&&key!="name"&&key!="kind"&&key!="game"&&key!="root"&&key!="author"&&key!="license"&&key!="pack_version")return false;
        if(!fields.emplace(key,value).second)return false;
    }
    if(fields["version"]!="1"||!id_valid(fields["id"])||!printable(fields["name"],191)||!printable(fields["kind"],31)||fields["game"]!="GZLE01")return false;
    text(pack.info.id,sizeof pack.info.id,fields["id"]);text(pack.info.name,sizeof pack.info.name,fields["name"]);text(pack.info.kind,sizeof pack.info.kind,fields["kind"]);
    if(fields["kind"]!="textures"){pack.info.status=BW_ASSET_PACK_UNSUPPORTED;text(pack.info.message,sizeof pack.info.message,"Gameplay/code packs must be compiled into BlueWake; they cannot be loaded as assets");return true;}
    if(!printable(fields["root"],4095)||fields["root"].find('\\')!=std::string::npos)return false;
    const auto relative=path(fields["root"].c_str());if(!safe_relative(relative))return false;
    auto component=folder;
    for(const auto& part:relative){component/=part;if(fs::is_symlink(fs::symlink_status(component,ec))||ec)return false;}
    pack.root=folder/relative;if(!within(pack.root,folder))return false;
    text(pack.info.root,sizeof pack.info.root,utf8(pack.root));std::string hash,reason;
    if(!fingerprint(pack,hash,pack.info.files,pack.info.bytes,reason)){text(pack.info.message,sizeof pack.info.message,reason);return false;}
    text(pack.info.hash,sizeof pack.info.hash,hash);pack.info.status=BW_ASSET_PACK_READY;
    text(pack.info.message,sizeof pack.info.message,"Content fingerprint checked; texture decoding is checked at renderer startup");return true;
}
std::string serialize(const std::vector<Selection>& selections,const char* name=nullptr) {
    std::ostringstream out;out<<(name?"[asset-pack-preset]\n":"[asset-packs]\n")<<"version=1\n";
    if(name)out<<"name="<<hex(name)<<'\n';
    for(const auto& selection:selections)out<<"entry="<<hex(selection.id)<<','<<selection.enabled<<','<<selection.hash<<'\n';return out.str();
}
bool parse_selection(const std::string& content,bool preset,std::vector<Selection>& selections) {
    std::istringstream in(content);std::string line;unsigned seen=0;std::set<std::string> ids;selections.clear();
    while(std::getline(in,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#'||line[0]==';')continue;
        if(line==(preset?"[asset-pack-preset]":"[asset-packs]")){if(seen)return false;seen=1;continue;}
        if(line=="version=1"){if(seen!=1)return false;seen=3;continue;}
        if(preset&&line.rfind("name=",0)==0){std::string name;if(seen!=3||!unhex(line.substr(5),name)||!printable(name,191))return false;seen=7;continue;}
        if(seen!=(preset?7u:3u)||line.rfind("entry=",0)!=0||selections.size()>=max_packs*2)return false;
        const auto a=line.find(',',6),b=a==std::string::npos?a:line.find(',',a+1);if(a==std::string::npos||b==std::string::npos)return false;
        Selection value;const auto enabled=line.substr(a+1,b-a-1);value.hash=line.substr(b+1);
        if(!unhex(line.substr(6,a-6),value.id)||!id_valid(value.id)||(enabled!="0"&&enabled!="1")||(!value.hash.empty()&&!hash_valid(value.hash))||(enabled=="1"&&value.hash.empty())||!ids.insert(value.id).second)return false;
        value.enabled=enabled=="1";selections.push_back(std::move(value));
    }
    return seen==(preset?7u:3u);
}
void order(BluewakeAssetPacks& manager) {
    std::vector<Pack> ordered;ordered.reserve(manager.packs.size()+manager.selections.size());
    for(const auto& selection:manager.selections) {
        auto found=std::find_if(manager.packs.begin(),manager.packs.end(),[&](const auto& pack){return pack.info.id==selection.id;});
        Pack pack;
        if(found!=manager.packs.end()){pack=*found;manager.packs.erase(found);}else {
            text(pack.info.id,sizeof pack.info.id,selection.id);text(pack.info.name,sizeof pack.info.name,selection.id);pack.info.status=BW_ASSET_PACK_MISSING;
            text(pack.info.message,sizeof pack.info.message,"Referenced pack is missing; original textures remain available");
        }
        pack.info.enabled=selection.enabled;text(pack.info.expected_hash,sizeof pack.info.expected_hash,selection.hash);
        if(selection.enabled&&pack.info.status==BW_ASSET_PACK_READY&&selection.hash!=pack.info.hash){pack.info.status=BW_ASSET_PACK_HASH_CHANGED;text(pack.info.message,sizeof pack.info.message,"Pack contents changed; explicitly re-enable to accept this revision");}
        ordered.push_back(std::move(pack));
    }
    for(auto& pack:manager.packs)ordered.push_back(std::move(pack));manager.packs=std::move(ordered);
    for(size_t i=0;i<manager.packs.size();++i){manager.packs[i].info.order=static_cast<uint32_t>(i);manager.packs[i].info.priority=static_cast<uint32_t>(i+1);}
}
void capture(BluewakeAssetPacks& manager) {
    manager.selections.clear();std::set<std::string> ids;
    for(const auto& pack:manager.packs)if(id_valid(pack.info.id)&&ids.insert(pack.info.id).second)manager.selections.push_back({pack.info.id,pack.info.expected_hash,pack.info.enabled});
    manager.dirty=manager.changed=true;
}
bool refresh(BluewakeAssetPacks& manager,BluewakeAssetPackResult* out) {
    std::vector<Pack> packs;std::vector<fs::path> folders;std::error_code ec;
    fs::create_directories(manager.directory,ec);if(ec)return result(out,false,"Asset pack directory could not be created");
    for(fs::directory_iterator it(manager.directory,ec),end;it!=end&&!ec;it.increment(ec))if(it->is_directory(ec))folders.push_back(it->path());
    if(ec||folders.size()>max_packs)return result(out,false,"Asset pack catalog traversal failed or exceeded its pack limit");
    std::sort(folders.begin(),folders.end(),[](const auto& a,const auto& b){return utf8(a.filename())<utf8(b.filename());});
    for(const auto& folder:folders) {
        Pack pack;pack.info.status=BW_ASSET_PACK_BROKEN;
        const std::string fallback="invalid_"+std::to_string(XXH64(utf8(folder.filename()).data(),utf8(folder.filename()).size(),0));
        text(pack.info.id,sizeof pack.info.id,fallback);text(pack.info.name,sizeof pack.info.name,utf8(folder.filename()));
        bool valid=false;
        try {valid=!fs::is_symlink(fs::symlink_status(folder,ec))&&!ec&&manifest(pack,folder);}catch(...){valid=false;}
        if(!valid){
            pack.info.status=BW_ASSET_PACK_BROKEN;if(!pack.info.message[0])text(pack.info.message,sizeof pack.info.message,"Invalid manifest or unsafe/missing texture root; pack skipped");
        }
        packs.push_back(std::move(pack));
    }
    for(auto& pack:packs)if(std::count_if(packs.begin(),packs.end(),[&](const auto& other){return std::strcmp(other.info.id,pack.info.id)==0;})>1){pack.info.status=BW_ASSET_PACK_DUPLICATE;text(pack.info.message,sizeof pack.info.message,"Duplicate pack ID; all copies are skipped");}
    // Runtime registrations belong to the startup snapshot. Refresh only scans
    // metadata/files; it never modifies the renderer's registry or caches.
    const auto previous=std::move(manager.packs);manager.packs=std::move(packs);order(manager);
    for(auto& pack:manager.packs) {
        const auto old=std::find_if(previous.begin(),previous.end(),[&](const auto& item){return std::strcmp(item.info.id,pack.info.id)==0;});
        if(old!=previous.end()&&old->info.loaded) {
            pack.info.loaded=true;pack.info.registrations=old->info.registrations;
            text(pack.info.loaded_hash,sizeof pack.info.loaded_hash,old->info.loaded_hash);
            if(!pack.info.enabled||pack.info.status!=BW_ASSET_PACK_READY||std::strcmp(pack.info.hash,pack.info.loaded_hash)!=0)manager.changed=true;
        }
    }
    return result(out,true,"Asset pack catalog refreshed; renderer changes apply after restart");
}
bool install(BluewakeAssetPacks& manager,std::vector<Selection> selections,bool dirty,BluewakeAssetPackResult* out) {
    auto previous_selections=manager.selections;auto previous_packs=manager.packs;
    const bool previous_dirty=manager.dirty,previous_changed=manager.changed;
    try {
        manager.selections=std::move(selections);manager.dirty=dirty;manager.changed=true;
        if(refresh(manager,out))return true;
    }catch(...) {result(out,false,"Pack selections could not be checked; previous selections were preserved");}
    manager.selections=std::move(previous_selections);manager.packs=std::move(previous_packs);
    manager.dirty=previous_dirty;manager.changed=previous_changed;return false;
}
}

extern "C" {
BluewakeAssetPacks* bluewake_asset_packs_create(const char* directory,BluewakeAssetPackResult* out) {
    try {
        if(!directory||!*directory){bw_assets::result(out,false,"Asset packs need a private data directory");return nullptr;}
        auto manager=std::make_unique<BluewakeAssetPacks>();manager->data=fs::absolute(bw_assets::path(directory));manager->directory=manager->data/"AssetPacks";manager->state=manager->data/"asset_packs.ini";
        if(fs::exists(manager->state)){std::string content;if(!bw_assets::read(manager->state,content)||!bw_assets::parse_selection(content,false,manager->selections)){bw_assets::result(out,false,"Invalid asset_packs.ini; preserved for repair, no selections were applied");return nullptr;}}
        if(!bw_assets::refresh(*manager,out))return nullptr;return manager.release();
    }catch(...){bw_assets::result(out,false,"Asset pack catalog could not be opened");return nullptr;}
}
void bluewake_asset_packs_destroy(BluewakeAssetPacks* manager){delete manager;}
bool bluewake_asset_packs_refresh(BluewakeAssetPacks* manager,BluewakeAssetPackResult* out){if(!manager)return bw_assets::result(out,false,"Asset catalog unavailable");try{std::lock_guard lock(manager->mutex);return bw_assets::refresh(*manager,out);}catch(...){return bw_assets::result(out,false,"Asset catalog refresh failed; previous selections were kept");}}
size_t bluewake_asset_packs_count(BluewakeAssetPacks* manager){if(!manager)return 0;std::lock_guard lock(manager->mutex);return manager->packs.size();}
bool bluewake_asset_packs_get(BluewakeAssetPacks* manager,size_t index,BluewakeAssetPackInfo* out){if(!manager||!out)return false;std::lock_guard lock(manager->mutex);if(index>=manager->packs.size())return false;*out=manager->packs[index].info;return true;}
bool bluewake_asset_packs_enable(BluewakeAssetPacks* manager,const char* id,bool enabled,BluewakeAssetPackResult* out){
    if(!manager||!id)return bw_assets::result(out,false,"Asset catalog unavailable");std::lock_guard lock(manager->mutex);
    auto found=std::find_if(manager->packs.begin(),manager->packs.end(),[&](const auto& pack){return std::strcmp(pack.info.id,id)==0;});
    if(found==manager->packs.end())return bw_assets::result(out,false,"Pack was not found");
    if(enabled&&found->info.status!=BW_ASSET_PACK_READY&&found->info.status!=BW_ASSET_PACK_HASH_CHANGED)return bw_assets::result(out,false,"Missing, duplicate, unsupported or broken packs cannot be enabled");
    found->info.enabled=enabled;if(enabled){bw_assets::text(found->info.expected_hash,sizeof found->info.expected_hash,found->info.hash);found->info.status=BW_ASSET_PACK_READY;}
    bw_assets::capture(*manager);return bw_assets::result(out,true,"Pack selection changed; restart to apply");
}
bool bluewake_asset_packs_move(BluewakeAssetPacks* manager,const char* id,size_t index,BluewakeAssetPackResult* out){
    if(!manager||!id)return bw_assets::result(out,false,"Asset catalog unavailable");std::lock_guard lock(manager->mutex);
    auto found=std::find_if(manager->packs.begin(),manager->packs.end(),[&](const auto& pack){return std::strcmp(pack.info.id,id)==0;});if(found==manager->packs.end()||index>=manager->packs.size())return bw_assets::result(out,false,"Invalid pack or order position");
    auto pack=std::move(*found);manager->packs.erase(found);manager->packs.insert(manager->packs.begin()+index,std::move(pack));
    for(size_t i=0;i<manager->packs.size();++i){manager->packs[i].info.order=static_cast<uint32_t>(i);manager->packs[i].info.priority=static_cast<uint32_t>(i+1);}bw_assets::capture(*manager);return bw_assets::result(out,true,"Pack order changed; later entries have higher priority after restart");
}
bool bluewake_asset_packs_dirty(BluewakeAssetPacks* manager){if(!manager)return false;std::lock_guard lock(manager->mutex);return manager->dirty;}
bool bluewake_asset_packs_restart_needed(BluewakeAssetPacks* manager){if(!manager)return false;std::lock_guard lock(manager->mutex);return manager->runtime_attached&&manager->changed;}
bool bluewake_asset_packs_save(BluewakeAssetPacks* manager,BluewakeAssetPackResult* out){if(!manager)return bw_assets::result(out,false,"Asset catalog unavailable");std::lock_guard lock(manager->mutex);if(!bw_assets::atomic_write(manager->state,bw_assets::serialize(manager->selections)))return bw_assets::result(out,false,"Could not save asset_packs.ini; previous selections were preserved");manager->dirty=false;return bw_assets::result(out,true,"Asset pack selections saved");}
bool bluewake_asset_packs_reload(BluewakeAssetPacks* manager,BluewakeAssetPackResult* out){
    if(!manager)return bw_assets::result(out,false,"Asset catalog unavailable");std::lock_guard lock(manager->mutex);std::string content;std::vector<bw_assets::Selection> selections;
    if(!bw_assets::read(manager->state,content)||!bw_assets::parse_selection(content,false,selections))return bw_assets::result(out,false,"Invalid asset_packs.ini; current selections were preserved");
    return bw_assets::install(*manager,std::move(selections),false,out);
}
BluewakeAssetPackSnapshot* bluewake_asset_packs_snapshot(BluewakeAssetPacks* manager){
    if(!manager)return nullptr;
    try{std::lock_guard lock(manager->mutex);auto snapshot=std::make_unique<BluewakeAssetPackSnapshot>();snapshot->owner=manager;snapshot->packs=manager->packs;snapshot->selections=manager->selections;snapshot->dirty=manager->dirty;snapshot->changed=manager->changed;return snapshot.release();}catch(...){return nullptr;}
}
bool bluewake_asset_packs_restore(BluewakeAssetPacks* manager,const BluewakeAssetPackSnapshot* snapshot,BluewakeAssetPackResult* out){
    if(!manager||!snapshot||snapshot->owner!=manager)return bw_assets::result(out,false,"Pack transaction snapshot unavailable or belongs to another catalog");
    try{auto packs=snapshot->packs;auto selections=snapshot->selections;std::lock_guard lock(manager->mutex);manager->packs=std::move(packs);manager->selections=std::move(selections);manager->dirty=snapshot->dirty;manager->changed=snapshot->changed;return bw_assets::result(out,true,"Unsaved pack change rolled back; renderer and saved selections were kept");}catch(...){return bw_assets::result(out,false,"Pack rollback failed; keep the game open and inspect the preserved selection file");}
}
void bluewake_asset_packs_free_snapshot(BluewakeAssetPackSnapshot* snapshot){delete snapshot;}
bool bluewake_asset_packs_export(BluewakeAssetPacks* manager,const char* file,const char* name,BluewakeAssetPackResult* out){
    if(!manager||!file||!*file||!name||!bw_assets::printable(name,191))return bw_assets::result(out,false,"Choose a preset path and name");std::lock_guard lock(manager->mutex);
    auto extension=bw_assets::utf8(bw_assets::path(file).extension());std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return c>='A'&&c<='Z'?c+32:c;});
    if(extension!=".bwpackpreset")return bw_assets::result(out,false,"Asset pack presets must use .bwpackpreset; settings and bindings were kept");
    if(!bw_assets::atomic_write(bw_assets::path(file),bw_assets::serialize(manager->selections,name)))return bw_assets::result(out,false,"Pack preset export failed; previous file was preserved");
    return bw_assets::result(out,true,"Asset pack preset exported; no game/texture files are included");
}
bool bluewake_asset_packs_import(BluewakeAssetPacks* manager,const char* file,BluewakeAssetPackResult* out){
    if(!manager||!file)return bw_assets::result(out,false,"Choose a pack preset file");std::lock_guard lock(manager->mutex);std::string content;std::vector<bw_assets::Selection> selections;
    if(!bw_assets::read(bw_assets::path(file),content)||!bw_assets::parse_selection(content,true,selections))return bw_assets::result(out,false,"Invalid asset pack preset; existing selections were kept");
    return bw_assets::install(*manager,std::move(selections),true,out);
}
size_t bluewake_asset_packs_conflicts(BluewakeAssetPacks* manager){if(!manager)return 0;std::lock_guard lock(manager->mutex);return manager->conflicts.size();}
bool bluewake_asset_packs_conflict(BluewakeAssetPacks* manager,size_t index,BluewakeAssetPackConflict* out){if(!manager||!out)return false;std::lock_guard lock(manager->mutex);if(index>=manager->conflicts.size())return false;*out=manager->conflicts[index];return true;}
}
