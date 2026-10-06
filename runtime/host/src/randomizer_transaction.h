// SPDX-License-Identifier: GPL-3.0-or-later
// Copied-value transaction validation only. The native adapter owns live proof.
#pragma once
#include "randomizer_seed.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bluewake::randomizer::storage { class CommitReceipt; }
namespace bluewake::randomizer::transaction {
enum class State { Unselected, Selected, ItemCreated, NativeAwardCompleted, Saved, Cancelled };
enum class Status { Accepted, Duplicate, Unavailable, WrongState, StaleOwner,
  InvalidEvidence, UnsupportedReward, PostconditionFailed, CommitPending, InvalidCommit };
enum class Invalidation { SceneChange, ActorRemoved, AliasChange, NativeLoad,
  MachineLoad, CpuReplaced, RamReplaced, ModuleReload, Shutdown };

// Exact registered records, copied by a qualified adapter at real boundaries.
// Nonzero tokens are owner-issued monotonic lifetimes, never pointer values.
// These fields alone are NOT proof that a native registry or boundary was read.
struct Owner {
  std::string module_digest;
  std::uint64_t cpu = 0, ram = 0, code_load = 0, alias = 0;
  std::uint64_t native_epoch = 0, scene = 0, native_card_load = 0;
  std::uint32_t player = 0, player_pid = 0;
  std::string stage;
  std::int8_t room = -1;
  std::uint8_t save_table = 255;
  std::uint8_t quest = 255; // Native selected quest/file slot, proven by adapter.
};
struct Actor {
  std::uint32_t address = 0, pid = 0;
  std::uint16_t process = 0, module = 0;
  std::uint64_t materialization = 0, registration = 0;
  // Chest execution is init2/create2; Demo_Item award is Delete init3/create2.
  // Registration is the actual actor tag/queue, not an in-use delete tag.
  std::uint8_t init_state = 0, create_result = 0;
  std::int8_t room = -1;
};
struct Inventory {
  std::uint8_t camera_slot = 255, camera_obtained = 0;
  std::int32_t rupee_queue = 0; // Native accumulator, not the rendered wallet.
};
struct Selection {
  Owner owner;
  Actor chest;
  Inventory inventory_before;
  std::uint64_t call = 0;
  std::uint32_t entry = 0, lr = 0, position_argument = 0;
  std::uint32_t parameters = 0;
  std::uint16_t home_angle_z = 0;
  std::uint8_t original_item = 255;
  std::uint32_t stack = 0;
  // Call tokens deduplicate. The adapter must separately prove native SP,
  // owner and import/canonical edge identity at entry and matching return.
};
struct Creation {
  Owner owner;
  Actor chest;
  std::uint64_t call = 0;
  std::uint32_t return_pc = 0, item_pid = 0;
  std::uint32_t stack = 0;
};
struct AwardEntry {
  Owner owner;
  Actor item;
  Inventory inventory_before; // Same real invocation, not earlier selection/VI.
  std::uint64_t call = 0;
  std::uint32_t entry = 0, lr = 0;
  std::uint8_t argument = 255, actor_item = 255;
  std::uint32_t stack = 0;
};
struct AwardReturn {
  Owner owner;
  Actor item;
  Inventory inventory_after;
  std::uint64_t call = 0;
  std::uint32_t return_pc = 0;
  std::uint32_t stack = 0;
  // execItemGet is void: intentionally no result/success register field.
};
struct Mount {
  std::string origin_card_digest, module_digest;
  std::uint8_t quest = 255;
};
struct Decision {
  Status status = Status::Unavailable;
  std::optional<std::uint8_t> reward; // The adapter may substitute only this argument.
};
struct RewardRecord {
  std::string location_id, item_id;
  std::uint8_t native_item = 255;
  std::uint64_t award_sequence = 0;
  State state = State::Unselected;
};
struct CommitCandidate {
  std::string profile, origin_card, card_digest, ledger_digest, ledger_bytes;
  std::uint64_t previous_generation = 0, generation = 0, commit_token = 0;
};

class Ledger {
public:
  Ledger(seed::Profile profile, Mount mount);
  Ledger(const Ledger&) = delete;
  Ledger& operator=(const Ledger&) = delete;
  Ledger(Ledger&&) = default;
  Ledger& operator=(Ledger&&) = default;
  bool valid() const { return valid_; }
  // Bind only after the adapter proves native CARD load and live ownership.
  // Ordinary scene changes retain awarded pending records; reset/load revokes
  // unsaved work, while Saved receipts remain separately immutable.
  // A Cancelled row quarantines this Ledger: no retry or checkpoint. Recovery
  // needs a fresh Ledger restored from a verified Store pair and an actual
  // native reload of that same pair; cancellation does not roll back the game.
  Status bind(const Owner& owner);
  Decision select(const Selection& evidence);
  Status created(const Creation& evidence);
  // Bind at the actual Demo_Item Delete/execItemGet boundary: actor remains
  // registered, initState3/createResult2. Do not bind its earlier initState2.
  Status bind_item(const Owner& owner, const Actor& item);
  Status award_enter(const AwardEntry& evidence);
  Status award_return(const AwardReturn& evidence);
  void invalidate(Invalidation why);
  State state(const std::string& location_id) const;
  const std::vector<RewardRecord>& records() const { return records_; }
  std::uint64_t generation() const { return generation_; }
  // Initial generation may contain no rewards. Later checkpoints require
  // the current bound owner and no unresolved native item transaction.
  // This validates copied storage state only: the qualified adapter must
  // correlate genuine native serialization/save completion and snapshot the
  // same isolated CARD before calling. A digest does not prove native saving.
  std::optional<CommitCandidate> prepare_commit(const std::string& card_digest);
  Status confirm_saved(const storage::CommitReceipt& durable_receipt);
  // The owner obtains the receipt from the store's verified load, not a number.
  // Parsing never authorizes a guest read or proves CARD inventory compatibility.
  Status restore_saved(const storage::CommitReceipt& durable_receipt,
                       const std::string& canonical_ledger);
  void abandon_commit(); // Does not mark any reward Saved.
private:
  struct Active {
    Selection selected;
    std::uint32_t item_pid = 0;
    std::optional<Actor> item;
    std::uint64_t award_call = 0;
    std::uint32_t award_stack = 0;
    Inventory award_inventory;
  };
  seed::Profile profile_;
  Mount mount_;
  bool valid_ = false;
  std::optional<Owner> owner_;
  std::optional<Owner> session_; // Retained through an ordinary scene suspension.
  std::optional<Active> active_;
  std::optional<CommitCandidate> commit_;
  std::vector<RewardRecord> records_;
  std::uint64_t generation_ = 0, commit_sequence_ = 0, award_sequence_ = 0;
  std::uint64_t last_select_call_ = 0, last_award_call_ = 0;
  bool owner_matches(const Owner&) const;
  void cancel_active();
};
} // namespace bluewake::randomizer::transaction
