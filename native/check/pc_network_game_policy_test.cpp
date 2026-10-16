// SPDX-License-Identifier: GPL-3.0-or-later
#include "pc/pc_network_game_policy.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { \
    std::fprintf(stderr, "network game policy: FAIL line %d: %s\n", __LINE__, #value); \
    std::exit(1); } } while (0)

static void localProjectionPolicy() {
    using pc::network::localClaimProjectionValue;
    auto journal = std::make_unique<PcNetworkClientState>(); // bounded ledger stays off Windows' default stack
    journal->canonical_present = true; journal->serial = 19; journal->revision = 2;
    std::memset(journal->profile_identity, 'a', 64);
    std::memset(journal->room_identity, 'b', 32);
    PcProgressIdentity current{};
    current.session_id = 7; current.epoch = 12; current.quest_generation = 4;
    current.card_serial = 8; current.quest_slot = 1;
    std::memcpy(current.profile_id, journal->profile_identity, sizeof current.profile_id);
    PcProgressClaim claim{};
    claim.identity = current; claim.invocation_id = 33;
    claim.authored_reward_id = (uint64_t(PC_PERMANENT_PRODUCER_FAIRY) << 56) | 17;
    claim.key = PC_PERMANENT_WALLET; claim.operation = PC_PERMANENT_STEP; claim.value = 1;
    claim.state = PC_PROGRESS_CLAIM_COMPLETED; claim.completed_proof = claim.durably_queued = 1;
    journal->outgoing_count = 1;
    journal->outgoing[0] = {claim.authored_reward_id, uint16_t(claim.key), uint16_t(claim.operation), claim.value};
    PcNetworkClientProjection projection{};
    projection.serial = journal->serial; projection.revision = journal->revision; projection.provisional = true;
    std::memcpy(projection.room_identity, journal->room_identity, sizeof projection.room_identity);
    projection.facts.values[claim.key] = 1;
    uint32_t target = 777;
    const auto ready = [&] { return localClaimProjectionValue(claim, current, current.epoch, *journal, projection, &target); };
    const uint64_t serial = journal->serial, revision = journal->revision;
    const auto pending = journal->outgoing[0];
    CHECK(ready() && target == 1); // native wallet2 can reconcile to ONE locally durable source
    CHECK(ready() && target == 1); // already-owned target follows the same gate; native apply decides ALREADY
    CHECK(journal->serial == serial && journal->revision == revision && journal->outgoing_count == 1 &&
          journal->reward_count == 0 && journal->outgoing[0].reward_id == pending.reward_id);
    CHECK(!localClaimProjectionValue(claim, current, current.epoch, *journal, projection, nullptr));
    CHECK(!localClaimProjectionValue(claim, current, current.epoch - 1, *journal, projection, &target));
    claim.completed_proof = 0; CHECK(!ready()); claim.completed_proof = 1;
    claim.durably_queued = 0; CHECK(!ready()); claim.durably_queued = 1;
    const uint32_t rejectedStates[] = {PC_PROGRESS_CLAIM_ARMED, PC_PROGRESS_CLAIM_CANCELLED_EFFECT, PC_PROGRESS_CLAIM_STALE};
    for (uint32_t state : rejectedStates) {
        claim.state = state; CHECK(!ready());
    }
    claim.state = PC_PROGRESS_CLAIM_COMPLETED;
    auto wrong = current; ++wrong.quest_slot;
    CHECK(!localClaimProjectionValue(claim, wrong, wrong.epoch, *journal, projection, &target));
    wrong = current; ++wrong.card_serial; CHECK(!localClaimProjectionValue(claim, wrong, wrong.epoch, *journal, projection, &target));
    wrong = current; ++wrong.session_id; CHECK(!localClaimProjectionValue(claim, wrong, wrong.epoch, *journal, projection, &target));
    wrong = current; ++wrong.quest_generation; CHECK(!localClaimProjectionValue(claim, wrong, wrong.epoch, *journal, projection, &target));
    wrong = current; wrong.profile_id[0] = 'c'; CHECK(!localClaimProjectionValue(claim, wrong, wrong.epoch, *journal, projection, &target));
    journal->profile_identity[0] = 'c'; CHECK(!ready()); journal->profile_identity[0] = 'a';
    journal->canonical_present = false; CHECK(!ready()); journal->canonical_present = true;
    journal->serial = 0; CHECK(!ready()); journal->serial = serial;
    ++projection.serial; CHECK(!ready()); --projection.serial;
    ++projection.revision; CHECK(!ready()); --projection.revision;
    projection.provisional = false; CHECK(!ready()); projection.provisional = true;
    projection.room_identity[0] = 'c'; CHECK(!ready()); projection.room_identity[0] = 'b';
    journal->room_identity[0] = 0; CHECK(!ready()); journal->room_identity[0] = 'b';
    journal->outgoing_count = 0; CHECK(!ready()); journal->outgoing_count = 1;
    ++journal->outgoing[0].reward_id; CHECK(!ready()); journal->outgoing[0] = pending;
    journal->outgoing[0].key = PC_PERMANENT_BOMB_MAX; CHECK(!ready()); journal->outgoing[0] = pending;
    journal->outgoing[0].operation = PC_PERMANENT_MAX; CHECK(!ready()); journal->outgoing[0] = pending;
    journal->outgoing[0].value = 2; CHECK(!ready()); journal->outgoing[0] = pending;
    projection.facts.values[claim.key] = 0; CHECK(!ready());
    projection.facts.values[claim.key] = 2; CHECK(!ready()); // valid rank, wrong projection for this one source
    projection.facts.values[claim.key] = 1;
    journal->outgoing_count = 2; journal->outgoing[1] = pending;
    CHECK(!ready()); // repeated source cannot ratchet a local projection either
    journal->outgoing[1].reward_id += 1; projection.facts.values[claim.key] = 2;
    CHECK(ready() && target == 2); // a second DISTINCT source legitimately contributes another tier
    journal->outgoing_count = 1; journal->facts.values[claim.key] = 2;
    CHECK(ready() && target == 2); // saturation still retains the pending exact operation
    journal->facts.values[claim.key] = 0;
    projection.facts = journal->facts;
    claim.key = PC_PERMANENT_ARROW_MAX; journal->outgoing[0].key = uint16_t(claim.key);
    projection.facts.values[claim.key] = 60; CHECK(ready() && target == 60); // absent canonical capacity means base30
    projection.facts.values[claim.key] = 30; CHECK(!ready());
    projection.facts.values[claim.key] = 99; CHECK(!ready()); // duplicated native effect is not another durable operation
    journal->facts.values[claim.key] = 60; CHECK(ready() && target == 99);
    projection.facts = journal->facts;
    claim.key = PC_PERMANENT_BOW; claim.operation = PC_PERMANENT_MAX; claim.value = 2;
    journal->outgoing[0] = {claim.authored_reward_id, uint16_t(claim.key), uint16_t(claim.operation), claim.value};
    projection.facts.values[claim.key] = 2; CHECK(ready() && target == 2);
    projection.facts.values[claim.key] = 1; CHECK(!ready());
    target = 777; projection.facts.values[claim.key] = 4; CHECK(!ready() && target == 777);
}

