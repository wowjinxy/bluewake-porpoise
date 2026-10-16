// SPDX-License-Identifier: GPL-3.0-or-later
// Tests the actual adapter's copied-value correlation policy; no mock game,
// invented item grant, SDK, socket, save file or native runtime is involved.
#include "pc/pc_progression_logic.h"
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
using pc::progression::Award;

void correlation() {
    Award award;
    CHECK(!award.ready(7, true)); // ownership alone is never an earned reward
    CHECK(!award.effectApplied(20, 7));
    CHECK(!award.messageCompleted(20, 7, 12));
    CHECK(!award.eventEnded(10, 7, 100));
    award.begin(10, 12, 7, 1);
    CHECK(award.same(10, 12, 7));
    CHECK(!award.same(10, 12, 8) && !award.same(11, 12, 7));
    CHECK(!award.effectApplied(20, 7)); // no normal item-message completion yet
    CHECK(!award.messageCompleted(20, 8, 12)); // stale player/scene/save epoch
    CHECK(!award.messageCompleted(20, 7, 13)); // unrelated native event
    CHECK(award.messageCompleted(20, 7, 12));
    CHECK(!award.messageCompleted(21, 7, 12)); // cannot replace the correlated item actor
    CHECK(!award.effectApplied(21, 7) && !award.effectApplied(20, 8));
    CHECK(award.effectApplied(20, 7));
    CHECK(!award.ready(7, true)); // ordinary actor deletion is insufficient
    CHECK(!award.eventEnded(11, 7, 100) && !award.eventEnded(10, 8, 100));
    CHECK(award.eventEnded(10, 7, 100));
    CHECK(!award.ready(7, false) && !award.ready(8, true));
    CHECK(award.ready(7, true));
    CHECK(!award.mustCancel(7, false, -1, 108)); // deletion/end may occur in adjacent frames
    CHECK(award.mustCancel(7, false, -1, 109));
    CHECK(award.mustCancel(8, true, 12, 100));
    award = {}; // the real adapter clears it immediately after one completion
    CHECK(!award.ready(7, true) && !award.effectApplied(20, 7));
    CHECK(!award.eventEnded(10, 7, 101));

    award.begin(10, 12, 8, 2);
    CHECK(award.mustCancel(8, false, -1, 100));
    CHECK(award.mustCancel(8, true, 13, 100));
    CHECK(!award.mustCancel(8, true, 12, 100));
    CHECK(award.messageCompleted(20, 8, 12));
    CHECK(award.eventEnded(10, 8, 100));
    CHECK(!award.ready(8, true));
    CHECK(award.effectApplied(20, 8)); // the inverse end/effect ordering is also legitimate
    CHECK(award.ready(8, true));

    // Song ownership is written at the accepted native GET_DANCE start, before
    // its message. Only that explicit producer path permits the inverse order.
    award.begin(10, 12, 9, 3);
    award.item = 20;
    CHECK(!award.messageCompleted(21, 9, 12));
    CHECK(!award.effectApplied(20, 9));
    award.effectBeforeMessage = true;
    CHECK(!award.effectApplied(21, 9));
    CHECK(award.effectApplied(20, 9));
    CHECK(award.eventEnded(10, 9, 100));
    CHECK(!award.ready(9, true));
    CHECK(award.messageCompleted(20, 9, 12));
    CHECK(award.ready(9, true));
    award.graceFrames = 120; // real queued max-magic effects may settle later
    CHECK(!award.mustCancel(9, false, -1, 220));
    CHECK(award.mustCancel(9, false, -1, 221));
}

