// SPDX-License-Identifier: GPL-3.0-or-later
/* Exercise production dialogue hooks using isolated guest RAM. The fixture
 * supplies native character-store results and models the primary-source
 * JMessage ready loop and MSG allowance/wait parser. When an own-disc prepared
 * tree exists, extracted real optimized calls qualify the dynamic host watch
 * handshake. Font results and the remaining native parser are synthetic. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "dialogue_speed.h"
#include "game_events.h"

#include <assert.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#ifdef BLUEWAKE_DIALOGUE_OPTIMIZED_CALL_FIXTURE
#include "dispatch_loop.h"
#endif

enum {
    RAM_SIZE = 0x01800000u,
    PROCESSOR = 0x80010000u, CONTROL = 0x80011000u,
    MESSAGE = 0x80012000u, ENTRY = 0x80013000u,
    TEXT = 0x80014000u, STACK = 0x80020000u,
    CHARACTER = 0x801E0888u, RETURN = 0x8029F360u,
    SEQUENCE_SLOT = 0x803F7018u, CONTROL_SLOT = 0x803F7014u,
    MESSAGE_CODE = 0x803F7030u, HEADER = 0x803F703Cu,
    WIDE = 0x803F7040u, STAGE = 0x803C9D3Cu,
    NEXT_ENABLED = 0x803C9D54u, ROOM = 0x803F6A78u,
    OVERLAP = 0x803F6160u, PAUSE = 0x803F7097u
};
static CPUState cpu;
static uint8_t* ram_copy;

static void write32(uint32_t address, uint32_t value) { mem_write32(&cpu, address, value); }
static void write8(uint32_t address, uint8_t value) { mem_write8(&cpu, address, value); }
static uint32_t wait(void) { return mem_read32(&cpu, PROCESSOR + 0x90u); }
static BwDialogueSpeedStats stats(void) {
    BwDialogueSpeedStats result;
    bluewake_dialogue_speed_stats(&result);
    return result;
}
static void reset(float multiplier) {
    memset(cpu.ram, 0, cpu.ram_size);
    memset(cpu.gpr, 0, sizeof cpu.gpr);
    cpu.exception = 0;
    cpu.pc = CHARACTER;
    cpu.lr = RETURN;
    cpu.gpr[1] = STACK;
    cpu.downcount = -50;
    cpu.cycle_budget = 50;
    write32(SEQUENCE_SLOT, PROCESSOR);
    write32(CONTROL_SLOT, CONTROL);
    write32(PROCESSOR, 0x803930FCu);
    write32(PROCESSOR + 4u, CONTROL);
    write32(PROCESSOR + 8u, TEXT);
    write32(PROCESSOR + 0x38u, MESSAGE);
    write32(PROCESSOR + 0x8Cu, 1u);
    write32(CONTROL, 0x80393150u);
    write32(CONTROL + 0xCu, PROCESSOR);
    write32(CONTROL + 0x18u, ENTRY);
    write32(CONTROL + 0x68u, 'a');
    write8(MESSAGE + 0x164u, 6u);
    write32(MESSAGE_CODE, 0x1234u);
    memcpy(cpu.ram + (STAGE - 0x80000000u), "sea\0\0\0\0\0", 8);
    write8(ROOM, 44);
    bluewake_game_events_attach(&cpu);
    assert(bluewake_dialogue_speed_configure(multiplier));
    bluewake_dialogue_speed_attach(&cpu);
}
static void entry(uint32_t stack, bool mirror) {
    cpu.gpr[1] = stack;
    cpu.gpr[3] = PROCESSOR;
    cpu.lr = RETURN | (mirror ? 0x40000000u : 0u);
    cpu.pc = CHARACTER | (mirror ? 0x40000000u : 0u);
    bluewake_dialogue_speed_dispatch(&cpu, cpu.pc);
}
static void finish(uint32_t stack, uint32_t native_wait, bool mirror) {
    write32(PROCESSOR + 0x90u, native_wait);
    cpu.gpr[1] = stack;
    cpu.gpr[30] = PROCESSOR;
    cpu.lr = RETURN | (mirror ? 0x40000000u : 0u);
    cpu.pc = RETURN | (mirror ? 0x40000000u : 0u);
    bluewake_dialogue_speed_dispatch(&cpu, cpu.pc);
}
static void character(uint32_t native_wait) {
    write32(PROCESSOR + 8u, mem_read32(&cpu, PROCESSOR + 8u) + 1u);
    entry(STACK, false);
    finish(STACK, native_wait, false);
}
static void assert_cpu_unchanged(const CPUState* before) {
    assert(memcmp(&cpu, before, sizeof cpu) == 0);
}
static void test_native_and_config(void) {
    reset(1);
    assert(bluewake_dialogue_speed_multiplier() == 1);
    assert(!bluewake_dialogue_speed_observes(CHARACTER));
    assert(!bluewake_dialogue_speed_observes(RETURN));
    write32(PROCESSOR + 0x90u, 37u);
    cpu.gpr[3] = PROCESSOR;
    cpu.gpr[30] = PROCESSOR;
    const CPUState before = cpu;
    memcpy(ram_copy, cpu.ram, cpu.ram_size);
    bluewake_dialogue_speed_dispatch(&cpu, CHARACTER);
    bluewake_dialogue_speed_dispatch(&cpu, RETURN);
    bluewake_dialogue_speed_dispatch(&cpu, RETURN);
    assert_cpu_unchanged(&before);
    assert(memcmp(ram_copy, cpu.ram, cpu.ram_size) == 0);
    assert(stats().entries == 0 && stats().scaled == 0);
    assert(!bluewake_dialogue_speed_configure(NAN));
    assert(!bluewake_dialogue_speed_configure(INFINITY));
    assert(!bluewake_dialogue_speed_configure(0));
    assert(!bluewake_dialogue_speed_configure(10.1f));
    assert(bluewake_dialogue_speed_multiplier() == 1);
}
static void test_scaling_and_replay(void) {
    reset(2);
    assert(bluewake_dialogue_speed_observes(CHARACTER));
    assert(!bluewake_dialogue_speed_observes(RETURN));
    entry(STACK, false);
    /* An exhausted initial PC and its later edge repeat the same invocation. */
    entry(STACK, false);
    assert(stats().entries == 1 && bluewake_dialogue_speed_observes(RETURN));
    write32(PROCESSOR + 0x90u, 1);
    cpu.gpr[30] = PROCESSOR;
    cpu.pc = RETURN;
    const CPUState before = cpu;
    bluewake_dialogue_speed_dispatch(&cpu, RETURN);
    assert(wait() == 0 && stats().completed == 1 && stats().scaled == 1);
    assert_cpu_unchanged(&before);
    assert(!bluewake_dialogue_speed_observes(RETURN));
    bluewake_dialogue_speed_dispatch(&cpu, RETURN);
    assert(wait() == 0 && stats().completed == 1 && stats().scaled == 1);
    character(1);
    assert(wait() == 1 && stats().completed == 2 && stats().scaled == 1);

    reset(10);
    unsigned total_wait = 0;
    for (unsigned i = 0; i < 100; ++i) { character(1); total_wait += wait(); }
    assert(total_wait == 10 && stats().completed == 100);
    reset(2.5f);
    total_wait = 0;
    for (unsigned i = 0; i < 25; ++i) { character(1); total_wait += wait(); }
    assert(total_wait == 10);

    reset(2);
    entry(STACK, true);
    assert(bluewake_dialogue_speed_observes(CHARACTER | 0x40000000u));
    assert(bluewake_dialogue_speed_observes(RETURN | 0x40000000u));
    finish(STACK, 1, true);
    assert(wait() == 0 && stats().completed == 1);
    assert(cpu.pc == (RETURN | 0x40000000u) && cpu.lr == cpu.pc);
}
static void test_native_ready_loop(void) {
    /* TSequenceProcessor::process tests on_isReady between characters. Native
     * do_isReady decrements a positive wait and returns false immediately;
     * a zero wait permits another character in the same native30Hz update. */
    const float factors[] = {1, 2, 10};
    for (unsigned f = 0; f < 3; ++f) {
        reset(factors[f]);
        unsigned shown = 0;
        for (unsigned frame = 0; frame < 10; ++frame) {
            for (unsigned safety = 0; safety < 20; ++safety) {
                if (wait() > 0) { write32(PROCESSOR + 0x90u, wait() - 1u); break; }
                character(1);
                ++shown;
            }
        }
        assert(shown == 10u * (unsigned)factors[f]);
    }
    reset(10);
    /* A scripted wait is not a character callback. Even an unrelated replay
     * at the generic return cannot reduce it; native readiness counts it. */
    write32(PROCESSOR + 0x90u, 7);
    write8(PROCESSOR + 0x160u, 1);
    cpu.pc = RETURN; cpu.gpr[30] = PROCESSOR;
    for (unsigned remaining = 7; remaining > 0; --remaining) {
        bluewake_dialogue_speed_dispatch(&cpu, RETURN);
        assert(wait() == remaining);
        write32(PROCESSOR + 0x90u, remaining - 1);
    }
}
static void test_native_guards(void) {
    const uint32_t flags[] = {0x15Fu, 0x160u, 0x161u};
    for (unsigned i = 0; i < 3; ++i) {
        reset(2);
        write8(PROCESSOR + flags[i], 1);
        entry(STACK, false); finish(STACK, 1, false);
        assert(wait() == 1 && stats().entries == 0);
        reset(2);
        entry(STACK, false);
        write8(PROCESSOR + flags[i], 1);
        finish(STACK, 1, false);
        assert(wait() == 1 && stats().completed == 0);
    }
    for (unsigned draw = 1; draw <= 3; ++draw) {
        reset(2); write8(ENTRY + 0xDu, (uint8_t)draw);
        entry(STACK, false); finish(STACK, 1, false);
        assert(wait() == 1 && stats().entries == 0);
    }
    reset(2); write32(PROCESSOR + 0x8Cu, 0);
    entry(STACK, false); finish(STACK, 0, false);
    assert(wait() == 0 && stats().entries == 0); /* Native fast span. */
    reset(2); entry(STACK, false); finish(STACK, 0, false);
    assert(wait() == 0 && stats().completed == 0); /* No positive store. */
    reset(2); entry(STACK, false); finish(STACK, 50, false);
    assert(wait() == 50 && stats().completed == 0); /* Stale/script wait. */
    reset(2); write8(HEADER, 1); entry(STACK, false); write8(WIDE, 1);
    finish(STACK, 1, false);
    assert(wait() == 1 && stats().completed == 0); /* First encoded half. */
    reset(2); write8(HEADER, 1); write8(WIDE, 1); entry(STACK, false);
    write8(WIDE, 0); finish(STACK, 1, false);
    assert(wait() == 0 && stats().completed == 1); /* Completed encoded glyph. */
    reset(2); entry(STACK, false); write8(MESSAGE + 0x164u, 5);
    finish(STACK, 1, false);
    assert(wait() == 1 && stats().completed == 0); /* Page prompt. */

    reset(2); write32(PROCESSOR + 0x8Cu, 4);
    character(4); assert(wait() == 2); /* Ordinary native store. */
    write32(CONTROL + 0x68u, 0x20u);
    character(1); assert(wait() == 0); /* Space store is always one tick. */
    write32(CONTROL + 0x68u, 0xAu);
    character(1); assert(wait() == 1); /* Nonterminal newline; page stop gated. */
    reset(2); write32(PROCESSOR + 0x8Cu, 0xFFFFFFFFu);
    entry(STACK, false); finish(STACK, 1, false);
    assert(wait() == 1 && stats().entries == 0);
    reset(2); write32(PROCESSOR + 0x8Cu, 256);
    entry(STACK, false); finish(STACK, 1, false);
    assert(wait() == 1 && stats().entries == 0);
}
static void test_identity_and_lifecycle(void) {
    const uint32_t changed[] = {
        SEQUENCE_SLOT, CONTROL_SLOT, PROCESSOR, PROCESSOR + 4u,
        PROCESSOR + 0x38u, CONTROL, CONTROL + 0xCu,
        CONTROL + 0x18u, MESSAGE_CODE, PROCESSOR + 8u,
        PROCESSOR + 0x8Cu, STAGE, STAGE + 4u, STAGE + 8u
    };
    for (unsigned i = 0; i < sizeof changed / sizeof changed[0]; ++i) {
        reset(2); entry(STACK, false);
        write32(changed[i], mem_read32(&cpu, changed[i]) + 4u);
        finish(STACK, 1, false);
        assert(wait() == 1 && stats().completed == 0);
    }
    const uint32_t blocked[] = {PAUSE, NEXT_ENABLED, ROOM, HEADER};
    for (unsigned i = 0; i < sizeof blocked / sizeof blocked[0]; ++i) {
        reset(2); entry(STACK, false);
        write8(blocked[i], mem_read8(&cpu, blocked[i]) + 1);
        finish(STACK, 1, false);
        assert(wait() == 1 && stats().completed == 0);
    }
    reset(2); entry(STACK, false); write32(OVERLAP, TEXT);
    finish(STACK, 1, false); assert(wait() == 1 && stats().completed == 0);
    reset(2); entry(STACK, false);
    bluewake_game_events_reset(&cpu, BW_GAME_RESET_STATE_LOAD);
    finish(STACK, 1, false); assert(wait() == 1 && stats().completed == 0);
    reset(2); entry(STACK, false);
    bluewake_dialogue_speed_reset(&cpu);
    finish(STACK, 1, false); assert(wait() == 1 && stats().completed == 0);
    reset(2); entry(STACK, false);
    assert(bluewake_dialogue_speed_configure(3));
    finish(STACK, 1, false); assert(wait() == 1 && stats().completed == 0);
    reset(2); entry(STACK, false);
    assert(bluewake_dialogue_speed_configure(1));
    assert(bluewake_dialogue_speed_configure(2));
    finish(STACK, 1, false); /* Same factor bits, different publication generation. */
    assert(wait() == 1 && stats().completed == 0);
    reset(2); entry(STACK, false);
    assert(!bluewake_dialogue_speed_configure(NAN));
    finish(STACK, 1, false);
    assert(wait() == 0 && stats().completed == 1); /* Invalid UI value publishes nothing. */
    reset(2); entry(STACK, false);
    assert(bluewake_dialogue_speed_configure(1));
    assert(bluewake_dialogue_speed_multiplier() == 1);
    assert(!bluewake_dialogue_speed_observes(CHARACTER));
    assert(bluewake_dialogue_speed_observes(RETURN)); /* Retain armed off-transition. */
    finish(STACK, 1, false);
    assert(wait() == 1 && !bluewake_dialogue_speed_observes(RETURN));
    reset(2); entry(STACK, false);
    write8(NEXT_ENABLED, 1);
    bluewake_game_events_retrace(&cpu);
    write8(NEXT_ENABLED, 0);
    finish(STACK, 1, false);
    assert(wait() == 1 && stats().completed == 0); /* Generation detects a transition round trip. */
    reset(2); entry(STACK, false);
    uint8_t* old_ram = cpu.ram;
    cpu.ram = malloc(cpu.ram_size); assert(cpu.ram != NULL);
    memcpy(cpu.ram, old_ram, cpu.ram_size);
    finish(STACK, 1, false);
    assert(wait() == 1 && stats().completed == 0);
    free(cpu.ram); cpu.ram = old_ram;
    bluewake_dialogue_speed_reset(&cpu);
    reset(2); entry(STACK, false); cpu.exception = PPC_EXC_PROGRAM;
    finish(STACK, 1, false); assert(wait() == 1 && stats().completed == 0);
    reset(2); entry(STACK, false); bluewake_dialogue_speed_reset(NULL);
    finish(STACK, 1, false);
    assert(wait() == 1 && !bluewake_dialogue_speed_observes(CHARACTER));
}