int main() {
    localProjectionPolicy();
    CHECK(pc::network::effectiveFact(PC_PERMANENT_ARROW_MAX, 0) == 30);
    CHECK(pc::network::effectiveFact(PC_PERMANENT_BOMB_MAX, 0) == 30);
    CHECK(pc::network::effectiveFact(PC_PERMANENT_ARROW_MAX, 60) == 60);
    CHECK(pc::network::effectiveFact(PC_PERMANENT_WALLET, 0) == 0);
    using pc::network::ReadAuthorization;
    ReadAuthorization unowned;
    CHECK(!unowned.authorized());
    CHECK(!unowned.requested(0));
    CHECK(unowned.requested(1));
    CHECK(unowned.result(1, true));
    CHECK(!unowned.authorized()); // successful I/O is not an actual quest load
    CHECK(!unowned.suspended());

    ReadAuthorization quest;
    quest.questReady();
    CHECK(quest.authorized());
    CHECK(quest.requested(2));
    CHECK(!quest.authorized());
    CHECK(quest.suspended());
    CHECK(quest.result(2, false));
    CHECK(!quest.authorized());
    CHECK(quest.suspended());
    CHECK(quest.requested(3));
    CHECK(!quest.authorized());
    CHECK(!quest.result(2, true)); // delayed completion of the failed request
    CHECK(!quest.authorized());
    CHECK(quest.result(3, true));
    CHECK(quest.authorized()); // a retry restores the original authorization
    CHECK(!quest.suspended());
    CHECK(!quest.result(3, false)); // duplicate failure cannot poison success
    CHECK(quest.authorized());

    CHECK(quest.requested(4));
    CHECK(quest.result(4, false));
    CHECK(quest.requested(5));
    CHECK(quest.result(5, false));
    CHECK(quest.requested(6));
    CHECK(quest.result(6, true));
    CHECK(quest.authorized()); // more than one failed native retry

    CHECK(quest.requested(7));
    quest.questReady(); // another quest revokes the previous request token
    CHECK(!quest.result(7, false));
    CHECK(!quest.result(7, true));
    CHECK(quest.authorized());
    CHECK(!quest.suspended());
    CHECK(quest.requested(8));
    CHECK(quest.requested(9));
    CHECK(!quest.result(8, true));
    CHECK(!quest.authorized());
    CHECK(quest.result(9, true));
    CHECK(quest.authorized());
    CHECK(quest.requested(10));
    quest.questFailed();
    CHECK(!quest.authorized());
    CHECK(!quest.result(10, true));
    CHECK(quest.requested(11));
    CHECK(quest.result(11, true));
    CHECK(!quest.authorized()); // physical I/O cannot authorize a failed load
    quest.questReady();
    CHECK(quest.authorized());

    using pc::network::ReadProvenance;
    ReadProvenance source;
    char hashA[65], hashB[65];
    std::memset(hashA, 'a', 64); hashA[64] = 0;
    std::memset(hashB, 'b', 64); hashB[64] = 0;
    CHECK(!source.consume(1, hashA));
    source.requested(1, hashA);
    CHECK(!source.consume(1, hashA)); // request alone proves no successful I/O
    source.requested(2, hashA);
    CHECK(source.result(2, hashA, true));
    CHECK(source.consume(2, hashA));
    CHECK(!source.consume(2, hashA)); // each actual load consumes once
    source.requested(3, hashA);
    CHECK(source.result(3, hashA, true));
    source.requested(4, hashB); // a new attempt revokes the old successful read
    CHECK(!source.result(3, hashA, true));
    CHECK(!source.result(4, hashB, false));
    CHECK(!source.consume(3, hashA));
    source.requested(5, hashA);
    CHECK(!source.result(5, hashB, true)); // completion cannot replace prehash
    CHECK(!source.consume(5, hashA));
    source.requested(6, hashA);
    CHECK(source.result(6, hashA, true));
    CHECK(!source.consume(6, hashB));
    CHECK(!source.consume(6, hashA)); // mismatched load also consumes proof
    source.requested(7, hashA);
    CHECK(source.result(7, hashA, true));
    source.reset(); // actual quest init or failed load revokes recovery proof
    CHECK(!source.consume(7, hashA));
    source.requested(8, "");
    CHECK(!source.result(8, hashA, true));
    source.requested(0, hashA);
    CHECK(!source.result(0, hashA, true));
    source.requested(9, hashA);
    CHECK(!source.result(8, hashB, false)); // old failure cannot poison new read
    CHECK(source.result(9, hashA, true));
    CHECK(!source.result(9, hashA, false)); // duplicate result cannot replace it
    CHECK(source.consume(9, hashA));

    PcNetworkProfile profile{};
    std::strcpy(profile.config.server, "192.0.2.10");
    std::strcpy(profile.config.room, "unchanged-room");
    profile.config.port = 49383;
    std::strcpy(profile.bind, "127.0.0.2");
    const PcNetworkProfile before = profile;
    const auto host = pc::network::joinConfig(profile, true);
    CHECK(std::strcmp(host.server, "127.0.0.2") == 0);
    CHECK(host.create_room);
    CHECK(host.port == profile.config.port);
    CHECK(std::strcmp(host.room, profile.config.room) == 0);
    CHECK(std::memcmp(&before, &profile, sizeof profile) == 0);
    // The exact helper is used for initial Host and native quest-load rejoin.
    const auto reload = pc::network::joinConfig(profile, true);
    CHECK(std::strcmp(reload.server, host.server) == 0);
    const auto client = pc::network::joinConfig(profile, false);
    CHECK(std::strcmp(client.server, "192.0.2.10") == 0);
    CHECK(!client.create_room);
    std::strcpy(profile.bind, "0.0.0.0");
    const auto wildcard = pc::network::joinConfig(profile, true);
    CHECK(std::strcmp(wildcard.server, "127.0.0.1") == 0);
    CHECK(wildcard.create_room);
    std::printf("native network game policy: %u checks passed (no gameplay/I-O proof)\n", checks);
    return 0;
}
