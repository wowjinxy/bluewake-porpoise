#include "pc/pc_network_game.h"
#include "pc_internal.h"
#include "JSystem/JKernel/JKRHeap.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
struct State {
    PcNetworkProfile mounted{}, next{};
    PcNetworkGameSnapshot view{};
    PcNetworkSession* session = nullptr;
    PcNetworkServer* server = nullptr;
    std::string preferences, root;
    uint32_t canonical = 0;
    uint64_t generation = 0, epoch = 0;
    bool prepared = false, authorized = false, dirty = false, reseed = false;
    bool readAuthorization = false, wantConnection = false;
    PcProgressIdentity remoteIdentity{};
    uint64_t questActivation = 0, remoteActivation = 0;
} s;
void error(char* out, unsigned cap, const char* message) {
    if (out && cap) std::snprintf(out, cap, "%s", message);
}
void say(const char* message) { std::snprintf(s.view.message, sizeof s.view.message, "%s", message); }
bool explicit_value(const char* key) { const char* v = std::getenv(key); return v && *v; }
void fail(const char* message) { s.view.failed = true; say(message); }
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
extern "C" void pc_network_game_leave() {
    JKRPcHostAllocScope host;
    if (s.session) pc_network_leave(s.session);
    if (s.server) { pc_network_server_stop(s.server); s.server = nullptr; }
    s.view.local_host = false;
    s.wantConnection = false;
    s.generation = 0;
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
    PcNetworkConfig config = s.mounted.config;
    if (host) {
        char reason[192]{};
        const std::string store = s.root + "/host-state";
        s.server = pc_network_server_start_persistent(s.mounted.bind, config.port, store.c_str(), reason, sizeof reason);
        if (!s.server) { say(reason[0] ? reason : "Cannot start the owned persistent host"); return false; }
        // The mounted endpoint is immutable. Hosting binds exactly that configured port.
        if (pc_network_server_port(s.server) != config.port) {
            pc_network_game_leave(); fail("Host port differs from the mounted room route"); return false;
        }
        // Host always joins the listener this process owns. The configured
        // public endpoint still defines the immutable client-save route.
        std::strcpy(config.server, !std::strcmp(s.mounted.bind, "0.0.0.0") ? "127.0.0.1" : s.mounted.bind);
        config.create_room = true;
    } else config.create_room = false;
    if (!pc_network_join(s.session, &config)) {
        pc_network_game_leave(); say("Cannot join the mounted room"); return false;
    }
    s.view.local_host = host;
    s.wantConnection = true;
    say("Connecting. Permanent Telescope ownership is the current sharing whitelist.");
    return true;
}

