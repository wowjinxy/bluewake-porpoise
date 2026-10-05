// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "network_store.h"
#include "network_wire.h"
#include "network_digest.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
namespace fs=std::filesystem;
using namespace bw_net;
static unsigned checks=0;
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"check failed at line%d: %s\n",__LINE__,#x);std::abort();}++checks;} while(0)
static BwNetworkCompatibility manifest(char digest='1'){
    BwNetworkCompatibility c{};std::strcpy(c.game_id,"GZLE01");c.progression_schema=BW_PROGRESSION_SCHEMA;
    for(char* field:{c.build_id,c.module_digest,c.options_digest})std::memset(field,digest,64);return c;
}
static StoredRoom room(const char* name="test",char compatibility='1'){
    StoredRoom r;r.name=name;r.compatibility=manifest(compatibility);r.id=std::string(32,'a');room_set_credential(r,"private-test-password");
    r.clients.emplace(std::string(32,'b'),RoomClientHistory{});return r;
}
static void award(StoredRoom& r,BwProgressionDelta d){auto& h=r.clients.begin()->second;++h.sequence;
    h.recent.push_back({h.sequence,d});if(h.recent.size()>BW_NETWORK_QUEUE)h.recent.pop_front();if(bw_progression_merge(&r.progress,d))++r.revision;}