static atomic_bool setter_start, setter_done;
#ifdef _WIN32
static DWORD WINAPI setting_worker(LPVOID user) {
#else
static void* setting_worker(void* user) {
#endif
    (void)user;
    while (!atomic_load_explicit(&setter_start, memory_order_acquire)) {}
    for (unsigned i = 0; i < 20000; ++i) {
        assert(bluewake_dialogue_speed_configure((float)(1u + i % 10u)));
        const float value = bluewake_dialogue_speed_multiplier();
        assert(isfinite(value) && value >= 1 && value <= 10);
        assert(!bluewake_dialogue_speed_configure(NAN));
    }
    atomic_store_explicit(&setter_done, true, memory_order_release);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}
static void test_configuration_publication(void) {
    reset(2);
    atomic_store(&setter_start, false);
    atomic_store(&setter_done, false);
#ifdef _WIN32
    HANDLE worker = CreateThread(NULL, 0, setting_worker, NULL, 0, NULL);
    assert(worker != NULL);
#else
    pthread_t worker;
    assert(pthread_create(&worker, NULL, setting_worker, NULL) == 0);
#endif
    atomic_store_explicit(&setter_start, true, memory_order_release);
    unsigned iterations = 0;
    while (!atomic_load_explicit(&setter_done, memory_order_acquire) || iterations < 1000) {
        character(1);
        assert(wait() <= 1u && cpu.exception == 0);
        ++iterations;
    }
#ifdef _WIN32
    assert(WaitForSingleObject(worker, INFINITE) == WAIT_OBJECT_0);
    assert(CloseHandle(worker));
#else
    assert(pthread_join(worker, NULL) == 0);
#endif
    assert(bluewake_dialogue_speed_configure(1));
    bluewake_dialogue_speed_dispatch(&cpu, RETURN);
    assert(!bluewake_dialogue_speed_observes(CHARACTER));
    assert(!bluewake_dialogue_speed_observes(RETURN));
}
static void test_invalid_nested_and_owner(void) {
    reset(2);
    cpu.gpr[3] = PROCESSOR; cpu.gpr[1] = STACK; cpu.lr = RETURN + 4u;
    bluewake_dialogue_speed_dispatch(&cpu, CHARACTER);
    assert(stats().entries == 0);
    entry(STACK + 1u, false); assert(stats().entries == 0);
    entry(0x817FFFFCu, false); assert(stats().entries == 0);
    write32(SEQUENCE_SLOT, 0x817FFFFCu);
    cpu.gpr[3] = 0x817FFFFCu; cpu.lr = RETURN; cpu.gpr[1] = STACK;
    bluewake_dialogue_speed_dispatch(&cpu, CHARACTER);
    assert(stats().entries == 0 && cpu.exception == 0);
    reset(2); write32(CONTROL + 0x18u, 0x817FFFFCu);
    entry(STACK, false); assert(stats().entries == 0);
    reset(2); write32(PROCESSOR + 8u, 0x81800000u);
    entry(STACK, false); assert(stats().entries == 0);
    reset(2); entry(STACK, false);
    write32(PROCESSOR + 0x90u, 1); cpu.pc = RETURN; cpu.gpr[30] = PROCESSOR + 4u;
    bluewake_dialogue_speed_dispatch(&cpu, RETURN);
    assert(wait() == 1 && stats().completed == 0);
    cpu.gpr[30] = PROCESSOR; bluewake_dialogue_speed_dispatch(&cpu, RETURN);
    assert(wait() == 1 && stats().completed == 0); /* Wrong owner consumes. */
    reset(2); entry(STACK, false);
    CPUState other = cpu; other.gpr[30] = PROCESSOR;
    write32(PROCESSOR + 0x90u, 1);
    bluewake_dialogue_speed_dispatch(&other, RETURN);
    assert(wait() == 1 && stats().completed == 0);
    finish(STACK, 1, false); assert(wait() == 0); /* Correct CPU still owns it. */

    reset(2);
    entry(STACK, false); entry(STACK - 0x100u, false);
    assert(stats().entries == 2);
    finish(STACK - 0x100u, 1, false); assert(wait() == 0);
    finish(STACK, 1, false); assert(wait() == 1 && stats().completed == 2);
    reset(2);
    for (unsigned i = 0; i < 9; ++i) entry(STACK - i * 0x100u, false);
    assert(stats().entries == 8 && stats().pending_overflow == 1);
    finish(STACK - 8u * 0x100u, 1, false); assert(wait() == 1);
    for (unsigned i = 0; i < 8; ++i) finish(STACK - i * 0x100u, 1, false);
    assert(stats().completed == 8 && !bluewake_dialogue_speed_observes(RETURN));
    reset(2); entry(STACK, false);
    /* A same-stack fresh message cancels the old callback rather than sharing
     * its token or fractional delay accumulator. */
    write32(MESSAGE_CODE, 0x4321);
    entry(STACK, false); finish(STACK, 1, false);
    assert(wait() == 0 && stats().entries == 2 && stats().cancelled == 1);
    reset(2); entry(STACK, false);
    bluewake_dialogue_speed_dispatch(&cpu, 0x801E0DC8u);
    assert(stats().completed == 0 && bluewake_dialogue_speed_observes(RETURN));
}

enum {
    LEGACY_MSG = 0x80030000u, LEGACY_PROC = LEGACY_MSG + 0xE04u,
    LEGACY_TEXT = 0x80033000u, LEGACY_CALL = 0x800322B4u,
    LEGACY_RETURN = 0x8021429Cu, LEGACY_FIRST = 0x800346B4u,
    LEGACY_NEXT = 0x800346D0u, LEGACY_LENGTH = 0x8002E95Cu,
    LEGACY_HIO = 0x803E6BB0u, LEGACY_BODY_STACK = STACK - 0x150u
};
static uint32_t legacy_wait(void) { return mem_read32(&cpu, LEGACY_PROC + 0x158u); }
static void legacy_reset(float multiplier, const char* text) {
    reset(multiplier);
    mem_write16(&cpu, LEGACY_MSG + 8u, 0x1E7u);
    write32(LEGACY_MSG + 4u, 73u);
    write32(LEGACY_MSG + 0x10u, 0x80393798u);
    write32(LEGACY_MSG + 0xB8u, 0x80372190u);
    write32(LEGACY_MSG + 0xD8u, 0x80393784u);
    write32(LEGACY_MSG + 0xECu, 44u);
    mem_write16(&cpu, LEGACY_MSG + 0xF8u, 6u);
    mem_write16(&cpu, LEGACY_MSG + 0x104u, 44u);
    write8(LEGACY_MSG + 0x116u, 3u);
    write32(LEGACY_MSG + 0x111Cu, LEGACY_TEXT);
    write32(LEGACY_MSG + 0x1140u, 44u);
    /* Actual fpc MSG allocation initializes this member with dataInit rather
     * than invoking its C++ constructor. Orca's native 0x961 typing fixture
     * has a zero vtable and the two MSG method tables above. */
    write32(LEGACY_PROC, 0u);
    write32(LEGACY_PROC + 0xCu, LEGACY_MSG + 0x100u);
    write32(LEGACY_PROC + 0x3Cu, LEGACY_TEXT);
    write32(LEGACY_PROC + 0x15Cu, 2u);
    write8(LEGACY_PROC + 0x27Cu, 6u);
    write8(LEGACY_PROC + 0x29Cu, 1u);
    memcpy(cpu.ram + (LEGACY_TEXT - 0x80000000u), text, strlen(text) + 1u);
}
static void legacy_enter(void) {
    cpu.gpr[1] = STACK; cpu.gpr[3] = LEGACY_PROC; cpu.gpr[30] = LEGACY_MSG;
    cpu.lr = LEGACY_RETURN; cpu.pc = LEGACY_CALL;
    bluewake_dialogue_speed_dispatch(&cpu, cpu.pc);
}
static void legacy_character(uint32_t address) {
    cpu.gpr[1] = LEGACY_BODY_STACK;
    write32(LEGACY_BODY_STACK, STACK);
    write32(LEGACY_BODY_STACK + 0x154u, LEGACY_RETURN);
    cpu.gpr[31] = LEGACY_PROC;
    const uint32_t cursor = mem_read32(&cpu, LEGACY_PROC + 0x118u);
    const uint8_t first = mem_read8(&cpu, LEGACY_TEXT + cursor);
    cpu.gpr[28] = (first >> 4u == 8u || first >> 4u == 9u) ?
        (uint32_t)first << 8u | mem_read8(&cpu, LEGACY_TEXT + cursor + 1u) : first;
    cpu.lr = address; cpu.pc = address;
    bluewake_dialogue_speed_dispatch(&cpu, address);
}
static void legacy_finish(uint32_t consumed, uint32_t wait_result, uint8_t status) {
    write32(LEGACY_PROC + 0x118u, consumed);
    write32(LEGACY_PROC + 0x158u, wait_result);
    write8(LEGACY_PROC + 0x27Cu, status);
    cpu.gpr[1] = STACK; cpu.gpr[30] = LEGACY_MSG; cpu.lr = LEGACY_RETURN; cpu.pc = LEGACY_RETURN;
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_RETURN);
}
static void test_legacy_allowance_and_replay(void) {
    legacy_reset(1, "abcdefghijklmnopqrstuvwxyz");
    assert(!bluewake_dialogue_speed_observes(LEGACY_CALL));
    cpu.gpr[3] = LEGACY_PROC; cpu.gpr[30] = LEGACY_MSG; cpu.lr = LEGACY_RETURN;
    memcpy(ram_copy, cpu.ram, cpu.ram_size);
    const CPUState unchanged = cpu;
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_CALL);
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_FIRST);
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_RETURN);
    assert_cpu_unchanged(&unchanged);
    assert(memcmp(ram_copy, cpu.ram, cpu.ram_size) == 0 && stats().legacy_entries == 0);

    legacy_reset(2, "abcdefghijklmnopqrstuvwxyz");
    legacy_enter(); legacy_enter();
    assert(stats().legacy_entries == 1 && bluewake_dialogue_speed_observes(LEGACY_FIRST));
    cpu.gpr[30] = 0;
    legacy_character(LEGACY_FIRST);
    assert(cpu.gpr[30] == 1 && stats().legacy_budget_scaled == 1);
    const CPUState applied = cpu;
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_FIRST);
    assert_cpu_unchanged(&applied);
    // A later ordinary letter uses the remaining native allowance. It must
    // never refill that allowance and create an infinite native typing loop.
    write32(LEGACY_PROC + 0x118u, 1u); cpu.gpr[30] = 0;
    legacy_character(LEGACY_NEXT);
    assert(cpu.gpr[30] == 0 && stats().legacy_budget_scaled == 1);
    legacy_finish(2, 0, 6);
    assert(stats().legacy_completed == 1 && !bluewake_dialogue_speed_observes(LEGACY_FIRST));
    const CPUState completed = cpu;
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_RETURN);
    assert_cpu_unchanged(&completed); assert(stats().legacy_completed == 1);

    legacy_reset(1.5f, "abcdefghijklmnopqrstuvwxyz");
    uint32_t consumed = 0;
    for (unsigned frame = 0; frame < 4; ++frame) {
        legacy_enter(); cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST);
        consumed += cpu.gpr[30] + 1u;
        legacy_finish(consumed, 0, 6);
    }
    assert(consumed == 6 && stats().legacy_budget_scaled == 4);
    legacy_reset(10, "abcdefghijklmnopqrstuvwxyz");
    legacy_enter(); cpu.gpr[30] = 0; legacy_character(LEGACY_NEXT);
    assert(cpu.gpr[30] == 9); legacy_finish(10, 0, 6);
}
static void test_legacy_spacing_and_native_waits(void) {
    const char* spaces[] = {" ", "\n", "\x81\x40", "\x81\x41", "\x87\x8C"};
    for (unsigned i = 0; i < sizeof spaces / sizeof spaces[0]; ++i) {
        legacy_reset(2, spaces[i]); legacy_enter();
        legacy_finish((uint32_t)strlen(spaces[i]), 2, 6);
        assert(legacy_wait() == 1 && stats().legacy_spacing_scaled == 1);
        // dMsg_outnowProc decrements a positive timer and does not call the
        // parser that update. Even zero waits do not manufacture extra calls.
        write32(LEGACY_PROC + 0x158u, legacy_wait() - 1u);
        assert(legacy_wait() == 0 && stats().legacy_entries == 1);
        bluewake_dialogue_speed_dispatch(&cpu, LEGACY_RETURN);
        assert(legacy_wait() == 0 && stats().legacy_spacing_scaled == 1);
    }
    legacy_reset(2, " "); write8(LEGACY_PROC + 0x29Cu, 0); legacy_enter();
    legacy_finish(1, 2, 6); assert(legacy_wait() == 2);
    legacy_reset(2, "\n"); legacy_enter(); legacy_finish(1, 2, 7);
    assert(legacy_wait() == 2); // A page stop remains native.
    legacy_reset(2, "\n"); legacy_enter(); legacy_finish(1, 2, 8);
    assert(legacy_wait() == 2); // Choice preparation remains native.
    legacy_reset(2, "a"); legacy_enter(); legacy_finish(1, 2, 6);
    assert(legacy_wait() == 2); // Not a spacing source, even if the timer matches.
    legacy_reset(2, "\n"); legacy_enter(); legacy_finish(1, 30, 6);
    assert(legacy_wait() == 30); // Arbitrary/scripted timer never admitted.
    // Native WAIT tag with a two-tick operand: numerical equality to the
    // spacing timer is insufficient evidence to change its scripted wait.
    const char script[] = {0x1A,7,0,0,7,0,2,'\n',0};
    legacy_reset(2, "");
    memcpy(cpu.ram + (LEGACY_TEXT - 0x80000000u), script, sizeof script);
    legacy_enter(); legacy_finish(8, 2, 6);
    assert(legacy_wait() == 2 && stats().legacy_spacing_scaled == 0);
    legacy_reset(2, "\n"); legacy_enter(); write8(LEGACY_PROC + 0x296u, 1);
    legacy_finish(1, 2, 6); assert(legacy_wait() == 2);
    // A naturally completed forced wait may resume ordinary characters; its
    // timer remains zero and its native force-wait marker remains untouched.
    legacy_reset(2, "ab"); write8(LEGACY_PROC + 0x296u, 1); legacy_enter();
    cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST);
    assert(cpu.gpr[30] == 1 && legacy_wait() == 0 && mem_read8(&cpu, LEGACY_PROC + 0x296u) == 1);
    legacy_finish(2, 0, 6);
}
static void test_legacy_guards_and_lifecycle(void) {
    const uint32_t flags[] = {0x294,0x297,0x298,0x299,0x29A};
    for (unsigned i = 0; i < sizeof flags / sizeof flags[0]; ++i) {
        legacy_reset(2, "abc"); write8(LEGACY_PROC + flags[i], 1); legacy_enter();
        assert(stats().legacy_entries == 0); // Choices/script sends/native fast spans.
        legacy_reset(2, "abc"); legacy_enter(); write8(LEGACY_PROC + flags[i], 1);
        cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST);
        assert(cpu.gpr[30] == 0 && stats().legacy_cancelled == 1);
    }
    const uint32_t messages[] = {0x10D,0x10C};
    const uint8_t restricted[] = {2,5};
    for (unsigned i = 0; i < 2; ++i) {
        legacy_reset(2, "abc"); write8(LEGACY_MSG + messages[i], restricted[i]); legacy_enter();
        assert(stats().legacy_entries == 0); // Unskippable drawing and native instant/demo style.
    }
    legacy_reset(2, "abc"); mem_write16(&cpu, LEGACY_HIO, 3); legacy_enter();
    assert(stats().legacy_entries == 0); // Existing nonnative/native-fast allowance wins.
    legacy_reset(2, "abc"); write32(LEGACY_PROC + 0x158u, 1); legacy_enter();
    assert(stats().legacy_entries == 0); // Native still waiting; no per-VI shortening.
    legacy_reset(2, "abc"); legacy_enter(); write32(LEGACY_MSG + 4u, 74);
    cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST);
    assert(cpu.gpr[30] == 0 && stats().legacy_cancelled == 1); // Reused allocation, new process ID.
    legacy_reset(2, "abc"); legacy_enter(); write32(STAGE + 8u, 1);
    cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST);
    assert(cpu.gpr[30] == 0); // Same-VI stage/spawn change.
    legacy_reset(2, "abc"); legacy_enter(); write8(ROOM, 45);
    cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST); assert(cpu.gpr[30] == 0);
    legacy_reset(2, "abc"); legacy_enter(); write8(PAUSE, 1);
    cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST); assert(cpu.gpr[30] == 0);
    legacy_reset(2, "abc"); legacy_enter(); bluewake_dialogue_speed_reset(&cpu);
    cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST); assert(cpu.gpr[30] == 0);
    legacy_reset(2, "abc"); legacy_enter(); bluewake_dialogue_speed_configure(1);
    cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST); assert(cpu.gpr[30] == 0);
    assert(!bluewake_dialogue_speed_observes(LEGACY_RETURN));
    const uint32_t malformed[] = {0,0x81800000u,UINT32_MAX};
    for (unsigned i = 0; i < 3; ++i) {
        legacy_reset(2, "abc"); write32(LEGACY_PROC + 0x3Cu, malformed[i]);
        write32(LEGACY_MSG + 0x111Cu, malformed[i]); legacy_enter();
        assert(stats().legacy_entries == 0);
    }
    legacy_reset(2, "abc"); write32(LEGACY_PROC + 0x118u, 0x10001u); legacy_enter();
    assert(stats().legacy_entries == 0);
    legacy_reset(2, "abc"); legacy_enter(); cpu.gpr[30] = 0;
    legacy_character(LEGACY_FIRST); legacy_finish(1, 0, 6);
    legacy_enter(); cpu.gpr[30] = 0; legacy_character(LEGACY_NEXT);
    assert(cpu.gpr[30] == 1); // New invocation, same allocation, new cursor is allowed.
}
static void test_legacy_abi_and_nested(void) {
    legacy_reset(2, "abc");
    cpu.gpr[1] = STACK; cpu.gpr[3] = LEGACY_PROC; cpu.gpr[30] = LEGACY_MSG;
    cpu.lr = LEGACY_RETURN + 4u;
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_CALL);
    assert(stats().legacy_entries == 0);
    cpu.lr = LEGACY_RETURN; cpu.gpr[1] = STACK + 1u;
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_CALL);
    assert(stats().legacy_entries == 0);
    cpu.gpr[1] = 0x817FFFFCu;
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_CALL);
    assert(stats().legacy_entries == 0 && cpu.exception == 0);
    legacy_reset(2, "abc"); write32(LEGACY_MSG + 0x10u, 0x8039379Cu); legacy_enter();
    assert(stats().legacy_entries == 0);
    legacy_reset(2, "abc"); write32(LEGACY_PROC + 0xCu, LEGACY_MSG + 0x104u); legacy_enter();
    assert(stats().legacy_entries == 0);
    legacy_reset(2, "abc"); write32(LEGACY_PROC, 0x80372624u); legacy_enter();
    cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST);
    assert(stats().legacy_entries == 1 && cpu.gpr[30] == 1);
    legacy_finish(2, 0, 6); assert(stats().legacy_completed == 1);
    const uint32_t class_fields[] = {0xB8u, 0xD8u, 0xE04u};
    for (unsigned i = 0; i < sizeof class_fields / sizeof class_fields[0]; ++i) {
        legacy_reset(2, "abc"); write32(LEGACY_MSG + class_fields[i], 0x80372194u);
        legacy_enter(); assert(stats().legacy_entries == 0);
        legacy_reset(2, "abc"); legacy_enter();
        write32(LEGACY_MSG + class_fields[i], 0x80372194u);
        cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST);
        assert(cpu.gpr[30] == 0 && stats().legacy_budget_scaled == 0 && stats().legacy_cancelled == 1);
    }
    // Even a change to the other valid construction form cancels an armed
    // invocation; its old PID and embedded addresses cannot prove lifetime.
    legacy_reset(2, "abc"); legacy_enter(); write32(LEGACY_PROC, 0x80372624u);
    cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST);
    assert(cpu.gpr[30] == 0 && stats().legacy_cancelled == 1);

    for (unsigned wrong = 0; wrong < 6; ++wrong) {
        legacy_reset(2, "abc"); legacy_enter(); cpu.gpr[30] = 0;
        legacy_character(LEGACY_FIRST); // Establish a realistic body frame.
        // Restart before applying the budget, then corrupt one ABI/lifetime fact.
        bluewake_dialogue_speed_reset(&cpu); legacy_enter(); cpu.gpr[30] = 0;
        cpu.gpr[1] = LEGACY_BODY_STACK; cpu.gpr[31] = LEGACY_PROC;
        cpu.gpr[28] = 'a'; cpu.lr = LEGACY_FIRST;
        write32(LEGACY_BODY_STACK, STACK);
        write32(LEGACY_BODY_STACK + 0x154u, LEGACY_RETURN);
        if (wrong == 0) cpu.gpr[31] += 4u;
        if (wrong == 1) cpu.lr += 4u;
        if (wrong == 2) write32(LEGACY_BODY_STACK, STACK + 4u);
        if (wrong == 3) write32(LEGACY_BODY_STACK + 0x154u, LEGACY_RETURN + 4u);
        if (wrong == 4) cpu.gpr[28] = 'z';
        if (wrong == 5) cpu.gpr[30] = 3u;
        const uint32_t before = cpu.gpr[30];
        const uint64_t scaled_before = stats().legacy_budget_scaled;
        bluewake_dialogue_speed_dispatch(&cpu, LEGACY_FIRST);
        assert(cpu.gpr[30] == before && stats().legacy_budget_scaled == scaled_before);
        assert(!bluewake_dialogue_speed_observes(LEGACY_RETURN));
    }
    legacy_reset(2, "abc"); legacy_enter();
    cpu.gpr[1] = LEGACY_BODY_STACK; cpu.gpr[31] = LEGACY_PROC;
    cpu.gpr[28] = 'a'; cpu.gpr[30] = 0; cpu.lr = LEGACY_FIRST;
    write32(LEGACY_BODY_STACK, STACK); write32(LEGACY_BODY_STACK + 0x154u, LEGACY_RETURN);
    CPUState other = cpu;
    bluewake_dialogue_speed_dispatch(&other, LEGACY_FIRST);
    assert(other.gpr[30] == 0 && stats().legacy_budget_scaled == 0);
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_FIRST | 0x40000000u);
    assert(cpu.gpr[30] == 1 && stats().legacy_budget_scaled == 1);
    legacy_finish(1, 0, 6);

    // Genuinely nested callbacks have distinct stack frames. Each allowance
    // is bounded once, and a duplicate entry cannot consume a second slot.
    legacy_reset(2, "abcdef"); legacy_enter();
    cpu.gpr[1] = STACK - 0x200u; cpu.gpr[3] = LEGACY_PROC;
    cpu.gpr[30] = LEGACY_MSG; cpu.lr = LEGACY_RETURN;
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_CALL);
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_CALL);
    assert(stats().legacy_entries == 2);
    cpu.gpr[1] = LEGACY_BODY_STACK - 0x200u; cpu.gpr[31] = LEGACY_PROC;
    cpu.gpr[30] = 0; cpu.gpr[28] = 'a'; cpu.lr = LEGACY_FIRST;
    write32(cpu.gpr[1], STACK - 0x200u); write32(cpu.gpr[1] + 0x154u, LEGACY_RETURN);
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_FIRST);
    assert(cpu.gpr[30] == 1);
    write32(LEGACY_PROC + 0x118u, 2u);
    cpu.gpr[1] = STACK - 0x200u; cpu.gpr[30] = LEGACY_MSG; cpu.lr = LEGACY_RETURN;
    bluewake_dialogue_speed_dispatch(&cpu, LEGACY_RETURN);
    cpu.gpr[30] = 0; legacy_character(LEGACY_NEXT);
    assert(cpu.gpr[30] == 1); legacy_finish(4, 0, 6);
    assert(stats().legacy_completed == 2 && stats().legacy_budget_scaled == 2);

    legacy_reset(2, "abc");
    for (unsigned i = 0; i < 9; ++i) {
        cpu.gpr[1] = STACK - i * 0x200u; cpu.gpr[3] = LEGACY_PROC;
        cpu.gpr[30] = LEGACY_MSG; cpu.lr = LEGACY_RETURN;
        bluewake_dialogue_speed_dispatch(&cpu, LEGACY_CALL);
    }
    assert(stats().legacy_entries == 8 && stats().pending_overflow == 1);
    bluewake_dialogue_speed_reset(&cpu);
    assert(!bluewake_dialogue_speed_observes(LEGACY_RETURN));
    legacy_reset(2, "abc"); legacy_enter();
    bluewake_game_events_reset(&cpu, BW_GAME_RESET_STATE_LOAD);
    cpu.gpr[30] = 0; legacy_character(LEGACY_FIRST);
    assert(cpu.gpr[30] == 0 && stats().legacy_cancelled == 1);
}

