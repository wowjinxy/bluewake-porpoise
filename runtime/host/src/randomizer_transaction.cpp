// SPDX-License-Identifier: GPL-3.0-or-later
#include "randomizer_transaction.h"
#include "randomizer_store.h"
#include <algorithm>
#include <limits>

namespace bluewake::randomizer::transaction {
namespace {
constexpr std::size_t MaxLedgerBytes = 256 * 1024;
bool pid_ok(std::uint32_t p) { return p != 0 && p < UINT32_MAX - 1; }
bool address_ok(std::uint32_t p) { return p >= 0x80000000u && p < 0x81800000u && (p & 3u) == 0; }
bool owner_ok(const Owner& o) {
  return seed::is_digest(o.module_digest) && o.cpu && o.ram && o.code_load && o.alias && o.native_epoch &&
    o.scene && o.native_card_load && address_ok(o.player) && pid_ok(o.player_pid) &&
    !o.stage.empty() && o.stage.size() <= 8 && o.stage.find('\0') == std::string::npos && o.room >= 0 && o.save_table < 16 && o.quest < 3;
}
bool session_same(const Owner& a, const Owner& b) {
  return a.module_digest == b.module_digest && a.cpu == b.cpu && a.ram == b.ram && a.code_load == b.code_load &&
    a.native_epoch == b.native_epoch && a.native_card_load == b.native_card_load && a.quest == b.quest;
}
bool owner_same(const Owner& a, const Owner& b) {
  return session_same(a,b) && a.alias == b.alias && a.scene == b.scene && a.player == b.player && a.player_pid == b.player_pid &&
    a.stage == b.stage && a.room == b.room && a.save_table == b.save_table;
}
bool actor_base_ok(const Actor& a) {
  return address_ok(a.address) && pid_ok(a.pid) && a.materialization && a.registration && a.create_result == 2;
}
bool chest_ok(const Actor& a) {
  return actor_base_ok(a) && a.init_state == 2 && a.process == 294 && a.module == 113;
}
bool deleting_item_ok(const Actor& a) {
  // Native fpcDt_ToDeleteQ sets3 before Demo_Item's subDelete awards it.
  // The ordinary actor tag remains registered until subDelete returns.
  return actor_base_ok(a) && a.init_state == 3 && a.process == 259 && a.module == 131;
}
bool actor_same(const Actor& a, const Actor& b) {
  return a.address == b.address && a.pid == b.pid && a.process == b.process && a.module == b.module &&
    a.materialization == b.materialization && a.registration == b.registration && a.init_state == b.init_state &&
    a.create_result == b.create_result && a.room == b.room;
}
void put32(std::string& out, std::uint32_t n) { for (int i=3;i>=0;--i) out.push_back(char(n>>(i*8))); }
void put64(std::string& out, std::uint64_t n) { for (int i=7;i>=0;--i) out.push_back(char(n>>(i*8))); }
void putstr(std::string& out, const std::string& s) { put32(out,std::uint32_t(s.size())); out+=s; }
std::string encode(const seed::Profile& p, const Mount& mount, std::uint64_t generation,
                   std::uint64_t token, const std::vector<RewardRecord>& records) {
  std::string out; putstr(out,"BlueWake.WindWaker.RewardLedger"); put32(out,1);
  putstr(out,p.identity()); putstr(out,mount.origin_card_digest); putstr(out,mount.module_digest); out.push_back(char(mount.quest));
  put64(out,generation); put64(out,token);
  std::vector<const RewardRecord*> order;
  for(const auto& r:records) if(r.state==State::Saved || r.state==State::NativeAwardCompleted) order.push_back(&r);
  std::sort(order.begin(),order.end(),[](const auto* a,const auto* b){return a->location_id<b->location_id;});
  put32(out,std::uint32_t(order.size()));
  for(const auto* r:order){putstr(out,r->location_id);putstr(out,r->item_id);out.push_back(char(r->native_item));put64(out,r->award_sequence);}
  return out.size()<=MaxLedgerBytes?out:std::string{};
}
struct Reader {
  const std::string& s; std::size_t at=0; bool good=true;
  std::uint8_t byte(){if(at==s.size()){good=false;return 0;}return std::uint8_t(s[at++]);}
  std::uint32_t u32(){std::uint32_t n=0;for(int i=0;i<4;++i)n=(n<<8)|byte();return n;}
  std::uint64_t u64(){std::uint64_t n=0;for(int i=0;i<8;++i)n=(n<<8)|byte();return n;}
  std::string str(std::size_t max){const auto n=u32();if(!good||n>max||n>s.size()-at){good=false;return{};}auto v=s.substr(at,n);at+=n;return v;}
};
}
Ledger::Ledger(seed::Profile p, Mount m):profile_(std::move(p)),mount_(std::move(m)) {
  valid_=seed::is_digest(profile_.identity()) && profile_.audited_linkug_binding() &&
    seed::is_digest(mount_.origin_card_digest) && seed::is_digest(mount_.module_digest) && mount_.quest<3;
}
bool Ledger::owner_matches(const Owner& o) const { return owner_ && owner_ok(o) && owner_same(*owner_,o); }
Status Ledger::bind(const Owner& o) {
  if(!valid_ || !owner_ok(o) || o.module_digest!=mount_.module_digest || o.quest!=mount_.quest) {
    cancel_active();commit_.reset();owner_.reset();
    // An explicitly attempted bind cannot keep an old borrowed owner live.
    // Historical Saved receipts are preserved; unsaved rows are revoked.
    for(auto& r:records_) if(r.state!=State::Saved) r.state=State::Cancelled;
    return Status::Unavailable;
  }
  if(commit_) return Status::CommitPending;
  if((owner_ && !owner_same(*owner_,o)) || (session_ && !session_same(*session_,o))) {
    const bool same_session=session_ && session_same(*session_,o);
    cancel_active();
    if(!same_session) {
      for(auto& r:records_) if(r.state!=State::Saved) r.state=State::Cancelled;
      last_select_call_=last_award_call_=0;
    }
  }
  owner_=o;session_=o; return Status::Accepted;
}
State Ledger::state(const std::string& id) const {
  const auto r=std::find_if(records_.begin(),records_.end(),[&](const auto& v){return v.location_id==id;});
  return r==records_.end()?State::Unselected:r->state;
}
Decision Ledger::select(const Selection& e) {
  if(!valid_ || !profile_.reward_at(seed::linkug_location_id())) return {Status::Unavailable,{}};
  if(commit_) return {Status::CommitPending,{}};
  if(!owner_matches(e.owner)) { cancel_active(); return {Status::StaleOwner,{}}; }
  if(active_ && active_->selected.call==e.call && active_->selected.stack!=e.stack) {
    cancel_active();return {Status::StaleOwner,{}};
  }
  if(e.owner.stage!="LinkUG" || e.owner.room!=0 || e.owner.save_table!=11 || !chest_ok(e.chest) ||
     (e.chest.room!=-1 && e.chest.room!=0) ||
     (e.entry!=0x800261E8u && e.entry!=0xC00261E8u) || e.lr!=0xC1DF2780u ||
     e.position_argument!=e.chest.address+0x1F8u || e.parameters!=0xFF000280u ||
     e.home_angle_z!=0x06FFu || e.original_item!=0x06 || !e.call || !address_ok(e.stack)) return {Status::InvalidEvidence,{}};
  const auto reward=seed::native_reward(*profile_.reward_at(seed::linkug_location_id()));
  if(!reward) return {Status::UnsupportedReward,{}};
  if(active_) {
    if(active_->selected.call==e.call && actor_same(active_->selected.chest,e.chest)) return {Status::Duplicate,{}};
    return {Status::WrongState,{}};
  }
  const auto old=state(seed::linkug_location_id());
  if(old==State::Saved || old==State::NativeAwardCompleted) return {Status::Duplicate,{}};
  if(old==State::Cancelled) return {Status::WrongState,{}};
  if(e.call<=last_select_call_) return {Status::InvalidEvidence,{}};
  if(reward->kind==seed::NativeRewardKind::BasicPictoBox) {
    const auto& i=e.inventory_before;
    if((i.camera_obtained&2) || ((i.camera_obtained&1)?i.camera_slot!=0x23:i.camera_slot!=0xFF)) return {Status::PostconditionFailed,{}};
  } else if(e.inventory_before.rupee_queue>std::numeric_limits<std::int32_t>::max()-100) return {Status::PostconditionFailed,{}};
  last_select_call_=e.call; active_=Active{e,0,{},0,0,{}};
  auto r=std::find_if(records_.begin(),records_.end(),[](const auto& v){return v.location_id==seed::linkug_location_id();});
  RewardRecord row{seed::linkug_location_id(),*profile_.reward_at(seed::linkug_location_id()),reward->item,0,State::Selected};
  if(r==records_.end()) records_.push_back(std::move(row)); else *r=std::move(row);
  return {Status::Accepted,reward->item};
}
Status Ledger::created(const Creation& e) {
  if(!active_) return Status::WrongState;
  if(!owner_matches(e.owner) || !owner_same(e.owner,active_->selected.owner) || !actor_same(e.chest,active_->selected.chest)) { cancel_active(); return Status::StaleOwner; }
  if(e.call!=active_->selected.call || e.return_pc!=0xC1DF2780u) return Status::InvalidEvidence;
  if(e.stack!=active_->selected.stack) {cancel_active();return Status::StaleOwner;}
  if(!pid_ok(e.item_pid)){cancel_active();return Status::InvalidEvidence;}
  if(active_->item_pid) return active_->item_pid==e.item_pid?Status::Duplicate:Status::InvalidEvidence;
  active_->item_pid=e.item_pid;
  for(auto& r:records_) if(r.location_id==seed::linkug_location_id()) r.state=State::ItemCreated;
  return Status::Accepted;
}
Status Ledger::bind_item(const Owner& o,const Actor& item) {
  if(!active_ || !active_->item_pid) return Status::WrongState;
  if(!owner_matches(o) || !owner_same(o,active_->selected.owner)) { cancel_active(); return Status::StaleOwner; }
  if(!deleting_item_ok(item) || item.pid!=active_->item_pid) return Status::InvalidEvidence;
  if(active_->item) { if(actor_same(*active_->item,item)) return Status::Duplicate; cancel_active(); return Status::StaleOwner; }
  active_->item=item;return Status::Accepted;
}
Status Ledger::award_enter(const AwardEntry& e) {
  if(!active_ || !active_->item) return Status::WrongState;
  if(!owner_matches(e.owner) || !owner_same(e.owner,active_->selected.owner) || !actor_same(e.item,*active_->item)) { cancel_active(); return Status::StaleOwner; }
  if(active_->award_call && active_->award_call==e.call && active_->award_stack!=e.stack) {
    cancel_active();return Status::StaleOwner;
  }
  const auto reward=seed::native_reward(*profile_.reward_at(seed::linkug_location_id()));
  if(!reward || (e.entry!=0x800C2DFCu && e.entry!=0xC00C2DFCu) || e.lr!=0xC0770A08u ||
     e.argument!=reward->item || e.actor_item!=reward->item || !e.call || !address_ok(e.stack)) return Status::InvalidEvidence;
  if(active_->award_call) return active_->award_call==e.call?Status::Duplicate:Status::InvalidEvidence;
  if(e.call<=last_award_call_) return Status::InvalidEvidence;
  const auto& i=e.inventory_before;
  if(reward->kind==seed::NativeRewardKind::BasicPictoBox ?
     (i.camera_obtained&2) || ((i.camera_obtained&1)?i.camera_slot!=0x23:i.camera_slot!=0xFF) :
     i.rupee_queue>std::numeric_limits<std::int32_t>::max()-100) { cancel_active(); return Status::PostconditionFailed; }
  active_->award_call=e.call;active_->award_stack=e.stack;active_->award_inventory=i;last_award_call_=e.call;return Status::Accepted;
}
Status Ledger::award_return(const AwardReturn& e) {
  if(!active_ || !active_->item || !active_->award_call) return Status::WrongState;
  if(!owner_matches(e.owner) || !owner_same(e.owner,active_->selected.owner) || !actor_same(e.item,*active_->item)) { cancel_active(); return Status::StaleOwner; }
  if(e.return_pc!=0xC0770A08u || e.call!=active_->award_call) return Status::InvalidEvidence;
  if(e.stack!=active_->award_stack) {cancel_active();return Status::StaleOwner;}
  const auto reward=seed::native_reward(*profile_.reward_at(seed::linkug_location_id()));
  const auto& before=active_->award_inventory;const auto& after=e.inventory_after;
  // Return is consumed even when its observed native postcondition fails.
  active_->award_call=0;
  const bool correct=reward && (reward->kind==seed::NativeRewardKind::BasicPictoBox?
    after.camera_slot==0x23 && after.camera_obtained==std::uint8_t(before.camera_obtained|1) && after.rupee_queue==before.rupee_queue:
    after.rupee_queue==before.rupee_queue+100 && after.camera_slot==before.camera_slot && after.camera_obtained==before.camera_obtained);
  if(!correct){cancel_active();return Status::PostconditionFailed;}
  if(award_sequence_==UINT64_MAX){cancel_active();return Status::InvalidEvidence;}
  for(auto& r:records_) if(r.location_id==seed::linkug_location_id()){r.state=State::NativeAwardCompleted;r.award_sequence=++award_sequence_;}
  active_.reset();return Status::Accepted;
}
void Ledger::cancel_active() {
  if(active_) for(auto& r:records_) if(r.location_id==seed::linkug_location_id() && r.state!=State::Saved && r.state!=State::NativeAwardCompleted) r.state=State::Cancelled;
  active_.reset();
}
void Ledger::invalidate(Invalidation why) {
  cancel_active();commit_.reset();
  if(why!=Invalidation::SceneChange && why!=Invalidation::ActorRemoved && why!=Invalidation::AliasChange) {
    for(auto& r:records_) if(r.state!=State::Saved) r.state=State::Cancelled;
    last_select_call_=last_award_call_=0;
    session_.reset();
  }
  owner_.reset();
}
std::optional<CommitCandidate> Ledger::prepare_commit(const std::string& digest) {
  if(!valid_ || !seed::is_digest(digest) || active_ || commit_ || generation_==UINT64_MAX || commit_sequence_==UINT64_MAX) return {};
  // Native substitution may already have opened the chest or created/granted
  // an item. Cancellation proves no rollback; refuse to save an omitted row.
  if(std::any_of(records_.begin(),records_.end(),[](const auto& r){return r.state==State::Cancelled;})) return {};
  if(generation_==0 ? digest!=mount_.origin_card_digest : !owner_) return {};
  CommitCandidate c; c.profile=profile_.identity();c.origin_card=mount_.origin_card_digest;c.card_digest=digest;
  c.previous_generation=generation_;c.generation=generation_+1;c.commit_token=++commit_sequence_;
  c.ledger_bytes=encode(profile_,mount_,c.generation,c.commit_token,records_);
  if(c.ledger_bytes.empty()) return {};
  c.ledger_digest=seed::sha256(c.ledger_bytes);commit_=c;return c;
}
Status Ledger::confirm_saved(const storage::CommitReceipt& receipt) {
  if(!commit_) return Status::WrongState;
  const auto& c=*commit_;
  if(receipt.profile()!=c.profile || receipt.origin_card()!=c.origin_card || receipt.number()!=c.generation ||
     receipt.card_digest()!=c.card_digest || receipt.ledger_digest()!=c.ledger_digest || !seed::is_digest(receipt.record_digest())) return Status::InvalidCommit;
  for(auto& r:records_) if(r.state==State::NativeAwardCompleted) r.state=State::Saved;
  generation_=c.generation;commit_.reset();return Status::Accepted;
}
Status Ledger::restore_saved(const storage::CommitReceipt& receipt,const std::string& bytes) {
  if(!valid_ || owner_ || active_ || commit_ || !records_.empty() || generation_ || bytes.size()>MaxLedgerBytes ||
     receipt.profile()!=profile_.identity() || receipt.origin_card()!=mount_.origin_card_digest || !receipt.number() ||
     !seed::is_digest(receipt.card_digest()) || !seed::is_digest(receipt.record_digest()) || receipt.ledger_digest()!=seed::sha256(bytes)) return Status::InvalidCommit;
  Reader r{bytes};
  if(r.str(64)!="BlueWake.WindWaker.RewardLedger" || r.u32()!=1 || r.str(64)!=profile_.identity() ||
     r.str(64)!=mount_.origin_card_digest || r.str(64)!=mount_.module_digest || r.byte()!=mount_.quest || r.u64()!=receipt.number()) return Status::InvalidCommit;
  const auto token=r.u64();const auto count=r.u32();
  if(!r.good || !token || count>seed::MaxPlacements) return Status::InvalidCommit;
  std::vector<RewardRecord> loaded;std::string previous;std::uint64_t max_award=0;
  for(std::uint32_t i=0;i<count;++i) {
    RewardRecord entry;entry.location_id=r.str(256);entry.item_id=r.str(256);entry.native_item=r.byte();entry.award_sequence=r.u64();entry.state=State::Saved;
    const auto placement=profile_.reward_at(entry.location_id);
    const auto reward=seed::native_reward(entry.item_id);
    if(!r.good || entry.location_id!=seed::linkug_location_id() || (!previous.empty() && entry.location_id<=previous) ||
       !placement || *placement!=entry.item_id || !reward || reward->item!=entry.native_item || !entry.award_sequence) return Status::InvalidCommit;
    previous=entry.location_id;max_award=std::max(max_award,entry.award_sequence);loaded.push_back(std::move(entry));
  }
  if(!r.good || r.at!=bytes.size() || encode(profile_,mount_,receipt.number(),token,loaded)!=bytes) return Status::InvalidCommit;
  records_=std::move(loaded);generation_=receipt.number();commit_sequence_=token;award_sequence_=max_award;return Status::Accepted;
}
void Ledger::abandon_commit(){commit_.reset();}
} // namespace bluewake::randomizer::transaction
