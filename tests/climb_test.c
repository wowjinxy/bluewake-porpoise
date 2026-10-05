/* Exercise the real host climbing hooks against isolated synthetic guest RAM.
 * No translated game, graphics, input devices, or personal saves are needed. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "climb.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    PLAYER = 0x80010000u,
    PLAYER_SLOT = 0x803CA74Cu,
    FRONT_CODE = 0x8010F0DCu,
    FRONT_ABOVE = 0x8010F554u,
    CLIMB_CODE = 0x80135FE4u,
    FRONT_PLANE = 0x8010EEBCu,
    FRONT_REPROBE = 0x8010F01Cu,
    CLIMB_PLANE = 0x80135FFCu,
    FRONT_PLANE_NEXT = 0x8010EED8u,
    FRONT_REPROBE_NEXT = 0x8010F0D0u,
    CLIMB_PLANE_NEXT = 0x80136014u,
    BG_TABLE = 0x803C5EA8u,
    BGW = 0x80018000u,
    BGD = 0x80018100u,
    TRIANGLES = 0x80019000u,
    ATTRIBUTES = 0x80019400u,
    FIRST_PLANE = 0x80019800u,
    NEXT_PLANE = 0x80019810u,
    EVENT_MODE = 0x803C9EA2u,
    MENU_PAUSE = 0x803F7097u,
    POS = 0x1F8u,
    DEMO_TYPE = 0x304u,
    DEMO_MODE = 0x314u,
    ACCH_FLAGS = 0x494u,
    ACCH_POS = 0x498u,
    LINE_POLY = 0x644u,
    POLY = 0x940u,
    PROC = 0x31D8u,
    FRONT_TYPE = 0x34B9u,
    STICK = 0x35B0u,
    WATER = 0x35D4u,
    MODE = 0x3618u,
};

static CPUState cpu;

static void set_env(const char* name, const char* value) {
#if defined(_WIN32)
    assert(_putenv_s(name, value) == 0);
#else
    assert(setenv(name, value, 1) == 0);
#endif
}

static void put_float(u32 address, float value) {
    u32 bits;
    memcpy(&bits, &value, sizeof bits);
    mem_write32(&cpu, address, bits);
}

static void ticks(unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        bluewake_climb_retrace(&cpu);
}

static void reset(const char* duration) {
    memset(cpu.ram, 0, cpu.ram_size);
    memset(cpu.gpr, 0, sizeof cpu.gpr);
    cpu.exception = 0;
    mem_write32(&cpu, PLAYER_SLOT, PLAYER);
    mem_write32(&cpu, PLAYER + ACCH_POS, PLAYER + POS);
    mem_write32(&cpu, PLAYER + ACCH_FLAGS, 0x20u);
    mem_write32(&cpu, PLAYER + PROC, 4u);
    mem_write32(&cpu, PLAYER + 0x29Cu, 0x100u);
    mem_write8(&cpu, PLAYER + FRONT_TYPE, 1u);
    put_float(PLAYER + POS + 4u, 200.0f);
    put_float(PLAYER + WATER, -1000.0f);
    put_float(PLAYER + STICK, 1.0f);
    for (u32 i = 0; i < 12u; ++i) {
        mem_write8(&cpu, PLAYER + LINE_POLY + i, (u8)(0x20u + i));
        mem_write8(&cpu, PLAYER + POLY + i, 0xA5u);
    }
    set_env("BLUEWAKE_CLIMB", "1");
    set_env("BLUEWAKE_CLIMB_STAMINA", duration);
    set_env("BLUEWAKE_CLIMB_TRACE", "0");
    bluewake_climb_attach(&cpu);
    /* Establish the actor/scene identity before exercising a collision pair. */
    ticks(1u);
}

static bool hud(float* fraction, bool* exhausted) {
    float x, y, aspect, alpha;
    const bool visible = bluewake_climb_hud(fraction, exhausted, &x, &y, &aspect, &alpha);
    if (visible) {
        assert(isfinite(*fraction) && *fraction >= 0.0f && *fraction <= 1.0f);
        assert(isfinite(x) && isfinite(y) && isfinite(aspect) && isfinite(alpha));
    }
    return visible;
}

static float fraction(void) {
    float value = 1.0f;
    bool exhausted = false;
    assert(hud(&value, &exhausted));
    return value;
}

