// SPDX-License-Identifier: GPL-3.0-or-later
#include "randomizer_seed.h"
#include "network_digest.h" // Existing portable SHA-256; no networking dependency.
#include <algorithm>
#include <limits>
#include <set>
#include <type_traits>

namespace bluewake::randomizer::seed {
namespace {
constexpr std::size_t MaxCatalogBytes = 4 * 1024 * 1024;
constexpr std::size_t MaxRecords = 2048, MaxOptions = 64, MaxList = 256;
const std::string prefix = "BlueWake.WindWaker.Randomizer.SourceCatalog.v1.9775811b6fd039992822fbaf30ea6259b339f4b0:";
const std::string orange = prefix + "item:fff15ce906e6d2014987ebed1f1886f82a74f008cdcb6dd04adf651f509d2d5c";
const std::string camera = prefix + "item:89b76d7e7148570650c254d87b093c47f68867bde5399b0d05e926a32c59c9a7";
const std::string chest = prefix + "location:6affff61e89d9a4c401c6ba1d3809445990747a618eb26891d2f6a5ffbbf5a8a";

bool text_ok(const std::string& s, std::size_t max, bool empty = false) {
  if ((!empty && s.empty()) || s.size() > max) return false;
  return std::all_of(s.begin(), s.end(), [](unsigned char c) { return c >= 0x20 && c != 0x7f; });
}
struct Writer {
  std::string bytes;
  std::size_t limit;
  bool good = true;
  void u8(std::uint8_t x) { if (bytes.size() < limit) bytes.push_back(char(x)); else good = false; }
  void u32(std::uint32_t x) { for (int i = 3; i >= 0; --i) u8(std::uint8_t(x >> (i * 8))); }
  void u64(std::uint64_t x) { for (int i = 7; i >= 0; --i) u8(std::uint8_t(x >> (i * 8))); }
  void str(const std::string& x) {
    if (x.size() > std::numeric_limits<std::uint32_t>::max() || x.size() > limit - std::min(limit, bytes.size())) { good = false; return; }
    u32(std::uint32_t(x.size()));
    for (unsigned char c : x) u8(c);
  }
};
struct Reader {
  const std::string& bytes;
  std::size_t at = 0;
  bool good = true;
  std::uint8_t u8() { if (at == bytes.size()) { good = false; return 0; } return std::uint8_t(bytes[at++]); }
  std::uint32_t u32() { std::uint32_t n = 0; for (int i = 0; i < 4; ++i) n = (n << 8) | u8(); return n; }
  std::uint64_t u64() { std::uint64_t n = 0; for (int i = 0; i < 8; ++i) n = (n << 8) | u8(); return n; }
  std::string str(std::size_t max) {
    const auto n = u32();
    if (!good || n > max || n > bytes.size() - at) { good = false; return {}; }
    auto x = bytes.substr(at, n); at += n; return x;
  }
};
void option_write(Writer& w, const OptionValue& v) {
  if (const auto* b = std::get_if<bool>(&v)) { w.u8(0); w.u8(*b ? 1 : 0); }
  else if (const auto* s = std::get_if<std::string>(&v)) { w.u8(1); w.str(*s); }
  else { w.u8(2); const auto& list = std::get<std::vector<std::string>>(v); w.u32(std::uint32_t(list.size())); for (const auto& s : list) w.str(s); }
}
std::string profile_bytes(const Request& r, const std::string& catalog) {
  Writer w{{}, MaxProfileBytes};
  w.str("BlueWake.WindWaker.SeedProfile"); w.u32(ProfileVersion);
  w.str("SplitMix64-Rejection-FisherYates-v1"); w.str(catalog);
  w.u64(r.seed); w.str(r.logic_contract_digest); w.str(r.start_policy_digest);
  w.u32(std::uint32_t(r.options.size()));
  for (const auto& o : r.options) { w.str(o.first); option_write(w, o.second); }
  w.u32(std::uint32_t(r.placements.size()));
  for (const auto& p : r.placements) { w.str(p.location_id); w.str(p.item_id); }
  return w.good ? w.bytes : std::string{};
}
template<class T> void sorted_records(const std::vector<T>& rows, Writer& w) {
  std::vector<const T*> order; order.reserve(rows.size());
  for (const auto& r : rows) order.push_back(&r);
  std::sort(order.begin(), order.end(), [](const T* a, const T* b) { return a->source_id < b->source_id; });
  w.u32(std::uint32_t(order.size()));
  for (const auto* r : order) {
    w.str(r->source_id); w.str(r->name);
    if constexpr (std::is_same_v<T, Item>) {
      w.u8(r->logic_item ? 1 : 0); w.u32(std::uint32_t(r->native_name_ids.size()));
      for (auto n : r->native_name_ids) w.u8(n);
    } else if constexpr (std::is_same_v<T, Macro>) w.str(r->requirement);
    else {
      w.str(r->requirement); w.str(r->original_item); w.u32(std::uint32_t(r->types.size()));
      for (const auto& t : r->types) w.str(t);
      w.u32(std::uint32_t(r->paths.size()));
      for (const auto& p : r->paths) { w.u8(std::uint8_t(p.kind)); w.str(p.raw); }
    }
  }
}
Result<Profile> failure(const std::string& why) { return {{}, {why}}; }
}

bool is_digest(const std::string& s) { return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }); }
std::string sha256(const std::string& bytes) { return bw_net::digest(bytes); }
const std::string& orange_rupee_id() { return orange; }
const std::string& basic_picto_id() { return camera; }
const std::string& linkug_location_id() { return chest; }
std::optional<NativeReward> native_reward(const std::string& id) {
  if (id == orange) return NativeReward{NativeRewardKind::OrangeRupee, 0x06};
  if (id == camera) return NativeReward{NativeRewardKind::BasicPictoBox, 0x23};
  return {};
}
std::uint64_t StableRandom::next() {
  auto z = (state_ += UINT64_C(0x9e3779b97f4a7c15));
  z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
  return z ^ (z >> 31);
}
std::optional<std::uint64_t> StableRandom::bounded(std::uint64_t bound) {
  if (!bound) return {};
  const auto threshold = (std::uint64_t(0) - bound) % bound;
  // Bounded work even for an adversarial requested bound/state.
  for (unsigned i = 0; i < 64; ++i) { const auto r = next(); if (r >= threshold) return r % bound; }
  return {};
}
std::optional<std::string> Profile::reward_at(const std::string& id) const {
  const auto i = std::lower_bound(request_.placements.begin(), request_.placements.end(), id,
    [](const Placement& p, const std::string& x) { return p.location_id < x; });
  if (i == request_.placements.end() || i->location_id != id) return {};
  return i->item_id;
}
struct ProfileBuilder::Impl {
  Catalog catalog;
  std::map<std::string, const Item*> items;
  std::map<std::string, const Location*> locations;
  std::map<std::string, const OptionSpec*> options;
  std::vector<std::string> errors;
  std::string digest;
};
ProfileBuilder::ProfileBuilder(Catalog catalog) {
  auto p = std::make_shared<Impl>(); p->catalog = std::move(catalog);
  auto& c = p->catalog;
  if (c.items.empty() || c.items.size() > MaxRecords || c.locations.empty() || c.locations.size() > MaxRecords ||
      c.macros.size() > MaxRecords || c.options.size() > MaxOptions) {
    p->errors.push_back("Catalog record bounds exceeded or empty item/location table."); impl_ = std::move(p); return;
  }
  std::set<std::string> ids;
  auto record = [&](const std::string& id, const std::string& name) {
    if (!text_ok(id, 256) || !text_ok(name, 256) || !ids.insert(id).second) p->errors.push_back("Malformed or duplicate catalog identity.");
  };
  for (const auto& i : c.items) { record(i.source_id, i.name); if (i.native_name_ids.size() > 256) p->errors.push_back("Item identity list exceeds bound."); p->items.emplace(i.source_id, &i); }
  for (const auto& m : c.macros) { record(m.source_id, m.name); if (!text_ok(m.requirement, 16384)) p->errors.push_back("Malformed macro requirement."); }
  for (const auto& l : c.locations) {
    record(l.source_id, l.name); p->locations.emplace(l.source_id, &l);
    if (!text_ok(l.requirement, 16384) || !text_ok(l.original_item, 256) || l.paths.empty() || l.paths.size() > 128 || l.types.size() > 64) { p->errors.push_back("Malformed location."); continue; }
    for (const auto& t : l.types) if (!text_ok(t, 256)) p->errors.push_back("Malformed location type.");
    for (const auto& path : l.paths) if (unsigned(path.kind) > unsigned(PathKind::ScalableObjectResource) || !text_ok(path.raw, 1024)) p->errors.push_back("Malformed location path.");
  }
  for (const auto& o : c.options) {
    if (!text_ok(o.name, 256) || !p->options.emplace(o.name, &o).second || o.choices.size() > MaxList) { p->errors.push_back("Malformed or duplicate option."); continue; }
    std::set<std::string> choices;
    for (const auto& s : o.choices) if (!text_ok(s, 256) || !choices.insert(s).second) p->errors.push_back("Malformed or duplicate choice.");
    if (o.kind == OptionKind::Boolean ? !o.choices.empty() :
        (o.kind == OptionKind::Choice || o.kind == OptionKind::List) ? o.choices.empty() : true) p->errors.push_back("Unsupported option kind/choice schema.");
  }
  Writer w{{}, MaxCatalogBytes}; w.str("BlueWake.WindWaker.SeedCatalog.v1");
  // Fail before the evaluator and encoder on malformed/unbounded input.
  if (p->errors.empty()) {
    sorted_records(c.items, w); sorted_records(c.macros, w); sorted_records(c.locations, w);
    w.u32(std::uint32_t(p->options.size()));
    for (const auto& o : p->options) { w.str(o.first); w.u8(std::uint8_t(o.second->kind)); w.u32(std::uint32_t(o.second->choices.size())); for (const auto& choice : o.second->choices) w.str(choice); }
    if (!w.good) p->errors.push_back("Catalog encoding exceeds byte bound.");
    else {
      Logic logic(c);
      if (!logic.valid()) p->errors.push_back("Catalog logic is invalid.");
      else p->digest = sha256(w.bytes);
    }
  }
  impl_ = std::move(p);
}
bool ProfileBuilder::valid() const { return impl_->errors.empty(); }
const std::vector<std::string>& ProfileBuilder::errors() const { return impl_->errors; }
const std::string& ProfileBuilder::catalog_digest() const { return impl_->digest; }
Result<Profile> ProfileBuilder::build(Request r) const {
  if (!valid()) return {{}, impl_->errors};
  if (!is_digest(r.logic_contract_digest) || !is_digest(r.start_policy_digest)) return failure("Logic/start identity must be lowercase SHA-256 metadata.");
  if (r.options.size() != impl_->options.size()) return failure("Every catalog option must be supplied exactly once.");
  for (auto& o : r.options) {
    const auto spec = impl_->options.find(o.first);
    if (spec == impl_->options.end()) return failure("Unknown option.");
    const auto& s = *spec->second;
    auto allowed = [&](const std::string& v) { return std::find(s.choices.begin(), s.choices.end(), v) != s.choices.end(); };
    if (s.kind == OptionKind::Boolean) { if (!std::holds_alternative<bool>(o.second)) return failure("Wrong boolean option type."); }
    else if (s.kind == OptionKind::Choice) { const auto* v = std::get_if<std::string>(&o.second); if (!v || !allowed(*v)) return failure("Unknown choice or wrong option type."); }
    else {
      auto* list = std::get_if<std::vector<std::string>>(&o.second);
      if (!list || list->size() > MaxList) return failure("Wrong list option type or bound.");
      std::sort(list->begin(), list->end());
      if (std::adjacent_find(list->begin(), list->end()) != list->end()) return failure("Duplicate list option member.");
      for (const auto& v : *list) if (!allowed(v)) return failure("Unknown list option member.");
    }
  }
  if (r.placements.empty() || r.placements.size() > MaxPlacements) return failure("Placement count outside bound.");
  std::sort(r.placements.begin(), r.placements.end(), [](const Placement& a, const Placement& b) { return a.location_id < b.location_id; });
  std::string previous;
  for (const auto& p : r.placements) {
    if (!impl_->locations.count(p.location_id) || !impl_->items.count(p.item_id)) return failure("Unknown catalog placement/reward identity.");
    if (p.location_id == previous) return failure("Duplicate location placement.");
    previous = p.location_id;
  }
  Profile p; p.request_ = std::move(r); p.catalog_digest_ = impl_->digest;
  const auto l = impl_->locations.find(chest), o = impl_->locations.end();
  const auto a = impl_->items.find(orange), b = impl_->items.find(camera);
  p.audited_linkug_ = l != o && a != impl_->items.end() && b != impl_->items.end() &&
    l->second->name == "Outset Island - Underneath Link's House" && l->second->requirement == "Nothing" &&
    l->second->original_item == "Orange Rupee" && l->second->paths.size() == 1 &&
    l->second->paths[0].kind == PathKind::ChestResource && l->second->paths[0].raw == "LinkUG/Stage.arc/Chest000" &&
    a->second->name == "Orange Rupee" && a->second->logic_item && a->second->native_name_ids == std::vector<std::uint8_t>{6} &&
    b->second->name == "Picto Box" && !b->second->logic_item && b->second->native_name_ids == std::vector<std::uint8_t>{35};
  p.encoded_ = profile_bytes(p.request_, p.catalog_digest_);
  if (p.encoded_.empty()) return failure("Profile exceeds byte bound.");
  p.identity_ = sha256(p.encoded_);
  return {std::move(p), {}};
}
Result<Profile> ProfileBuilder::generate(GenerateRequest r) const {
  if (!valid()) return {{}, impl_->errors};
  if (!r.identity.placements.empty() || r.locations.empty() || r.locations.size() > MaxPlacements ||
      r.reward_pool.size() != r.locations.size() || r.plando.size() > r.locations.size()) return failure("Generation requires an explicit equally sized location/reward pool and bounded plando.");
  std::sort(r.locations.begin(), r.locations.end()); std::sort(r.reward_pool.begin(), r.reward_pool.end());
  if (std::adjacent_find(r.locations.begin(), r.locations.end()) != r.locations.end()) return failure("Duplicate generation location.");
  for (const auto& l : r.locations) if (!impl_->locations.count(l)) return failure("Unknown generation location.");
  for (const auto& i : r.reward_pool) if (!impl_->items.count(i)) return failure("Unknown generation reward.");
  std::set<std::string> fixed;
  for (const auto& p : r.plando) {
    if (!std::binary_search(r.locations.begin(), r.locations.end(), p.location_id) || !fixed.insert(p.location_id).second) return failure("Unknown/duplicate plando location.");
    const auto item = std::lower_bound(r.reward_pool.begin(), r.reward_pool.end(), p.item_id);
    if (item == r.reward_pool.end() || *item != p.item_id) return failure("Plando exceeds reward-pool multiplicity.");
    r.reward_pool.erase(item); r.identity.placements.push_back(p);
  }
  StableRandom random(r.identity.seed);
  for (std::size_t i = r.reward_pool.size(); i > 1; --i) {
    const auto j = random.bounded(i);
    if (!j) return failure("PRNG rejection work limit.");
    std::swap(r.reward_pool[i - 1], r.reward_pool[std::size_t(*j)]);
  }
  std::size_t next = 0;
  for (const auto& l : r.locations) if (!fixed.count(l)) r.identity.placements.push_back({l, r.reward_pool[next++]});
  return build(std::move(r.identity));
}
Result<Profile> ProfileBuilder::decode(const std::string& encoded) const {
  if (encoded.size() > MaxProfileBytes) return failure("Profile byte limit exceeded.");
  Reader reader{encoded};
  if (reader.str(64) != "BlueWake.WindWaker.SeedProfile" || reader.u32() != ProfileVersion ||
      reader.str(64) != "SplitMix64-Rejection-FisherYates-v1" || reader.str(64) != impl_->digest) return failure("Unknown format/generator or catalog identity.");
  Request r; r.seed = reader.u64(); r.logic_contract_digest = reader.str(64); r.start_policy_digest = reader.str(64);
  const auto options = reader.u32(); if (!reader.good || options > MaxOptions) return failure("Encoded option bounds.");
  for (std::uint32_t i = 0; i < options; ++i) {
    const auto name = reader.str(256); const auto tag = reader.u8(); OptionValue v;
    if (tag == 0) { const auto b = reader.u8(); if (b > 1) return failure("Noncanonical boolean."); v = b != 0; }
    else if (tag == 1) v = reader.str(256);
    else if (tag == 2) {
      const auto n = reader.u32(); if (!reader.good || n > MaxList) return failure("Encoded option list bound.");
      std::vector<std::string> list; for (std::uint32_t j = 0; j < n; ++j) list.push_back(reader.str(256)); v = std::move(list);
    } else return failure("Unknown option value tag.");
    if (!r.options.emplace(name, std::move(v)).second) return failure("Duplicate encoded option.");
  }
  const auto count = reader.u32(); if (!reader.good || count > MaxPlacements) return failure("Encoded placement bounds.");
  for (std::uint32_t i = 0; i < count; ++i) r.placements.push_back({reader.str(256), reader.str(256)});
  if (!reader.good || reader.at != encoded.size()) return failure("Truncated profile or trailing data.");
  auto result = build(std::move(r));
  if (result && result.value->encode() != encoded) return failure("Noncanonical encoding/order.");
  return result;
}
} // namespace bluewake::randomizer::seed
