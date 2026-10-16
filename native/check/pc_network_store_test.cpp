// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted durable-store cases from BlueWake tests/network_store_test.cpp.
#include "network_store.h"
#include "network_wire.h"
#include "network_digest.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#ifdef _WIN32
#include <windows.h>
#endif
namespace fs=std::filesystem;
using namespace pc_net;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
#define STORE(x) do { const bool succeeded=(x); if(!succeeded)std::fprintf(stderr,"store error: %s\n",error.c_str()); CHECK(succeeded); } while(0)
static StoredRoom room(const char* name="test"){
    StoredRoom r;pc_network_native_compatibility(&r.compatibility);r.name=name;r.id=std::string(32,'a');
    room_set_credential(r,"fixture-secret-not-in-file");r.clients.emplace(std::string(32,'b'),RoomClientHistory{});return r;
}
static void award(StoredRoom& r){auto& h=r.clients.begin()->second;++h.sequence;h.recent.push_back({h.sequence,{1,1}});
    if(h.recent.size()>PC_NETWORK_QUEUE)h.recent.pop_front();if(pc_network_fact_merge(&r.progress,{1,1}))++r.revision;}
static void claim(StoredRoom& r,PcNetworkReward reward){auto& h=r.clients.begin()->second;++h.sequence;
    if(!r.rewards.count(reward.reward_id)){r.rewards.emplace(reward.reward_id,reward);
        r.progress.values[reward.key]=pc_permanent_operation_result(reward.key,r.progress.values[reward.key],reward.operation,reward.value);++r.revision;}
    h.recent.push_back({h.sequence,{reward.key,r.progress.values[reward.key]},reward});
    if(h.recent.size()>PC_NETWORK_QUEUE)h.recent.pop_front();}
static fs::path file(const fs::path& base,const StoredRoom& r){return base/(room_storage_key(r.compatibility,r.name.c_str())+".bwroom");}
static Bytes read(const fs::path& path){std::ifstream stream(path,std::ios::binary);CHECK(bool(stream));return Bytes(std::istreambuf_iterator<char>(stream),{});}
static void write(const fs::path& path,const Bytes& bytes){std::ofstream stream(path,std::ios::binary|std::ios::trunc);CHECK(bool(stream));
    stream.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));CHECK(bool(stream));}
