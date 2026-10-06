// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic bounded native-layout observations only; no CPU, guest, SDL or UI.
#include "randomizer_actor_owner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE UINT32_C(0x80372028)
#define TARGET UINT32_C(0x80700000)
#define OTHERS UINT32_C(0x80500000)
#define BASE_TYPE UINT32_C(0x803F6A18)
#define ACTOR_TYPE UINT32_C(0x803F69D0)
enum { CAP = 1024 };
static unsigned checks;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
    fprintf(stderr, "randomizer_actor_owner_test:%u: %s\n", (unsigned)__LINE__, #expr); exit(1); \
} } while (0)
typedef struct Spec { uint32_t profile, methods, size; uint16_t process; uint8_t init, group; } Spec;
static const Spec native[] = {
    {0x8038FD8Cu, 0x8038FD68u, 0x4C28u, 0xA9u, 2, 1},
    {0xC1DF3E4Cu, 0xC1DF3E2Cu, 0x770u, 0x126u, 2, 0},
    {0xC07711D8u, 0xC07711B8u, 0x65Cu, 0x103u, 3, 0},
};
typedef struct Bytes {
    uint8_t queue[12], base[4], type[4], target[0x4C28], others[CAP * 0x100];
    uint8_t profile[3][0x34];
} Bytes;
typedef struct Fixture {
    Bytes bytes;
    uint8_t scratch[0x4C28];
    uint32_t fail_address, fail_size;
    uint32_t override_address, override_value;
    unsigned override_occurrence, override_reads, requests;
    bool ephemeral;
} Fixture;
static Fixture fixture;
static Bytes prior;

