// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone copied-value journal fixtures, NOT native CARD/save/load proof.
#include "pc/network/pc_network_client_store.h"
#include "network_digest.h"
#include "network_wire.h"
#include "atomic_file.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace pc_net;
namespace fs=std::filesystem;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
#define GOOD(x) do {bool ok=(x);if(!ok)std::fprintf(stderr,"journal error: %s\n",error);CHECK(ok);} while(0)
using Store=std::unique_ptr<PcNetworkClientStore,decltype(&pc_network_client_close)>;
static fs::path ioPath(const fs::path& path){
#ifdef _WIN32
    std::unique_ptr<wchar_t,decltype(&std::free)> wide(pc_net_atomic_wide_path(path.u8string().c_str()),std::free);CHECK(bool(wide));return fs::path(wide.get());
#else
    return path;
#endif
}
static Store open(const fs::path& path,const PcNetworkCompatibility& compatibility,const char* profile,char* error){
    return Store(pc_network_client_open(path.u8string().c_str(),&compatibility,profile,error,256),pc_network_client_close);}
static Bytes read(const fs::path& path){std::ifstream in(ioPath(path),std::ios::binary);CHECK(bool(in));return Bytes(std::istreambuf_iterator<char>(in),{});}
static void write(const fs::path& path,const Bytes& bytes){std::ofstream out(ioPath(path),std::ios::binary|std::ios::trunc);CHECK(bool(out));out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));CHECK(bool(out));}
static Bytes gci(){Bytes bytes(0x18040,0);std::memcpy(bytes.data(),"GZLE01",6);std::memcpy(bytes.data()+8,"gczelda\0",8);bytes[0x39]=12;return bytes;}
static PcNetworkReward step(unsigned id){return {UINT64_C(3)<<56|id,50,PC_PERMANENT_STEP,1};}
static void checksum(Bytes& bytes){CHECK(bytes.size()>=84);const auto hash=digest(std::string(bytes.begin(),bytes.end()-64));std::copy(hash.begin(),hash.end(),bytes.end()-64);}
int main(){
    const auto base=fs::absolute(fs::temp_directory_path())/("BlueWake-native-client-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(!fs::exists(base));fs::create_directories(base);auto folder=base/fs::u8path(u8"journal-\u2603");
    while(folder.u8string().size()<310)folder/="owned-long-component";
    PcNetworkCompatibility compatibility{};pc_network_native_compatibility(&compatibility);const std::string profile(64,'a'),room(32,'b');char error[256]{};
    auto store=open(folder,compatibility,profile.c_str(),error);CHECK(store);CHECK(!open(folder,compatibility,profile.c_str(),error));
    auto state=std::make_unique<PcNetworkClientState>();CHECK(pc_network_client_copy(store.get(),state.get())&&!state->canonical_present&&state->serial==1);
    PcNetworkClientProjection replay{};replay.serial=123;
    CHECK(!pc_network_client_projection(store.get(),&replay,error,sizeof error)&&replay.serial==123);
    GOOD(pc_network_client_enqueue(store.get(),step(1),error,sizeof error));CHECK(pc_network_client_copy(store.get(),state.get())&&state->outgoing_count==1);
    const auto serial=state->serial;GOOD(pc_network_client_enqueue(store.get(),step(1),error,sizeof error));CHECK(pc_network_client_copy(store.get(),state.get())&&state->serial==serial);
    auto conflict=step(1);conflict.key=51;CHECK(!pc_network_client_enqueue(store.get(),conflict,error,sizeof error));
    PcNetworkFacts empty{},projected{};projected.values[50]=60;uint64_t token=0;
    CHECK(!pc_network_client_prepare_offline_save(store.get(),0,&projected,0,&token,error,sizeof error)); // never bound
    GOOD(pc_network_client_stage(store.get(),room.c_str(),&empty,nullptr,0,0,error,sizeof error));
    GOOD(pc_network_client_projection(store.get(),&replay,error,sizeof error));CHECK(replay.provisional&&replay.revision==0&&replay.facts.values[50]==60&&replay.facts.values[51]==30);
    const auto projectedSerial=replay.serial;GOOD(pc_network_client_projection(store.get(),&replay,error,sizeof error));CHECK(replay.serial==projectedSerial);
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->serial==projectedSerial&&state->outgoing_count==1); // projection is not an ACK/mutation
    const auto journal=folder/"client.bwjournal";const auto unchanged=read(journal);
    CHECK(!pc_network_client_stage(store.get(),std::string(32,'c').c_str(),&empty,nullptr,0,0,error,sizeof error));CHECK(read(journal)==unchanged);
    CHECK(!pc_network_client_prepare_save(store.get(),0,&projected,0,&token,error,sizeof error)); // unconfirmed outbox
    auto ratchet=projected;ratchet.values[50]=99;CHECK(!pc_network_client_prepare_offline_save(store.get(),0,&ratchet,0,&token,error,sizeof error));
    GOOD(pc_network_client_prepare_offline_save(store.get(),0,&projected,0,&token,error,sizeof error));
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->intents[0].provisional&&state->intents[0].pending_count==1&&state->intents[0].revision==0);
    auto bytes=gci();const auto actualGci=base/"synthetic-fixture.gci";write(actualGci,bytes);char hash[65]{};
    GOOD(pc_network_client_hash_gci(actualGci.u8string().c_str(),hash,error,sizeof error));CHECK(hash==digest(std::string(bytes.begin(),bytes.end())));
    store.reset();store=open(folder,compatibility,profile.c_str(),error);CHECK(store); // provisional crash before receipt
    bool recovered=false;GOOD(pc_network_client_recover_loaded(store.get(),0,actualGci.u8string().c_str(),hash,&projected,&recovered,error,sizeof error));CHECK(recovered);
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->receipts[0].valid&&state->receipts[0].provisional&&state->receipts[0].pending_count==1&&state->outgoing_count==1);
    store.reset();store=open(folder,compatibility,profile.c_str(),error);CHECK(store);CHECK(pc_network_client_copy(store.get(),state.get())&&state->outgoing_count==1&&state->receipts[0].provisional);
    bool confirmed=false;CHECK(!pc_network_client_confirm_loaded(store.get(),0,actualGci.u8string().c_str(),hash,&projected,&confirmed,error,sizeof error)&&!confirmed);
    const auto first=step(1);GOOD(pc_network_client_stage(store.get(),room.c_str(),&projected,&first,1,1,error,sizeof error));
    GOOD(pc_network_client_projection(store.get(),&replay,error,sizeof error));CHECK(!replay.provisional&&replay.facts.values[50]==60&&replay.revision==1); // staged ACK cannot step twice
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->outgoing_count==0&&state->receipts[0].provisional&&state->receipts[0].revision==0);
    GOOD(pc_network_client_confirm_loaded(store.get(),0,actualGci.u8string().c_str(),hash,&projected,&confirmed,error,sizeof error));
    CHECK(confirmed&&pc_network_client_copy(store.get(),state.get())&&!state->receipts[0].provisional&&state->receipts[0].reconciled&&state->receipts[0].revision==1);
    GOOD(pc_network_client_prepare_save(store.get(),1,&projected,1,&token,error,sizeof error));
    bytes[100]=17;write(actualGci,bytes);store.reset(); // crash after GCI publication, before journal receipt
    store=open(folder,compatibility,profile.c_str(),error);CHECK(store);CHECK(pc_network_client_copy(store.get(),state.get())&&state->intents[1].active&&!state->receipts[1].valid);
    GOOD(pc_network_client_hash_gci(actualGci.u8string().c_str(),hash,error,sizeof error));recovered=false;
    CHECK(!pc_network_client_recover_loaded(store.get(),1,actualGci.u8string().c_str(),hash,&ratchet,&recovered,error,sizeof error)&&!recovered);
    CHECK(!pc_network_client_recover_loaded(store.get(),1,actualGci.u8string().c_str(),std::string(64,'0').c_str(),&projected,&recovered,error,sizeof error));
    GOOD(pc_network_client_recover_loaded(store.get(),1,actualGci.u8string().c_str(),hash,&projected,&recovered,error,sizeof error));
    CHECK(recovered&&pc_network_client_copy(store.get(),state.get())&&state->receipts[1].recovered&&!state->receipts[1].provisional&&!state->intents[1].active);
    GOOD(pc_network_client_prepare_save(store.get(),2,&projected,1,&token,error,sizeof error));
    const auto priorHash=std::string(hash);bytes[101]=18;write(actualGci,bytes);
    CHECK(!pc_network_client_recover_loaded(store.get(),2,actualGci.u8string().c_str(),priorHash.c_str(),&projected,&recovered,error,sizeof error));
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->intents[2].active);GOOD(pc_network_client_cancel_save(store.get(),token,error,sizeof error));
    GOOD(pc_network_client_prepare_save(store.get(),2,&projected,1,&token,error,sizeof error));
    CHECK(!pc_network_client_commit_save(store.get(),token,(base/"missing.gci").u8string().c_str(),error,sizeof error));
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->intents[2].active); // read refusal never poisons journal
#ifdef _WIN32
    HANDLE cardLock=CreateFileW(ioPath(actualGci).c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);CHECK(cardLock!=INVALID_HANDLE_VALUE);
    CHECK(!pc_network_client_commit_save(store.get(),token,actualGci.u8string().c_str(),error,sizeof error));CHECK(CloseHandle(cardLock));
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->intents[2].active);
#endif
    GOOD(pc_network_client_commit_save(store.get(),token,actualGci.u8string().c_str(),error,sizeof error));
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->receipts[2].valid&&!state->receipts[2].provisional&&!state->receipts[2].recovered);
    // A real write-denial preserves the acknowledged journal/outbox and fails
    // closed; an owned pending file remains available as private diagnostics.
    const auto beforeFailure=read(journal);
