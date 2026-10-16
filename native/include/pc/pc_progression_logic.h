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
            (message && item != actor)) return false;
        item = actor; message = true;
        return true;
    }
    bool effectApplied(uint32_t actor, uint64_t currentEpoch) {
        if (!active || !message || actor != item || epoch != currentEpoch) return false;
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
            (ended ? frame - endedFrame > 8 : !eventRunning || currentEvent != event));
    }
};

inline int applyReadiness(uint32_t key, uint32_t value, uint64_t expectedEpoch,
                          uint64_t currentEpoch, bool safe, bool owned) {
    if (key != PC_PROGRESS_FACT_TELESCOPE || value != 1) return PC_PROGRESS_REJECTED;
    if (expectedEpoch != currentEpoch) return PC_PROGRESS_STALE;
    if (!safe) return PC_PROGRESS_DEFERRED;
    return owned ? PC_PROGRESS_ALREADY : PC_PROGRESS_APPLIED;
}

inline bool confirmsActiveQuestSave(bool prepared, int nativeResult,
                                    const PcProgressIdentity& request,
                                    const PcProgressIdentity& current) {
    return prepared && nativeResult == 1 && request.session_id == current.session_id &&
           request.epoch == current.epoch && request.card_serial == current.card_serial &&
           request.quest_slot == current.quest_slot && request.quest_slot >= 0 && request.quest_slot <= 2 &&
           std::memcmp(request.profile_id, current.profile_id, sizeof(request.profile_id)) == 0;
}
} // namespace pc::progression
#endif
