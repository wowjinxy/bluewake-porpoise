// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_inventory_context.h"
#include <algorithm>
#include <cstring>

namespace bluewake::randomizer::native_subset {
namespace {
constexpr const char* kPrefix = "BlueWake.WindWaker.Randomizer.SourceCatalog.v1.9775811b6fd039992822fbaf30ea6259b339f4b0:";
struct ItemPin { const char* name; const char* hash; };
constexpr ItemPin kItems[] = {
  {"Wind Waker", "af278fb900c54e27b238b26ba69ce9a80961d4c328e20e1ab95d4a3790cc9a63"},
  {"Grappling Hook", "2493167703018e599e0f95c0bd19288c7420c440dd225cb7645d7b24ece75ffa"},
  {"Wind's Requiem", "c69c4369445446d2f82915356d935367f61d6910d3ad14ded4f07888b28549e8"}
};
struct MacroPin { const char* name; const char* hash; const char* requirement; };
constexpr MacroPin kMacros[] = {
  {"Can Play Wind's Requiem", "5d2bb76adcf35048c262c2b7f5501f1b07bfff282545a88d7cdc2a6fa2155f75", "Wind Waker & Wind's Requiem"},
  {"Can Defeat Gohma", "870a4048802b0e0d7ccfcdc881bf06344062e32f18cbe333791c96ac480bb767", "Grappling Hook"}
};
bool fingerprint_known(const ModuleFingerprint& module) {
  return std::any_of(module.begin(), module.end(), [](std::uint8_t b) { return b != 0; });
}
bool stamp_complete(const OwnerStamp& owner) {
  if (!fingerprint_known(owner.module) || !owner.cpu_lifetime || !owner.memory_lifetime ||
      !owner.native_epoch || !owner.scene_generation || owner.player < 0x80000000u ||
      owner.player >= 0x81800000u || (owner.player & 3u) || !owner.stage[0] || owner.stage[8]) return false;
  for (std::size_t i = 0; i < 8 && owner.stage[i]; ++i)
    if (static_cast<unsigned char>(owner.stage[i]) < 32 || static_cast<unsigned char>(owner.stage[i]) > 126) return false;
  return true;
}
bool recollection_stage(const OwnerStamp& owner) {
  return std::memcmp(owner.stage.data(), "Xboss", 5) == 0 &&
         owner.stage[5] >= '0' && owner.stage[5] <= '3' && !owner.stage[6];
}
FactState slot_fact(std::optional<std::uint8_t> slot,
                    std::optional<std::uint8_t> flags, std::uint8_t expected) {
  if (!slot || !flags) return FactState::Unknown;
  // Higher bits of native mItemFlags[2] also hold bottle bookkeeping.
  const bool obtained = (*flags & 1u) != 0;
  if (obtained && *slot == expected) return FactState::Present;
  if (!obtained && *slot == 0xFFu) return FactState::Absent;
  return FactState::Contradictory;
}
bool pinned_catalog(const Catalog& catalog) {
  // Bound the constructor's outer scans for this closed, pinned-only wrapper.
  if (catalog.items.size() > 512 || catalog.macros.size() > 1024 ||
      catalog.locations.size() > 1024 || catalog.options.size() > 64) return false;
  for (const auto& pin : kItems) {
    const auto id = std::string(kPrefix) + "item:" + pin.hash;
    unsigned matched = 0;
    for (const auto& item : catalog.items) {
      if (item.name != pin.name && item.source_id != id) continue;
      if (item.name != pin.name || item.source_id != id || !item.logic_item) return false;
      ++matched;
    }
    if (matched != 1) return false;
  }
  for (const auto& pin : kMacros) {
    const auto id = std::string(kPrefix) + "macro:" + pin.hash;
    unsigned matched = 0;
    for (const auto& macro : catalog.macros) {
      if (macro.name != pin.name && macro.source_id != id) continue;
      // Exact expression pins close the entire dependency set. No arbitrary
      // expressions, OR alternatives, options, aliases or location references
      // can enter even when their result would have been true.
      if (macro.name != pin.name || macro.source_id != id || macro.requirement != pin.requirement) return false;
      ++matched;
    }
    if (matched != 1) return false;
  }
  return true;
}
} // namespace

bool same_owner(const OwnerStamp& a, const OwnerStamp& b) {
  return a.module == b.module && a.cpu_lifetime == b.cpu_lifetime &&
      a.memory_lifetime == b.memory_lifetime && a.module_generation == b.module_generation &&
      a.native_epoch == b.native_epoch && a.scene_generation == b.scene_generation &&
      a.alias_generation == b.alias_generation && a.native_frame == b.native_frame &&
      a.player == b.player && a.stage == b.stage;
}
FactState PartialInventory::fact(CoveredItem item) const {
  const auto index = static_cast<unsigned>(item);
  return index < facts_.size() ? facts_[index] : FactState::Unknown;
}
PartialInventory project_inventory(const CopiedInventoryObservation& observed,
    const std::optional<OwnerStamp>& current, const ModuleFingerprint& expected) {
  PartialInventory out;
  if (!observed.before || !observed.after || !current || !observed.safety ||
      !stamp_complete(*observed.before) || !stamp_complete(*observed.after) || !stamp_complete(*current)) return out;
  if (!same_owner(*observed.before, *observed.after) || !same_owner(*observed.after, *current)) {
    out.status_ = Status::Stale; return out;
  }
  if (!fingerprint_known(expected) || observed.after->module != expected) {
    out.status_ = Status::UnsupportedOwner; return out;
  }
  const auto& safety = *observed.safety;
  if (!safety.scene_active || !safety.player_valid || !safety.controls_ready || safety.paused ||
      safety.event_running || safety.transitioning || safety.save_or_load_active || safety.machine_request_pending ||
      safety.recollection_stage || recollection_stage(*observed.after)) {
    out.status_ = Status::UnsafeContext; return out;
  }
  out.owner_ = observed.after;
  out.facts_[0] = slot_fact(observed.waker_slot, observed.waker_obtained, 0x22);
  out.facts_[1] = slot_fact(observed.hook_slot, observed.hook_obtained, 0x25);
  out.facts_[2] = !observed.songs ? FactState::Unknown :
      (*observed.songs & 1u) ? FactState::Present : FactState::Absent;
  if (observed.collected_swords) out.sword_.permanent_hero_sword = (*observed.collected_swords & 1u) != 0;
  out.sword_.equipped_id = observed.equipped_sword;
  out.status_ = Status::Ready;
  for (std::size_t i = 0; i < out.facts_.size(); ++i) {
    const auto state = out.facts_[i];
    if (state == FactState::Contradictory) out.status_ = Status::Contradictory;
    else if (state == FactState::Unknown && out.status_ != Status::Contradictory) out.status_ = Status::Incomplete;
    if (state == FactState::Present || state == FactState::Absent)
      out.context_.items.emplace(kItems[i].name, state == FactState::Present ? 1u : 0u);
  }
  // Context remains private and has exactly the three covered items only when
  // Ready. No options, native selected X/Y/Z, progressive gear or defaults.
  return out;
}
CapabilityLogic::CapabilityLogic(Catalog catalog) {
  if (!pinned_catalog(catalog)) return;
  auto logic = std::make_unique<Logic>(std::move(catalog));
  if (!logic->valid()) { catalog_status_ = Status::InvalidCatalog; return; }
  logic_ = std::move(logic); catalog_status_ = Status::Ready;
}
CapabilityResult CapabilityLogic::evaluate(ApprovedCapability capability,
    const PartialInventory& context, const std::optional<OwnerStamp>& current) const {
  const char* item = nullptr; const char* macro = nullptr;
  switch (capability) {
    case ApprovedCapability::OwnWindWaker: item = kItems[0].name; break;
    case ApprovedCapability::OwnGrapplingHook: item = kItems[1].name; break;
    case ApprovedCapability::LearnedWindsRequiem: item = kItems[2].name; break;
    case ApprovedCapability::PlayWindsRequiem: macro = kMacros[0].name; break;
    case ApprovedCapability::DefeatGohma: macro = kMacros[1].name; break;
    default: return {Status::UnsupportedCapability, std::nullopt, 0};
  }
  if (catalog_status_ != Status::Ready) return {catalog_status_, std::nullopt, 0};
  if (!context.owner_) return {context.status_, std::nullopt, 0};
  if (!current || !stamp_complete(*current)) return {Status::Incomplete, std::nullopt, 0};
  if (!same_owner(*context.owner_, *current)) return {Status::Stale, std::nullopt, 0};
  if (context.status_ != Status::Ready) return {context.status_, std::nullopt, 0};
  const auto result = macro ? logic_->macro(macro, context.context_) : logic_->expression(item, context.context_);
  if (!result.valid) return {Status::InvalidCatalog, std::nullopt, result.work};
  return {Status::Ready, result.reachable, result.work};
}
} // namespace bluewake::randomizer::native_subset