extern "C" void pc_network_game_frame() {
    if (!s.prepared || !s.mounted.enabled) return; // no sockets or actor searches in personal mode
    JKRPcHostAllocScope host;
    pc_progress_frame();
    PcNativeEvent event{};
    for (unsigned count = 0; count < 128; ++count) {
        const int popped = pc_progress_pop_event(&event);
        if (popped < 0) { pc_network_game_leave(); fail("Native event queue overflow; sharing stopped"); return; }
        if (!popped) break;
        if (event.kind == PC_NATIVE_QUEST_LOADED || event.kind == PC_NATIVE_QUEST_INITIALIZED) {
            ++s.questActivation;
            s.authorized = true;
            s.dirty = false;
            s.reseed = true;
            s.view.unsaved_remote = false;
            // Revoke unacknowledged work from the previous quest/lifecycle. The
            // owned durable server stays up; the client requests a fresh snapshot.
            if (s.session && s.wantConnection) {
                pc_network_leave(s.session);
                PcNetworkConfig current = s.mounted.config;
                current.create_room = s.view.local_host;
                if (!pc_network_join(s.session, &current)) fail("Native quest changed; reconnect failed");
                s.generation = 0;
                s.canonical = 0;
            }
        }
        if (event.kind == PC_NATIVE_CARD_READ_REQUESTED) {
            s.readAuthorization = s.authorized;
            s.authorized = false;
            s.dirty = false;
        }
        if (event.kind == PC_NATIVE_CARD_READ_COMPLETED) {
            s.authorized = s.readAuthorization;
            s.reseed = true;
        }
        if (event.kind == PC_NATIVE_CARD_READ_FAILED) s.authorized = false;
        if (event.kind == PC_NATIVE_QUEST_LOADED) ++s.view.native_loads;
        if (event.kind == PC_NATIVE_SAVE_COMMITTED) {
            ++s.view.native_saves;
            if (event.fact_key == PC_PROGRESS_FACT_TELESCOPE && event.fact_value == 1 &&
                event.identity.session_id == s.remoteIdentity.session_id &&
                s.questActivation == s.remoteActivation &&
                event.identity.quest_slot == s.remoteIdentity.quest_slot &&
                !std::strcmp(event.identity.profile_id, s.remoteIdentity.profile_id) &&
                event.identity.save_generation > s.remoteIdentity.save_generation)
                s.view.unsaved_remote = false;
        }
        if (event.kind == PC_NATIVE_SAVE_FAILED) say("Native save failed. Received progress remains unsaved; retry the game save.");
        if (event.kind == PC_NATIVE_AWARD_COMPLETED && event.origin == PC_NATIVE_LOCAL &&
            event.fact_key == PC_PROGRESS_FACT_TELESCOPE && event.fact_value == 1) {
            ++s.view.local_awards;
            s.dirty = true;
        }
    }
    pc_progress_capture(&s.view.progress);
    if (s.view.progress.event_overflow) { pc_network_game_leave(); fail("Native event history lost; sharing stopped"); return; }
    s.view.authorized = s.authorized;
    if (s.view.progress.identity.epoch != s.epoch) {
        s.epoch = s.view.progress.identity.epoch;
        // Canonical facts stay tied to this immutable room; old actor observations do not.
    }
    if (!s.session) return;
    pc_network_status(s.session, &s.view.session);
    if (s.view.session.status != PC_NETWORK_CONNECTED) return;
    if (s.view.session.generation != s.generation) {
        s.generation = s.view.session.generation;
        s.canonical = 0;
        // A confirmed native load/clean quest authorizes reseeding this room's own save.
        // This is an idempotent canonical fact, never a newly earned award event.
        s.reseed = true;
    }
    PcNetworkUpdate update{};
    for (unsigned count = 0; count < PC_NETWORK_QUEUE && pc_network_poll(s.session, &update); ++count) {
        if (!pc_network_fact_valid(update.delta)) {
            pc_network_game_leave(); fail("Incoming native fact rejected; sharing stopped"); return;
        }
        if (update.generation < s.generation) continue;
        if (update.generation > s.generation) {
            // Welcome can race the separate status() copy. Adopt the validated
            // newer snapshot without dropping its first canonical fact.
            s.generation = update.generation;
            s.canonical = 0;
            s.reseed = true;
        }
        if (update.delta.key == PC_NETWORK_FACT_TELESCOPE && update.delta.value == 1) s.canonical = 1;
    }
    if (!s.authorized) return;
    if (s.reseed && s.view.progress.safe_to_apply) {
        if (s.view.progress.telescope_owned && !s.dirty &&
            !pc_network_submit(s.session, {PC_NETWORK_FACT_TELESCOPE, 1})) return;
        s.reseed = false;
    }
    if (s.dirty && s.view.progress.telescope_owned &&
        pc_network_submit(s.session, {PC_NETWORK_FACT_TELESCOPE, 1})) s.dirty = false;
    if (s.canonical && !s.view.progress.telescope_owned) {
        const int applied = pc_progress_apply(PC_PROGRESS_FACT_TELESCOPE, 1, s.view.progress.identity.epoch);
        if (applied == PC_PROGRESS_APPLIED) {
            ++s.view.remote_applies;
            s.view.unsaved_remote = true;
            s.remoteIdentity = s.view.progress.identity;
            s.remoteActivation = s.questActivation;
            say("Shared Telescope received. Save through the game's save screen to keep it offline.");
        } else if (applied == PC_PROGRESS_REJECTED) {
            pc_network_game_leave(); fail("Native progression adapter rejected an incoming fact");
        }
    }
    s.view.pending_local = s.dirty;
    s.view.pending_remote = s.canonical && !s.view.progress.telescope_owned;
}
extern "C" void pc_network_game_snapshot(PcNetworkGameSnapshot* copy) {
    if (!copy) return;
    if (s.session) pc_network_status(s.session, &s.view.session);
    *copy = s.view;
}