#ifdef _WIN32
    HANDLE locked=CreateFileW(ioPath(journal).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);CHECK(locked!=INVALID_HANDLE_VALUE);
    CHECK(!pc_network_client_enqueue(store.get(),{UINT64_C(2)<<56|7,2,PC_PERMANENT_MAX,1},error,sizeof error));CHECK(CloseHandle(locked));
    CHECK(read(journal)==beforeFailure&&!pc_network_client_copy(store.get(),state.get()));store.reset();store=open(folder,compatibility,profile.c_str(),error);CHECK(store);
#else
    auto altered=beforeFailure;altered.back()^=1;write(journal,altered);
    CHECK(!pc_network_client_enqueue(store.get(),{UINT64_C(2)<<56|7,2,PC_PERMANENT_MAX,1},error,sizeof error));CHECK(!pc_network_client_copy(store.get(),state.get()));
    store.reset();write(journal,beforeFailure);store=open(folder,compatibility,profile.c_str(),error);CHECK(store);
#endif
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->outgoing_count==0);store.reset();
    CHECK(!open(folder,compatibility,std::string(64,'f').c_str(),error));CHECK(read(journal)==beforeFailure);
    auto old=compatibility;old.progression_schema=1;CHECK(!open(folder,old,profile.c_str(),error));
    old=compatibility;old.progression_schema=2;CHECK(!open(folder,old,profile.c_str(),error));
    CHECK(read(journal)==beforeFailure); // schema refusal does not rewrite the journal
    unsigned malformed=0;const auto reject=[&](Bytes invalid){auto dir=base/("malformed-"+std::to_string(++malformed));fs::create_directories(dir);write(dir/"client.bwjournal",invalid);
        CHECK(!open(dir,compatibility,profile.c_str(),error));CHECK(read(dir/"client.bwjournal")==invalid);};
    auto invalid=beforeFailure;invalid.back()^=1;reject(invalid);invalid=beforeFailure;invalid[11]=2;checksum(invalid);reject(invalid);
    invalid=beforeFailure;invalid.push_back(0);reject(invalid);reject(Bytes(PC_NETWORK_CLIENT_MAX_FILE+1,0));
    const auto linked=base/"linked";fs::create_directories(linked);write(linked/"client.bwjournal",beforeFailure);fs::create_hard_link(linked/"client.bwjournal",base/"journal-alias");CHECK(!open(linked,compatibility,profile.c_str(),error));
    CHECK(!open(base/"escape"/".."/"outside",compatibility,profile.c_str(),error));
    fs::create_hard_link(actualGci,base/"gci-alias");CHECK(!pc_network_client_hash_gci(actualGci.u8string().c_str(),hash,error,sizeof error));
    // Full ledger fits well below512KiB, and remains sorted on reopen.
    const auto bounded=base/"ledger-bound";store=open(bounded,compatibility,profile.c_str(),error);CHECK(store);
    std::vector<PcNetworkReward> all;for(unsigned i=0;i<PC_NETWORK_REWARDS;++i)all.push_back({UINT64_C(2)<<56|(i+1),1,PC_PERMANENT_MAX,1});
    PcNetworkFacts telescope{};telescope.values[1]=1;GOOD(pc_network_client_stage(store.get(),room.c_str(),&telescope,all.data(),unsigned(all.size()),PC_NETWORK_REWARDS,error,sizeof error));
    CHECK(fs::file_size(bounded/"client.bwjournal")<PC_NETWORK_CLIENT_MAX_FILE);store.reset();store=open(bounded,compatibility,profile.c_str(),error);CHECK(store);
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->reward_count==PC_NETWORK_REWARDS&&state->rewards[PC_NETWORK_REWARDS-1].reward_id==all.back().reward_id);
    all.push_back({UINT64_C(2)<<56|(PC_NETWORK_REWARDS+1),1,PC_PERMANENT_MAX,1});CHECK(!pc_network_client_stage(store.get(),room.c_str(),&telescope,all.data(),unsigned(all.size()),PC_NETWORK_REWARDS+1,error,sizeof error));
    store.reset();const auto queue=base/"outbox-bound";store=open(queue,compatibility,profile.c_str(),error);CHECK(store);
    for(unsigned i=0;i<PC_NETWORK_QUEUE;++i)GOOD(pc_network_client_enqueue(store.get(),{UINT64_C(2)<<56|(i+1),1,PC_PERMANENT_MAX,1},error,sizeof error));
    CHECK(!pc_network_client_enqueue(store.get(),{UINT64_C(2)<<56|(PC_NETWORK_QUEUE+1),1,PC_PERMANENT_MAX,1},error,sizeof error));
    store.reset();store=open(queue,compatibility,profile.c_str(),error);CHECK(store);CHECK(pc_network_client_copy(store.get(),state.get())&&state->outgoing_count==PC_NETWORK_QUEUE);
    store.reset();store=open(base/"base-capacity",compatibility,profile.c_str(),error);CHECK(store);
    GOOD(pc_network_client_stage(store.get(),room.c_str(),&empty,nullptr,0,0,error,sizeof error));
    PcNetworkFacts baseCapacity{};baseCapacity.values[50]=baseCapacity.values[51]=30;
    GOOD(pc_network_client_prepare_save(store.get(),0,&baseCapacity,0,&token,error,sizeof error));GOOD(pc_network_client_cancel_save(store.get(),token,error,sizeof error));
    store.reset();store=open(base/"max-projection",compatibility,profile.c_str(),error);CHECK(store);
    PcNetworkFacts camera{};camera.values[9]=1;const PcNetworkReward ownedCamera{UINT64_C(2)<<56|10,9,PC_PERMANENT_MAX,1};
    GOOD(pc_network_client_stage(store.get(),room.c_str(),&camera,&ownedCamera,1,1,error,sizeof error));
    const PcNetworkReward cameraUpgrade{UINT64_C(2)<<56|11,9,PC_PERMANENT_MAX,2};
    GOOD(pc_network_client_enqueue(store.get(),cameraUpgrade,error,sizeof error));GOOD(pc_network_client_enqueue(store.get(),cameraUpgrade,error,sizeof error));
    GOOD(pc_network_client_enqueue(store.get(),{UINT64_C(3)<<56|10,48,PC_PERMANENT_STEP,1},error,sizeof error));
    GOOD(pc_network_client_enqueue(store.get(),{UINT64_C(3)<<56|11,48,PC_PERMANENT_STEP,1},error,sizeof error));
    GOOD(pc_network_client_projection(store.get(),&replay,error,sizeof error));
    CHECK(replay.provisional&&replay.facts.values[9]==2&&replay.facts.values[48]==2&&replay.facts.values[50]==30&&replay.facts.values[51]==30);
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->outgoing_count==3&&state->revision==1); // duplicate operation once
    // Crash AFTER intent but BEFORE GCI: a qualified deficient snapshot may
    // retire only that intent, preserving priorreceipt and completed outbox.
    store.reset();const auto replayDir=base/"offline-replay";store=open(replayDir,compatibility,profile.c_str(),error);CHECK(store);
    GOOD(pc_network_client_stage(store.get(),room.c_str(),&empty,nullptr,0,0,error,sizeof error));
    const auto replayGci=base/"replay-fixture.gci";auto replayBytes=gci();write(replayGci,replayBytes);
    GOOD(pc_network_client_prepare_save(store.get(),0,&empty,0,&token,error,sizeof error));
    GOOD(pc_network_client_commit_save(store.get(),token,replayGci.u8string().c_str(),error,sizeof error));
    const auto oldReceiptToken=token;GOOD(pc_network_client_enqueue(store.get(),step(1),error,sizeof error));
    GOOD(pc_network_client_projection(store.get(),&replay,error,sizeof error));
    GOOD(pc_network_client_prepare_offline_save(store.get(),0,&replay.facts,0,&token,error,sizeof error));const auto abandonedToken=token;
    store.reset();store=open(replayDir,compatibility,profile.c_str(),error);CHECK(store);
    GOOD(pc_network_client_hash_gci(replayGci.u8string().c_str(),hash,error,sizeof error));
    PcNetworkClientLoadDisposition disposition=PC_NETWORK_CLIENT_LOAD_NONE;
    GOOD(pc_network_client_reconcile_loaded(store.get(),1,replayGci.u8string().c_str(),hash,&empty,&disposition,error,sizeof error));
    CHECK(disposition==PC_NETWORK_CLIENT_LOAD_NONE&&pc_network_client_copy(store.get(),state.get())&&state->intents[0].active);
    CHECK(!pc_network_client_prepare_offline_save(store.get(),1,&replay.facts,0,&token,error,sizeof error)); // conservative other-slot pending intent
    CHECK(!pc_network_client_reconcile_loaded(store.get(),0,replayGci.u8string().c_str(),std::string(64,'0').c_str(),&empty,&disposition,error,sizeof error));
    auto invalidFacts=empty;invalidFacts.values[0]=1;
    CHECK(!pc_network_client_reconcile_loaded(store.get(),0,replayGci.u8string().c_str(),hash,&invalidFacts,&disposition,error,sizeof error));
    CHECK(!pc_network_client_reconcile_loaded(store.get(),0,(base/"missing.gci").u8string().c_str(),hash,&empty,&disposition,error,sizeof error));
    CHECK(!pc_network_client_reconcile_loaded(store.get(),0,replayGci.u8string().c_str(),hash,&ratchet,&disposition,error,sizeof error)); // ambiguous higher STEP retained
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->intents[0].token==abandonedToken&&state->outgoing_count==1);
    GOOD(pc_network_client_reconcile_loaded(store.get(),0,replayGci.u8string().c_str(),hash,&empty,&disposition,error,sizeof error));
    CHECK(disposition==PC_NETWORK_CLIENT_LOAD_NOT_PRESENT&&pc_network_client_copy(store.get(),state.get())&&!state->intents[0].active&&state->outgoing_count==1&&state->receipts[0].token==oldReceiptToken);
    GOOD(pc_network_client_projection(store.get(),&replay,error,sizeof error));CHECK(replay.provisional&&replay.facts.values[50]==60);
    GOOD(pc_network_client_prepare_offline_save(store.get(),0,&replay.facts,0,&token,error,sizeof error));CHECK(token!=abandonedToken);
    replayBytes[100]=60;write(replayGci,replayBytes);store.reset(); // written but receipt publication absent
    store=open(replayDir,compatibility,profile.c_str(),error);CHECK(store);GOOD(pc_network_client_hash_gci(replayGci.u8string().c_str(),hash,error,sizeof error));
    GOOD(pc_network_client_reconcile_loaded(store.get(),0,replayGci.u8string().c_str(),hash,&replay.facts,&disposition,error,sizeof error));
    CHECK(disposition==PC_NETWORK_CLIENT_LOAD_PERSISTED&&pc_network_client_copy(store.get(),state.get())&&state->receipts[0].provisional&&state->receipts[0].recovered&&state->outgoing_count==1);
    store.reset();const auto braceletDir=base/"bracelet-outbox";
    store=open(braceletDir,compatibility,profile.c_str(),error);CHECK(store);
    GOOD(pc_network_client_stage(store.get(),room.c_str(),&empty,nullptr,0,0,error,sizeof error));
    const PcNetworkReward bracelet{UINT64_C(1)<<56|24,PC_PERMANENT_POWER_BRACELETS,PC_PERMANENT_MAX,1};
    GOOD(pc_network_client_enqueue(store.get(),bracelet,error,sizeof error));
    GOOD(pc_network_client_enqueue(store.get(),bracelet,error,sizeof error));
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->outgoing_count==1);
    GOOD(pc_network_client_projection(store.get(),&replay,error,sizeof error));
    CHECK(replay.provisional&&replay.facts.values[24]==1&&state->revision==0);
    PcNetworkFacts ownedBracelet{};ownedBracelet.values[24]=1;
    GOOD(pc_network_client_stage(store.get(),room.c_str(),&ownedBracelet,&bracelet,1,1,error,sizeof error));
    store.reset();store=open(braceletDir,compatibility,profile.c_str(),error);CHECK(store);
    CHECK(pc_network_client_copy(store.get(),state.get())&&state->outgoing_count==0&&
        state->reward_count==1&&state->revision==1&&state->facts.values[24]==1);
    std::printf("native network client journal: PASS checks=%u fixture=%s\n",checks,base.u8string().c_str());return 0;
}