static void checksum(Bytes& bytes){CHECK(bytes.size()>=84);const auto sum=digest(std::string(bytes.begin(),bytes.end()-64));std::copy(sum.begin(),sum.end(),bytes.end()-64);}
int main(){
    const auto temp=fs::absolute(fs::temp_directory_path()).lexically_normal();
    const auto base=temp/("BlueWake-native-store-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(!fs::exists(base));fs::create_directories(base);const auto folder=base/fs::u8path(u8"durable-\u2603");
    auto r=room();std::string error;Bytes saved;
    {
        RoomStore store;STORE(store.open(folder.u8string().c_str(),error));RoomStore competing;
        CHECK(!competing.open(folder.u8string().c_str(),error));STORE(store.save(r,error));
        StoredRoom got;bool found=false;CHECK(store.load(r.compatibility,r.name.c_str(),got,found,error)&&found&&got.revision==0);
        CHECK(room_credential_matches(got,"fixture-secret-not-in-file")&&!room_credential_matches(got,"wrong"));
        award(r);STORE(store.save(r,error));saved=read(file(folder,r));
        CHECK(std::string(saved.begin(),saved.end()).find("fixture-secret-not-in-file")==std::string::npos);
        auto invalid=r;invalid.progress.values[1]=0;CHECK(!store.save(invalid,error));
        invalid=r;invalid.clients.begin()->second.recent.front().delta.value=0;CHECK(!store.save(invalid,error));
        invalid=r;++invalid.revision;CHECK(!store.save(invalid,error));
        invalid=r;invalid.id[0]='c';CHECK(!store.save(invalid,error));
        invalid=r;invalid.progress.values[0]=1;CHECK(!store.save(invalid,error));
        CHECK(read(file(folder,r))==saved);
        auto different=r.compatibility;different.rules_digest[0]^=1;CHECK(!store.load(different,r.name.c_str(),got,found,error));
        CHECK(store.load(r.compatibility,"different",got,found,error)&&!found);
    }
    {
        RoomStore store;STORE(store.open(folder.u8string().c_str(),error));StoredRoom got;bool found=false;
        CHECK(store.load(r.compatibility,r.name.c_str(),got,found,error)&&found&&got.id==r.id&&got.revision==1);
        for(unsigned i=0;i<140;++i)award(got);STORE(store.save(got,error));
        CHECK(got.revision==1&&got.clients.begin()->second.sequence==141&&got.clients.begin()->second.recent.size()==128);
        CHECK(got.clients.begin()->second.recent.front().sequence==14);
        saved=read(file(folder,r));r=got;
#ifdef _WIN32
        // Genuine publication failure: an owned read handle denies replacement
        // while reads still succeed. Never change ACLs or unrelated files.
        HANDLE locked=CreateFileW(file(folder,r).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        CHECK(locked!=INVALID_HANDLE_VALUE);auto next=r;award(next);CHECK(!store.save(next,error));CHECK(!error.empty());
        CHECK(CloseHandle(locked));CHECK(read(file(folder,r))==saved);CHECK(!store.save(next,error));
#else
        auto corrupt=saved;corrupt.back()^=1;write(file(folder,r),corrupt);
        CHECK(!store.load(r.compatibility,r.name.c_str(),got,found,error));CHECK(!store.save(got,error));
#endif
    }
    {
        RoomStore recovered;StoredRoom got;bool found=false;
#ifdef _WIN32
        CHECK(recovered.open(folder.u8string().c_str(),error));
        CHECK(recovered.load(r.compatibility,r.name.c_str(),got,found,error)&&found&&got.clients.begin()->second.sequence==141);
#else
        CHECK(!recovered.open(folder.u8string().c_str(),error));
#endif
    }
    unsigned malformed=0;
    const auto reject=[&](Bytes bytes){const auto dir=base/("invalid-"+std::to_string(++malformed));fs::create_directories(dir);
        write(file(dir,r),bytes);RoomStore store;CHECK(!store.open(dir.u8string().c_str(),error));CHECK(!error.empty());};
    auto invalid=saved;std::memcpy(invalid.data(),"BWRS",4);checksum(invalid);reject(invalid);
    invalid=saved;invalid[7]=1;checksum(invalid);reject(invalid); // prior store schema
    invalid=saved;invalid[11]=1;checksum(invalid);reject(invalid); // legacy protocol 1
    invalid=saved;invalid[15]=1;checksum(invalid);reject(invalid); // prior fact schema
    invalid=saved;invalid.back()^=1;reject(invalid);
    invalid=saved;invalid.push_back(0);reject(invalid);
    invalid=saved;invalid.resize(80);reject(invalid);
    reject(Bytes(RoomStore::max_file_bytes+1,0));
    {
        const auto ledger=base/"authored";auto entry=room("authored");const PcNetworkReward first{UINT64_C(3)<<56|1,50,PC_PERMANENT_STEP,1};
        RoomStore store;STORE(store.open(ledger.u8string().c_str(),error));STORE(store.save(entry,error));
        claim(entry,first);STORE(store.save(entry,error));CHECK(entry.progress.values[50]==60&&entry.revision==1&&entry.rewards.size()==1);
        claim(entry,first);STORE(store.save(entry,error));CHECK(entry.progress.values[50]==60&&entry.revision==1&&entry.rewards.size()==1);
        claim(entry,{UINT64_C(3)<<56|2,50,PC_PERMANENT_STEP,1});STORE(store.save(entry,error));CHECK(entry.progress.values[50]==99&&entry.revision==2);
        claim(entry,first);STORE(store.save(entry,error));CHECK(entry.progress.values[50]==99&&entry.revision==2);
        auto changed=entry;changed.rewards.begin()->second.value=2;CHECK(!store.save(changed,error));
        changed=entry;changed.rewards.erase(changed.rewards.begin());CHECK(!store.save(changed,error));
        changed=entry;changed.progress.values[48]=1;++changed.revision;CHECK(!store.save(changed,error)); // no producer, no rank ratchet
        changed=entry;changed.clients.begin()->second.recent.back().reward={};CHECK(!store.save(changed,error));
        StoredRoom loaded;bool found=false;STORE(store.load(entry.compatibility,entry.name.c_str(),loaded,found,error));
        CHECK(found&&loaded.rewards.size()==2&&loaded.progress.values[50]==99&&loaded.revision==2);
    }
    {
        RoomStore reopened;StoredRoom loaded;bool found=false;STORE(reopened.open((base/"authored").u8string().c_str(),error));
        PcNetworkCompatibility compatibility{};pc_network_native_compatibility(&compatibility);
        STORE(reopened.load(compatibility,"authored",loaded,found,error));
        CHECK(found&&loaded.rewards.size()==2&&loaded.progress.values[50]==99&&loaded.revision==2);
        CHECK(loaded.clients.begin()->second.sequence==4&&loaded.clients.begin()->second.recent.back().reward.reward_id==(UINT64_C(3)<<56|1));
    }
    {
        const auto bound=base/"bounds";RoomStore store;CHECK(store.open(bound.u8string().c_str(),error));
        for(unsigned i=0;i<RoomStore::max_rooms;++i){auto entry=room(("room"+std::to_string(i)).c_str());CHECK(store.save(entry,error));}
        CHECK(!store.save(room("too_many"),error)&&store.rooms()==16);
        auto excessive=room("room0");for(unsigned i=0;i<129;++i){char player[33];std::snprintf(player,sizeof player,"%032x",i);excessive.clients[player]={};}
        CHECK(!store.save(excessive,error));
    }
    {
        const auto linked=base/"hardlink";fs::create_directories(linked);write(file(linked,r),saved);
        fs::create_hard_link(file(linked,r),linked/"outside_alias");RoomStore store;CHECK(!store.open(linked.u8string().c_str(),error));
    }
    std::printf("native network durable store: PASS checks=%u fixture=%s\n",checks,base.u8string().c_str());
    return 0;
}
