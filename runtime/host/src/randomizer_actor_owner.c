// SPDX-License-Identifier: GPL-3.0-or-later
#include "randomizer_actor_owner.h"
#include <stddef.h>
#include <string.h>

enum { MAX_ACTORS = 1024, NATIVE_BASE = 0x80000000u, NATIVE_END = 0x81800000u };
typedef struct ActorSpec {
    uint32_t profile, methods, size;
    uint16_t process;
    uint8_t init_state, group;
} ActorSpec;
static const ActorSpec specs[] = {
    {0x8038FD8Cu, 0x8038FD68u, 0x4C28u, 0xA9u, 2, 1},
    {0xC1DF3E4Cu, 0xC1DF3E2Cu, 0x770u, 0x126u, 2, 0},
    {0xC07711D8u, 0xC07711B8u, 0x65Cu, 0x103u, 3, 0}
};
static bool raw_span(uint32_t address, uint32_t size) {
    return address >= NATIVE_BASE && address < NATIVE_END &&
        (address & 3u) == 0 && size && size <= NATIVE_END - address;
}
static uint16_t be16(const uint8_t* p) { return ((uint16_t)p[0] << 8) | p[1]; }
static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static bool read32(const BwRandomizerActorView* v, uint32_t a, uint32_t* n) {
    const uint8_t* p = v->resolve(v->user, a, 4);
    if (!p) return false;
    *n = be32(p); return true;
}
bool bw_randomizer_actor_owner(const BwRandomizerActorView* v,
    BwRandomizerActorKind kind, uint32_t address, uint32_t expected_pid,
    BwRandomizerActorOwner* out) {
    if (out) memset(out, 0, sizeof *out);
    if (!v || !v->resolve || !out || (unsigned)kind >= sizeof specs / sizeof specs[0]) return false;
    const ActorSpec* s = &specs[(unsigned)kind];
    if (!raw_span(address, s->size) || expected_pid >= UINT32_MAX - 1u) return false;
    uint32_t head, tail, count;
    if (!read32(v, 0x80372028u, &head) || !read32(v, 0x8037202Cu, &tail) ||
        !read32(v, 0x80372030u, &count) || !count || count > MAX_ACTORS) return false;
    uint32_t visited[MAX_ACTORS], n = 0, previous = 0, node = head, matches = 0;
    while (node) {
        if (n >= count || !raw_span(node, 0x14)) return false;
        for (uint32_t i = 0; i < n; ++i) if (visited[i] == node) return false;
        visited[n++] = node;
        const uint8_t* tag = v->resolve(v->user, node, 0x14);
        if (!tag || be32(tag) != previous || be32(tag + 4) != 0x80372028u || tag[0x10] != 1) return false;
        const uint32_t actor = be32(tag + 0xC), next = be32(tag + 8);
        if (!raw_span(actor, 0xD8) || actor + 0xC4u != node) return false;
        if (actor == address) ++matches;
        previous = node; node = next;
    }
    if (n != count || previous != tail || matches != 1) return false;
    uint32_t now_head, now_tail, now_count, base_type, actor_type;
    if (!read32(v, 0x80372028u, &now_head) || !read32(v, 0x8037202Cu, &now_tail) ||
        !read32(v, 0x80372030u, &now_count) || head != now_head || tail != now_tail || count != now_count ||
        !read32(v, 0x803F6A18u, &base_type) || !base_type ||
        !read32(v, 0x803F69D0u, &actor_type) || !actor_type) return false;
    const uint8_t* a = v->resolve(v->user, address, s->size);
    if (!a) return false;
    const uint32_t pid = be32(a + 4);
    if (!pid || pid >= UINT32_MAX - 1u || (expected_pid && expected_pid != pid) ||
        be32(a) != base_type || be32(a + 0xC0) != actor_type ||
        be16(a + 8) != s->process || be16(a + 0xE) != s->process ||
        a[0xC] != s->init_state || a[0xD] != 2 || be32(a + 0x14) != 0 ||
        be32(a + 0x10) != s->profile || be32(a + 0xEC) != s->methods ||
        a[0x1BE] != s->group || be32(a + 0xC4 + 0xC) != address || a[0xD4] != 1) return false;
    const BwRandomizerActorOwner copied = {address, pid, s->profile, s->methods, address + 0xC4u,
        s->process, s->init_state, 2, (int8_t)a[0x20A]};
    const uint8_t* profile = v->resolve(v->user, s->profile, 0x30);
    if (!profile || be16(profile + 8) != s->process || be32(profile + 0x10) != s->size ||
        be32(profile + 0x24) != s->methods || profile[0x2C] != s->group) return false;
    *out = copied;
    return true;
}
