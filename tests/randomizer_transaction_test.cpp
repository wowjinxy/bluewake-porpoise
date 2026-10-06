// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic copied boundary records and actual durable file store, no native game.
#include "randomizer_transaction.h"
#include "randomizer_store.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
using namespace bluewake::randomizer;
using namespace bluewake::randomizer::transaction;
namespace fs=std::filesystem;
static std::size_t checks;
static void check(bool b){++checks;if(!b){std::cerr<<"transaction check "<<checks<<" failed\n";std::abort();}}
static seed::Profile profile(bool orange=false){
  seed::Request r;r.logic_contract_digest=seed::sha256("fixture logic metadata");r.start_policy_digest=seed::sha256("fixture start metadata");
  r.options={{"logic_obscurity",std::string("None")},{"logic_precision",std::string("None")},{"required_bosses",false},{"skip_rematch_bosses",false},{"sword_mode",std::string("No Starting Sword")}};
  r.placements={{seed::linkug_location_id(),orange?seed::orange_rupee_id():seed::basic_picto_id()}};
  auto p=seed::ProfileBuilder(imported_catalog()).build(r);check(bool(p));return *p.value;
}
static storage::Bytes card(){return storage::Bytes(64,83);}
static Mount mount(){return {seed::sha256(std::string(64,83)),seed::sha256("synthetic module"),0};}
static Owner owner(){return {mount().module_digest,1,2,3,4,5,6,7,0x80AC727C,33,"LinkUG",0,11,0};}
static Actor chest(){return {0x80ACC384,35,294,113,8,9,2,2,-1};}
static Actor item(){return {0x80AB7000,36,259,131,10,11,3,2,0};}
static Selection selection(){return {owner(),chest(),{},50,0x800261E8,0xC1DF2780,chest().address+0x1F8,0xFF000280,0x06FF,6,0x81700000};}
static Creation creation(){return {owner(),chest(),50,0xC1DF2780,item().pid,0x81700000};}
static AwardEntry entry(){return {owner(),item(),{},70,0xC00C2DFC,0xC0770A08,35,35,0x81700100};}
static AwardReturn returned(){return {owner(),item(),{35,1,0},70,0xC0770A08,0x81700100};}
static Ledger selected(){Ledger l(profile(),mount());check(l.valid());check(l.bind(owner())==Status::Accepted);const auto d=l.select(selection());check(d.status==Status::Accepted&&d.reward==35);return l;}
static Ledger created(){auto l=selected();check(l.created(creation())==Status::Accepted);check(l.bind_item(owner(),item())==Status::Accepted);return l;}
static Ledger armed(){auto l=created();check(l.award_enter(entry())==Status::Accepted);return l;}
static Ledger awarded(){auto l=armed();check(l.award_return(returned())==Status::Accepted);check(l.state(seed::linkug_location_id())==State::NativeAwardCompleted);return l;}
static storage::Bytes bytes(const std::string& s){return {s.begin(),s.end()};}
static storage::CommitReceipt commit(storage::Store& store,Ledger& ledger,storage::Bytes payload){
  const auto proposal=ledger.prepare_commit(seed::sha256(std::string(payload.begin(),payload.end())));check(bool(proposal));
  storage::CommitReceipt receipt;std::string error;
  check(store.commit(proposal->previous_generation,payload,bytes(proposal->ledger_bytes),receipt,error));
  check(error.empty());check(ledger.confirm_saved(receipt)==Status::Accepted);return receipt;
}
int main(int argc,char** argv){
  check(argc==2);const auto dir=fs::u8path(argv[1])/std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  check(fs::create_directories(dir));
  const auto p=profile();
  auto l=selected();check(l.state(seed::linkug_location_id())==State::Selected);
  check(!l.prepare_commit(mount().origin_card_digest));check(l.select(selection()).status==Status::Duplicate);
  check(l.created(creation())==Status::Accepted);check(l.state(seed::linkug_location_id())==State::ItemCreated);
  check(l.created(creation())==Status::Duplicate);check(l.bind_item(owner(),item())==Status::Accepted);check(l.bind_item(owner(),item())==Status::Duplicate);
  check(l.award_enter(entry())==Status::Accepted);check(l.award_enter(entry())==Status::Duplicate);check(!l.prepare_commit(mount().origin_card_digest));
  check(l.award_return(returned())==Status::Accepted);check(l.award_return(returned())==Status::WrongState);check(l.select(selection()).status==Status::Duplicate);
  check(l.records().size()==1&&l.records().front().award_sequence==1);
  // Stale copied ownership cancels a pending native invocation; no revival.
  const std::vector<std::function<void(Owner&)>> stale={
    [](auto& x){++x.cpu;},[](auto& x){++x.ram;},[](auto& x){++x.code_load;},[](auto& x){++x.alias;},
    [](auto& x){++x.native_epoch;},[](auto& x){++x.scene;},[](auto& x){++x.native_card_load;},
    [](auto& x){x.player+=4;},[](auto& x){++x.player_pid;},[](auto& x){x.stage="sea";},
    [](auto& x){x.room=1;},[](auto& x){x.save_table=0;},[](auto& x){x.quest=1;},[](auto& x){x.module_digest=seed::sha256("other");}};
  for(const auto& change:stale){auto a=armed();auto r=returned();change(r.owner);check(a.award_return(r)==Status::StaleOwner);check(a.state(seed::linkug_location_id())==State::Cancelled);check(a.award_return(returned())==Status::WrongState);}
  const std::vector<std::function<void(Selection&)>> invalid={
    [](auto& x){x.chest.init_state=1;},[](auto& x){x.chest.init_state=3;},[](auto& x){x.chest.create_result=1;},[](auto& x){x.chest.registration=0;},[](auto& x){x.chest.materialization=0;},
    [](auto& x){x.chest.process=259;},[](auto& x){x.chest.module=131;},[](auto& x){x.chest.room=2;},[](auto& x){x.chest.pid=UINT32_MAX-1;},
    [](auto& x){x.chest.pid=UINT32_MAX;},[](auto& x){x.chest.pid=0;},[](auto& x){x.chest.address=1;},[](auto& x){x.parameters^=128;},
    [](auto& x){x.home_angle_z=0x23FF;},[](auto& x){x.original_item=35;},[](auto& x){++x.position_argument;},[](auto& x){x.entry=0x800261EC;},
    [](auto& x){x.lr=0xC1DF2784;},[](auto& x){x.call=0;},[](auto& x){x.stack=0;},[](auto& x){x.stack=0x81800000;}};
  for(const auto& change:invalid){Ledger a(p,mount());check(a.bind(owner())==Status::Accepted);auto s=selection();change(s);check(a.select(s).status==Status::InvalidEvidence);check(a.records().empty());}
  auto a=selected();auto c=creation();c.item_pid=UINT32_MAX;check(a.created(c)==Status::InvalidEvidence);check(a.state(seed::linkug_location_id())==State::Cancelled);check(a.created(creation())==Status::WrongState);
  a=created();auto it=item();++it.materialization;check(a.bind_item(owner(),it)==Status::StaleOwner);check(a.award_enter(entry())==Status::WrongState);
  const std::vector<std::function<void(Actor&)>> invalid_item={
    [](auto& x){x.init_state=0;},[](auto& x){x.init_state=1;},[](auto& x){x.init_state=2;},[](auto& x){x.init_state=4;},
    [](auto& x){x.create_result=0;},[](auto& x){x.create_result=1;},[](auto& x){x.create_result=3;},
    [](auto& x){x.registration=0;},[](auto& x){x.materialization=0;},[](auto& x){x.pid=0;},[](auto& x){x.pid=UINT32_MAX-1;},
    [](auto& x){x.pid=UINT32_MAX;},[](auto& x){x.process=294;},[](auto& x){x.module=113;}};
  for(const auto& change:invalid_item) {
    a=selected();check(a.created(creation())==Status::Accepted);it=item();change(it);
    check(a.bind_item(owner(),it)==Status::InvalidEvidence);check(a.state(seed::linkug_location_id())==State::ItemCreated);
    check(a.award_enter(entry())==Status::WrongState);check(a.award_return(returned())==Status::WrongState);
  }
  a=created();auto executable_entry=entry();executable_entry.item.init_state=2;
  check(a.award_enter(executable_entry)==Status::StaleOwner);check(a.state(seed::linkug_location_id())==State::Cancelled);
  a=armed();auto executable_return=returned();executable_return.item.init_state=2;
  check(a.award_return(executable_return)==Status::StaleOwner);check(a.state(seed::linkug_location_id())==State::Cancelled);
  a=selected();c=creation();c.stack+=4;check(a.created(c)==Status::StaleOwner);check(a.state(seed::linkug_location_id())==State::Cancelled);
  a=armed();auto wrong_stack=returned();wrong_stack.stack+=4;check(a.award_return(wrong_stack)==Status::StaleOwner);check(a.award_return(returned())==Status::WrongState);
  for(auto stack:{std::uint32_t(0),std::uint32_t(0x81700004)}) {
    a=selected();auto replay=selection();replay.stack=stack;
    check(a.select(replay).status==Status::StaleOwner);check(a.state(seed::linkug_location_id())==State::Cancelled);
  }
  for(auto stack:{std::uint32_t(0),std::uint32_t(0x81700104)}) {
    a=armed();auto replay=entry();replay.stack=stack;
    check(a.award_enter(replay)==Status::StaleOwner);check(a.award_return(returned())==Status::WrongState);
  }
  a=armed();auto wrong_quest=owner();wrong_quest.quest=1;
  check(a.bind(wrong_quest)==Status::Unavailable);check(a.state(seed::linkug_location_id())==State::Cancelled);
  check(a.award_return(returned())==Status::WrongState);
  a=created();auto e=entry();e.actor_item=6;check(a.award_enter(e)==Status::InvalidEvidence);check(a.award_return(returned())==Status::WrongState);
  a=created();e=entry();e.item.pid=37;check(a.award_enter(e)==Status::StaleOwner);check(a.state(seed::linkug_location_id())==State::Cancelled);
  a=armed();auto r=returned();r.inventory_after.camera_obtained=0;check(a.award_return(r)==Status::PostconditionFailed);check(a.award_return(returned())==Status::WrongState);
  a=armed();r=returned();r.inventory_after.rupee_queue=100;check(a.award_return(r)==Status::PostconditionFailed);
  // Basic Picto must not downgrade an already Deluxe camera; unrelated bits survive.
  Ledger basic(p,mount());check(basic.bind(owner())==Status::Accepted);auto s=selection();s.inventory_before={38,2,0};check(basic.select(s).status==Status::PostconditionFailed);
  s=selection();s.inventory_before={35,0,0};check(basic.select(s).status==Status::PostconditionFailed);
  s=selection();s.inventory_before={255,4,0};check(basic.select(s).status==Status::Accepted);check(basic.created(creation())==Status::Accepted);check(basic.bind_item(owner(),item())==Status::Accepted);
  e=entry();e.inventory_before={255,4,12};check(basic.award_enter(e)==Status::Accepted);r=returned();r.inventory_after={35,5,12};check(basic.award_return(r)==Status::Accepted);
  // Native Orange uses +100 to its signed queue, not an immediate wallet delta.
  Ledger orange(profile(true),mount());check(orange.bind(owner())==Status::Accepted);s=selection();check(orange.select(s).reward==6);check(orange.created(creation())==Status::Accepted);check(orange.bind_item(owner(),item())==Status::Accepted);
  e=entry();e.argument=e.actor_item=6;e.inventory_before.rupee_queue=17;check(orange.award_enter(e)==Status::Accepted);r=returned();r.inventory_after={255,0,117};check(orange.award_return(r)==Status::Accepted);
  for(auto why:{Invalidation::SceneChange,Invalidation::ActorRemoved,Invalidation::AliasChange,Invalidation::NativeLoad,Invalidation::MachineLoad,Invalidation::CpuReplaced,Invalidation::RamReplaced,Invalidation::ModuleReload,Invalidation::Shutdown}){
    auto x=armed();x.invalidate(why);check(x.state(seed::linkug_location_id())==State::Cancelled);check(x.award_return(returned())==Status::WrongState);
    check(x.bind(owner())==Status::Accepted);check(x.select(selection()).status==Status::WrongState);
    check(!x.prepare_commit(mount().origin_card_digest));
  }
  // A completed historical award survives ordinary alias/scene changes. It
  // cannot retain live item/callback reads or authorize a commit while unbound.
  a=awarded();a.invalidate(Invalidation::AliasChange);check(a.state(seed::linkug_location_id())==State::NativeAwardCompleted);
  auto next=owner();++next.alias;check(a.bind(next)==Status::Accepted);a.invalidate(Invalidation::SceneChange);next.scene++;next.stage="sea";next.room=44;next.save_table=0;
  check(a.bind(next)==Status::Accepted);check(a.state(seed::linkug_location_id())==State::NativeAwardCompleted);
  for(auto why:{Invalidation::NativeLoad,Invalidation::MachineLoad,Invalidation::CpuReplaced,Invalidation::RamReplaced,Invalidation::ModuleReload,Invalidation::Shutdown}){
    auto x=awarded();x.invalidate(why);check(x.state(seed::linkug_location_id())==State::Cancelled);
    check(x.bind(owner())==Status::Accepted);check(x.select(selection()).status==Status::WrongState);
    check(!x.prepare_commit(mount().origin_card_digest));
  }
  a=awarded();a.invalidate(Invalidation::SceneChange);next=owner();next.cpu++;check(a.bind(next)==Status::Accepted);check(a.state(seed::linkug_location_id())==State::Cancelled);
  // Real Store-issued receipts are required: a number or default receipt cannot save.
  storage::Store store;std::string error;check(store.open((dir/"profile").u8string().c_str(),p.identity(),error));
  Ledger durable(p,mount());const auto baseline=commit(store,durable,card());check(baseline.number()==1);check(durable.generation()==1);
  Ledger recovered(p,mount());storage::Generation old_generation;storage::CommitReceipt old_receipt;bool found=false;
  check(store.load(old_generation,found,error,&old_receipt)&&found);
  check(recovered.restore_saved(old_receipt,std::string(old_generation.ledger.begin(),old_generation.ledger.end()))==Status::Accepted);
  // This copied bind is synthetic. Production must genuinely reload the same
  // verified prior CARD before supplying the new native ownership record.
  auto reloaded_owner=owner();++reloaded_owner.native_epoch;++reloaded_owner.native_card_load;
  check(recovered.bind(reloaded_owner)==Status::Accepted);auto fresh_selection=selection();fresh_selection.owner=reloaded_owner;
  check(recovered.select(fresh_selection).status==Status::Accepted);
  check(durable.bind(owner())==Status::Accepted);check(durable.select(selection()).status==Status::Accepted);check(durable.created(creation())==Status::Accepted);check(durable.bind_item(owner(),item())==Status::Accepted);check(durable.award_enter(entry())==Status::Accepted);check(durable.award_return(returned())==Status::Accepted);
  durable.invalidate(Invalidation::AliasChange);next=owner();next.alias++;next.scene++;next.stage="sea";next.room=44;next.save_table=0;check(durable.bind(next)==Status::Accepted);
  auto updated=card();updated[0]=84;const auto candidate=durable.prepare_commit(seed::sha256(std::string(updated.begin(),updated.end())));check(bool(candidate));
  check(durable.confirm_saved(storage::CommitReceipt{})==Status::InvalidCommit);check(durable.confirm_saved(baseline)==Status::InvalidCommit);check(durable.state(seed::linkug_location_id())==State::NativeAwardCompleted);
  check(durable.bind(next)==Status::CommitPending);check(!durable.prepare_commit(candidate->card_digest));
  storage::CommitReceipt saved;check(store.commit(candidate->previous_generation,updated,bytes(candidate->ledger_bytes),saved,error));check(durable.confirm_saved(saved)==Status::Accepted);
  check(saved.number()==2);check(durable.state(seed::linkug_location_id())==State::Saved);check(durable.confirm_saved(saved)==Status::WrongState);
  for(auto quest:{std::uint8_t(1),std::uint8_t(2),std::uint8_t(3),std::uint8_t(255)}) {
    auto wrong_owner=owner();wrong_owner.quest=quest;
    check(durable.bind(wrong_owner)==Status::Unavailable);
    auto wrong_selection=selection();wrong_selection.owner=wrong_owner;
    check(durable.select(wrong_selection).status==Status::StaleOwner);
    check(durable.state(seed::linkug_location_id())==State::Saved && durable.generation()==2);
    Ledger fresh(p,mount());check(fresh.bind(wrong_owner)==Status::Unavailable);
    check(fresh.select(wrong_selection).status==Status::StaleOwner);check(fresh.records().empty());
  }
  for(auto why:{Invalidation::AliasChange,Invalidation::SceneChange,Invalidation::MachineLoad,Invalidation::Shutdown}){durable.invalidate(why);check(durable.state(seed::linkug_location_id())==State::Saved);check(durable.records().front().award_sequence==1);}
  Ledger reload(p,mount());check(reload.restore_saved(saved,candidate->ledger_bytes)==Status::Accepted);check(reload.generation()==2);check(reload.state(seed::linkug_location_id())==State::Saved);
  check(reload.bind(owner())==Status::Accepted);check(reload.select(selection()).status==Status::Duplicate);check(reload.records().size()==1);
  Ledger wrong(p,mount());check(wrong.restore_saved(saved,candidate->ledger_bytes+"x")==Status::InvalidCommit);check(wrong.records().empty());
  auto different_mount=mount();different_mount.module_digest=seed::sha256("other module");Ledger other_module(p,different_mount);
  check(other_module.restore_saved(saved,candidate->ledger_bytes)==Status::InvalidCommit);check(other_module.records().empty());
  different_mount=mount();different_mount.quest=1;Ledger other_quest(p,different_mount);
  check(other_quest.restore_saved(saved,candidate->ledger_bytes)==Status::InvalidCommit);check(other_quest.records().empty());
  auto altered=candidate->ledger_bytes;altered.back()^=1;check(wrong.restore_saved(saved,altered)==Status::InvalidCommit);
  check(wrong.restore_saved(storage::CommitReceipt{},candidate->ledger_bytes)==Status::InvalidCommit);
  // Store can mint a receipt for arbitrary opaque bytes; core must still reject
  // a malformed/noncanonical ledger rather than trusting the storage checksum.
  storage::Store malformed_store;check(malformed_store.open((dir/"malformed").u8string().c_str(),p.identity(),error));storage::CommitReceipt malformed;
  check(malformed_store.commit(0,card(),bytes(candidate->ledger_bytes+"x"),malformed,error));check(wrong.restore_saved(malformed,candidate->ledger_bytes+"x")==Status::InvalidCommit);
  Ledger abandoned(p,mount());const auto initial=abandoned.prepare_commit(mount().origin_card_digest);check(bool(initial));abandoned.abandon_commit();check(abandoned.confirm_saved(baseline)==Status::WrongState);check(abandoned.generation()==0);
  std::cout<<"randomizer transaction checks "<<checks<<" PASS (synthetic boundaries, real paired store)\n";
}
