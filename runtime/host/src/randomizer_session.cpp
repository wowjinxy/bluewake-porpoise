// SPDX-License-Identifier: GPL-3.0-or-later
#include "randomizer_session.h"
#include <algorithm>
#include <atomic>
#include <limits>

namespace bluewake::randomizer::session {
namespace {
std::atomic<std::uint64_t> sequence{0};
std::uint64_t issue() {
  auto old=sequence.load(std::memory_order_relaxed);
  while(old!=std::numeric_limits<std::uint64_t>::max())
    if(sequence.compare_exchange_weak(old,old+1,std::memory_order_relaxed))return old+1;
  return 0;
}
std::string digest(const storage::Bytes& bytes) { return seed::sha256(std::string(bytes.begin(),bytes.end())); }
bool card_ok(const storage::Bytes& bytes) { return !bytes.empty() && bytes.size()<=storage::Store::MaxCardBytes; }
bool profile_ok(const seed::Profile& profile) {
  if(!seed::is_digest(profile.identity()) || !profile.audited_linkug_binding() || profile.placements().size()!=1 ||
     profile.placements().front().location_id!=seed::linkug_location_id() || !seed::native_reward(profile.placements().front().item_id))return false;
  const seed::ProfileBuilder canonical(imported_catalog());
  const auto decoded=canonical.decode(profile.encode());
  return decoded && decoded.value->identity()==profile.identity();
}
storage::Bytes bytes(const std::string& text) { return {text.begin(),text.end()}; }
}
Session::Session(const seed::Profile& profile,const transaction::Mount& mount)
  :owner_(std::this_thread::get_id()),profile_(profile),mount_(mount),ledger_(profile,mount) {}
std::unique_ptr<Session> Session::open(const std::string& directory,const seed::Profile& profile,
    const transaction::Mount& mount,const storage::Bytes* initial,std::string& error) {
  error.clear();
  try {
    if(directory.empty() || directory.size()>2048 || directory.find('\0')!=std::string::npos ||
       !profile_ok(profile) || !seed::is_digest(mount.origin_card_digest) ||
       !seed::is_digest(mount.module_digest) || mount.quest>=3) {
      error="Invalid dedicated seed session/profile/mount";return {};
    }
    if(initial && (!card_ok(*initial) || digest(*initial)!=mount.origin_card_digest)) {
      error="Explicit initial CARD does not match expected origin";return {};
    }
    auto session=std::unique_ptr<Session>(new Session(profile,mount));
    if(!session->ledger_.valid() || !session->store_.open(directory.c_str(),profile.identity(),error))return {};
    storage::Generation generation;storage::CommitReceipt receipt;bool found=false;
    if(!session->store_.load(generation,found,error,&receipt))return {};
    if(found) {
      if(generation.origin_card!=mount.origin_card_digest ||
         session->ledger_.restore_saved(receipt,std::string(generation.ledger.begin(),generation.ledger.end()))!=transaction::Status::Accepted) {
        error="Stored pair does not match canonical ledger/module/quest/origin";return {};
      }
    } else {
      if(!initial) { error="A new seed session requires an explicit initial CARD";return {}; }
      const auto candidate=session->ledger_.prepare_commit(mount.origin_card_digest);
      if(!candidate || !session->store_.commit(0,*initial,bytes(candidate->ledger_bytes),receipt,error) ||
         session->ledger_.confirm_saved(receipt)!=transaction::Status::Accepted) {
        if(error.empty())error="Cannot initialize the complete seed pair";return {};
      }
    }
    session->confirmed_card_digest_=receipt.card_digest();
    session->confirmed_generation_=receipt.number();
    if(!session->store_.restore_working_card(error))return {};
    session->token_=issue();
    if(!session->token_) {error="Seed session lease counter exhausted";return {};}
    error.clear();return session;
  } catch(...) {error="Cannot open dedicated seed session";return {};}
}
bool Session::ready(std::string& error) const {
  if(owner_!=std::this_thread::get_id()){error="Seed session belongs to another thread";return false;}
  if(inside_callback_){error="Seed session access is not reentrant";return false;}
  if(retired_ || !token_){error="Seed session is retired; reopen verified pair";return false;}
  return true;
}
bool Session::cancelled() const {
  return std::any_of(ledger_.records().begin(),ledger_.records().end(),[](const auto& record){return record.state==transaction::State::Cancelled;});
}
void Session::retire() { ledger_.invalidate(transaction::Invalidation::Shutdown);retired_=true;token_=0; }
bool Session::lease(Lease& out,std::string& error) const {
  out={};if(!ready(error))return false;
  out=Lease(token_);error.clear();return true;
}
bool Session::working_card_path(std::string& out,std::string& error) const {
  out.clear();if(!ready(error))return false;
  out=store_.working_card_path();error.clear();return true;
}
bool Session::snapshot(Snapshot& out,std::string& error) const {
  out={};
  if(owner_!=std::this_thread::get_id()){error="Seed session belongs to another thread";return false;}
  if(inside_callback_){error="Seed session access is not reentrant";return false;}
  out.profile=profile_.identity();out.origin_card=mount_.origin_card_digest;out.module=mount_.module_digest;out.quest=mount_.quest;
  out.card_digest=confirmed_card_digest_;out.generation=confirmed_generation_;
  out.retired=retired_;out.recovery_needed=retired_||cancelled();out.rewards=ledger_.records();
  error.clear();return true;
}
bool Session::with_ledger(const Lease& lease,const std::function<void(transaction::Ledger&)>& callback,std::string& error) {
  if(!ready(error))return false;
  if(!lease.valid() || lease.token_!=token_ || !callback){error="Invalid or stale seed session lease/callback";return false;}
  inside_callback_=true;
  try {callback(ledger_);} catch(...) {
    inside_callback_=false;retire();error="Seed owner callback failed; reopen verified pair";return false;
  }
  inside_callback_=false;
  if(cancelled()){retire();error="Cancelled native transaction requires verified pair reload";return false;}
  error.clear();return true;
}
bool Session::publish_snapshot(const storage::Bytes& card,std::string& error) {
  if(!ready(error))return false;
  try {
    if(!card_ok(card)){retire();error="Invalid copied CARD bounds; reopen verified pair";return false;}
    // Allocate before publishing; the successful acknowledgement below must
    // update the cached tuple without any allocation or throwing operation.
    auto next_card_digest=digest(card);
    const auto candidate=ledger_.prepare_commit(next_card_digest);
    if(!candidate){retire();error="Ledger cannot publish this copied CARD; reopen verified pair";return false;}
    storage::CommitReceipt receipt;
    if(!store_.commit(candidate->previous_generation,card,bytes(candidate->ledger_bytes),receipt,error)) {
      retire();return false;
    }
    if(ledger_.confirm_saved(receipt)!=transaction::Status::Accepted) {
      retire();error="Published pair acknowledgement mismatch; reopen verified pair";return false;
    }
    confirmed_card_digest_.swap(next_card_digest);
    confirmed_generation_=receipt.number();
    error.clear();return true;
  } catch(...) {retire();error="Seed pair publication failed; reopen verified pair";return false;}
}
} // namespace bluewake::randomizer::session