static void close_to(float actual, float expected) {
    assert(fabsf(actual - expected) < 0.002f);
}

static void classify(u32 owner, u32 code) {
    cpu.gpr[29] = owner;
    cpu.gpr[3] = code;
    bluewake_climb_hook(&cpu, FRONT_CODE);
}

static void above(u32 owner, bool hit) {
    cpu.gpr[29] = owner;
    cpu.gpr[3] = hit ? 1u : 0u;
    bluewake_climb_hook(&cpu, FRONT_ABOVE);
}

static void plain_unchanged(void) {
    assert(mem_read8(&cpu, PLAYER + FRONT_TYPE) == 1u);
    for (u32 i = 0; i < 12u; ++i)
        assert(mem_read8(&cpu, PLAYER + POLY + i) == 0xA5u);
}

static void grab(void) {
    classify(PLAYER, 0u);
    /* The higher line probe legitimately replaces the first collision record. */
    for (u32 i = 0; i < 12u; ++i)
        mem_write8(&cpu, PLAYER + LINE_POLY + i, (u8)(0x70u + i));
    above(PLAYER, true);
    assert(mem_read8(&cpu, PLAYER + FRONT_TYPE) == 3u);
    for (u32 i = 0; i < 12u; ++i)
        assert(mem_read8(&cpu, PLAYER + POLY + i) == (u8)(0x20u + i));
}

static u32 collision(u32 owner, u32 code) {
    cpu.gpr[31] = owner;
    cpu.gpr[3] = code;
    bluewake_climb_hook(&cpu, CLIMB_CODE);
    return cpu.gpr[3];
}

static void climbing(float stick) {
    mem_write32(&cpu, PLAYER + PROC, 0x3Fu);
    mem_write32(&cpu, PLAYER + MODE, 0x10000u);
    mem_write32(&cpu, PLAYER + ACCH_FLAGS, 0u);
    put_float(PLAYER + STICK, stick);
}

static void climb_ticks(unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        if ((i & 1u) == 0u)
            (void)collision(PLAYER, 0u); /* The game queries at 30 Hz. */
        ticks(1u);
    }
}

static void stamina_and_ivy(void) {
    reset("1");
    grab();
    climbing(1.0f);
    climb_ticks(30u);
    close_to(fraction(), 0.5f);
    for (unsigned i = 0; i < 5u; ++i) {
        cpu.gpr[3] = PLAYER;
        bluewake_climb_hook(&cpu, 0x80122D30u);
    }
    close_to(fraction(), 0.5f); /* Legacy dispatches must not charge VI time twice. */
    climb_ticks(31u); /* Allow a single tick for double rounding at zero. */
    float left;
    bool exhausted;
    assert(hud(&left, &exhausted) && exhausted && left == 0.0f);
    assert(collision(PLAYER, 0u) == 0u); /* Let the game perform its normal fall. */
    assert(collision(PLAYER, 1u) == 1u); /* Real ivy remains usable even when empty. */
    mem_write8(&cpu, PLAYER + FRONT_TYPE, 1u);
    classify(PLAYER, 0u);
    above(PLAYER, true);
    assert(mem_read8(&cpu, PLAYER + FRONT_TYPE) == 1u);

    reset("1");
    grab();
    climbing(0.0f);
    climb_ticks(60u);
    close_to(fraction(), 0.6f); /* Hanging costs 40% rather than a full wheel. */
    climb_ticks(91u);
    assert(hud(&left, &exhausted) && exhausted && left == 0.0f);

    reset("1");
    climbing(1.0f);
    for (unsigned i = 0; i < 120u; ++i) {
        assert(collision(PLAYER, 1u) == 1u);
        ticks(1u);
    }
    assert(!hud(&left, &exhausted)); /* Initial/native ivy never creates a wheel. */

    reset("1");
    grab();
    climbing(1.0f);
    climb_ticks(30u);
    assert(collision(PLAYER, 1u) == 1u);
    ticks(120u);
    close_to(fraction(), 0.5f); /* Switching to real ivy also clears old contact. */
}