void applyGate() {
    using pc::progression::applyReadiness;
    CHECK(applyReadiness(0, 1, 5, 5, true, false) == PC_PROGRESS_REJECTED);
    CHECK(applyReadiness(2, 1, 5, 5, true, false) == PC_PROGRESS_APPLIED);
    CHECK(applyReadiness(15, 1, 5, 5, true, false) == PC_PROGRESS_REJECTED); // bottle excluded
    CHECK(applyReadiness(1, 0, 5, 5, true, false) == PC_PROGRESS_REJECTED);
    CHECK(applyReadiness(1, 2, 5, 5, true, false) == PC_PROGRESS_REJECTED);
    CHECK(applyReadiness(1, 1, 4, 5, true, false) == PC_PROGRESS_STALE);
    CHECK(applyReadiness(1, 1, 4, 5, false, true) == PC_PROGRESS_STALE);
    CHECK(applyReadiness(1, 1, 5, 5, false, false) == PC_PROGRESS_DEFERRED);
    CHECK(applyReadiness(1, 1, 5, 5, false, true) == PC_PROGRESS_DEFERRED);
    CHECK(applyReadiness(1, 1, 5, 5, true, true) == PC_PROGRESS_ALREADY);
    CHECK(applyReadiness(1, 1, 5, 5, true, false) == PC_PROGRESS_APPLIED);
    CHECK(applyReadiness(9, 2, 5, 5, true, false) == PC_PROGRESS_APPLIED);
    CHECK(applyReadiness(13, 3, 5, 5, true, false) == PC_PROGRESS_APPLIED);
    CHECK(applyReadiness(13, 4, 5, 5, true, false) == PC_PROGRESS_REJECTED);
    CHECK(applyReadiness(49, 16, 5, 5, false, false) == PC_PROGRESS_DEFERRED);
    CHECK(applyReadiness(49, 31, 5, 5, true, false) == PC_PROGRESS_REJECTED);
    CHECK(applyReadiness(128, 1, 5, 5, true, false) == PC_PROGRESS_REJECTED);
    CHECK(applyReadiness(65537, 1, 5, 5, true, false) == PC_PROGRESS_REJECTED);
}

void correctionPolicy() {
    using pc::progression::canonicalCorrection;
    uint32_t value = 0;
    CHECK(canonicalCorrection(48, value) && value == 0);
    CHECK(!pc_permanent_fact_valid(48, value)); // never an accepted wire delta
    value = 0; CHECK(canonicalCorrection(50, value) && value == 30);
    value = 0; CHECK(canonicalCorrection(51, value) && value == 30);
    value = 0; CHECK(!canonicalCorrection(1, value));
    value = 1; CHECK(!canonicalCorrection(50, value));
    value = 60; CHECK(canonicalCorrection(50, value) && value == 60);
    value = 0; CHECK(!canonicalCorrection(65584, value)); // no narrowed key alias
    PcProgressClaim claim{};
    claim.operation = PC_PERMANENT_MAX;
    CHECK(!pc::progression::retainCancelledClaim(claim, false));
    CHECK(!pc::progression::retainCancelledClaim(claim, true)); // never strips/publishes a cancelled fixed tool
    claim.operation = PC_PERMANENT_STEP;
    CHECK(!pc::progression::retainCancelledClaim(claim, false)); // cancelled before effect releases save guard
    CHECK(pc::progression::retainCancelledClaim(claim, true)); // actual progressive effect needs reconciliation
}

