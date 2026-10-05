// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_menu.h"
#include "health_host.h"
#include "network_game.h"
#ifdef rename
#undef rename
#endif
#include "network_digest.h"
#include <windows.h>
#include <bcrypt.h>
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
BwNetworkPreferences launched{}, saved{}, editing{};
std::string directory, failure, status, room_card;
bool prepared;
char host_bind[64] = "127.0.0.1";

bool same_preferences(const BwNetworkPreferences& a, const BwNetworkPreferences& b) {
    return a.room_mode == b.room_mode && bw_network_same_room(&a.config, &b.config) &&
        a.config.create_room == b.config.create_room && std::strcmp(a.config.player_name, b.config.player_name) == 0 &&
        std::strcmp(a.config.room_password, b.config.room_password) == 0;
}
// Native incremental SHA256 avoids copying a large translated module into RAM.
bool file_digest(const std::filesystem::path& path, std::string& digest) {
    FILE* input = _wfopen(path.c_str(), L"rb"); if (!input) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0, hash_size = 0, received = 0;
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (ok) ok = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof object_size, &received, 0) >= 0 &&
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_size), sizeof hash_size, &received, 0) >= 0 &&
        object_size <= 65536 && hash_size == 32;
    std::vector<unsigned char> object(object_size), bytes(1024 * 1024), output(hash_size);
    if (ok) ok = BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) >= 0;
    size_t count = 0;
    while (ok && (count = std::fread(bytes.data(), 1, bytes.size(), input)) != 0)
        ok = BCryptHashData(hash, bytes.data(), static_cast<ULONG>(count), 0) >= 0;
    ok = ok && !std::ferror(input);
    if (std::fclose(input) != 0) ok = false;
    if (ok) ok = BCryptFinishHash(hash, output.data(), hash_size, 0) >= 0;
    if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!ok) return false;
    const char* hex = "0123456789abcdef"; digest.clear(); digest.reserve(64);
    for (auto byte : output) { digest += hex[byte >> 4]; digest += hex[byte & 15]; }
    return true;
}
std::string normalized_options(const char* value) {
    std::map<std::string, bool> values;
    const std::string list = value ? value : "";
    size_t start = 0;
    while (start < list.size()) {
        const size_t end = list.find(',', start); std::string token = list.substr(start, end - start);
        bool enabled = !token.empty() && token[0] != '-'; if (!enabled && !token.empty()) token.erase(0, 1);
        if (!token.empty() && token != "none") values[token] = enabled;
        if (end == std::string::npos) break; start = end + 1;
    }
    std::string result;
    for (const auto& [name, enabled] : values) result += std::to_string(name.size()) + ":" + name + ":" + (enabled ? "1;" : "0;");
    return result;
}
std::string normalized_mods(const char* value) {
    // host_mods_enable enables a compiled mod if any exact positive token is
    // present. Negative tokens do not undo that enablement (unlike options).
    std::set<std::string> names;
    const std::string list = value ? value : "";
    size_t start = 0;
    while (start < list.size()) {
        const size_t end = list.find(',', start);
        const std::string token = list.substr(start, end - start);
        if (!token.empty() && token[0] != '-' && token != "none") names.insert(token);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    std::string result;
    for (const auto& name : names) result += std::to_string(name.size()) + ":" + name + ":1;";
    return result;
}
bool compatibility(const char* module, BwNetworkCompatibility& result) {
    std::string module_digest;
    if (!file_digest(std::filesystem::u8path(module), module_digest)) { failure = "The actual game translation could not be fingerprinted for room compatibility."; return false; }
    result = {}; std::strcpy(result.game_id, "GZLE01"); result.progression_schema = BW_PROGRESSION_SCHEMA;
    // This names wire/native semantics, not the app's timestamp or UI build.
    // Bump this revision with any incompatible progression/bridge semantics.
    const auto build = bw_net::digest("BlueWake/GZLE01/Progression1/NativeColdBoot2/TCP1");
    const char* options = std::getenv("BLUEWAKE_OPTIONS");
    const bool defaults_off = options && std::strncmp(options, "none", 4) == 0;
    const auto options_digest = bw_net::digest(std::string(defaults_off ? "defaults=off;" : "defaults=native;") +
        "mods=" + normalized_mods(std::getenv("BLUEWAKE_MODS")) + ";options=" + normalized_options(options));
    std::strcpy(result.build_id, build.c_str()); std::strcpy(result.module_digest, module_digest.c_str());
    std::strcpy(result.options_digest, options_digest.c_str()); return true;
}
const char* status_name(BwNetworkStatus value) {
    switch (value) {
    case BW_NETWORK_CONNECTING: return "Connecting";
    case BW_NETWORK_CONNECTED: return "Connected";
    case BW_NETWORK_RECONNECTING: return "Reconnecting";
    case BW_NETWORK_REJECTED: return "Rejected";
    case BW_NETWORK_FAILED: return "Connection failed";
    default: return "Offline";
    }
}
bool persist(const BwNetworkPreferences& value) {
    char error[256]{};
    if (!bw_network_preferences_save(directory.c_str(), &value, error, sizeof error)) { failure = error; return false; }
    saved = value; failure.clear();
    status = bw_network_menu_restart_needed() ? "Network settings saved. Restart BlueWake to mount the selected room or personal save." : "Network settings saved.";
    return true;
}
}
extern "C" bool bw_network_menu_prepare(const char* data_dir, const char* module_path, char* path, unsigned size) {
    if (!data_dir || !*data_dir || !module_path || !*module_path || !path || !size || prepared) {
        failure = "Network startup needs the data folder, game translation and an unused startup context."; return false;
    }
    path[0] = 0;
    try {
        directory = data_dir; char error[256]{};
        if (!bw_network_preferences_load(data_dir, &launched, error, sizeof error)) { failure = error; return false; }
        if (!bw_health_host_prepare_room(launched.room_mode)) {
            failure = "Room saves require native damage and healing settings. Restore Native before restarting; no card was mounted."; return false;
        }
        const auto previous = launched.config.compatibility;
        if (!compatibility(module_path, launched.config.compatibility)) return false;
        if (launched.room_mode && (std::strcmp(previous.module_digest, launched.config.compatibility.module_digest) ||
            std::strcmp(previous.options_digest, launched.config.compatibility.options_digest) || std::strcmp(previous.build_id, launched.config.compatibility.build_id)))
            status = "Game translation or gameplay options changed. This compatibility profile uses a separate room save; previous room saves are retained.";
        if (launched.room_mode) {
            if (!bw_network_room_card_path(data_dir, &launched.config, path, size, error, sizeof error)) { failure = error; return false; }
            room_card = path;
        }
        // Persist generated player identity/current manifest before any backend
        // opens; a failure never silently routes a room into a personal card.
        if (!bw_network_preferences_save(data_dir, &launched, error, sizeof error)) { failure = error; return false; }
        if (!bw_network_game_prepare_with_store(&launched, data_dir)) { failure = "The network game context could not accept the selected startup room."; return false; }
        saved = editing = launched; prepared = true; failure.clear(); return true;
    } catch (...) { failure = "Network preferences or compatibility could not be initialized; no card was mounted."; return false; }
}
extern "C" const char* bw_network_menu_error(void) { return failure.c_str(); }
extern "C" bool bw_network_menu_restart_needed(void) {
    return prepared && (saved.room_mode != launched.room_mode || (saved.room_mode && !bw_network_same_room(&saved.config, &launched.config)));
}
extern "C" void bw_network_menu_draw(void) {
    ImGui::SeparatorText("Shared progress co-op (experimental)");
    if (!prepared) { ImGui::TextWrapped("%s", failure.empty() ? "Network startup is unavailable." : failure.c_str()); return; }
    BwNetworkGameSnapshot current{}; bw_network_game_snapshot(&current);
    ImGui::TextWrapped("Explore independently and share audited permanent progress. Room saves are separate from personal saves. Remote Link rendering is still pending.");
    ImGui::Text("Status: %s", status_name(current.session.status));
    if (*current.session.message) ImGui::TextWrapped("%s", current.session.message);
    if (*current.message) ImGui::TextWrapped("%s", current.message);
    if (current.local_host && current.persistent_host)
        ImGui::TextWrapped("Room progress is saved on this host across server restarts.");
    ImGui::TextWrapped("Mounted saves: %s", launched.room_mode ? room_card.c_str() : "personal card");
    ImGui::Checkbox("Use room saves after restart", &editing.room_mode);
    ImGui::InputText("Server address", editing.config.server, sizeof editing.config.server);
    int port = editing.config.port;
    if (ImGui::InputInt("Port", &port, 1, 100)) editing.config.port = uint16_t(std::clamp(port, 1, 65535));
    ImGui::InputText("Room", editing.config.room, sizeof editing.config.room);
    ImGui::InputText("Player name", editing.config.player_name, sizeof editing.config.player_name);
    ImGui::InputText("Room password", editing.config.room_password, sizeof editing.config.room_password, ImGuiInputTextFlags_Password);
    ImGui::Checkbox("Create room / host locally at startup", &editing.config.create_room);
    ImGui::TextWrapped("Use localhost or a numeric IP address. A local server defaults to 127.0.0.1; hosting on a LAN address is an explicit choice.");
    if (ImGui::Button("Save network settings")) persist(editing);
    ImGui::SameLine(); if (ImGui::Button("Discard network edits")) { editing = saved; failure.clear(); }
    if (bw_network_menu_restart_needed()) ImGui::TextWrapped("Restart required to change the mounted save route. Use Save and restart below.");
    const bool can_connect = launched.room_mode && editing.room_mode && !bw_network_menu_restart_needed() && bw_network_same_room(&editing.config, &launched.config) && !current.pending_command;
    ImGui::BeginDisabled(!can_connect);
    if (ImGui::Button("Join / rejoin room")) {
        if (!persist(editing) || !bw_network_game_request_join(&editing.config)) failure = "The join request could not be queued for this mounted room.";
        else status = "Join requested for the next game update.";
    }
    ImGui::InputText("Host bind address", host_bind, sizeof host_bind);
    if (ImGui::Button("Host room")) {
        editing.config.create_room = true;
        if (!persist(editing) || !bw_network_game_request_host(&editing.config, host_bind)) failure = "The host request needs this mounted room and a numeric IPv4 bind address.";
        else status = "Host requested for the next game update.";
    }
    ImGui::EndDisabled();
    ImGui::SameLine(); ImGui::BeginDisabled(!launched.room_mode || current.pending_command);
    if (ImGui::Button("Leave room")) { bw_network_game_request_leave(); status = "Leave requested. This process keeps its isolated room save."; }
    ImGui::EndDisabled();
    if (current.session.peer_count && ImGui::BeginTable("##room-roster", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Player"); ImGui::TableSetupColumn("Connection"); ImGui::TableSetupColumn("Area"); ImGui::TableSetupColumn("Room"); ImGui::TableHeadersRow();
        for (unsigned i = 0; i < std::min(current.session.peer_count, BW_NETWORK_PEERS); ++i) {
            const auto& peer = current.session.peers[i]; ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(peer.player_name);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(peer.online ? "Online" : "Offline");
            ImGui::TableNextColumn(); ImGui::TextUnformatted(peer.stage);
            ImGui::TableNextColumn(); ImGui::Text("%d", peer.room);
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("Room revision %llu / applied %llu / waiting %u", static_cast<unsigned long long>(current.session.server_revision),
        static_cast<unsigned long long>(current.applied), current.deferred_updates);
    if (!same_preferences(saved, editing)) ImGui::TextDisabled("Network edits are not saved.");
    if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
    if (!failure.empty()) ImGui::TextWrapped("%s", failure.c_str());
}
extern "C" void bw_network_menu_shutdown(void) { bw_network_game_detach(); }
