// SPDX-License-Identifier: GPL-3.0-or-later
// Passive value-only partial inventory projection. No CPU/RAM reader, capture authorization or host API.
#pragma once
#include "randomizer_logic.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace bluewake::randomizer::native_subset {
using ModuleFingerprint = std::array<std::uint8_t, 32>;

// All fields are copied values. Lifetime tokens must be provided by a future
// qualified owner; address reuse is not a lifetime token. No pointer is retained.
struct OwnerStamp {
  ModuleFingerprint module{};
  std::uint64_t cpu_lifetime = 0, memory_lifetime = 0, module_generation = 0;
  std::uint64_t native_epoch = 0, scene_generation = 0, alias_generation = 0;
  std::uint32_t native_frame = 0, player = 0;
  std::array<char, 9> stage{};
};
bool same_owner(const OwnerStamp&, const OwnerStamp&);

// These are copied context facts, not proof of a completed native boundary.
// There is deliberately no native_boundary_authorized flag or live capture API.
struct ContextSafety {
  bool scene_active = false, player_valid = false, controls_ready = false;
  bool paused = false, event_running = false, transitioning = false;
  bool save_or_load_active = false, machine_request_pending = false;
  std::uint8_t recollection_stage = 0;
};
struct CopiedInventoryObservation {
  std::optional<OwnerStamp> before, after;
  std::optional<ContextSafety> safety;
  std::optional<std::uint8_t> waker_slot, waker_obtained;
  std::optional<std::uint8_t> hook_slot, hook_obtained, songs;
  std::optional<std::uint8_t> collected_swords, equipped_sword;
};
enum class Status {
  Ready, Incomplete, Contradictory, Stale, UnsafeContext, UnsupportedOwner,
  UnsupportedCapability, CatalogDrift, InvalidCatalog
};
enum class CoveredItem { WindWaker, GrapplingHook, WindsRequiem };
enum class FactState { Unknown, Absent, Present, Contradictory };
struct NativeSwordFacts {
  std::optional<bool> permanent_hero_sword;
  std::optional<std::uint8_t> equipped_id;
};
class PartialInventory;
PartialInventory project_inventory(const CopiedInventoryObservation&,
    const std::optional<OwnerStamp>& current, const ModuleFingerprint& expected_module);

class PartialInventory {
public:
  // Default is incomplete, never an implicitly empty inventory.
  PartialInventory() = default;
  Status status() const { return status_; }
  FactState fact(CoveredItem) const;
  NativeSwordFacts sword_facts() const { return sword_; }
  std::optional<OwnerStamp> owner() const { return owner_; }
private:
  Status status_ = Status::Incomplete;
  std::optional<OwnerStamp> owner_;
  std::array<FactState, 3> facts_{};
  NativeSwordFacts sword_;
  Context context_; // Never exposed or augmented with defaults/options.
  friend PartialInventory project_inventory(const CopiedInventoryObservation&,
      const std::optional<OwnerStamp>&, const ModuleFingerprint&);
  friend class CapabilityLogic;
};
enum class ApprovedCapability {
  OwnWindWaker, OwnGrapplingHook, LearnedWindsRequiem, PlayWindsRequiem, DefeatGohma
};
struct CapabilityResult {
  Status status = Status::Incomplete;
  // Unavailable is nullopt. Evaluated absence is false, not an error/default.
  std::optional<bool> value;
  std::uint32_t work = 0;
};
class CapabilityLogic {
public:
  // Own an immutable passive evaluator. The constructor audits exact selected
  // record IDs/names/logic flags and macro texts before creating Logic.
  explicit CapabilityLogic(Catalog catalog);
  CapabilityResult evaluate(ApprovedCapability, const PartialInventory&,
      const std::optional<OwnerStamp>& current) const;
  Status catalog_status() const { return catalog_status_; }
private:
  Status catalog_status_ = Status::CatalogDrift;
  std::unique_ptr<const Logic> logic_;
};
} // namespace bluewake::randomizer::native_subset