static void contact_and_candidates(void) {
    reset("1");
    classify(PLAYER + 4u, 0u);
    above(PLAYER, true);
    plain_unchanged();

    reset("1");
    classify(PLAYER, 0u);
    above(PLAYER + 4u, true);
    above(PLAYER, true);
    plain_unchanged(); /* A rejected owner consumes the candidate too. */

    reset("1");
    classify(PLAYER, 0u);
    ticks(5u);
    above(PLAYER, true);
    plain_unchanged();

    reset("1");
    classify(PLAYER, 0u);
    above(PLAYER, false);
    above(PLAYER, true);
    plain_unchanged();

    reset("1");
    classify(PLAYER, 0u);
    bluewake_climb_reload();
    above(PLAYER, true);
    plain_unchanged();

    reset("1");
    climbing(1.0f);
    assert(collision(PLAYER + 4u, 0u) == 0u);
    ticks(60u);
    float left;
    bool exhausted;
    assert(!hud(&left, &exhausted));

    reset("1");
    grab();
    climbing(1.0f);
    climb_ticks(30u);
    ticks(6u); /* A missed wall query expires rather than sustaining a climb. */
    const float expired = fraction();
    assert(expired >= 0.43f && expired <= 0.5f);
    ticks(120u);
    close_to(fraction(), expired);

    reset("1");
    classify(PLAYER, 0u);
    mem_write32(&cpu, PLAYER_SLOT, PLAYER + 0x4000u);
    ticks(1u);
    mem_write32(&cpu, PLAYER_SLOT, PLAYER);
    above(PLAYER, true);
    plain_unchanged();

    reset("1");
    classify(PLAYER, 0u);
    mem_write32(&cpu, 0x803C9D3Cu, 0x4F757473u); /* A different stage invalidates the pair. */
    ticks(1u);
    above(PLAYER, true);
    plain_unchanged();
}

static void refused_grabs(void) {
    static const float bad_sticks[] = {0.0f, 0.15f, NAN, INFINITY};
    for (unsigned i = 0; i < sizeof bad_sticks / sizeof bad_sticks[0]; ++i) {
        reset("1");
        put_float(PLAYER + STICK, bad_sticks[i]);
        classify(PLAYER, 0u);
        above(PLAYER, true);
        plain_unchanged();
    }
    for (unsigned state = 0; state < 8u; ++state) {
        reset("1");
        switch (state) {
        case 0: mem_write8(&cpu, EVENT_MODE, 2u); break;
        case 1: mem_write8(&cpu, MENU_PAUSE, 1u); break;
        case 2: mem_write16(&cpu, PLAYER + DEMO_TYPE, 1u); break;
        case 3: mem_write32(&cpu, PLAYER + DEMO_MODE, 4u); break;
        case 4: mem_write32(&cpu, PLAYER + ACCH_POS, 0u); break;
        case 5: mem_write32(&cpu, PLAYER + 0x29Cu, 0u); break;
        case 6: mem_write32(&cpu, PLAYER + 0x2A0u, 0x02000000u); break;
        case 7: put_float(PLAYER + WATER, 100.0f); break;
        }
        classify(PLAYER, 0u);
        above(PLAYER, true);
        plain_unchanged();
    }
    reset("1");
    mem_write32(&cpu, PLAYER_SLOT, 0x817FFFFCu);
    classify(0x817FFFFCu, 0u);
    above(0x817FFFFCu, true);
    assert(cpu.exception == 0u); /* Bound the whole player layout before reads. */
}