#ifdef BLUEWAKE_DIALOGUE_OPTIMIZED_CALL_FIXTURE
/* The actual translated bctrl block is extracted by CMake; only its guest
 * callee's character result is synthetic. Production dispatch_loop.h and
 * direct_calls.h provide the optimized edge decisions. An empty static watch
 * table intentionally models a preexisting module before the new constants. */
unsigned bw_direct_depth;
bool bw_direct_enabled, bw_edge_watch_ready, bw_edge_filter_enabled;
static const bool clean = false;
static const u32 zero = 0;
const bool* bw_host_sources_dirty = &clean;
const bool* bw_host_decrementer_pending = &clean;
const u32* bw_host_pi_cause = &zero;
const u32* bw_host_pi_mask = &zero;
BwHostCanSkipFn bw_host_can_skip;
void* bw_host_can_skip_user;
u32 bw_edge_watch_table[BW_EDGE_WATCH_SLOTS];
static unsigned direct_guest_calls, edge_entries, edge_returns;
#ifdef BLUEWAKE_DIALOGUE_LEGACY_OPTIMIZED_CALL_FIXTURE
static BwChunkFn legacy_chunks[13];
BwChunkFn* const bw_chunk_fns = legacy_chunks;
#endif
static bool dialogue_can_skip(void* user, const CPUState* context, u32 address) {
    (void)user; (void)context;
    return !bluewake_dialogue_speed_observes(address);
}
static void native_character_result(CPUState* context) {
    --context->downcount;
    mem_write32(context, PROCESSOR + 0x90u, 1);
    context->gpr[30] = PROCESSOR;
    context->pc = RETURN;
}
bool bw_call_translated(CPUState* context, u32 target) {
    assert(target == CHARACTER && bw_edge_unwatched(target));
    ++direct_guest_calls;
    native_character_result(context);
    return true;
}
static void optimized_character_call(CPUState* ctx) {
    --ctx->downcount;
    ctx->ctr = CHARACTER;
#include "dialogue_optimized_call_under_test.inc"
label_8029F360:
    ctx->gpr[3] = 1u;
    ctx->pc = RETURN + 4u;
}
static int translated_dispatch(CPUState* context, u32 address) {
    if (address == 0x8029F35Cu) {
        context->gpr[3] = PROCESSOR;
        optimized_character_call(context);
        return 1;
    }
    if (address == CHARACTER) { native_character_result(context); return 1; }
    if (address == RETURN) {
        --context->downcount;
        context->gpr[3] = 1u;
        context->pc = RETURN + 4u;
        return 1;
    }
    return 0;
}
static bool dialogue_edge(void* user, CPUState* context, u32 address) {
    (void)user;
    if (address == CHARACTER) ++edge_entries;
    if (address == RETURN) ++edge_returns;
    bluewake_dialogue_speed_dispatch(context, address);
    return false;
}
static void setup_optimized(float factor) {
    reset(factor);
    memset(bw_edge_watch_table, 0, sizeof bw_edge_watch_table);
    bw_direct_depth = 0;
    bw_direct_enabled = bw_edge_watch_ready = bw_edge_filter_enabled = true;
    bw_host_can_skip = dialogue_can_skip;
    direct_guest_calls = edge_entries = edge_returns = 0;
    cpu.downcount = 0;
    cpu.cycle_budget = 100;
    cpu.pc = 0x8029F35Cu;
}
static void test_optimized_watch_and_first_pc(void) {
    setup_optimized(1);
    assert(bluewake_chassis_dispatch_loop(&cpu, cpu.pc, translated_dispatch,
                                        dialogue_edge, NULL));
    assert(direct_guest_calls == 1 && edge_entries == 0 && edge_returns == 0);
    assert(wait() == 1 && stats().entries == 0);

    setup_optimized(2);
    assert(bluewake_chassis_dispatch_loop(&cpu, cpu.pc, translated_dispatch,
                                        dialogue_edge, NULL));
    assert(direct_guest_calls == 0 && edge_entries == 1 && edge_returns == 1);
    assert(wait() == 0 && stats().entries == 1 && stats().completed == 1);

    setup_optimized(2);
    cpu.cycle_budget = 1;
    assert(bluewake_chassis_dispatch_loop(&cpu, cpu.pc, translated_dispatch,
                                        dialogue_edge, NULL));
    assert(cpu.pc == CHARACTER && stats().entries == 0); /* Budget before entry. */
    cpu.downcount = 0;
    bluewake_dialogue_speed_dispatch(&cpu, cpu.pc); /* Main's initial-PC service. */
    bluewake_dialogue_speed_dispatch(&cpu, cpu.pc); /* Safe entry replay. */
    assert(bluewake_chassis_dispatch_loop(&cpu, cpu.pc, translated_dispatch,
                                        dialogue_edge, NULL));
    assert(cpu.pc == RETURN && wait() == 1 && stats().entries == 1);
    cpu.downcount = 0;
    bluewake_dialogue_speed_dispatch(&cpu, cpu.pc); /* Budget-deferred return. */
    bluewake_dialogue_speed_dispatch(&cpu, cpu.pc); /* Safe return replay. */
    assert(wait() == 0 && stats().completed == 1);
    assert(bluewake_chassis_dispatch_loop(&cpu, cpu.pc, translated_dispatch,
                                        dialogue_edge, NULL));
    assert(cpu.pc == RETURN + 4u && stats().completed == 1 && cpu.gpr[3] == 1);
    puts("dialogue_speed_test: actual optimized bctrl and first-PC observation passed");
}
#ifdef BLUEWAKE_DIALOGUE_LEGACY_OPTIMIZED_CALL_FIXTURE
static unsigned legacy_direct_strings, legacy_direct_lengths;
static void native_legacy_end(CPUState* ctx) {
    ctx->gpr[1] = mem_read32(ctx, ctx->gpr[1]);
    ctx->gpr[30] = LEGACY_MSG; // Native epilogue restores the caller's register.
    ctx->lr = LEGACY_RETURN;
    ctx->pc = LEGACY_RETURN;
}
static void native_legacy_next(CPUState* ctx, bool first) {
    const uint32_t cursor = mem_read32(ctx, LEGACY_PROC + 0x118u);
    const uint8_t byte = mem_read8(ctx, LEGACY_TEXT + cursor);
    if (byte == 0) {
        mem_write8(ctx, LEGACY_PROC + 0x27Cu, 14u);
        native_legacy_end(ctx);
    } else if (byte == 0x1Au) {
        // Primary-source native WAIT control command. This is deliberately a
        // real tag shape, not a timer result mislabeled as ordinary spacing.
        assert(mem_read8(ctx, LEGACY_TEXT + cursor + 1u) == 7u);
        assert(mem_read8(ctx, LEGACY_TEXT + cursor + 4u) == 7u);
        mem_write32(ctx, LEGACY_PROC + 0x118u, cursor + 7u);
        mem_write32(ctx, LEGACY_PROC + 0x158u,
                    mem_read16(ctx, LEGACY_TEXT + cursor + 5u));
        mem_write8(ctx, LEGACY_PROC + 0x296u, 1u);
        native_legacy_end(ctx);
    } else if (byte == '\n') {
        const uint32_t lines = mem_read32(ctx, LEGACY_PROC + 0x130u) + 1u;
        mem_write32(ctx, LEGACY_PROC + 0x118u, cursor + 1u);
        mem_write32(ctx, LEGACY_PROC + 0x130u, lines);
        if (lines >= mem_read8(ctx, LEGACY_MSG + 0x116u)) {
            mem_write8(ctx, LEGACY_PROC + 0x27Cu, 7u);
        } else {
            mem_write32(ctx, LEGACY_PROC + 0x158u, mem_read32(ctx, LEGACY_PROC + 0x15Cu));
        }
        native_legacy_end(ctx); // Newline/page stop ignores extra-character allowance.
    } else {
        ctx->gpr[28] = (byte >> 4u == 8u || byte >> 4u == 9u) ?
            (uint32_t)byte << 8u | mem_read8(ctx, LEGACY_TEXT + cursor + 1u) : byte;
        ctx->pc = first ? 0x800346B0u : 0x800346CCu;
    }
}
static void native_legacy_begin(CPUState* ctx) {
    --ctx->downcount;
    const uint32_t stack = ctx->gpr[1];
    ctx->gpr[1] -= 0x150u;
    mem_write32(ctx, ctx->gpr[1], stack);
    mem_write32(ctx, ctx->gpr[1] + 0x154u, ctx->lr);
    ctx->gpr[31] = ctx->gpr[3];
    ctx->gpr[30] = (uint32_t)(int32_t)(int8_t)mem_read16(ctx, LEGACY_HIO);
    mem_write8(ctx, LEGACY_PROC + 0x27Cu, 6u);
    native_legacy_next(ctx, true);
}
static void native_legacy_consume(CPUState* ctx) {
    --ctx->downcount;
    const uint32_t character = ctx->gpr[28];
    const uint32_t cursor = mem_read32(ctx, LEGACY_PROC + 0x118u);
    mem_write32(ctx, LEGACY_PROC + 0x118u, cursor + (character > 255u ? 2u : 1u));
    if (character == 0x8140u || character == 0x8141u || character == 0x878Cu ||
        (character == 0x20u && mem_read8(ctx, LEGACY_PROC + 0x29Cu) != 0))
        mem_write32(ctx, LEGACY_PROC + 0x158u, mem_read32(ctx, LEGACY_PROC + 0x15Cu));
    // Native ordinary-character allowance: one letter is already emitted;
    // zero returns, otherwise decrement and parse the next native token.
    if ((int8_t)ctx->gpr[30] == 0) native_legacy_end(ctx);
    else { --ctx->gpr[30]; native_legacy_next(ctx, false); }
}
static void native_font_result(CPUState* ctx) {
    --ctx->downcount;
    ++legacy_direct_lengths;
    ctx->fpr[1] = 8.0; // Font width is irrelevant to scheduling qualification.
    ctx->pc = ctx->lr;
}
static void optimized_legacy_first_call(CPUState* ctx) {
    --ctx->downcount;
#include "dialogue_legacy_first_under_test.inc"
label_800346B4:
    native_legacy_consume(ctx);
}
static void optimized_legacy_next_call(CPUState* ctx) {
    --ctx->downcount;
#include "dialogue_legacy_next_under_test.inc"
label_800346D0:
    native_legacy_consume(ctx);
}
static void native_string_result(CPUState* ctx) {
    ++legacy_direct_strings;
    native_legacy_begin(ctx);
    unsigned bound = 0;
    while (ctx->pc != LEGACY_RETURN) {
        assert(++bound <= 64u);
        if (ctx->pc == 0x800346B0u) optimized_legacy_first_call(ctx);
        else if (ctx->pc == 0x800346CCu) optimized_legacy_next_call(ctx);
        else assert(false); // Default/native direct-call path must stay direct.
    }
}
static void optimized_legacy_call(CPUState* ctx) {
    --ctx->downcount;
#include "dialogue_legacy_call_under_test.inc"
label_8021429C:
    ctx->pc = LEGACY_RETURN + 4u;
}
static int translated_legacy_dispatch(CPUState* ctx, u32 address) {
    if (address == 0x80214298u) {
        ctx->gpr[3] = LEGACY_PROC; ctx->gpr[30] = LEGACY_MSG;
        optimized_legacy_call(ctx);
        return 1;
    }
    if (address == LEGACY_CALL) { native_legacy_begin(ctx); return 1; }
    if (address == 0x800346B0u) { optimized_legacy_first_call(ctx); return 1; }
    if (address == 0x800346CCu) { optimized_legacy_next_call(ctx); return 1; }
    if (address == LEGACY_LENGTH) { native_font_result(ctx); return 1; }
    if (address == LEGACY_FIRST || address == LEGACY_NEXT) { native_legacy_consume(ctx); return 1; }
    if (address == LEGACY_RETURN) {
        --ctx->downcount; ctx->pc = LEGACY_RETURN + 4u;
        return 1;
    }
    return 0;
}
static void setup_legacy_optimized(float multiplier, const char* text) {
    setup_optimized(multiplier);
    legacy_reset(multiplier, text);
    legacy_chunks[11] = native_font_result;
    legacy_chunks[12] = native_string_result;
    legacy_direct_lengths = legacy_direct_strings = 0;
    cpu.gpr[1] = STACK;
    cpu.pc = 0x80214298u;
    cpu.downcount = 0; cpu.cycle_budget = 100;
}
static void legacy_native_update(void) {
    // dMsg_outnowProc waits for a native update before calling stringSet.
    // The host neither decrements timers nor manufactures another invocation.
    if (legacy_wait() != 0) { write32(LEGACY_PROC + 0x158u, legacy_wait() - 1u); return; }
    cpu.pc = 0x80214298u; cpu.downcount = 0; cpu.cycle_budget = 100;
    assert(bluewake_chassis_dispatch_loop(&cpu, cpu.pc, translated_legacy_dispatch,
                                        dialogue_edge, NULL));
    assert(cpu.pc == LEGACY_RETURN + 4u && cpu.gpr[30] == LEGACY_MSG);
}
static void test_legacy_optimized_watch_and_scheduling(void) {
    setup_legacy_optimized(1, "abcdefghijklmnopqrstuvwxyz");
    legacy_native_update(); legacy_native_update(); legacy_native_update();
    assert(mem_read32(&cpu, LEGACY_PROC + 0x118u) == 3u);
    assert(legacy_direct_strings == 3 && stats().legacy_entries == 0);
    setup_legacy_optimized(4, "abcdefghijklmnopqrstuvwxyz");
    legacy_native_update(); legacy_native_update(); legacy_native_update();
    assert(mem_read32(&cpu, LEGACY_PROC + 0x118u) == 12u);
    assert(legacy_direct_strings == 0 && stats().legacy_entries == 3);
    assert(stats().legacy_completed == 3 && stats().legacy_budget_scaled == 3);

    setup_legacy_optimized(4, "abc\ndef");
    write8(LEGACY_MSG + 0x116u, 1u); // Stop at this page's native line limit.
    legacy_native_update();
    assert(mem_read32(&cpu, LEGACY_PROC + 0x118u) == 4u);
    assert(mem_read8(&cpu, LEGACY_PROC + 0x27Cu) == 7u && legacy_wait() == 0);
    assert(stats().legacy_spacing_scaled == 0);
    const uint8_t scripted[] = {'a',0x1A,7,0,0,7,0,2,'b',0};
    setup_legacy_optimized(4, "");
    memcpy(cpu.ram + (LEGACY_TEXT - 0x80000000u), scripted, sizeof scripted);
    legacy_native_update();
    assert(mem_read32(&cpu, LEGACY_PROC + 0x118u) == 8u && legacy_wait() == 2u);
    assert(stats().legacy_spacing_scaled == 0 && stats().legacy_completed == 1);
    legacy_native_update(); legacy_native_update();
    assert(legacy_wait() == 0 && stats().legacy_entries == 1);

    // Stop at each instruction-boundary budget and resume using main's
    // initial-PC service. This traverses the actual outnow/font call bodies
    // with an empty old watch table, including ordinary return replay.
    setup_legacy_optimized(4, "abcdefghijklmnop");
    for (unsigned turn = 0; cpu.pc != LEGACY_RETURN + 4u; ++turn) {
        assert(turn < 100u);
        cpu.downcount = 0; cpu.cycle_budget = 1;
        bluewake_dialogue_speed_dispatch(&cpu, cpu.pc);
        bluewake_dialogue_speed_dispatch(&cpu, cpu.pc);
        assert(bluewake_chassis_dispatch_loop(&cpu, cpu.pc, translated_legacy_dispatch,
                                            dialogue_edge, NULL));
    }
    assert(mem_read32(&cpu, LEGACY_PROC + 0x118u) == 4u);
    assert(stats().legacy_entries == 1 && stats().legacy_completed == 1);
    assert(stats().legacy_budget_scaled == 1 && cpu.gpr[30] == LEGACY_MSG);
    puts("dialogue_speed_test: actual optimized MSG calls, native allowance/page/script waits and budget resumes passed");
}
#endif
#endif