void saveCorrelation() {
    using pc::progression::confirmsActiveQuestSave;
    PcProgressIdentity request{};
    request.session_id = 11; request.epoch = 5; request.card_serial = 15;
    request.quest_slot = 1; std::strcpy(request.profile_id, "room-profile");
    CHECK(!confirmsActiveQuestSave(false, 1, request, request)); // initial file creation/old buffer
    CHECK(!confirmsActiveQuestSave(true, 0, request, request)); // native busy is not committed
    CHECK(!confirmsActiveQuestSave(true, 2, request, request)); // native failure
    CHECK(confirmsActiveQuestSave(true, 1, request, request));
    auto current = request; ++current.epoch;
    CHECK(!confirmsActiveQuestSave(true, 1, request, current));
    current = request; ++current.session_id;
    CHECK(!confirmsActiveQuestSave(true, 1, request, current));
    current = request; ++current.card_serial;
    CHECK(!confirmsActiveQuestSave(true, 1, request, current));
    current = request; ++current.quest_generation;
    CHECK(!confirmsActiveQuestSave(true, 1, request, current)); // reload same slot invalidates old receipt
    current = request; current.quest_slot = 2;
    CHECK(!confirmsActiveQuestSave(true, 1, request, current));
    current = request; current.profile_id[0] = 'x';
    CHECK(!confirmsActiveQuestSave(true, 1, request, current));
    request.quest_slot = 255;
    CHECK(!confirmsActiveQuestSave(true, 1, request, request));

    request.quest_slot = 1;
    current = request; ++current.epoch; ++current.scene_id; ++current.player_id;
    CHECK(pc::progression::sameQuest(request, current)); // unresolved claim survives scene travel
    ++current.quest_generation;
    CHECK(!pc::progression::sameQuest(request, current)); // cannot correct a different quest
}

void localDurabilityPolicy() {
    using pc::progression::claimCanBeDurable;
    using pc::progression::claimPermitsSave;
    PcProgressIdentity current{};
    current.session_id = 12; current.epoch = 8; current.quest_generation = 3;
    current.quest_slot = 0; current.card_serial = 4;
    std::strcpy(current.profile_id, "room-profile");
    PcProgressClaim claim{};
    claim.identity = current;
    claim.invocation_id = 9;
    claim.authored_reward_id = (uint64_t(PC_PERMANENT_PRODUCER_FAIRY) << 56) | 25;
    claim.key = PC_PERMANENT_WALLET; claim.operation = PC_PERMANENT_STEP; claim.value = 1;
    CHECK(!claimCanBeDurable(claim, current, current.epoch)); // merely armed is not a durable reward
    CHECK(!claimPermitsSave(claim));
    claim.state = PC_PROGRESS_CLAIM_COMPLETED;
    CHECK(!claimCanBeDurable(claim, current, current.epoch)); // state alone does not prove native completion
    claim.completed_proof = 1;
    CHECK(claimCanBeDurable(claim, current, current.epoch));
    CHECK(!claimPermitsSave(claim)); // actual intent/outbox persistence must occur first
    claim.durably_queued = 1;
    CHECK(claimPermitsSave(claim)); // local durability suffices; no host ACK is invented
    CHECK(!claimCanBeDurable(claim, current, current.epoch - 1));
    auto moved = current; ++moved.epoch; ++moved.scene_id; ++moved.player_id;
    CHECK(claimCanBeDurable(claim, moved, moved.epoch)); // copied completed reward survives scene travel
    ++moved.quest_generation;
    CHECK(!claimCanBeDurable(claim, moved, moved.epoch));
    claim.state = PC_PROGRESS_CLAIM_CANCELLED_EFFECT;
    CHECK(!claimCanBeDurable(claim, current, current.epoch));
    CHECK(!claimPermitsSave(claim));
    claim.state = PC_PROGRESS_CLAIM_STALE;
    CHECK(!claimCanBeDurable(claim, current, current.epoch));
    CHECK(!claimPermitsSave(claim));
    claim.state = PC_PROGRESS_CLAIM_COMPLETED; claim.authored_reward_id = 0;
    CHECK(!claimCanBeDurable(claim, current, current.epoch));
    CHECK(!claimPermitsSave(claim));
    claim.authored_reward_id = (uint64_t(PC_PERMANENT_PRODUCER_EVENT) << 56) | 26;
    claim.key = PC_PERMANENT_BOW; claim.operation = PC_PERMANENT_MAX; claim.value = 2;
    CHECK(claimCanBeDurable(claim, current, current.epoch));
    CHECK(claimPermitsSave(claim)); // fixed ownership follows the same durable-outbox boundary
    claim.value = 4;
    CHECK(!claimCanBeDurable(claim, current, current.epoch));
    CHECK(!claimPermitsSave(claim));
    claim.key = 65549; claim.value = 2;
    CHECK(!claimCanBeDurable(claim, current, current.epoch));
}

