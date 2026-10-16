// SPDX-License-Identifier: GPL-3.0-or-later
// Game-free correlation rules used by the real native producer adapter.
#ifndef PC_PROGRESSION_LOGIC_H
#define PC_PROGRESSION_LOGIC_H
#include "pc/pc_progression.h"
#include <cstring>

namespace pc::progression {
struct Award {
    bool active = false, message = false, effect = false, ended = false;
    uint32_t producer = 0, item = UINT32_MAX;
    int32_t event = -1;
    uint64_t epoch = 0, invocation = 0;
    unsigned endedFrame = 0;
    unsigned graceFrames = 8;
    bool effectBeforeMessage = false;

    bool same(uint32_t who, int32_t nativeEvent, uint64_t currentEpoch) const {
        return active && producer == who && event == nativeEvent && epoch == currentEpoch;
    }
    void begin(uint32_t who, int32_t nativeEvent, uint64_t currentEpoch, uint64_t id) {
        *this = {};
        active = true; producer = who; event = nativeEvent;
        epoch = currentEpoch; invocation = id;
    }
    bool messageCompleted(uint32_t actor, uint64_t currentEpoch, int32_t currentEvent) {
        if (!active || epoch != currentEpoch || event != currentEvent ||
            (item != UINT32_MAX && item != actor)) return false;
        item = actor; message = true;
        return true;
    }
    bool effectApplied(uint32_t actor, uint64_t currentEpoch) {
        if (!active || (!message && !effectBeforeMessage) || actor != item || epoch != currentEpoch) return false;
        effect = true;
        return true;
    }
    bool eventEnded(uint32_t who, uint64_t currentEpoch, unsigned frame) {
        if (!active || producer != who || epoch != currentEpoch) return false;
        ended = true; endedFrame = frame;
        return true;
    }
    bool ready(uint64_t currentEpoch, bool nativeOwned) const {
        return active && message && effect && ended && epoch == currentEpoch && nativeOwned;
    }
    bool mustCancel(uint64_t currentEpoch, bool eventRunning, int32_t currentEvent, unsigned frame) const {
        return active && (epoch != currentEpoch ||
            (ended ? frame - endedFrame > graceFrames : !eventRunning || currentEvent != event));
    }
};

inline bool sameQuest(const PcProgressIdentity& a, const PcProgressIdentity& b) {
    return a.session_id == b.session_id && a.quest_generation == b.quest_generation &&
           a.card_serial == b.card_serial && a.quest_slot == b.quest_slot &&
           std::memcmp(a.profile_id, b.profile_id, sizeof(a.profile_id)) == 0;
}

inline bool retainCancelledClaim(const PcProgressClaim& claim, bool effectApplied) {
    // Fixed ownership is never rolled back or published as a cancelled reward.
    // Only a progressive effect needs canonical correction before saving.
    return effectApplied && claim.operation == PC_PERMANENT_STEP;
}

inline bool completedClaimValid(const PcProgressClaim& claim) {
    return claim.completed_proof &&
           (claim.state == PC_PROGRESS_CLAIM_COMPLETED || claim.state == PC_PROGRESS_CLAIM_STALE) &&
           pc_permanent_producer_valid(claim.authored_reward_id) &&
           claim.key <= UINT16_MAX && claim.operation <= UINT16_MAX &&
           pc_permanent_operation_valid(static_cast<uint16_t>(claim.key),
               static_cast<uint16_t>(claim.operation), claim.value);
}

inline bool sameProfile(const PcProgressIdentity& a, const PcProgressIdentity& b) {
    return a.session_id == b.session_id &&
           std::memcmp(a.profile_id, b.profile_id, sizeof(a.profile_id)) == 0;
}

inline bool journaledClaimMatches(const PcProgressClaim& claim, const PcProgressIdentity& current,
    uint64_t invocation, uint64_t authored, uint32_t key, uint32_t operation, uint32_t value) {
    return completedClaimValid(claim) && sameProfile(claim.identity, current) &&
           claim.invocation_id == invocation && claim.authored_reward_id == authored &&
           claim.key == key && claim.operation == operation && claim.value == value;
}

inline bool retireClaimOnActivation(const PcProgressClaim& claim,
                                   const PcProgressIdentity& current, bool trusted) {
    return trusted && claim.durably_queued && completedClaimValid(claim) &&
           sameProfile(claim.identity, current);
}

inline bool claimCanBeDurable(const PcProgressClaim& claim,
                              const PcProgressIdentity& current, uint64_t expectedEpoch) {
    return current.epoch == expectedEpoch && sameQuest(claim.identity, current) &&
           claim.state == PC_PROGRESS_CLAIM_COMPLETED && completedClaimValid(claim);
}

inline bool claimPermitsSave(const PcProgressClaim& claim) {
    return claim.state == PC_PROGRESS_CLAIM_COMPLETED && claim.durably_queued &&
           completedClaimValid(claim);
}

// Zero is never a wire award. Only receipt-backed progressive correction may
// represent the original wallet tier or normalize absent ammo capacity to30.
inline bool canonicalCorrection(uint32_t key, uint32_t& value) {
    if (key > UINT16_MAX) return false;
    if (pc_permanent_server_authoritative(static_cast<uint16_t>(key)) && value == 0) {
        if (key != PC_PERMANENT_WALLET) value = 30;
        return true;
    }
    return pc_permanent_fact_valid(static_cast<uint16_t>(key), value);
}

inline int applyReadiness(uint32_t key, uint32_t value, uint64_t expectedEpoch,
                          uint64_t currentEpoch, bool safe, bool owned) {
    if (key > UINT16_MAX || !pc_permanent_fact_valid((uint16_t)key, value)) return PC_PROGRESS_REJECTED;
    if (expectedEpoch != currentEpoch) return PC_PROGRESS_STALE;
    if (!safe) return PC_PROGRESS_DEFERRED;
    return owned ? PC_PROGRESS_ALREADY : PC_PROGRESS_APPLIED;
}

inline bool confirmsActiveQuestSave(bool prepared, int nativeResult,
                                    const PcProgressIdentity& request,
                                    const PcProgressIdentity& current) {
    return prepared && nativeResult == 1 && sameQuest(request, current) &&
           request.epoch == current.epoch && request.quest_slot >= 0 && request.quest_slot <= 2;
}
} // namespace pc::progression
#endif
