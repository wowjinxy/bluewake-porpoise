#include "pc/pc_network_game.h"
#include "pc/pc_network_game_policy.h"
#include "pc/pc_progression_logic.h"
#include "pc/network/pc_network_client_store.h"
#include "pc_internal.h"
#include "JSystem/JKernel/JKRHeap.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>
#include <deque>
#include <iterator>
#include <string>

namespace {
struct State {
    PcNetworkProfile mounted{}, next{};
    PcNetworkGameSnapshot view{};
    PcNetworkSession* session = nullptr;
    PcNetworkServer* server = nullptr;
    PcNetworkClientStore* clientStore = nullptr;
    PcNetworkClientState clientState{};
    PcNetworkClientProjection projection{};
    bool projectionValid = false;
    std::string preferences, root;
    PcNetworkFacts canonical{}, remoteNeeded{}, seedBaseline{};
    std::array<bool, PC_NETWORK_FACTS> remoteRequired{};
    std::array<PcNetworkReward, PC_NETWORK_REWARDS> rewards{};
    unsigned rewardCount = 0, reseedKey = 1;
    uint64_t generation = 0, revision = 0;
    bool prepared = false;
    bool baselineValid = false;
    uint64_t baselineGeneration = 0;
    bool wantConnection = false;
    pc::network::ReadAuthorization readAuthorization;
    pc::network::ReadProvenance readProvenance;
    PcProgressIdentity remoteIdentity{};
    uint64_t questActivation = 0, remoteActivation = 0;
    PcNetworkStatus lastStatus = PC_NETWORK_OFFLINE;
    struct Pending { PcNetworkReward reward; bool submitted = false; };
    std::deque<Pending> outgoing;
} s;
void error(char* out, unsigned cap, const char* message) {
    if (out && cap) std::snprintf(out, cap, "%s", message);
}
void say(const char* message) { std::snprintf(s.view.message, sizeof s.view.message, "%s", message); }
bool explicit_value(const char* key) { const char* v = std::getenv(key); return v && *v; }
void fail(const char* message) { s.view.failed = true; say(message); }
void invalidateCanonical() {
    s.canonical = {};
    s.generation = s.revision = 0;
    s.rewardCount = 0;
    s.reseedKey = 1;
    s.projectionValid = false;
    // These are acknowledged durable facts, still usable after host loss.
    // The provisional outbox projection remains separate from this ledger.
    if (s.clientStore && pc_network_client_copy(s.clientStore, &s.clientState) &&
        s.clientState.canonical_present) {
        s.canonical = s.clientState.facts;
        s.revision = s.clientState.revision;
        s.rewardCount = s.clientState.reward_count;
        std::copy(s.clientState.rewards, s.clientState.rewards + s.rewardCount, s.rewards.begin());
    }
}
const PcNetworkReward* knownReward(uint64_t id) {
    const auto end = s.rewards.begin() + s.rewardCount;
    const auto found = std::lower_bound(s.rewards.begin(), end, id,
        [](const PcNetworkReward& reward, uint64_t value) { return reward.reward_id < value; });
    return found != end && found->reward_id == id ? &*found : nullptr;
}
bool sameReward(const PcNetworkReward& a, const PcNetworkReward& b) {
    return a.reward_id == b.reward_id && a.key == b.key &&
        a.operation == b.operation && a.value == b.value;
}
bool enqueueReward(PcNetworkReward reward) {
    if (!pc_network_reward_valid(reward)) {
        say("This reward is not yet supported for sharing.");
        return false;
    }
    for (const auto& pending : s.outgoing) if (pending.reward.reward_id == reward.reward_id) {
        if (!sameReward(pending.reward, reward)) fail("A shared reward has conflicting outcomes; sharing stopped.");
        return sameReward(pending.reward, reward);
    }
    if (s.outgoing.size() >= PC_NETWORK_QUEUE) {
        fail("Too many unconfirmed rewards; reconnect and resolve progress before continuing.");
        return false;
    }
    char reason[192]{};
    if (!s.clientStore || !pc_network_client_enqueue(s.clientStore, reward, reason, sizeof reason)) {
        fail(reason[0] ? reason : "Cannot preserve this room reward; sharing stopped before submission.");
        return false;
    }
    s.outgoing.push_back({reward, false});
    return true;
}
bool pendingOperationFor(unsigned key) {
    return std::any_of(s.outgoing.begin(), s.outgoing.end(),
        [key](const State::Pending& pending) { return pending.reward.key == key; });
}
void receivedEffect(unsigned key, uint32_t value, bool localRecovery = false) {
    if (!localRecovery) ++s.view.remote_applies;
    s.view.unsaved_remote = true;
    s.remoteNeeded.values[key] = value;
    s.remoteRequired[key] = true;
    s.remoteIdentity = s.view.progress.identity;
    s.remoteActivation = s.questActivation;
    say(localRecovery ? "Local room progress recovered. Save through the game's save screen; host confirmation is still pending."
        : "Shared progress received. Save through the game's save screen to keep it offline.");
}
bool savedReceivedFacts(const PcNativeEvent& event) {
    if (!event.saved_facts_valid || event.identity.session_id != s.remoteIdentity.session_id ||
        s.questActivation != s.remoteActivation ||
        event.identity.quest_generation != s.remoteIdentity.quest_generation ||
        event.identity.quest_slot != s.remoteIdentity.quest_slot ||
        std::strcmp(event.identity.profile_id, s.remoteIdentity.profile_id) ||
        event.identity.save_generation <= s.remoteIdentity.save_generation) return false;
    for (unsigned key = 1; key < PC_NETWORK_FACTS; ++key) if (s.remoteRequired[key]) {
        const uint32_t saved = event.saved_facts[key], needed = s.remoteNeeded.values[key];
        if (pc_permanent_server_authoritative(static_cast<uint16_t>(key))
            ? saved != needed : saved < needed) return false;
    }
    return true;
}
uint32_t projectedClaimValue(unsigned key) {
    return pc::network::effectiveFact(static_cast<uint16_t>(key),
        (s.projectionValid ? s.projection.facts : s.canonical).values[key]);
}
int reconcileJournaledClaim(const PcProgressClaim& original, uint32_t* target) {
    // Enqueue may have changed journal serial or activated another quest.
    // Refresh both copied owners before considering local correlation release.
    pc_progress_capture(&s.view.progress);
    PcProgressClaim current[PC_PROGRESS_MAX_PENDING_CLAIMS]{};
    const unsigned count = pc_progress_copy_claims(current, PC_PROGRESS_MAX_PENDING_CLAIMS);
    if (count > PC_PROGRESS_MAX_PENDING_CLAIMS || !s.clientStore ||
        !pc_network_client_copy(s.clientStore, &s.clientState)) return PC_PROGRESS_DEFERRED;
    char reason[192]{};
    s.projectionValid = pc_network_client_projection(s.clientStore, &s.projection, reason, sizeof reason);
    if (!s.projectionValid) return PC_PROGRESS_DEFERRED;
    for (unsigned i = 0; i < count; ++i) if (current[i].invocation_id == original.invocation_id &&
        current[i].authored_reward_id == original.authored_reward_id && current[i].key == original.key &&
        current[i].operation == original.operation && current[i].value == original.value) {
        if (!pc::network::localClaimProjectionValue(current[i], s.view.progress.identity,
            s.view.progress.identity.epoch, s.clientState, s.projection, target)) return PC_PROGRESS_DEFERRED;
        // This releases only the native presentation correlation. The exact
        // durable outgoing operation still needs its actual host receipt.
        return pc_progress_resolve_claim(current[i].invocation_id, current[i].authored_reward_id,
            current[i].key, *target, s.view.progress.identity.epoch);
    }
    return PC_PROGRESS_STALE;
}
void pendingView() {
    s.view.pending_local = !s.outgoing.empty() || s.view.progress.pending_claims != 0;
    s.view.pending_remote = false;
    for (unsigned key = 1; key < PC_NETWORK_FACTS; ++key) {
        const auto value = (s.projectionValid ? s.projection.facts : s.canonical).values[key];
        const auto owned = pc::network::effectiveFact(static_cast<uint16_t>(key), s.view.progress.facts[key]);
        if (value && !pc::progression::factSettled(key, value, owned,
            s.view.progress.bracelets_equipped != 0)) {
            s.view.pending_remote = true;
            break;
        }
    }
    s.view.authorized = s.readAuthorization.authorized();
}
}