static void refill_and_pause(void) {
    reset("1");
    grab();
    climbing(1.0f);
    climb_ticks(30u);
    const float half = fraction();
    for (unsigned state = 0; state < 4u; ++state) {
        mem_write32(&cpu, PLAYER + PROC, state == 0u ? 0x3Cu : 4u);
        mem_write32(&cpu, PLAYER + ACCH_FLAGS, state == 1u ? 0u : 0x20u);
        mem_write32(&cpu, PLAYER + MODE, state == 2u ? 2u : state == 3u ? 0x40000u : 0u);
        ticks(240u);
        close_to(fraction(), half);
    }
    climbing(1.0f);
    (void)collision(PLAYER, 0u);
    mem_write8(&cpu, MENU_PAUSE, 1u);
    ticks(120u);
    mem_write8(&cpu, MENU_PAUSE, 0u);
    ticks(1u);
    close_to(fraction(), half); /* Pause neither drains nor refills old contact. */
    (void)collision(PLAYER, 0u);
    mem_write16(&cpu, PLAYER + DEMO_TYPE, 1u);
    ticks(120u);
    mem_write16(&cpu, PLAYER + DEMO_TYPE, 0u);
    ticks(1u);
    close_to(fraction(), half);

    reset("1");
    grab();
    climbing(1.0f);
    climb_ticks(61u);
    mem_write32(&cpu, PLAYER + PROC, 4u);
    mem_write32(&cpu, PLAYER + MODE, 0u);
    mem_write32(&cpu, PLAYER + ACCH_FLAGS, 0x20u);
    ticks(30u);
    close_to(fraction(), 0.0f);
    ticks(90u);
    close_to(fraction(), 0.5f);
    mem_write8(&cpu, PLAYER + FRONT_TYPE, 1u);
    classify(PLAYER, 0u);
    above(PLAYER, true);
    assert(mem_read8(&cpu, PLAYER + FRONT_TYPE) == 1u); /* Empty means wait for a full refill. */
    ticks(91u);
    float left = 1.0f;
    bool exhausted = true;
    (void)hud(&left, &exhausted);
    close_to(left, 1.0f);
    assert(!exhausted);
    mem_write8(&cpu, PLAYER + FRONT_TYPE, 1u);
    classify(PLAYER, 0u);
    above(PLAYER, true);
    assert(mem_read8(&cpu, PLAYER + FRONT_TYPE) == 3u);
}

static void lifecycle_and_settings(void) {
    reset("1");
    grab();
    climbing(1.0f);
    climb_ticks(30u);
    bluewake_climb_reload();
    ticks(60u);
    close_to(fraction(), 0.5f); /* Reload drops contact rather than charging ghosts. */
    set_env("BLUEWAKE_CLIMB", "0");
    bluewake_climb_reload();
    float left;
    bool exhausted;
    assert(!hud(&left, &exhausted));
    set_env("BLUEWAKE_CLIMB", "1");
    bluewake_climb_reload();
    ticks(60u);
    assert(!hud(&left, &exhausted));
    assert(collision(PLAYER, 0u) == 1u);
    ticks(30u);
    close_to(fraction(), 1.0f - 4.0f / 60.0f); /* Contact expires after four VI ticks. */
    bluewake_climb_attach(&cpu);
    ticks(60u);
    assert(!hud(&left, &exhausted));

    static const char* invalid[] = {"nan", "inf", "-1", "0", "nonsense"};
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        reset(invalid[i]);
        grab();
        climbing(1.0f);
        climb_ticks(60u);
        close_to(fraction(), 11.0f / 12.0f); /* Invalid duration falls back to 12 s. */
        assert(hud(&left, &exhausted) && !exhausted);
    }
}

static void camera_fallback(void) {
    const u32 camera = 0x80018000u;
    reset("1");
    mem_write32(&cpu, 0x803CA718u, camera);
    put_float(camera + 0xD0u, 45.0f);
    put_float(camera + 0xD4u, 16.0f / 9.0f);
    put_float(camera + 0xDCu, 100.0f);
    put_float(camera + 0xE0u, -1000.0f);
    put_float(camera + 0xE8u, 100.0f);
    grab();
    climbing(1.0f);
    climb_ticks(2u);
    float left, x, y, aspect, alpha;
    bool exhausted;
    assert(bluewake_climb_hud(&left, &exhausted, &x, &y, &aspect, &alpha));
    close_to(x, 0.5f);
    assert(y < 0.4f && isfinite(y));
    close_to(aspect, 16.0f / 9.0f);
    put_float(camera + 0xE0u, NAN);
    ticks(1u);
    assert(bluewake_climb_hud(&left, &exhausted, &x, &y, &aspect, &alpha));
    close_to(x, 0.5f);
    close_to(y, 0.45f); /* A bad camera uses the centered wheel, never stale coordinates. */
}

/* The geometry hooks read the game's collision registry, rather than trusting
 * whatever plane pointer happens to be in r3. Keep this small registry valid
 * except for the specific corruption each test introduces. */
static void plane(u32 address, float x, float y, float z) {
    put_float(address, x);
    put_float(address + 4u, y);
    put_float(address + 8u, z);
    put_float(address + 12u, 123.0f);
}

