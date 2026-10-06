// SPDX-License-Identifier: GPL-3.0-or-later
// Storage/core ownership only. No guest read, award or native save authority.
#pragma once
#include "randomizer_transaction.h"
#include "randomizer_store.h"
#include <functional>
#include <memory>
#include <thread>

namespace bluewake::randomizer::session {
class Session;
class Lease {
public:
  Lease() = default;
  bool valid() const { return token_ != 0; }
  bool operator==(const Lease& other) const { return token_ == other.token_; }
private:
  explicit Lease(std::uint64_t token) : token_(token) {}
  std::uint64_t token_ = 0;
  friend class Session;
};
struct Snapshot {
  std::string profile, origin_card, module, card_digest;
  std::uint8_t quest = 255;
  std::uint64_t generation = 0; // Last confirmed core generation, not a guess after failure.
  bool retired = false, recovery_needed = false;
  std::vector<transaction::RewardRecord> rewards;
};
class Session {
public:
  // BEFORE the native backend opens. Directory is an explicit dedicated seed
  // folder. No personal/source path is read or copied. Existing pairs must
  // match expected origin/module/quest and the current canonical catalog.
  // An absent pair requires explicit opaque initial bytes matching expected
  // origin; those bytes are bounded but are NOT validated as a native CARD.
  static std::unique_ptr<Session> open(const std::string& dedicated_directory,
      const seed::Profile&, const transaction::Mount& expected_mount,
      const storage::Bytes* explicit_initial_card, std::string& error);
  ~Session() = default; // Caller MUST close native backend first, on owner thread.
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session(Session&&) = delete;
  Session& operator=(Session&&) = delete;

  // Creator thread only. This lease is only storage/core access, never CPU,
  // native registry, starting-inventory, item-award or save authorization.
  bool lease(Lease& out, std::string& error) const;
  bool working_card_path(std::string& out, std::string& error) const;
  bool snapshot(Snapshot& out, std::string& error) const;
  // Scoped owner access; callback must not retain a Ledger pointer/reference.
  // Wrong thread/lease or reentrancy refuses without mutating owner state.
  // Callback exception or Cancelled row permanently retires this session.
  bool with_ledger(const Lease&, const std::function<void(transaction::Ledger&)>&,
                   std::string& error);
  // COPIED STORAGE ONLY. Caller must ALREADY prove genuine native serialization
  // and successful save completion, and copy this exact mounted working CARD
  // under its snapshot lock. Neither bytes nor digest imply native saving.
  // Actual publication failure retires session: no further awards or ack.
  // Close backend/destroy, then open a NEW session/verified pair to recover.
  bool publish_snapshot(const storage::Bytes& copied_card, std::string& error);
private:
  Session(const seed::Profile&, const transaction::Mount&);
  bool ready(std::string& error) const;
  bool cancelled() const;
  void retire();
  const std::thread::id owner_;
  const seed::Profile profile_;
  const transaction::Mount mount_;
  storage::Store store_;
  transaction::Ledger ledger_;
  // Exact last acknowledged pair, copied from one Store-issued receipt.
  // Kept together even if a later publication succeeds without an ack.
  std::string confirmed_card_digest_;
  std::uint64_t confirmed_generation_ = 0;
  std::uint64_t token_ = 0;
  bool retired_ = false, inside_callback_ = false;
};
} // namespace bluewake::randomizer::session
