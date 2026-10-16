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
}

void applyGate() {
    using pc::progression::applyReadiness;
    CHECK(applyReadiness(0, 1, 5, 5, true, false) == PC_PROGRESS_REJECTED);
    CHECK(applyReadiness(2, 1, 5, 5, true, false) == PC_PROGRESS_REJECTED);
    CHECK(applyReadiness(1, 0, 5, 5, true, false) == PC_PROGRESS_REJECTED);
    CHECK(applyReadiness(1, 2, 5, 5, true, false) == PC_PROGRESS_REJECTED);
    CHECK(applyReadiness(1, 1, 4, 5, true, false) == PC_PROGRESS_STALE);
    CHECK(applyReadiness(1, 1, 4, 5, false, true) == PC_PROGRESS_STALE);
    CHECK(applyReadiness(1, 1, 5, 5, false, false) == PC_PROGRESS_DEFERRED);
    CHECK(applyReadiness(1, 1, 5, 5, false, true) == PC_PROGRESS_DEFERRED);
    CHECK(applyReadiness(1, 1, 5, 5, true, true) == PC_PROGRESS_ALREADY);
    CHECK(applyReadiness(1, 1, 5, 5, true, false) == PC_PROGRESS_APPLIED);
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
    current = request; current.quest_slot = 2;
    CHECK(!confirmsActiveQuestSave(true, 1, request, current));
    current = request; current.profile_id[0] = 'x';
    CHECK(!confirmsActiveQuestSave(true, 1, request, current));
    request.quest_slot = 255;
    CHECK(!confirmsActiveQuestSave(true, 1, request, request));
}
} // namespace

int main() {
    correlation(); applyGate(); saveCorrelation();
    std::printf("native progression correlation: %u checks passed (no gameplay/save proof)\n", checks);
    return 0;
}