static void polygon(u32 address, u16 index, u16 background) {
    mem_write16(&cpu, address, index);
    mem_write16(&cpu, address + 2u, background);
}

static void wall_code(unsigned index, unsigned code) {
    /* Other collision properties must not affect or be rewritten by the mod. */
    mem_write32(&cpu, ATTRIBUTES + 16u * index + 4u, 0xA500005Au | (code << 8));
}

static void geometry_reset(void) {
    reset("1");
    mem_write32(&cpu, BG_TABLE, BGW);
    mem_write32(&cpu, BG_TABLE + 4u, 1u);
    mem_write32(&cpu, BGW + 0x94u, BGD);
    mem_write32(&cpu, BGD + 8u, 2u);
    mem_write32(&cpu, BGD + 12u, TRIANGLES);
    mem_write32(&cpu, BGD + 0x28u, 2u);
    mem_write32(&cpu, BGD + 0x2Cu, ATTRIBUTES);
    mem_write16(&cpu, TRIANGLES + 6u, 0u);
    mem_write16(&cpu, TRIANGLES + 10u + 6u, 1u);
    wall_code(0u, 0u);
    wall_code(1u, 0u);
    polygon(PLAYER + LINE_POLY, 0u, 0u);
    polygon(PLAYER + POLY, 0u, 0u);
    plane(FIRST_PLANE, sqrtf(0.96f), 0.2f, 0.0f);
    plane(NEXT_PLANE, sqrtf(0.96f), 0.2f, 0.0f);
}

static u32 front_plane(u32 owner, u32 address) {
    cpu.gpr[29] = owner;
    cpu.gpr[3] = address;
    cpu.gpr[31] = 0xABCD1234u;
    cpu.pc = FRONT_PLANE;
    bluewake_climb_hook(&cpu, FRONT_PLANE);
    assert(cpu.exception == 0u);
    return cpu.pc;
}

static u32 reprobe(u32 owner, u32 first, u32 next) {
    cpu.gpr[29] = owner;
    cpu.gpr[31] = first;
    cpu.gpr[3] = next;
    cpu.pc = FRONT_REPROBE;
    bluewake_climb_hook(&cpu, FRONT_REPROBE);
    assert(cpu.exception == 0u);
    assert(cpu.gpr[31] == first && cpu.gpr[3] == next);
    return cpu.pc;
}

static u32 climb_plane(u32 owner, u32 address) {
    cpu.gpr[31] = owner;
    cpu.gpr[3] = address;
    cpu.pc = CLIMB_PLANE;
    bluewake_climb_hook(&cpu, CLIMB_PLANE);
    assert(cpu.exception == 0u);
    assert(cpu.gpr[31] == owner && cpu.gpr[3] == address);
    return cpu.pc;
}

static void geometry_inclines_and_codes(void) {
    static const float accepted_y[] = {0.2f, -0.2f, 0.5f, -0.5f};
    for (unsigned i = 0; i < sizeof accepted_y / sizeof accepted_y[0]; ++i) {
        geometry_reset();
        const float y = accepted_y[i];
        plane(FIRST_PLANE, sqrtf(1.0f - y * y), y, 0.0f);
        assert(front_plane(PLAYER, FIRST_PLANE) == FRONT_PLANE_NEXT);
        assert(cpu.gpr[31] == FIRST_PLANE); /* The skipped native block set r31. */
        assert(mem_read32(&cpu, ATTRIBUTES + 4u) == 0xA500005Au);
        assert(mem_read32(&cpu, FIRST_PLANE + 12u) == 0x42F60000u);
        assert(collision(PLAYER, 0u) == 1u);
        assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE_NEXT);
    }

    geometry_reset();
    plane(FIRST_PLANE, 1.0f, 0.0f, 0.0f);
    assert(front_plane(PLAYER, FIRST_PLANE) == FRONT_PLANE);
    assert(cpu.gpr[31] == 0xABCD1234u); /* Native flat-wall code still runs. */
    assert(collision(PLAYER, 0u) == 1u);
    assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);

    for (unsigned code = 1u; code <= 15u; ++code) {
        geometry_reset();
        wall_code(0u, code);
        assert(front_plane(PLAYER, FIRST_PLANE) == FRONT_PLANE);
        assert(cpu.gpr[31] == 0xABCD1234u);
        assert(collision(PLAYER, code) == code);
        assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);
    }

    static const float invalid[][3] = {
        {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.865f, 0.501f, 0.0f}, {0.865f, -0.501f, 0.0f},
        {2.0f, 0.2f, 0.0f}, {0.1f, 0.01f, 0.0f},
        {NAN, 0.2f, 0.0f}, {0.9f, NAN, 0.0f}, {0.9f, 0.2f, INFINITY},
    };
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        geometry_reset();
        plane(FIRST_PLANE, invalid[i][0], invalid[i][1], invalid[i][2]);
        assert(front_plane(PLAYER, FIRST_PLANE) == FRONT_PLANE);
        assert(cpu.gpr[31] == 0xABCD1234u);
        assert(collision(PLAYER, 0u) == 1u);
        assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);
    }

    static const u32 invalid_pointers[] = {0u, 0x7FFFFFFCu, 0x817FFFF8u, 0xFFFFFFFFu};
    for (unsigned i = 0; i < sizeof invalid_pointers / sizeof invalid_pointers[0]; ++i) {
        geometry_reset();
        assert(front_plane(PLAYER, invalid_pointers[i]) == FRONT_PLANE);
        assert(collision(PLAYER, 0u) == 1u);
        assert(climb_plane(PLAYER, invalid_pointers[i]) == CLIMB_PLANE);
    }
}

