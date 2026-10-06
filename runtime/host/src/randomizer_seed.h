// SPDX-License-Identifier: GPL-3.0-or-later
// Declarative seed identity only: no guest access, starting-state or beatability proof.
#pragma once
#include "randomizer_logic.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bluewake::randomizer::seed {
constexpr std::uint32_t ProfileVersion = 1;
constexpr std::size_t MaxProfileBytes = 1024 * 1024;
constexpr std::size_t MaxPlacements = 1024;

struct Placement {
  std::string location_id, item_id;
  bool operator==(const Placement& rhs) const {
    return location_id == rhs.location_id && item_id == rhs.item_id;
  }
};
struct Request {
  std::uint64_t seed = 0;
  // Identity metadata supplied by the owner, never compatibility/logic proof.
  std::string logic_contract_digest, start_policy_digest;
  std::map<std::string, OptionValue> options; // Every catalog option is explicit.
  std::vector<Placement> placements;
};
struct GenerateRequest {
  Request identity; // placements must be empty; use plando below.
  std::vector<std::string> locations, reward_pool;
  std::vector<Placement> plando; // Consumes one occurrence from the reward pool.
};
template<class T> struct Result {
  std::optional<T> value;
  std::vector<std::string> errors;
  explicit operator bool() const { return value.has_value(); }
};

// SplitMix64, including wraparound constants, is the specified v1 generator.
// bounded() uses rejection below (0-bound)%bound; zero bound is refused.
class StableRandom {
public:
  explicit StableRandom(std::uint64_t seed) : state_(seed) {}
  std::uint64_t next();
  std::optional<std::uint64_t> bounded(std::uint64_t bound);
private:
  std::uint64_t state_;
};

class Profile {
public:
  std::uint64_t seed() const { return request_.seed; }
  const std::string& identity() const { return identity_; }
  const std::string& catalog_digest() const { return catalog_digest_; }
  const std::string& logic_contract_digest() const { return request_.logic_contract_digest; }
  const std::string& start_policy_digest() const { return request_.start_policy_digest; }
  const std::map<std::string, OptionValue>& options() const { return request_.options; }
  const std::vector<Placement>& placements() const { return request_.placements; }
  const std::string& encode() const { return encoded_; }
  bool audited_linkug_binding() const { return audited_linkug_; }
  std::optional<std::string> reward_at(const std::string& location_id) const;
private:
  Request request_;
  std::string catalog_digest_, identity_, encoded_;
  bool audited_linkug_ = false;
  friend class ProfileBuilder;
};

class ProfileBuilder {
public:
  explicit ProfileBuilder(Catalog catalog);
  bool valid() const;
  const std::vector<std::string>& errors() const;
  const std::string& catalog_digest() const;
  Result<Profile> build(Request request) const;
  // Sorted location IDs, sorted pool, plando consumption, then Fisher-Yates
  // from the last element down. No implicit pool, start items or options.
  Result<Profile> generate(GenerateRequest request) const;
  Result<Profile> decode(const std::string& encoded) const;
private:
  struct Impl;
  std::shared_ptr<const Impl> impl_;
};

enum class NativeRewardKind { OrangeRupee, BasicPictoBox };
struct NativeReward { NativeRewardKind kind; std::uint8_t item; };
// These two exact imported records are audited codecs. Other known catalog
// items can be represented declaratively but cannot authorize a native award.
std::optional<NativeReward> native_reward(const std::string& catalog_item_id);
const std::string& orange_rupee_id();
const std::string& basic_picto_id();
const std::string& linkug_location_id();
bool is_digest(const std::string& value);
std::string sha256(const std::string& bounded_bytes);
} // namespace bluewake::randomizer::seed
