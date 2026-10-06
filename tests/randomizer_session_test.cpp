// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic copied rewards/CARD payloads only. Actual storage/leases are used.
#include "randomizer_session.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace bluewake::randomizer;
namespace fs=std::filesystem;
using transaction::Status;using transaction::State;using session::Session;
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){std::fprintf(stderr,"session line%d failed: %s\n",__LINE__,#x);std::abort();}}while(0)
static seed::Profile profile(std::uint64_t value=7) {
  seed::Request r;r.seed=value;r.logic_contract_digest=seed::sha256("fixture logic metadata");r.start_policy_digest=seed::sha256("fixture start metadata");
  r.options={{"logic_obscurity",std::string("None")},{"logic_precision",std::string("None")},{"required_bosses",false},{"skip_rematch_bosses",false},{"sword_mode",std::string("No Starting Sword")}};
  r.placements={{seed::linkug_location_id(),seed::basic_picto_id()}};
  const auto result=seed::ProfileBuilder(imported_catalog()).build(r);CHECK(bool(result));return *result.value;
}
static storage::Bytes card(){return storage::Bytes(64,83);}
static std::string digest(const storage::Bytes& value){return seed::sha256(std::string(value.begin(),value.end()));}
static transaction::Mount mount(){return {digest(card()),seed::sha256("synthetic module"),0};}
static transaction::Owner owner(){return {mount().module_digest,1,2,3,4,5,6,7,0x80AC727C,33,"LinkUG",0,11,0};}
static transaction::Actor chest(){return {0x80ACC384,35,294,113,8,9,2,2,-1};}
static transaction::Actor item(){return {0x80AB7000,36,259,131,10,11,3,2,0};}
static transaction::Selection selection(){return {owner(),chest(),{},50,0x800261E8,0xC1DF2780,chest().address+0x1F8,0xFF000280,0x06FF,6,0x81700000};}
static session::Snapshot snapshot(Session& s){session::Snapshot out;std::string error;CHECK(s.snapshot(out,error)&&error.empty());return out;}
static session::Lease lease(Session& s){session::Lease out;std::string error;CHECK(s.lease(out,error)&&error.empty()&&out.valid());return out;}
static std::string path(Session& s){std::string out,error;CHECK(s.working_card_path(out,error)&&error.empty()&&!out.empty());return out;}
static storage::Bytes read(const fs::path& p){std::ifstream f(p,std::ios::binary);CHECK(bool(f));storage::Bytes b(std::istreambuf_iterator<char>(f),{});CHECK(!f.bad());return b;}
static void write(const fs::path& p,const storage::Bytes& b){std::ofstream f(p,std::ios::binary|std::ios::trunc);CHECK(bool(f));f.write(reinterpret_cast<const char*>(b.data()),b.size());f.close();CHECK(!f.fail());}
static std::unique_ptr<Session> open(const fs::path& p,const seed::Profile& profile,const transaction::Mount& mount,const storage::Bytes* initial,std::string& error){return Session::open(p.u8string(),profile,mount,initial,error);}
static void earn(Session& s,const session::Lease& token) {
  std::string error;
  CHECK(s.with_ledger(token,[](transaction::Ledger& ledger){
    CHECK(ledger.bind(owner())==Status::Accepted);const auto decision=ledger.select(selection());CHECK(decision.status==Status::Accepted&&decision.reward==35);
    CHECK(ledger.created({owner(),chest(),50,0xC1DF2780,item().pid,0x81700000})==Status::Accepted);
    CHECK(ledger.bind_item(owner(),item())==Status::Accepted);
    CHECK(ledger.award_enter({owner(),item(),{},70,0xC00C2DFC,0xC0770A08,35,35,0x81700100})==Status::Accepted);
    CHECK(ledger.award_return({owner(),item(),{35,1,0},70,0xC0770A08,0x81700100})==Status::Accepted);
  },error));CHECK(error.empty());
}
int main(int argc,char** argv) {
  CHECK(argc==2);const fs::path root=fs::u8path(argv[1])/std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  CHECK(fs::create_directories(root));const auto p=profile();const auto baseline=card();std::string error;
  CHECK(!open(root/"missing",p,mount(),nullptr,error));CHECK(!error.empty());
  CHECK(!fs::exists(root/"missing/current.bwseed")&&!fs::exists(root/"missing/working.card"));
  auto bad_card=baseline;bad_card[0]++;
  CHECK(!open(root/"bad-initial",p,mount(),&bad_card,error));CHECK(!fs::exists(root/"bad-initial"));
  const storage::Bytes empty,huge(storage::Store::MaxCardBytes+1,0);
  CHECK(!open(root/"empty",p,mount(),&empty,error));CHECK(!open(root/"oversized",p,mount(),&huge,error));
  CHECK(!open(root/"invalid-profile",seed::Profile{},mount(),&baseline,error));
  const auto folder=root/fs::u8path("profile-\xE2\x98\x83");
  auto s=open(folder,p,mount(),&baseline,error);CHECK(bool(s)&&error.empty());
  CHECK(read(fs::u8path(path(*s)))==baseline);const auto before=snapshot(*s);CHECK(before.generation==1&&!before.retired&&!before.recovery_needed&&before.rewards.empty());
  CHECK(before.profile==p.identity()&&before.quest==0&&before.origin_card==mount().origin_card_digest&&before.module==mount().module_digest);
  CHECK(before.card_digest==digest(baseline));
  const auto token=lease(*s);CHECK(lease(*s)==token);
  // Exclusive actual directory lease, without any native backend or process.
  CHECK(!open(folder,p,mount(),nullptr,error));CHECK(!error.empty());
  CHECK(!s->with_ledger(session::Lease{},[](auto&){CHECK(false);},error));CHECK(!error.empty());
  CHECK(snapshot(*s).generation==1&&!snapshot(*s).retired);
  auto second=open(root/"second",p,mount(),&baseline,error);CHECK(bool(second));const auto other=lease(*second);CHECK(!(other==token));
  CHECK(!s->with_ledger(other,[](auto&){CHECK(false);},error));CHECK(snapshot(*s).generation==1);second.reset();
  bool thread_ok=false;
  std::thread foreign([&]{
    std::string local,output;session::Lease copied;session::Snapshot view;bool called=false;
    thread_ok=!s->lease(copied,local)&&!copied.valid()&&!s->working_card_path(output,local)&&output.empty()&&
      !s->snapshot(view,local)&&view.profile.empty()&&!s->with_ledger(token,[&](auto&){called=true;},local)&&
      !s->publish_snapshot(baseline,local)&&!called;
  });foreign.join();CHECK(thread_ok);CHECK(lease(*s)==token&&!snapshot(*s).retired);
  CHECK(s->with_ledger(token,[&](transaction::Ledger& ledger){
    std::string nested;CHECK(!s->with_ledger(token,[](auto&){CHECK(false);},nested));
    CHECK(!s->publish_snapshot(baseline,nested));session::Lease out;CHECK(!s->lease(out,nested));
    CHECK(ledger.valid());
  },error));CHECK(error.empty()&&!snapshot(*s).retired);
  earn(*s,token);CHECK(snapshot(*s).rewards.front().state==State::NativeAwardCompleted);
  auto updated=baseline;updated[0]=84;write(fs::u8path(path(*s)),updated); // Synthetic native-backend stand-in, owned working path only.
  CHECK(s->publish_snapshot(updated,error)&&error.empty());const auto committed=snapshot(*s);
  CHECK(committed.generation==2&&!committed.recovery_needed&&committed.rewards.size()==1&&committed.rewards.front().state==State::Saved);
  CHECK(committed.card_digest==digest(updated)&&committed.card_digest!=committed.origin_card);
  const auto envelope=read(folder/"current.bwseed");CHECK(fs::exists(folder/"previous.bwseed"));s.reset();
  // Reopen verifies the pair before overwriting an uncommitted working copy.
  write(folder/"working.card",bad_card);s=open(folder,p,mount(),nullptr,error);CHECK(bool(s));CHECK(read(fs::u8path(path(*s)))==updated);
  CHECK(snapshot(*s).generation==2&&snapshot(*s).rewards.front().state==State::Saved);
  CHECK(snapshot(*s).card_digest==digest(updated));
  const auto renewed=lease(*s);CHECK(!(renewed==token));
  CHECK(!s->with_ledger(token,[](auto&){CHECK(false);},error));CHECK(!snapshot(*s).retired);
  CHECK(s->with_ledger(renewed,[](auto& ledger){CHECK(ledger.bind(owner())==Status::Accepted);CHECK(ledger.select(selection()).status==Status::Duplicate);},error));s.reset();
  auto bad_mount=mount();bad_mount.quest=1;CHECK(!open(folder,p,bad_mount,nullptr,error));CHECK(read(folder/"current.bwseed")==envelope);
  bad_mount=mount();bad_mount.module_digest=seed::sha256("other module");CHECK(!open(folder,p,bad_mount,nullptr,error));
  bad_mount=mount();bad_mount.origin_card_digest=digest(bad_card);CHECK(!open(folder,p,bad_mount,nullptr,error));
  CHECK(!open(folder,profile(8),mount(),nullptr,error));CHECK(!open(folder,p,mount(),&bad_card,error));CHECK(read(folder/"current.bwseed")==envelope);
  // Scope refuses a declarative full/multiple/unsupported pool before file writes.
  seed::Request request;request.seed=p.seed();request.logic_contract_digest=p.logic_contract_digest();request.start_policy_digest=p.start_policy_digest();request.options=p.options();request.placements=p.placements();
  const auto catalog=imported_catalog();for(const auto& l:catalog.locations)if(l.source_id!=seed::linkug_location_id()){request.placements.push_back({l.source_id,seed::orange_rupee_id()});break;}
  auto wider=seed::ProfileBuilder(catalog).build(request);CHECK(bool(wider));CHECK(!open(root/"multiple",*wider.value,mount(),&baseline,error));CHECK(!fs::exists(root/"multiple"));
  request.placements=p.placements();request.placements.front().item_id=catalog.items.front().source_id;wider=seed::ProfileBuilder(catalog).build(request);CHECK(bool(wider));
  CHECK(!open(root/"unsupported",*wider.value,mount(),&baseline,error));
  // Accepted substitution cancellation cannot resume or acknowledge a pair.
  const auto cancelled_folder=root/"cancelled";s=open(cancelled_folder,p,mount(),&baseline,error);CHECK(bool(s));const auto live=lease(*s);const auto original_pair=read(cancelled_folder/"current.bwseed");
  CHECK(s->with_ledger(live,[](auto& ledger){CHECK(ledger.bind(owner())==Status::Accepted);CHECK(ledger.select(selection()).status==Status::Accepted);},error));
  CHECK(!s->with_ledger(live,[](auto& ledger){ledger.invalidate(transaction::Invalidation::SceneChange);},error));
  CHECK(snapshot(*s).retired&&snapshot(*s).recovery_needed&&snapshot(*s).generation==1);
  CHECK(snapshot(*s).card_digest==digest(baseline));
  session::Lease dead;CHECK(!s->lease(dead,error)&&!dead.valid());CHECK(!s->with_ledger(live,[](auto&){CHECK(false);},error));
  CHECK(!s->publish_snapshot(updated,error));CHECK(read(cancelled_folder/"current.bwseed")==original_pair);s.reset();
  s=open(cancelled_folder,p,mount(),nullptr,error);CHECK(bool(s)&&!snapshot(*s).recovery_needed&&snapshot(*s).rewards.empty());
  CHECK(snapshot(*s).generation==1&&snapshot(*s).card_digest==digest(baseline));s.reset();
  // Callback exceptions retire access; no exception escapes or implicit reset.
  s=open(root/"exception",p,mount(),&baseline,error);CHECK(bool(s));const auto exception_lease=lease(*s);
  CHECK(!s->with_ledger(exception_lease,[](auto&){throw std::runtime_error("synthetic failure");},error));CHECK(snapshot(*s).retired);
  CHECK(!s->lease(dead,error));s.reset();
  s=open(root/"invalid-publish",p,mount(),&baseline,error);CHECK(bool(s));CHECK(!s->publish_snapshot(empty,error));CHECK(snapshot(*s).retired&&snapshot(*s).generation==1);
  CHECK(snapshot(*s).card_digest==digest(baseline));s.reset();
#ifdef _WIN32
  // Genuine Windows sharing exclusion blocks atomic current-envelope replace.
  // No injected Store fault, guest state, CARD backend or external device.
  const auto blocked_folder=root/"blocked";s=open(blocked_folder,p,mount(),&baseline,error);CHECK(bool(s));const auto blocked_lease=lease(*s);
  const auto old=read(blocked_folder/"current.bwseed");earn(*s,blocked_lease);write(fs::u8path(path(*s)),updated);
  HANDLE lock=CreateFileW((blocked_folder/"current.bwseed").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);CHECK(lock!=INVALID_HANDLE_VALUE);
  CHECK(!s->publish_snapshot(updated,error)&&!error.empty());CHECK(snapshot(*s).retired&&snapshot(*s).generation==1&&snapshot(*s).recovery_needed);
  CHECK(snapshot(*s).card_digest==digest(baseline));
  CHECK(snapshot(*s).rewards.front().state==State::Cancelled);CHECK(!s->with_ledger(blocked_lease,[](auto&){CHECK(false);},error));
  CHECK(read(blocked_folder/"current.bwseed")==old);CHECK(CloseHandle(lock));s.reset();
  s=open(blocked_folder,p,mount(),nullptr,error);CHECK(bool(s));CHECK(read(fs::u8path(path(*s)))==baseline);
  CHECK(snapshot(*s).generation==1&&snapshot(*s).rewards.empty()&&!snapshot(*s).recovery_needed);
  CHECK(snapshot(*s).card_digest==digest(baseline));s.reset();
#else
  std::cout<<"Windows atomic sharing-exclusion publication test unavailable on this platform\n";
#endif
#ifdef BLUEWAKE_RANDOMIZER_STORE_TEST
  // Private extra variant only: unchanged Store test hook fails AFTER the
  // complete new pair is on disk, before any receipt is returned. These are
  // synthetic bytes/awards; the ordinary public target has no fault define.
  const auto unacked_folder=root/"unacknowledged";
  s=open(unacked_folder,p,mount(),&baseline,error);CHECK(bool(s));const auto unacked_lease=lease(*s);
  const auto unacked_old=read(unacked_folder/"current.bwseed");earn(*s,unacked_lease);
  write(fs::u8path(path(*s)),updated);storage::fail_next_commit(storage::Fault::AfterPublish);
  CHECK(!s->publish_snapshot(updated,error)&&!error.empty());const auto failed=snapshot(*s);
  CHECK(failed.retired&&failed.recovery_needed&&failed.generation==1&&failed.card_digest==digest(baseline));
  CHECK(failed.rewards.front().state==State::Cancelled);CHECK(!s->with_ledger(unacked_lease,[](auto&){CHECK(false);},error));
  CHECK(read(unacked_folder/"previous.bwseed")==unacked_old);CHECK(read(unacked_folder/"current.bwseed")!=unacked_old);s.reset();
  s=open(unacked_folder,p,mount(),nullptr,error);CHECK(bool(s));const auto recovered=snapshot(*s);
  CHECK(recovered.generation==2&&recovered.card_digest==digest(updated)&&!recovered.recovery_needed);
  CHECK(recovered.rewards.size()==1&&recovered.rewards.front().state==State::Saved);
  CHECK(read(fs::u8path(path(*s)))==updated);s.reset();
#endif
  std::cout<<"randomizer session checks "<<checks<<" PASS (synthetic rewards/CARD, actual leases/store)\n";
}