static void geometry_registry_bounds(void) {
    for (unsigned corruption = 0; corruption < 14u; ++corruption) {
        geometry_reset();
        switch (corruption) {
        case 0: polygon(PLAYER + LINE_POLY, 0u, 0xFFFFu); break;
        case 1: mem_write32(&cpu, BG_TABLE + 4u, 0u); break;
        case 2: mem_write32(&cpu, BG_TABLE, 0x817FFFF0u); break;
        case 3: mem_write32(&cpu, BGW + 0x94u, 0x817FFFF0u); break;
        case 4: mem_write32(&cpu, BGD + 8u, 0u); break;
        case 5: polygon(PLAYER + LINE_POLY, 2u, 0u); break;
        case 6: mem_write32(&cpu, BGD + 8u, 0xFFFFFFFFu); break;
        case 7: mem_write32(&cpu, BGD + 12u, 0x817FFFF8u); break;
        case 8: mem_write32(&cpu, BGD + 12u, 0u); break;
        case 9: mem_write16(&cpu, TRIANGLES + 6u, 2u); break;
        case 10: mem_write32(&cpu, BGD + 0x28u, 0u); break;
        case 11: mem_write32(&cpu, BGD + 0x28u, 0xFFFFFFFFu); break;
        case 12: mem_write32(&cpu, BGD + 0x2Cu, 0x817FFFF8u); break;
        case 13: mem_write32(&cpu, BGD + 0x2Cu, 0u); break;
        }
        assert(front_plane(PLAYER, FIRST_PLANE) == FRONT_PLANE);
        assert(cpu.gpr[31] == 0xABCD1234u);
        /* Copy the malformed support into the active climb record as well. */
        for (u32 i = 0; i < 12u; ++i)
            mem_write8(&cpu, PLAYER + POLY + i, mem_read8(&cpu, PLAYER + LINE_POLY + i));
        assert(collision(PLAYER, 0u) == 1u);
        assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);
    }
}