extern "C" bool pc_network_game_prepare(const char* user, char* out, unsigned cap) {
    JKRPcHostAllocScope host;
    if (s.prepared || !user || !*user) { error(out, cap, "Network profile startup called twice or without user path"); return false; }
    s.preferences = explicit_value("COS_NETWORK_PROFILE") ? std::getenv("COS_NETWORK_PROFILE") : std::string(user) + "/network.ini";
    s.root = explicit_value("COS_NETWORK_ROOT") ? std::getenv("COS_NETWORK_ROOT") : std::string(user) + "/NetworkNative";
    if (!pc_network_profile_load(s.preferences.c_str(), explicit_value("COS_NETWORK_PROFILE"), &s.mounted, out, cap)) return false;
    if (s.mounted.enabled && explicit_value("COS_CARD_DIR")) {
        error(out, cap, "Room mode refuses COS_CARD_DIR; use the isolated room profile route"); return false;
    }
    const char* personal = explicit_value("COS_CARD_DIR") ? std::getenv("COS_CARD_DIR") : user;
    if (!pc_network_profile_prepare(&s.mounted, s.root.c_str(), personal, out, cap)) return false;
    if (!pc_progress_bind_profile(pc_network_profile_mounted_identity())) {
        error(out, cap, "Native progression profile binding failed"); return false;
    }
    if (s.mounted.enabled) {
        // The journal owns its own bounded directory, separate from native
        // CARD contents and the mounted profile's identity/lease files.
        const std::string clientDirectory = std::string(pc_network_profile_mounted_card_base()) + "/client-state";
        s.clientStore = pc_network_client_open(clientDirectory.c_str(),
            &s.mounted.config.compatibility, pc_network_profile_mounted_identity(), out, cap);
        if (!s.clientStore || !pc_network_client_copy(s.clientStore, &s.clientState)) {
            if (s.clientStore) { pc_network_client_close(s.clientStore); s.clientStore = nullptr; }
            if (out && cap && !*out) error(out, cap, "Cannot reopen the isolated room reward journal");
            return false;
        }
        for (unsigned i = 0; i < s.clientState.outgoing_count; ++i)
            s.outgoing.push_back({s.clientState.outgoing[i], false});
        invalidateCanonical();
    }
    s.prepared = s.view.prepared = true;
    s.view.room_mode = s.mounted.enabled;
    s.next = s.mounted;
    say(s.mounted.enabled ? "Room saves mounted. Host or Join to connect; save progress through the game's save screen." : "Personal saves. Configure a room and restart before connecting.");
    error(out, cap, "");
    return true;
}
extern "C" const char* pc_network_game_card_base() { return pc_network_profile_mounted_card_base(); }
extern "C" bool pc_network_game_room_mode() { return s.prepared && s.mounted.enabled; }
extern "C" bool pc_network_game_profile(PcNetworkProfile* copy) {
    if (!copy || !s.prepared) return false;
    *copy = s.next; return true;
}
extern "C" bool pc_network_game_save_profile(const PcNetworkProfile* next, char* out, unsigned cap) {
    JKRPcHostAllocScope host;
    if (!s.prepared || !next) { error(out, cap, "Network profile is not prepared"); return false; }
    if (!pc_network_profile_save(s.preferences.c_str(), next, out, cap)) return false;
    s.next = *next;
    s.view.restart_pending = true;
    say("Room settings saved for the next start. Restart to mount the selected saves.");
    return true;
}
extern "C" int pc_network_game_native_save_prepare(const PcProgressIdentity* identity,
    const uint32_t frozen[PC_PERMANENT_FACT_COUNT], uint64_t* token) {
    if (token) *token = 0;
    if (!s.prepared || !s.mounted.enabled) return 1;
    JKRPcHostAllocScope host;
    if (!identity || !frozen || !token || !s.clientStore || s.view.failed ||
        identity->quest_slot < 0 || identity->quest_slot >= PC_NETWORK_CLIENT_SLOTS ||
        std::strcmp(identity->profile_id, pc_network_profile_mounted_identity())) return 0;
    PcProgressClaim claims[PC_PROGRESS_MAX_PENDING_CLAIMS]{};
    const unsigned count = pc_progress_copy_claims(claims, PC_PROGRESS_MAX_PENDING_CLAIMS);
    if (count > PC_PROGRESS_MAX_PENDING_CLAIMS) return 0;
    for (unsigned i = 0; i < count; ++i) {
        const auto& claim = claims[i];
        if (claim.state != PC_PROGRESS_CLAIM_COMPLETED || claim.key >= PC_NETWORK_FACTS ||
            claim.operation > UINT16_MAX || !pc::progression::sameQuest(claim.identity, *identity) ||
            !enqueueReward({claim.authored_reward_id, static_cast<uint16_t>(claim.key),
                static_cast<uint16_t>(claim.operation), claim.value})) return 0;
        pc_progress_claim_journaled(claim.invocation_id, claim.authored_reward_id,
            claim.key, claim.operation, claim.value);
    }
    if (!pc_network_client_copy(s.clientStore, &s.clientState)) return 0;
    if (!s.clientState.canonical_present) {
        say("Connect to this room once before its first save. Earned rewards are preserved locally.");
        return 0;
    }
    // Do not confirm a save while a received bracelet's native equipment
    // effect still waits behind a grab/menu. Its collection bit alone can
    // already be present. Closing the menu lets ordinary apply repair settle.
    PcProgressSnapshot live{};
    pc_progress_capture(&live);
    if (frozen[PC_PERMANENT_POWER_BRACELETS] &&
        (!live.facts_valid || live.identity.epoch != identity->epoch ||
         !pc::progression::sameQuest(live.identity, *identity) ||
         !pc::progression::factSettled(PC_PERMANENT_POWER_BRACELETS, 1,
            live.facts[PC_PERMANENT_POWER_BRACELETS], live.bracelets_equipped != 0))) {
        say("Let the received equipment finish before retrying the game save.");
        return 0;
    }
    PcNetworkFacts facts{};
    std::copy(frozen, frozen + PC_PERMANENT_FACT_COUNT, facts.values);
    PcNetworkSnapshot status{};
    if (s.session) pc_network_status(s.session, &status);
    const bool provisional = s.clientState.outgoing_count != 0 || status.status != PC_NETWORK_CONNECTED;
    char reason[192]{};
    const bool prepared = provisional
        ? pc_network_client_prepare_offline_save(s.clientStore, static_cast<unsigned>(identity->quest_slot),
            &facts, s.clientState.revision, token, reason, sizeof reason)
        : pc_network_client_prepare_save(s.clientStore, static_cast<unsigned>(identity->quest_slot),
            &facts, s.clientState.revision, token, reason, sizeof reason);
    if (!prepared) say(reason[0] ? reason : "Room save intent could not be preserved; no CARD write was queued.");
    return prepared ? 1 : 0;
}
extern "C" void pc_network_game_native_read_prepare(char sha256[65]) {
    if (!sha256) return;
    sha256[0] = 0;
    if (!s.prepared || !s.mounted.enabled || !s.clientStore) return;
    JKRPcHostAllocScope host;
    const char* path = pc::runCardGciPath();
    char reason[192]{};
    // Missing files and deliberately denied diagnostic reads are normal native
    // I/O outcomes. Observe only; the real CARD worker still performs its read.
    if (path && *path && !pc_network_client_hash_gci(path, sha256, reason, sizeof reason))
        sha256[0] = 0;
}
extern "C" void pc_network_game_leave() {
    JKRPcHostAllocScope host;
    if (s.session) pc_network_leave(s.session);
    if (s.server) { pc_network_server_stop(s.server); s.server = nullptr; }
    s.view.local_host = false;
    s.wantConnection = false;
    invalidateCanonical();
    for (auto& pending : s.outgoing) pending.submitted = false;
    s.lastStatus = PC_NETWORK_OFFLINE;
    // Leaving never remounts personal saves. Native room facts can reseed on rejoin.
    say("Disconnected. The same isolated room saves remain mounted.");
}
extern "C" bool pc_network_game_start(bool host) {
    JKRPcHostAllocScope allocation;
    if (!s.prepared || !s.mounted.enabled || s.view.restart_pending || s.view.failed) {
        say("Select and save a valid room profile, then restart before connecting."); return false;
    }
    pc_network_game_leave();
    if (!s.session) s.session = pc_network_create();
    if (!s.session) { fail("Cannot create the bounded network session"); return false; }
    PcNetworkConfig config = pc::network::joinConfig(s.mounted, host);
    if (host) {
        char reason[192]{};
        const std::string store = s.root + "/host-state";
        s.server = pc_network_server_start_persistent(s.mounted.bind, config.port, store.c_str(), reason, sizeof reason);
        if (!s.server) { say(reason[0] ? reason : "Cannot start the owned persistent host"); return false; }
        // The mounted endpoint is immutable. Hosting binds exactly that configured port.
        if (pc_network_server_port(s.server) != config.port) {
            pc_network_game_leave(); fail("Host port differs from the mounted room route"); return false;
        }
    }
    if (!pc_network_join(s.session, &config)) {
        pc_network_game_leave(); say("Cannot join the mounted room"); return false;
    }
    s.view.local_host = host;
    s.wantConnection = true;
    say("Connecting to the room's shared permanent progress.");
    return true;
}