static void put16(uint8_t* p, uint16_t n) { p[0] = (uint8_t)(n >> 8); p[1] = (uint8_t)n; }
static void put32(uint8_t* p, uint32_t n) {
    p[0] = (uint8_t)(n >> 24); p[1] = (uint8_t)(n >> 16);
    p[2] = (uint8_t)(n >> 8); p[3] = (uint8_t)n;
}
static const uint8_t* span(uint32_t base, uint32_t length, const uint8_t* data,
                           uint32_t address, uint32_t size) {
    return size && address >= base && address - base < length && size <= length - (address - base)
        ? data + (address - base) : NULL;
}
static const uint8_t* resolve(void* user, uint32_t address, uint32_t size) {
    Fixture* f = (Fixture*)user;
    if (!f) return NULL;
    ++f->requests;
    if (address == f->fail_address && (!f->fail_size || f->fail_size == size)) return NULL;
    const uint8_t* p = span(QUEUE, sizeof f->bytes.queue, f->bytes.queue, address, size);
    if (!p) p = span(BASE_TYPE, 4, f->bytes.base, address, size);
    if (!p) p = span(ACTOR_TYPE, 4, f->bytes.type, address, size);
    if (!p) p = span(TARGET, sizeof f->bytes.target, f->bytes.target, address, size);
    if (!p) p = span(OTHERS, sizeof f->bytes.others, f->bytes.others, address, size);
    for (unsigned i = 0; !p && i < 3; ++i)
        p = span(native[i].profile, sizeof f->bytes.profile[i], f->bytes.profile[i], address, size);
    if (!p || size > sizeof f->scratch) return NULL;
    // A legal provider may invalidate its previous returned pointer on any read.
    const bool overridden = address == f->override_address && size == 4 &&
        ++f->override_reads == f->override_occurrence;
    if (f->ephemeral || overridden) {
        memset(f->scratch, 0xA5, sizeof f->scratch);
        memcpy(f->scratch, p, size);
        if (overridden) put32(f->scratch, f->override_value);
        p = f->scratch;
    }
    return p;
}
static uint8_t* actor_bytes(uint32_t address) {
    if (address == TARGET) return fixture.bytes.target;
    CHECK(address >= OTHERS && address - OTHERS < sizeof fixture.bytes.others);
    return fixture.bytes.others + (address - OTHERS);
}
static uint32_t at(unsigned index, unsigned selected) {
    return index == selected ? TARGET : OTHERS + index * 0x100u;
}
static void setup(BwRandomizerActorKind kind, unsigned count, unsigned selected) {
    memset(&fixture, 0, sizeof fixture);
    CHECK(count && count <= CAP && selected < count);
    const Spec* s = &native[kind];
    put32(fixture.bytes.base, 0x09130001u);
    put32(fixture.bytes.type, 0x09130002u);
    for (unsigned i = 0; i < count; ++i) {
        uint32_t address = at(i, selected);
        uint8_t* a = actor_bytes(address);
        put32(a, 0x09130001u);
        put32(a + 4, 100u + i);
        put32(a + 0xC0, 0x09130002u);
        uint8_t* tag = a + 0xC4;
        put32(tag, i ? at(i - 1, selected) + 0xC4u : 0);
        // Native cLs_Addition calls cNd_SetObject(node, list), not NULL.
        put32(tag + 4, QUEUE);
        put32(tag + 8, i + 1 < count ? at(i + 1, selected) + 0xC4u : 0);
        put32(tag + 0xC, address);
        tag[0x10] = 1;
    }
    put32(fixture.bytes.queue, at(0, selected) + 0xC4u);
    put32(fixture.bytes.queue + 4, at(count - 1, selected) + 0xC4u);
    put32(fixture.bytes.queue + 8, count);
    uint8_t* a = fixture.bytes.target;
    put32(a + 4, 731);
    put16(a + 8, s->process);
    put16(a + 0xE, s->process);
    a[0xC] = s->init; a[0xD] = 2;
    put32(a + 0x10, s->profile);
    put32(a + 0xEC, s->methods);
    a[0x1BE] = s->group; a[0x20A] = 0xFE;
    uint8_t* profile = fixture.bytes.profile[kind];
    put16(profile + 8, s->process);
    put32(profile + 0x10, s->size);
    put32(profile + 0x24, s->methods);
    profile[0x2C] = s->group;
    // The private delete queue's tag is empty even at a native Delete call.
    // fpcDtTg_Do removes it before calling Delete; actor_tag remains in ActorQ.
    CHECK(a[0x4C + 0x10] == 0);
}
static BwRandomizerActorOwner query(BwRandomizerActorKind kind, uint32_t expected, bool success) {
    BwRandomizerActorOwner out;
    memset(&out, 0xA5, sizeof out);
    prior = fixture.bytes;
    const BwRandomizerActorView view = {resolve, &fixture};
    CHECK(bw_randomizer_actor_owner(&view, kind, TARGET, expected, &out) == success);
    CHECK(memcmp(&fixture.bytes, &prior, sizeof prior) == 0);
    CHECK(fixture.requests <= CAP + 20u);
    if (!success) {
        const uint8_t* raw = (const uint8_t*)&out;
        for (size_t i = 0; i < sizeof out; ++i) CHECK(raw[i] == 0);
    } else {
        const Spec* s = &native[kind];
        CHECK(out.address == TARGET && out.pid == 731);
        CHECK(out.profile == s->profile && out.methods == s->methods);
        CHECK(out.actor_tag == TARGET + 0xC4u && out.process == s->process);
        CHECK(out.init_state == s->init && out.create_result == 2 && out.room == -2);
    }
    return out;
}
static void bad32(BwRandomizerActorKind kind, uint32_t offset, uint32_t value) {
    setup(kind, 1, 0); put32(fixture.bytes.target + offset, value); query(kind, 731, false);
}
static void fail_span(BwRandomizerActorKind kind, uint32_t address, uint32_t size) {
    setup(kind, 1, 0); fixture.fail_address = address; fixture.fail_size = size;
    query(kind, 731, false);
}