static void geometry_facet_changes(void) {
    static const float degrees[] = {10.0f, 25.0f, 30.0f, 30.1f, 90.0f, 180.0f};
    for (unsigned i = 0; i < sizeof degrees / sizeof degrees[0]; ++i) {
        geometry_reset();
        plane(FIRST_PLANE, 1.0f, 0.0f, 0.0f);
        assert(front_plane(PLAYER, FIRST_PLANE) == FRONT_PLANE);
        polygon(PLAYER + LINE_POLY, 1u, 0u); /* A new triangle of the same wall. */
        const float angle = degrees[i] * 3.14159265f / 180.0f;
        plane(NEXT_PLANE, cosf(angle), 0.0f, sinf(angle));
        assert(reprobe(PLAYER, FIRST_PLANE, NEXT_PLANE) ==
               (degrees[i] <= 30.0f ? FRONT_REPROBE_NEXT : FRONT_REPROBE));
    }

    for (unsigned denial = 0; denial < 10u; ++denial) {
        geometry_reset();
        plane(FIRST_PLANE, 1.0f, 0.0f, 0.0f);
        plane(NEXT_PLANE, 0.98f, 0.0f, 0.2f);
        if (denial != 0u)
            (void)front_plane(PLAYER, FIRST_PLANE);
        u32 owner = PLAYER, first = FIRST_PLANE, next = NEXT_PLANE;
        switch (denial) {
        case 0: break; /* A return without a matching first hit. */
        case 1: owner += 4u; break;
        case 2: first += 16u; break;
        case 3: ticks(3u); break;
        case 4: next = 0u; break; /* A missing actual plane is never a hit. */
        case 5:
            mem_write32(&cpu, BG_TABLE + 20u, BGW);
            mem_write32(&cpu, BG_TABLE + 24u, 1u);
            polygon(PLAYER + LINE_POLY, 1u, 1u);
            break;
        case 6: mem_write32(&cpu, BG_TABLE, BGW + 0x400u); break;
        case 7: wall_code(0u, 1u); break; /* Native ivy keeps its own geometry. */
        case 8: plane(NEXT_PLANE, 0.0f, 1.0f, 0.0f); break;
        case 9: plane(NEXT_PLANE, NAN, 0.0f, 0.0f); break;
        }
        assert(reprobe(owner, first, next) == FRONT_REPROBE);
    }
}

static void geometry_ownership_and_control(void) {
    for (unsigned state = 0; state < 10u; ++state) {
        geometry_reset();
        u32 owner = PLAYER;
        switch (state) {
        case 0: owner += 4u; break;
        case 1: mem_write8(&cpu, MENU_PAUSE, 1u); break;
        case 2: mem_write8(&cpu, EVENT_MODE, 2u); break;
        case 3: mem_write16(&cpu, PLAYER + DEMO_TYPE, 1u); break;
        case 4: mem_write32(&cpu, PLAYER + DEMO_MODE, 1u); break;
        case 5: put_float(PLAYER + STICK, 0.15f); break;
        case 6: put_float(PLAYER + STICK, NAN); break;
        case 7: mem_write32(&cpu, PLAYER + MODE, 0x40000u); break;
        case 8: mem_write32(&cpu, PLAYER + ACCH_POS, 0u); break;
        case 9: set_env("BLUEWAKE_CLIMB", "0"); bluewake_climb_reload(); break;
        }
        assert(front_plane(owner, FIRST_PLANE) == FRONT_PLANE);
        assert(cpu.gpr[31] == 0xABCD1234u);
    }

    geometry_reset();
    assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE); /* No preceding code query. */
    climbing(0.0f);
    assert(collision(PLAYER, 0u) == 1u);
    assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE_NEXT); /* Hanging is still valid. */
    assert(climb_plane(PLAYER + 4u, FIRST_PLANE) == CLIMB_PLANE);
    ticks(5u);
    assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);

    for (unsigned state = 0; state < 5u; ++state) {
        geometry_reset();
        assert(collision(PLAYER, 0u) == 1u);
        switch (state) {
        case 0: mem_write8(&cpu, MENU_PAUSE, 1u); break;
        case 1: mem_write8(&cpu, EVENT_MODE, 2u); break;
        case 2: mem_write16(&cpu, PLAYER + DEMO_TYPE, 1u); break;
        case 3: mem_write32(&cpu, PLAYER + DEMO_MODE, 1u); break;
        case 4: mem_write32(&cpu, PLAYER + ACCH_POS, 0u); break;
        }
        assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);
    }

    geometry_reset();
    (void)front_plane(PLAYER, FIRST_PLANE);
    mem_write32(&cpu, 0x803C9D3Cu, 0x4F757473u);
    ticks(1u);
    assert(reprobe(PLAYER, FIRST_PLANE, NEXT_PLANE) == FRONT_REPROBE);

    for (unsigned word = 0u; word < 2u; ++word) {
        geometry_reset();
        assert(collision(PLAYER, 0u) == 1u);
        mem_write32(&cpu, 0x803C9D3Cu + 4u * word, 0x4F757473u);
        /* Contact must expire even before the next VI notices a new scene. */
        assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);
    }

    geometry_reset();
    climbing(1.0f);
    climb_ticks(61u);
    assert(collision(PLAYER, 0u) == 0u);
    assert(front_plane(PLAYER, FIRST_PLANE) == FRONT_PLANE);
    assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);
}