extern "C" void pc_network_game_frame() {
    if (!s.prepared || !s.mounted.enabled) return; // no sockets or actor searches in personal mode
    JKRPcHostAllocScope host;
    pc_progress_frame();
    pc_progress_capture(&s.view.progress);
    PcNativeEvent event{};
    for (unsigned count = 0; count < 128; ++count) {
        const int popped = pc_progress_pop_event(&event);
        if (popped < 0) { pc_network_game_leave(); fail("Native event queue overflow; sharing stopped"); return; }
        if (!popped) break;
        if (event.kind == PC_NATIVE_QUEST_LOAD_FAILED) {
            s.readAuthorization.questFailed();
            s.readProvenance.reset();
            s.baselineValid = false;
            s.seedBaseline = {};
            invalidateCanonical();
            if (s.session) pc_network_leave(s.session);
            for (auto& pending : s.outgoing) pending.submitted = false;
            say("Native quest load failed; sharing is paused until a valid quest loads.");
        }
        if (event.kind == PC_NATIVE_QUEST_LOADED || event.kind == PC_NATIVE_QUEST_INITIALIZED) {
            const bool sourceRead = event.kind == PC_NATIVE_QUEST_LOADED &&
                s.readProvenance.consume(event.source_read_invocation, event.read_source_sha256);
            if (event.kind == PC_NATIVE_QUEST_INITIALIZED) s.readProvenance.reset();
            ++s.questActivation;
            s.readAuthorization.questReady();
            s.reseedKey = 1;
            s.view.unsaved_remote = false;
            s.remoteNeeded = {};
            s.remoteRequired.fill(false);
            s.baselineValid = event.saved_facts_valid != 0;
            s.baselineGeneration = event.identity.save_generation;
            s.seedBaseline = {};
            if (s.baselineValid)
                std::copy(std::begin(event.saved_facts), std::end(event.saved_facts), s.seedBaseline.values);
            if (sourceRead && s.baselineValid && s.clientStore) {
                char reason[192]{};
                PcNetworkClientLoadDisposition disposition = PC_NETWORK_CLIENT_LOAD_NONE;
                bool confirmed = false;
                const char* path = pc::runCardGciPath();
                if (!pc_network_client_reconcile_loaded(s.clientStore, static_cast<unsigned>(event.identity.quest_slot),
                    path, event.read_source_sha256, &s.seedBaseline, &disposition, reason, sizeof reason) ||
                    !pc_network_client_confirm_loaded(s.clientStore, static_cast<unsigned>(event.identity.quest_slot),
                    path, event.read_source_sha256, &s.seedBaseline, &confirmed, reason, sizeof reason))
                    say(reason[0] ? reason : "The prior room save still needs recovery.");
            }
            if (s.clientStore && pc_network_client_copy(s.clientStore, &s.clientState) &&
                event.identity.quest_slot >= 0 && event.identity.quest_slot < PC_NETWORK_CLIENT_SLOTS) {
                const auto& receipt = s.clientState.receipts[event.identity.quest_slot];
                s.view.provisional_save = receipt.valid && receipt.provisional && !receipt.reconciled;
            }
            // Actor observations belong to a quest lifecycle. Completed copied
            // authored rewards belong to this immutable room and remain queued.
            // The owned server stays up; request a complete canonical snapshot.
            if (s.session && s.wantConnection) {
                pc_network_leave(s.session);
                PcNetworkConfig current = pc::network::joinConfig(s.mounted, s.view.local_host);
                if (!pc_network_join(s.session, &current)) fail("Native quest changed; reconnect failed");
                invalidateCanonical();
                for (auto& pending : s.outgoing) pending.submitted = false;
            }
        }
        if (event.kind == PC_NATIVE_CARD_READ_REQUESTED) {
            s.readAuthorization.requested(event.invocation_id);
            s.readProvenance.requested(event.source_read_invocation == event.invocation_id
                ? event.invocation_id : 0, event.read_source_sha256);
        }
        if (event.kind == PC_NATIVE_CARD_READ_COMPLETED) {
            if (s.readAuthorization.result(event.invocation_id, true)) s.reseedKey = 1;
            s.readProvenance.result(event.invocation_id, event.read_source_sha256,
                event.source_read_invocation == event.invocation_id);
        }
        if (event.kind == PC_NATIVE_CARD_READ_FAILED) {
            s.readAuthorization.result(event.invocation_id, false);
            s.readProvenance.result(event.invocation_id, event.read_source_sha256, false);
        }
        if (event.kind == PC_NATIVE_QUEST_LOADED) ++s.view.native_loads;
        if (event.kind == PC_NATIVE_SAVE_COMMITTED) {
            ++s.view.native_saves;
            bool journalCommitted = event.client_save_token == 0;
            if (event.client_save_token && s.clientStore) {
                char reason[192]{};
                journalCommitted = pc_network_client_commit_save(s.clientStore, event.client_save_token,
                    pc::runCardGciPath(), reason, sizeof reason);
                if (!journalCommitted)
                    say(reason[0] ? reason : "Native save succeeded; the room receipt still needs recovery.");
                if (journalCommitted && pc_network_client_copy(s.clientStore, &s.clientState) &&
                    event.identity.quest_slot >= 0 && event.identity.quest_slot < PC_NETWORK_CLIENT_SLOTS) {
                    const auto& receipt = s.clientState.receipts[event.identity.quest_slot];
                    s.view.provisional_save = receipt.valid && receipt.provisional && !receipt.reconciled;
                }
            }
            if (event.saved_facts_valid &&
                event.identity.save_generation > s.baselineGeneration &&
                event.identity.session_id == s.view.progress.identity.session_id &&
                event.identity.quest_generation == s.view.progress.identity.quest_generation &&
                event.identity.quest_slot == s.view.progress.identity.quest_slot &&
                !std::strcmp(event.identity.profile_id, s.view.progress.identity.profile_id)) {
                std::copy(std::begin(event.saved_facts), std::end(event.saved_facts), s.seedBaseline.values);
                s.baselineValid = true;
                s.baselineGeneration = event.identity.save_generation;
                s.reseedKey = 1;
            }
            if (journalCommitted && savedReceivedFacts(event)) {
                s.view.unsaved_remote = false;
                s.remoteRequired.fill(false);
                s.remoteNeeded = {};
            }
        }
        if (event.kind == PC_NATIVE_SAVE_FAILED) {
            char reason[192]{};
            if (event.client_save_token && s.clientStore)
                pc_network_client_cancel_save(s.clientStore, event.client_save_token, reason, sizeof reason);
            say(reason[0] ? reason : "Native save failed. Received progress remains unsaved; retry the game save.");
        }
        if (event.kind == PC_NATIVE_SAVE_PROGRESS_PENDING)
            say("A room reward is awaiting confirmation. Reconnect, close the menu, then retry saving.");
        if (event.kind == PC_NATIVE_AWARD_COMPLETED && event.origin == PC_NATIVE_LOCAL) {
            ++s.view.local_awards;
            if (event.fact_key >= PC_NETWORK_FACTS || event.operation > UINT16_MAX) {
                fail("A native reward has an invalid shared-progress identity.");
                continue;
            }
            if (enqueueReward({event.authored_reward_id, static_cast<uint16_t>(event.fact_key),
                static_cast<uint16_t>(event.operation), event.operation_value}))
                pc_progress_claim_journaled(event.invocation_id, event.authored_reward_id,
                    event.fact_key, event.operation, event.operation_value);
        }
    }
    pc_progress_capture(&s.view.progress);
    if (s.view.progress.event_overflow) { pc_network_game_leave(); fail("Native event history lost; sharing stopped"); return; }
    pendingView();
    if (s.view.failed) return;
    if (s.session) pc_network_status(s.session, &s.view.session);
    else s.view.session = {};
    if (s.view.session.status == PC_NETWORK_CONNECTED && s.lastStatus != PC_NETWORK_CONNECTED)
        say("Connected. Each player explores independently; save received progress through the game's save screen.");
    s.lastStatus = s.view.session.status;
    const bool connected = s.view.session.status == PC_NETWORK_CONNECTED;
    // Drain notification bounds, then copy facts AND the complete authored
    // ledger at one validated generation/revision. Never pair a newer receipt
    // with an older capacity, or apply a partially received Welcome snapshot.
    PcNetworkUpdate update{};
    for (unsigned count = 0; connected && count < PC_NETWORK_QUEUE && pc_network_poll(s.session, &update); ++count) {
        if (!pc_network_fact_valid(update.delta)) {
            pc_network_game_leave(); fail("Incoming native fact rejected; sharing stopped"); return;
        }
    }
    if (connected && (s.view.session.generation != s.generation || s.view.session.server_revision != s.revision)) {
        PcNetworkSnapshot copied{};
        unsigned count = 0;
        if (!pc_network_copy_state(s.session, &s.canonical, s.rewards.data(),
            static_cast<unsigned>(s.rewards.size()), &count, &copied)) return;
        if (count > s.rewards.size()) { fail("Room reward history exceeded its bound."); return; }
        char reason[192]{};
        if (!s.clientStore || !pc_network_client_stage(s.clientStore, copied.room_identity,
            &s.canonical, s.rewards.data(), count, copied.server_revision, reason, sizeof reason)) {
            pc_network_game_leave();
            fail(reason[0] ? reason : "Cannot preserve received room progress; no native effect was applied.");
            return;
        }
        if (copied.generation != s.generation) s.reseedKey = 1;
        s.generation = copied.generation;
        s.revision = copied.server_revision;
        s.rewardCount = count;
        s.view.session = copied;
    }
    char projectionReason[192]{};
    s.projectionValid = s.clientStore && pc_network_client_projection(s.clientStore,
        &s.projection, projectionReason, sizeof projectionReason);
    // Transport owns copied completed rewards, independent of the current
    // scene's safe-to-apply state. Only durable exact receipts remove them.
    for (auto it = s.outgoing.begin(); it != s.outgoing.end();) {
        const auto* receipt = knownReward(it->reward.reward_id);
        if (receipt) {
            if (!sameReward(*receipt, it->reward)) { fail("A confirmed reward has conflicting outcomes."); return; }
            it = s.outgoing.erase(it);
        } else {
            if (connected && !it->submitted && pc_network_submit_reward(s.session, it->reward)) it->submitted = true;
            ++it;
        }
    }
    pendingView();
    if (!s.readAuthorization.authorized() || !s.view.progress.facts_valid || !s.projectionValid) return;

    PcProgressClaim claims[PC_PROGRESS_MAX_PENDING_CLAIMS]{};
    const unsigned claimCount = pc_progress_copy_claims(claims, PC_PROGRESS_MAX_PENDING_CLAIMS);
    if (claimCount > PC_PROGRESS_MAX_PENDING_CLAIMS) {
        fail("Pending native reward history exceeded its bound."); return;
    }
    std::array<unsigned, PC_NETWORK_FACTS> claimedKeys{};
    for (unsigned i = 0; i < claimCount; ++i) {
        if (claims[i].key >= PC_NETWORK_FACTS) { fail("Invalid pending native reward."); return; }
        ++claimedKeys[claims[i].key];
    }
    for (unsigned i = 0; i < claimCount; ++i) {
        const auto& claim = claims[i];
        if (claim.state == PC_PROGRESS_CLAIM_STALE || claim.state == PC_PROGRESS_CLAIM_ARMED) continue;
        const PcNetworkReward reward{claim.authored_reward_id, static_cast<uint16_t>(claim.key),
            static_cast<uint16_t>(claim.operation), claim.value};
        const auto* receipt = knownReward(claim.authored_reward_id);
        if (claim.state == PC_PROGRESS_CLAIM_COMPLETED) {
            if (!receipt) {
                if (enqueueReward(reward) && pc_progress_claim_journaled(claim.invocation_id,
                    claim.authored_reward_id, claim.key, claim.operation, claim.value)) {
                    uint32_t value = 0;
                    const int resolved = reconcileJournaledClaim(claim, &value);
                    if (resolved == PC_PROGRESS_APPLIED) receivedEffect(claim.key, value, true);
                    if (resolved == PC_PROGRESS_APPLIED || resolved == PC_PROGRESS_ALREADY) --claimedKeys[claim.key];
                }
                continue;
            }
            if (!sameReward(*receipt, reward)) { fail("A room reward receipt conflicts with the native reward."); return; }
        }
        // Cancelled presentations that changed native capacity are corrected
        // without inventing an earned reward. Receipt-backed claims use the
        // durable target, including independently journaled pending operations.
        const auto value = projectedClaimValue(claim.key);
        const int resolved = pc_progress_resolve_claim(claim.invocation_id, claim.authored_reward_id,
            claim.key, value, s.view.progress.identity.epoch);
        if (resolved == PC_PROGRESS_APPLIED) receivedEffect(claim.key, value, pendingOperationFor(claim.key));
        if (resolved == PC_PROGRESS_APPLIED || resolved == PC_PROGRESS_ALREADY) --claimedKeys[claim.key];
    }
    // A validated loaded room may reconcile fixed ownership. Progressive
    // capacities ONLY originate from authored STEP claims, never raw rank polls.
    if (connected && s.baselineValid && s.view.progress.safe_to_apply) while (s.reseedKey < PC_NETWORK_FACTS) {
        const auto key = static_cast<uint16_t>(s.reseedKey);
        if (claimedKeys[key]) break;
        const auto value = s.seedBaseline.values[key];
        if (!pc_permanent_server_authoritative(key) && value > s.canonical.values[key] &&
            pc_network_fact_valid({key, value}) && !pc_network_submit(s.session, {key, value})) break;
        ++s.reseedKey;
    }
    for (unsigned key = 1; key < PC_NETWORK_FACTS; ++key) {
        const uint32_t value = s.projection.facts.values[key];
        const uint32_t owned = pc::network::effectiveFact(static_cast<uint16_t>(key), s.view.progress.facts[key]);
        if (!value || claimedKeys[key] || pc::progression::factSettled(key, value, owned,
            s.view.progress.bracelets_equipped != 0)) continue;
        const int applied = pc_progress_apply(key, value, s.view.progress.identity.epoch);
        if (applied == PC_PROGRESS_APPLIED) receivedEffect(key, value, pendingOperationFor(key));
        else if (applied == PC_PROGRESS_REJECTED) {
            pc_network_game_leave(); fail("Native progression adapter rejected an incoming fact"); return;
        }
    }
    pc_progress_capture(&s.view.progress);
    pendingView();
}
extern "C" void pc_network_game_snapshot(PcNetworkGameSnapshot* copy) {
    if (!copy) return;
    if (s.session) pc_network_status(s.session, &s.view.session);
    // Transport queues can already be drained while a canonical game-thread
    // effect still waits for an unsafe menu/event/card operation to finish.
    pendingView();
    *copy = s.view;
}

