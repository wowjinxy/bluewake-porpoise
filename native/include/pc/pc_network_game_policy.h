// SPDX-License-Identifier: GPL-3.0-or-later
// Game-free policies used by the native game-thread network integration.
#ifndef PC_NETWORK_GAME_POLICY_H
#define PC_NETWORK_GAME_POLICY_H
#include "pc/pc_network_profile.h"
#include "pc/permanent_facts.h"
#include "pc/pc_progression_logic.h"
#include "pc/network/pc_network_client_store.h"
#include <cstring>

namespace pc::network {

inline uint32_t effectiveFact(uint16_t key, uint32_t value) {
    return !value && (key == PC_PERMANENT_ARROW_MAX || key == PC_PERMANENT_BOMB_MAX)
        ? 30 : value;
}

inline bool projectionIdentityHex(const char* text, unsigned length) {
    for (unsigned i = 0; i < length; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
    return text[length] == 0;
}

// Both inputs must be fresh copies from the SAME validated durable journal.
// This authorizes only native correlation reconciliation. It never mutates the
// journal, prunes its outbox, changes canonical revision, or acknowledges a host.
// Native apply must still enforce its live safe context and expected epoch.
inline bool localClaimProjectionValue(const PcProgressClaim& claim,
    const PcProgressIdentity& current, uint64_t expectedEpoch,
    const PcNetworkClientState& journal, const PcNetworkClientProjection& projection,
    uint32_t* target) {
    if (!target || !current.session_id || current.quest_slot < 0 ||
        current.quest_slot >= static_cast<int32_t>(PC_NETWORK_CLIENT_SLOTS) ||
        !pc::progression::claimCanBeDurable(claim, current, expectedEpoch) ||
        !pc::progression::claimPermitsSave(claim) || !journal.canonical_present ||
        !journal.serial || journal.outgoing_count == 0 || journal.outgoing_count > PC_NETWORK_QUEUE ||
        journal.reward_count > PC_NETWORK_REWARDS || !projection.provisional ||
        projection.serial != journal.serial || projection.revision != journal.revision ||
        !projectionIdentityHex(journal.profile_identity, 64) ||
        std::memcmp(current.profile_id, journal.profile_identity, sizeof current.profile_id) ||
        !projectionIdentityHex(journal.room_identity, 32) ||
        std::memcmp(projection.room_identity, journal.room_identity, sizeof projection.room_identity)) return false;
    const auto key = static_cast<uint16_t>(claim.key);
    uint32_t folded = journal.facts.values[key];
    if (folded && !pc_permanent_fact_valid(key, folded)) return false;
    bool exact = false;
    for (unsigned i = 0; i < journal.outgoing_count; ++i) {
        const auto& pending = journal.outgoing[i];
        if (!pc_permanent_producer_valid(pending.reward_id) ||
            !pc_permanent_operation_valid(pending.key, pending.operation, pending.value)) return false;
        // Even an exact duplicate source must not contribute another STEP.
        for (unsigned j = 0; j < i; ++j)
            if (journal.outgoing[j].reward_id == pending.reward_id) return false;
        if (pending.reward_id == claim.authored_reward_id) {
            if (pending.key != claim.key || pending.operation != claim.operation || pending.value != claim.value) return false;
            exact = true;
        }
        if (pending.key == key)
            folded = pc_permanent_operation_result(key, folded, pending.operation, pending.value);
    }
    const uint32_t value = effectiveFact(key, projection.facts.values[key]);
    if (!exact || value != effectiveFact(key, folded) || !pc_permanent_fact_valid(key, value)) return false;
    if (claim.operation == PC_PERMANENT_STEP) {
        if (value < pc_permanent_operation_result(key, 0, PC_PERMANENT_STEP, 1)) return false;
    } else if (value < claim.value) return false;
    *target = value;
    return true;
}

// A card read suspends an already authorized quest; it does not authorize one.
// Keep that original authorization through failed retries. Results from an old
// request or quest must never restore authorization to a different lifecycle.
class ReadAuthorization {
    bool authorized_ = false;
    bool suspended_ = false;
    bool beforeRead_ = false;
    uint64_t pending_ = 0;
public:
    bool authorized() const { return authorized_; }
    bool suspended() const { return suspended_; }
    void questReady() {
        authorized_ = true;
        suspended_ = beforeRead_ = false;
        pending_ = 0;
    }
    void questFailed() {
        authorized_ = suspended_ = beforeRead_ = false;
        pending_ = 0;
    }
    bool requested(uint64_t invocation) {
        if (!invocation) return false;
        if (!suspended_) beforeRead_ = authorized_;
        suspended_ = true;
        authorized_ = false;
        pending_ = invocation;
        return true;
    }
    bool result(uint64_t invocation, bool success) {
        if (!invocation || invocation != pending_ || !suspended_) return false;
        pending_ = 0;
        if (success) {
            authorized_ = beforeRead_;
            suspended_ = beforeRead_ = false;
        }
        return true;
    }
};

// Recovery additionally needs the hash from the exact accepted CARD read.
// Authorization alone can survive a failed retry; provenance cannot. A load
// consumes this evidence once, including when its metadata fails validation.
class ReadProvenance {
    uint64_t pending_ = 0, successful_ = 0;
    char hash_[65]{};
    static bool validHash(const char* hash) {
        if (!hash) return false;
        for (unsigned i = 0; i < 64; ++i)
            if (!((hash[i] >= '0' && hash[i] <= '9') ||
                  (hash[i] >= 'a' && hash[i] <= 'f'))) return false;
        return hash[64] == 0;
    }
public:
    void reset() { pending_ = successful_ = 0; hash_[0] = 0; }
    void requested(uint64_t invocation, const char* hash) {
        reset();
        if (!invocation || !validHash(hash)) return;
        pending_ = invocation;
        std::memcpy(hash_, hash, sizeof hash_);
    }
    bool result(uint64_t invocation, const char* hash, bool success) {
        if (!invocation || invocation != pending_) return false;
        pending_ = 0;
        if (!success || !validHash(hash) || std::strcmp(hash_, hash)) {
            reset();
            return false;
        }
        successful_ = invocation;
        return true;
    }
    bool consume(uint64_t invocation, const char* hash) {
        const bool matches = invocation && invocation == successful_ &&
            validHash(hash) && !std::strcmp(hash_, hash);
        reset();
        return matches;
    }
};

inline PcNetworkConfig joinConfig(const PcNetworkProfile& mounted, bool ownsHost) {
    PcNetworkConfig config = mounted.config;
    config.create_room = ownsHost;
    if (ownsHost) {
        // Always join this process's listener, including after a native load.
        // The public endpoint continues to define the immutable save route.
        std::strcpy(config.server, !std::strcmp(mounted.bind, "0.0.0.0")
            ? "127.0.0.1" : mounted.bind);
    }
    return config;
}
} // namespace pc::network
#endif
