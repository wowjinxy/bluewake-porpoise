// SPDX-License-Identifier: GPL-3.0-or-later
// Native schema/config split adapted from BlueWake network_preferences.cpp.
#include "network_wire.h"
#include "network_digest.h"
#include <cstdio>
#include <cstring>

namespace {
void error(char* out, unsigned size, const char* message) {
    if (out && size) std::snprintf(out, size, "%s", message);
}
bool terminated(const char* text, size_t capacity) { return std::memchr(text, 0, capacity) != nullptr; }
bool hex(const char* text, size_t length) {
    return std::strlen(text) == length && std::strspn(text, "0123456789abcdef") == length;
}
bool printable(const char* text, bool empty = false) {
    if (!empty && !*text) return false;
    for (; *text; ++text) if (uint8_t(*text) < 32 || uint8_t(*text) > 126) return false;
    return true;
}
bool slug(const char* text) {
    return *text && std::strspn(text, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::strlen(text);
}
}

extern "C" void pc_network_native_compatibility(PcNetworkCompatibility* out) {
    if (!out) return;
    *out = {};
    std::strcpy(out->game_id, "GZLE01");
    std::strcpy(out->build_id, "bluewake-native-gzle01-r0");
    out->progression_schema = PC_NETWORK_SCHEMA;
    out->disc_revision = 0;
    const auto semantics = pc_net::digest("BlueWake.Native.GZLE01.rev0.CXX.NativeItems.v1");
    const auto rules = pc_net::digest("permanent-facts:v1;1=Telescope:owned:1;no-resources;no-reward-flags");
    std::strcpy(out->semantic_digest, semantics.c_str());
    std::strcpy(out->rules_digest, rules.c_str());
}

extern "C" bool pc_network_normalize_endpoint(const char* address, char* out, unsigned capacity) {
    if (!address || !out || !capacity || std::strlen(address) > 255 || !pc_net::init()) return false;
    if (std::strcmp(address, "localhost") == 0) address = "127.0.0.1";
    in_addr v4{};
    in6_addr v6{};
    char normalized[INET6_ADDRSTRLEN]{};
    if (inet_pton(AF_INET, address, &v4) == 1) {
        if (!inet_ntop(AF_INET, &v4, normalized, sizeof normalized)) return false;
    } else if (inet_pton(AF_INET6, address, &v6) == 1) {
        if (!inet_ntop(AF_INET6, &v6, normalized, sizeof normalized)) return false;
    } else return false;
    if (std::strlen(normalized) >= capacity) return false;
    std::strcpy(out, normalized);
    return true;
}

extern "C" bool pc_network_config_valid(const PcNetworkConfig* config, char* out, unsigned size) {
    if (!config) { error(out, size, "Missing native network configuration"); return false; }
    const auto& c = *config;
    if (!terminated(c.server, sizeof c.server) || !terminated(c.room, sizeof c.room) ||
        !terminated(c.player_id, sizeof c.player_id) || !terminated(c.player_name, sizeof c.player_name) ||
        !terminated(c.room_password, sizeof c.room_password) ||
        !terminated(c.compatibility.game_id, sizeof c.compatibility.game_id) ||
        !terminated(c.compatibility.build_id, sizeof c.compatibility.build_id) ||
        !terminated(c.compatibility.semantic_digest, sizeof c.compatibility.semantic_digest) ||
        !terminated(c.compatibility.rules_digest, sizeof c.compatibility.rules_digest)) {
        error(out, size, "Unterminated native network configuration field"); return false;
    }
    PcNetworkCompatibility expected{};
    pc_network_native_compatibility(&expected);
    if (!pc_net::same(c.compatibility, expected)) {
        error(out, size, "Unsupported native disc/schema/semantic rules identity"); return false;
    }
    char endpoint[256]{};
    if (!c.port || !pc_network_normalize_endpoint(c.server, endpoint, sizeof endpoint) ||
        !slug(c.room) || !hex(c.player_id, 32) || !printable(c.player_name) ||
        !printable(c.room_password, true)) {
        error(out, size, "Use numeric IP/localhost, port, ASCII room/name, and stable 32-hex player ID"); return false;
    }
    error(out, size, "");
    return true;
}

extern "C" bool pc_network_fact_valid(PcNetworkFact fact) {
    return fact.key == PC_NETWORK_FACT_TELESCOPE && fact.value == 1;
}
extern "C" bool pc_network_fact_merge(PcNetworkFacts* state, PcNetworkFact fact) {
    if (!state || !pc_network_fact_valid(fact) || state->values[fact.key] == 1) return false;
    state->values[fact.key] = 1;
    return true;
}
extern "C" void pc_network_fact_clear(PcNetworkFacts* state) { if (state) *state = {}; }
extern "C" const char* pc_network_fact_name(uint16_t key) {
    return key == PC_NETWORK_FACT_TELESCOPE ? "Telescope" : "Unsupported";
}