static Bytes bytes(const fs::path& file){std::ifstream f(file,std::ios::binary);CHECK(bool(f));return Bytes(std::istreambuf_iterator<char>(f),{});}
static void write(const fs::path& file,const Bytes& b){std::ofstream f(file,std::ios::binary|std::ios::trunc);CHECK(bool(f));f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));CHECK(bool(f));}
static void checksum(Bytes& file){CHECK(file.size()>=84);const auto d=digest(std::string(file.begin(),file.end()-64));std::copy(d.begin(),d.end(),file.end()-64);}
static fs::path file(const fs::path& folder,const StoredRoom& r){return folder/(room_storage_key(r.compatibility,r.name.c_str())+".bwroom");}
static void malformed(const fs::path& base,const Bytes& invalid,const StoredRoom& r,unsigned n){
    const auto folder=base/("malformed-"+std::to_string(n));fs::create_directories(folder);write(file(folder,r),invalid);RoomStore store;std::string error;CHECK(!store.open(folder.u8string().c_str(),error));CHECK(!error.empty());
}
int main(){
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_CALL_REPORTFAULT);_set_error_mode(_OUT_TO_STDERR);
#endif
    const auto temp=fs::absolute(fs::temp_directory_path()).lexically_normal();
    const auto base=temp/("BlueWake-room-store-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(fs::equivalent(base.parent_path(),temp));CHECK(!fs::exists(base));fs::create_directories(base);
    const auto folder=base/fs::u8path(u8"durable-\u2603");auto r=room();std::string error;
    Bytes saved;
    {
        RoomStore store;CHECK(store.open(folder.u8string().c_str(),error));CHECK(store.rooms()==0);
        RoomStore competing;CHECK(!competing.open(folder.u8string().c_str(),error));
        CHECK(store.save(r,error));CHECK(store.rooms()==1);StoredRoom got;bool found=false;
        CHECK(store.load(r.compatibility,r.name.c_str(),got,found,error)&&found&&got.id==r.id&&got.revision==0);
        CHECK(room_credential_matches(got,"private-test-password"));CHECK(!room_credential_matches(got,"wrong"));
        award(r,{2,0x22});award(r,{34,60});CHECK(r.revision==2);CHECK(store.save(r,error));
        saved=bytes(file(folder,r));CHECK(saved.size()<RoomStore::max_file_bytes);
        CHECK(std::string(saved.begin(),saved.end()).find("private-test-password")==std::string::npos);
        auto stale=r;stale.revision=1;CHECK(!store.save(stale,error));
        stale=r;stale.clients.begin()->second.sequence=1;stale.clients.begin()->second.recent.pop_back();CHECK(!store.save(stale,error));
        stale=r;stale.progress.values[34]=30;CHECK(!store.save(stale,error));
        stale=r;stale.id[0]='b';CHECK(!store.save(stale,error));
        stale=r;stale.clients.begin()->second.recent.front().delta={34,30};CHECK(!store.save(stale,error));
        stale=r;stale.progress.values[3]=0x25;++stale.revision;CHECK(!store.save(stale,error));
        stale=r;++stale.revision;CHECK(!store.save(stale,error));
        auto excluded=r;excluded.progress.values[14]=0x50;CHECK(!store.save(excluded,error));
        excluded=r;excluded.progress.values[192]=0x10;CHECK(!store.save(excluded,error)); // unshared StageLife
        CHECK(bytes(file(folder,r))==saved);
        auto other=r;other.name="another";CHECK(store.save(other,error));
        other=r;other.compatibility.options_digest[0]='2';other.progress={};other.revision=0;other.clients.clear();CHECK(store.save(other,error));
        CHECK(store.load(other.compatibility,other.name.c_str(),got,found,error)&&found&&got.progress.values[2]==0);
        auto unknown=manifest('3');CHECK(store.load(unknown,"test",got,found,error)&&!found);
        CHECK(store.load(r.compatibility,"another",got,found,error)&&found&&got.progress.values[2]==0x22);
    }
    {
        RoomStore store;CHECK(store.open(folder.u8string().c_str(),error));StoredRoom got;bool found=false;
        CHECK(store.load(r.compatibility,"test",got,found,error)&&found&&got.revision==2&&got.id==r.id);
        CHECK(got.clients.begin()->second.sequence==2&&got.clients.begin()->second.recent.size()==2);
        for(unsigned i=0;i<140;++i)award(got,{2,0x22});CHECK(store.save(got,error));
        CHECK(got.revision==2&&got.clients.begin()->second.sequence==142&&got.clients.begin()->second.recent.size()==128);
        CHECK(got.clients.begin()->second.recent.front().sequence==15);
        // Genuine unexpected file replacement: the live store must refuse both
        // reads and another ACK-producing mutation rather than repair the file.
        auto corrupt=bytes(file(folder,r));corrupt.back()^=1;write(file(folder,r),corrupt);
        CHECK(!store.load(r.compatibility,"test",r,found,error));CHECK(!store.save(got,error));CHECK(bytes(file(folder,r))==corrupt);
    }
    unsigned attempt=0;
    auto invalid=saved;invalid[7]=2;checksum(invalid);malformed(base,invalid,r,++attempt);
    invalid=saved;invalid[11]=2;checksum(invalid);malformed(base,invalid,r,++attempt);
    invalid=saved;invalid[15]=2;checksum(invalid);malformed(base,invalid,r,++attempt);
    invalid=saved;invalid.back()^=1;malformed(base,invalid,r,++attempt);
    invalid=saved;invalid.push_back(0);malformed(base,invalid,r,++attempt);
    invalid=saved;invalid.resize(80);malformed(base,invalid,r,++attempt);
    invalid.assign(RoomStore::max_file_bytes+1,0);malformed(base,invalid,r,++attempt);
    // Recomputed integrity digest does not authorize another manifest/key or
    // a non-whitelisted progression field.
    invalid=saved;const std::string marker(64,'1');auto match=std::search(invalid.begin()+20,invalid.end()-64,marker.begin(),marker.end());CHECK(match!=invalid.end()-64);*match='2';checksum(invalid);malformed(base,invalid,r,++attempt);
    invalid=saved;Bytes body(invalid.begin()+20,invalid.end()-64);Reader reader{body};BwNetworkCompatibility decoded{};char text[65];
    CHECK(identity(reader,decoded));for(unsigned i=0;i<4;++i)CHECK(reader.text(text,sizeof text));reader.qword();CHECK(reader.word()==2);
    const auto delta_offset=20+reader.at;invalid[delta_offset]=0;invalid[delta_offset+1]=14;checksum(invalid);malformed(base,invalid,r,++attempt);
    {
        const auto bound=base/"bounds";RoomStore store;CHECK(store.open(bound.u8string().c_str(),error));
        for(unsigned i=0;i<RoomStore::max_rooms;++i){auto entry=room(("room"+std::to_string(i)).c_str());CHECK(store.save(entry,error));}
        CHECK(!store.save(room("too_many"),error));CHECK(store.rooms()==16);
        auto excessive=room("room0");for(unsigned i=0;i<129;++i){std::string player(32,'0');std::snprintf(&player[24],9,"%08x",i);player.resize(32);excessive.clients[player]={};}
        CHECK(!store.save(excessive,error));
    }
    {
        const auto pending=base/"pending";fs::create_directories(pending);const auto name=room_storage_key(r.compatibility,r.name.c_str())+".bwroom.tmp-"+std::string(32,'a');
        write(pending/name,Bytes{'p','a','r','t','i','a','l'});RoomStore store;CHECK(store.open(pending.u8string().c_str(),error));CHECK(store.rooms()==0);CHECK(!fs::exists(file(pending,r)));
    }
    {
        const auto linked=base/"hardlink";fs::create_directories(linked);write(file(linked,r),saved);fs::create_hard_link(file(linked,r),linked/"outside_alias");
        RoomStore store;CHECK(!store.open(linked.u8string().c_str(),error));
    }
    CHECK(fs::equivalent(fs::canonical(base).parent_path(),temp));fs::remove_all(base);
    std::printf("Durable room storage checks=%u; synthetic protocol facts only, no game/CARD/devices\n",checks);return 0;
}