int main(void) {
    cpu.ram_size = RAM_SIZE;
    cpu.ram = calloc(1, cpu.ram_size);
    ram_copy = malloc(cpu.ram_size);
    assert(cpu.ram != NULL && ram_copy != NULL);
    test_native_and_config();
    test_scaling_and_replay();
    test_native_ready_loop();
    test_native_guards();
    test_identity_and_lifecycle();
    test_invalid_nested_and_owner();
    test_configuration_publication();
    test_legacy_allowance_and_replay();
    test_legacy_spacing_and_native_waits();
    test_legacy_guards_and_lifecycle();
    test_legacy_abi_and_nested();
#ifdef BLUEWAKE_DIALOGUE_OPTIMIZED_CALL_FIXTURE
    test_optimized_watch_and_first_pc();
#ifdef BLUEWAKE_DIALOGUE_LEGACY_OPTIMIZED_CALL_FIXTURE
    test_legacy_optimized_watch_and_scheduling();
#endif
#endif
    bluewake_dialogue_speed_reset(NULL);
    bluewake_game_events_reset(NULL, BW_GAME_RESET_MACHINE_RESET);
    free(ram_copy); free(cpu.ram);
    puts("dialogue_speed_test: native character waits, scripts, lifecycle, replay and ownership passed");
    return 0;
}