static void geometry_repeated_collision(void) {
    geometry_reset();
    climbing(1.0f);
    assert(collision(PLAYER, 0u) == 1u);
    /* A cycle-budget exit can service this return once and then resume at the
     * same return next dispatch. r3 already carries the mod's synthetic ivy. */
    cpu.pc = CLIMB_CODE;
    bluewake_climb_hook(&cpu, CLIMB_CODE);
    bluewake_climb_hook(&cpu, CLIMB_CODE);
    assert(cpu.gpr[3] == 1u && cpu.gpr[31] == PLAYER && cpu.pc == CLIMB_CODE);
    assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE_NEXT);
    ticks(1u);
    close_to(fraction(), 1.0f - 1.0f / 60.0f);

    wall_code(0u, 1u); /* A fresh native ivy return must clear synthetic contact. */
    assert(collision(PLAYER, 1u) == 1u);
    assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);
    const float after_native_ivy = fraction();
    ticks(120u);
    close_to(fraction(), after_native_ivy);

    geometry_reset();
    climbing(1.0f);
    wall_code(0u, 1u);
    assert(collision(PLAYER, 1u) == 1u);
    bluewake_climb_hook(&cpu, CLIMB_CODE);
    assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);
    ticks(60u);
    float left;
    bool exhausted;
    assert(!hud(&left, &exhausted)); /* Initial native ivy never acquires a wheel. */

    geometry_reset();
    climbing(1.0f);
    assert(collision(PLAYER, 0u) == 1u);
    ticks(5u);
    const float expired = fraction();
    assert(collision(PLAYER, 1u) == 1u);
    assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);
    ticks(60u);
    close_to(fraction(), expired); /* An old rewritten value is not fresh support. */

    for (unsigned word = 0u; word < 2u; ++word) {
        geometry_reset();
        assert(collision(PLAYER, 0u) == 1u);
        mem_write32(&cpu, 0x803C9D3Cu + 4u * word, 0x4F757473u);
        assert(collision(PLAYER, 1u) == 1u);
        assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);
    }

    geometry_reset();
    assert(collision(PLAYER, 0u) == 1u);
    const u32 replacement = PLAYER + 0x4000u;
    for (u32 i = 0u; i < MODE + 4u; ++i)
        mem_write8(&cpu, replacement + i, mem_read8(&cpu, PLAYER + i));
    mem_write32(&cpu, replacement + ACCH_POS, replacement + POS);
    mem_write32(&cpu, PLAYER_SLOT, replacement);
    assert(collision(replacement, 1u) == 1u);
    assert(climb_plane(replacement, FIRST_PLANE) == CLIMB_PLANE);

    geometry_reset();
    climbing(1.0f);
    climb_ticks(60u);
    assert(cpu.gpr[3] == 1u); /* Last guest return was rewritten before exhaustion. */
    assert(hud(&left, &exhausted) && exhausted && left == 0.0f);
    cpu.pc = CLIMB_CODE;
    bluewake_climb_hook(&cpu, CLIMB_CODE);
    assert(cpu.gpr[3] == 0u); /* Resuming the same return must now permit native fall. */
    assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);

    geometry_reset();
    assert(collision(PLAYER, 0u) == 1u);
    mem_write32(&cpu, BG_TABLE + 4u, 0u);
    assert(collision(PLAYER, 1u) == 1u);
    assert(climb_plane(PLAYER, FIRST_PLANE) == CLIMB_PLANE);
}

int main(void) {
    assert(cpu_init(&cpu));
    stamina_and_ivy();
    contact_and_candidates();
    refused_grabs();
    refill_and_pause();
    lifecycle_and_settings();
    camera_fallback();
    geometry_inclines_and_codes();
    geometry_registry_bounds();
    geometry_facet_changes();
    geometry_ownership_and_control();
    geometry_repeated_collision();
    cpu_free(&cpu);
    puts("Climbing guest hooks: stamina, ivy, ownership, control, refill, sloped walls, facet seams and collision bounds passed");
    return 0;
}