namespace {
bool nativePoseAuthorized(uint64_t quest = 0) {
    if (!s.prepared || !s.mounted.enabled || !s.session || s.view.failed || s.view.restart_pending ||
        !s.readAuthorization.authorized()) return false;
    PcProgressSnapshot current{};
    pc_progress_capture(&current);
    const char* mounted = pc_network_profile_mounted_identity();
    return current.facts_valid && current.identity.quest_generation && current.identity.session_id &&
        current.identity.quest_slot >= 0 && current.identity.quest_slot < 3 && mounted &&
        !std::strcmp(current.identity.profile_id, mounted) &&
        current.identity.quest_generation == s.view.progress.identity.quest_generation &&
        current.identity.session_id == s.view.progress.identity.session_id &&
        current.identity.quest_slot == s.view.progress.identity.quest_slot &&
        (!quest || quest == current.identity.quest_generation);
}
}
extern "C" bool pc_network_game_publish_pose(const PcNetworkPose* pose) {
    JKRPcHostAllocScope allocation;
    if (!pose || !s.prepared || !s.mounted.enabled || !s.session) return false;
    // Canonical hide is permitted even when native authorization was revoked.
    if (!pose->active) return pc_network_publish_pose(s.session, pose);
    if (!pc_network_pose_valid(pose) || !nativePoseAuthorized(pose->quest_generation)) {
        const PcNetworkPose hidden{};
        pc_network_publish_pose(s.session, &hidden);
        return false;
    }
    return pc_network_publish_pose(s.session, pose);
}
extern "C" bool pc_network_game_copy_presence(PcNetworkPresenceSnapshot* out) {
    if (!out) return false;
    *out = {};
    JKRPcHostAllocScope allocation;
    if (!nativePoseAuthorized()) return false;
    PcNetworkSnapshot before{}, after{};
    PcNetworkPresenceSnapshot copy{};
    pc_network_status(s.session, &before);
    if (before.status != PC_NETWORK_CONNECTED || !pc_network_copy_presence(s.session, &copy)) return false;
    pc_network_status(s.session, &after);
    if (after.status != PC_NETWORK_CONNECTED || !after.generation || before.generation != after.generation ||
        copy.generation != after.generation || copy.count > PC_NETWORK_PEERS) return false;
    *out = copy;
    return true;
}