int main(void) {
    for (unsigned k = 0; k < 3; ++k) {
        BwRandomizerActorKind kind = (BwRandomizerActorKind)k;
        setup(kind, 1, 0); query(kind, 0, true); query(kind, 731, true);
        setup(kind, 1, 0); fixture.ephemeral = true; query(kind, 731, true);
        for (unsigned position = 0; position < 3; ++position) {
            setup(kind, 3, position); query(kind, 731, true);
        }
        setup(kind, 1, 0); query(kind, 732, false); query(kind, UINT32_MAX-1u, false);
        query(kind, UINT32_MAX, false);
        bad32(kind, 4, 0); bad32(kind, 4, UINT32_MAX-1u); bad32(kind, 4, UINT32_MAX);
        setup(kind, 1, 0); put32(fixture.bytes.target + 4, UINT32_MAX-2u);
        BwRandomizerActorOwner out;
        const BwRandomizerActorView view = {resolve, &fixture};
        CHECK(bw_randomizer_actor_owner(&view, kind, TARGET, UINT32_MAX-2u, &out));
        CHECK(out.pid == UINT32_MAX-2u);
        bad32(kind, 0, 0); bad32(kind, 0, 0x09130002u);
        bad32(kind, 0xC0, 0); bad32(kind, 0xC0, 0x09130001u);
        bad32(kind, 0x10, native[k].profile + 4); bad32(kind, 0xEC, native[k].methods + 4);
        bad32(kind, 0x14, 1);
        for (unsigned i = 0; i < 5; ++i) {
            setup(kind, 1, 0); fixture.bytes.target[0xC] = (uint8_t)i;
            query(kind, 731, i == native[k].init);
            setup(kind, 1, 0); fixture.bytes.target[0xD] = (uint8_t)i;
            query(kind, 731, i == 2);
        }
        setup(kind, 1, 0); put16(fixture.bytes.target + 8, native[k].process + 1); query(kind, 731, false);
        setup(kind, 1, 0); put16(fixture.bytes.target + 0xE, native[k].process + 1); query(kind, 731, false);
        setup(kind, 1, 0); fixture.bytes.target[0x1BE] = native[k].group ^ 1; query(kind, 731, false);
        for (unsigned i = 0; i < 4; ++i) {
            setup(kind, 1, 0);
            uint8_t* p = fixture.bytes.profile[k];
            if (i == 0) put16(p + 8, native[k].process + 1);
            if (i == 1) put32(p + 0x10, native[k].size - 1);
            if (i == 2) put32(p + 0x24, native[k].methods + 4);
            if (i == 3) p[0x2C] = native[k].group ^ 1;
            query(kind, 731, false);
        }
        fail_span(kind, TARGET, native[k].size);
        fail_span(kind, native[k].profile, 0x30);
        fail_span(kind, TARGET+0xC4, 0x14);
        fail_span(kind, BASE_TYPE, 4); fail_span(kind, ACTOR_TYPE, 4);
        for (unsigned i = 0; i < 3; ++i) fail_span(kind, QUEUE+i*4, 4);
        setup(kind, 1, 0); memset(fixture.bytes.base, 0, 4); query(kind, 731, false);
        setup(kind, 1, 0); memset(fixture.bytes.type, 0, 4); query(kind, 731, false);
        setup(kind, 1, 0); fixture.bytes.target[0xD4] = 0; query(kind, 731, false);
        setup(kind, 1, 0); fixture.bytes.target[0xD4] = 2; query(kind, 731, false);
        bad32(kind, 0xC4+4, 0); bad32(kind, 0xC4+4, QUEUE+4);
        bad32(kind, 0xC4+0xC, TARGET+4); bad32(kind, 0xC4, TARGET+0xC4);
        bad32(kind, 0xC4+8, TARGET+0xC4); // self cycle
        setup(kind, 1, 0); put32(fixture.bytes.queue + 8, 0); query(kind, 731, false);
        setup(kind, 1, 0); put32(fixture.bytes.queue + 8, CAP+1); query(kind, 731, false);
        setup(kind, 1, 0); put32(fixture.bytes.queue + 8, UINT32_MAX); query(kind, 731, false);
        setup(kind, 1, 0); put32(fixture.bytes.queue, 0); query(kind, 731, false);
        setup(kind, 1, 0); put32(fixture.bytes.queue + 4, 0); query(kind, 731, false);
        setup(kind, 1, 0); put32(fixture.bytes.queue + 4, TARGET+0xC8); query(kind, 731, false);
        setup(kind, 3, 1); put32(fixture.bytes.queue + 8, 2); query(kind, 731, false);
        setup(kind, 3, 1); put32(fixture.bytes.queue + 8, 4); query(kind, 731, false);
        setup(kind, 3, 1); put32(actor_bytes(at(2, 1)) + 0xC4 + 8, at(0, 1)+0xC4); query(kind, 731, false);
        setup(kind, 3, 1); put32(actor_bytes(at(2, 1)) + 0xC4, 0); query(kind, 731, false);
        setup(kind, 3, 1); put32(actor_bytes(at(0, 1)) + 0xC4 + 0xC, 0x817FFFF0u); query(kind, 731, false);
        setup(kind, 3, 1); put32(actor_bytes(at(0, 1)) + 0xC4 + 0xC, TARGET); query(kind, 731, false);
        setup(kind, 1, 0); put32(fixture.bytes.queue, TARGET+0xC5); query(kind, 731, false);
        setup(kind, 1, 0); put32(fixture.bytes.queue, 0x817FFFFCu); query(kind, 731, false);
        setup(kind, 1, 0); put32(fixture.bytes.queue, 0x7FFFFFFCu); query(kind, 731, false);
        for (unsigned i = 0; i < 3; ++i) {
            setup(kind, 1, 0);
            fixture.override_address = QUEUE + i*4;
            fixture.override_occurrence = 2;
            fixture.override_value = 0;
            query(kind, 731, false); // queue changed between traversal and revalidation
        }
    }
    setup(BW_RANDOMIZER_ACTOR_LINK, CAP, CAP/2); query(BW_RANDOMIZER_ACTOR_LINK, 731, true);
    for (unsigned n = 1; n <= 32; ++n) {
        setup(BW_RANDOMIZER_ACTOR_TBOX, n, n/2); query(BW_RANDOMIZER_ACTOR_TBOX, 731, true);
    }
    setup(BW_RANDOMIZER_ACTOR_DELETING_ITEM, 1, 0);
    CHECK(fixture.bytes.target[0x5C] == 0); query(BW_RANDOMIZER_ACTOR_DELETING_ITEM, 731, true);
    const BwRandomizerActorView view = {resolve, &fixture};
    BwRandomizerActorOwner out;
    const uint32_t invalid_addresses[] = {0, 0x7FFFFFFCu, TARGET+1, 0x81800000u,
        UINT32_MAX, 0x817FFFFCu, 0xC0700000u};
    for (unsigned i = 0; i < sizeof invalid_addresses/sizeof invalid_addresses[0]; ++i) {
        memset(&out, 0xA5, sizeof out); unsigned before = fixture.requests;
        CHECK(!bw_randomizer_actor_owner(&view, BW_RANDOMIZER_ACTOR_LINK, invalid_addresses[i], 731, &out));
        CHECK(before == fixture.requests);
        for (size_t j = 0; j < sizeof out; ++j) CHECK(((uint8_t*)&out)[j] == 0);
    }
    for (int kind = -1; kind <= 3; kind += 4) {
        memset(&out, 0xA5, sizeof out); unsigned before = fixture.requests;
        CHECK(!bw_randomizer_actor_owner(&view, (BwRandomizerActorKind)kind, TARGET, 731, &out));
        CHECK(before == fixture.requests);
        for (size_t j = 0; j < sizeof out; ++j) CHECK(((uint8_t*)&out)[j] == 0);
    }
    memset(&out, 0xA5, sizeof out);
    CHECK(!bw_randomizer_actor_owner(NULL, BW_RANDOMIZER_ACTOR_LINK, TARGET, 731, &out));
    for (size_t j = 0; j < sizeof out; ++j) CHECK(((uint8_t*)&out)[j] == 0);
    const BwRandomizerActorView missing = {NULL, &fixture}, no_user = {resolve, NULL};
    CHECK(!bw_randomizer_actor_owner(&missing, BW_RANDOMIZER_ACTOR_LINK, TARGET, 731, &out));
    CHECK(!bw_randomizer_actor_owner(&no_user, BW_RANDOMIZER_ACTOR_LINK, TARGET, 731, &out));
    CHECK(!bw_randomizer_actor_owner(&view, BW_RANDOMIZER_ACTOR_LINK, TARGET, 731, NULL));
    printf("PASS synthetic bounded actor owner: %u checks; no native authority/game execution\n", checks);
    return 0;
}