void journalRetirementPolicy() {
    using pc::progression::journaledClaimMatches;
    using pc::progression::retireClaimOnActivation;
    PcProgressIdentity current{};
    current.session_id = 31; current.epoch = 7; current.quest_generation = 4;
    std::strcpy(current.profile_id, "one-mounted-profile");
    PcProgressClaim claim{};
    claim.identity = current; claim.invocation_id = 14;
    claim.authored_reward_id = (uint64_t(PC_PERMANENT_PRODUCER_FAIRY) << 56) | 42;
    claim.key = PC_PERMANENT_WALLET; claim.operation = PC_PERMANENT_STEP; claim.value = 1;
    const auto matches = [&](const PcProgressIdentity& active) {
        return journaledClaimMatches(claim, active, claim.invocation_id, claim.authored_reward_id,
            claim.key, claim.operation, claim.value);
    };
    claim.state = PC_PROGRESS_CLAIM_COMPLETED; claim.durably_queued = 1;
    CHECK(!matches(current)); // copied COMPLETE state without genuine producer proof is insufficient
    CHECK(!retireClaimOnActivation(claim, current, true));
    claim.completed_proof = 1;
    claim.state = PC_PROGRESS_CLAIM_ARMED;
    CHECK(!matches(current) && !retireClaimOnActivation(claim, current, true));
    claim.state = PC_PROGRESS_CLAIM_CANCELLED_EFFECT;
    CHECK(!matches(current) && !retireClaimOnActivation(claim, current, true));
    claim.state = PC_PROGRESS_CLAIM_COMPLETED;
    CHECK(matches(current));
    CHECK(!retireClaimOnActivation(claim, current, false)); // failed load/profile rebind is not trusted activation
    CHECK(retireClaimOnActivation(claim, current, true));
    claim.durably_queued = 0;
    CHECK(!retireClaimOnActivation(claim, current, true));
    CHECK(matches(current)); // caller has just durably enqueued this exact completed operation
    CHECK(!journaledClaimMatches(claim, current, 15, claim.authored_reward_id, claim.key, claim.operation, claim.value));
    CHECK(!journaledClaimMatches(claim, current, 14, claim.authored_reward_id + 1, claim.key, claim.operation, claim.value));
    CHECK(!journaledClaimMatches(claim, current, 14, claim.authored_reward_id, PC_PERMANENT_BOMB_MAX, claim.operation, claim.value));
    CHECK(!journaledClaimMatches(claim, current, 14, claim.authored_reward_id, claim.key, PC_PERMANENT_MAX, claim.value));
    CHECK(!journaledClaimMatches(claim, current, 14, claim.authored_reward_id, claim.key, claim.operation, 2));
    auto other = current; other.profile_id[0] = 'x';
    CHECK(!matches(other) && !retireClaimOnActivation(claim, other, true));
    other = current; ++other.session_id;
    CHECK(!matches(other));
    other = current; ++other.quest_generation; ++other.epoch; other.quest_slot = 1;
    claim.state = PC_PROGRESS_CLAIM_STALE;
    CHECK(matches(other)); // completion queued before a same-profile activation may be journaled afterward
    claim.durably_queued = 1;
    CHECK(retireClaimOnActivation(claim, other, true));
    CHECK(!retireClaimOnActivation(claim, other, false));
    claim.completed_proof = 0;
    CHECK(!matches(other) && !retireClaimOnActivation(claim, other, true));
    claim.completed_proof = 1; claim.authored_reward_id = 0;
    CHECK(!matches(other) && !retireClaimOnActivation(claim, other, true));
}
} // namespace

int main() {
    correlation(); applyGate(); correctionPolicy(); saveCorrelation(); localDurabilityPolicy(); journalRetirementPolicy();
    std::printf("native progression correlation: %u checks passed (no gameplay/save proof)\n", checks);
    return 0;
}
