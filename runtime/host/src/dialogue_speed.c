// SPDX-License-Identifier: GPL-3.0-or-later
#include "dialogue_speed.h"
#include "game_events.h"

#include <math.h>
#include <stdatomic.h>
#include <string.h>

/* Audited zeldaret/tww GZLE01 d_mesg.cpp and JMessage/processor.cpp, and the
 * actual optimized composite's slow/fast bctrl blocks. do_character is called
 * at 8029F35C; its caller preserves the processor in r30 at 8029F360. The
 * character wait stores 801E0DB8/801E0DC4 and epilogue 801E0DC8 are intrachunk
 * labels and do not reliably reach the host. Never intercept them by merely
 * adding their addresses to the static watch list. */
static const uint32_t kCharacter = 0x801E0888u;
static const uint32_t kCharacterReturn = 0x8029F360u;
/* Ordinary MSG dialogue uses a different native scheduler. stringSet's
 * temporary r30 is the extra-character allowance, not waitTimer (which also
 * holds scripted waits). These ordinary charLength returns are real guarded
 * cross-chunk boundaries in both optimized paths. The native epilogue restores
 * r30 before the outnowProc return. Never synthesize another stringSet call. */
static const uint32_t kLegacyString = 0x800322B4u;
static const uint32_t kLegacyReturn = 0x8021429Cu;
static const uint32_t kLegacyCharacterFirst = 0x800346B4u;
static const uint32_t kLegacyCharacterNext = 0x800346D0u;
static const uint32_t kLegacyVtable = 0x80372624u;
static const uint32_t kLegacyProfile = 0x80393798u;
static const uint32_t kLegacyLeafMethods = 0x80372190u;
static const uint32_t kLegacyMethods = 0x80393784u;
static const uint32_t kLegacyAllowance = 0x803E6BB0u; /* g_msgHIO + 0x6C, s16 -> s8 */
static const uint32_t kSequenceSlot = 0x803F7018u;
static const uint32_t kControlSlot = 0x803F7014u;
static const uint32_t kSequenceVtable = 0x803930FCu;
static const uint32_t kControlVtable = 0x80393150u;
static const uint32_t kMessageCode = 0x803F7030u;
static const uint32_t kHeaderFlag = 0x803F703Cu;
static const uint32_t kWidePending = 0x803F7040u;
static const uint32_t kStage = 0x803C9D3Cu;
static const uint32_t kNextStageEnabled = 0x803C9D54u;
static const uint32_t kStayRoom = 0x803F6A78u;
static const uint32_t kOverlap = 0x803F6160u;
static const uint32_t kPause = 0x803F7097u;
enum { kPendingCount = 8u, kMaxWait = 255u };

typedef struct Identity {
    uint32_t processor, control, message, entry, code;
    uint64_t epoch, generation, local_epoch;
    uint8_t stage[12], stay_room;
} Identity;
typedef struct Pending {
    bool active;
    Identity identity;
    uint32_t stack, cursor, base_wait;
    uint8_t header;
} Pending;
typedef struct LegacyIdentity {
    Identity scene;
    uint32_t process_id, message_no, text, message_id, processor_vtable;
    uint8_t entry[0x18];
} LegacyIdentity;
typedef struct LegacyPending {
    bool active, budget_applied;
    LegacyIdentity identity;
    uint32_t stack, cursor, space_wait;
} LegacyPending;

static CPUState* g_cpu;
static const uint8_t* g_ram;
static uint32_t g_ram_size;
static uint64_t g_local_epoch;
/* The high word is a publication generation; the low word is the validated
 * float's bits. UI setters never read or mutate game-thread pending state. */
static _Atomic(uint64_t) g_configuration = UINT64_C(0x3F800000);
static uint64_t g_accepted_configuration = UINT64_C(0x3F800000);
static float g_multiplier = 1.0f;
static Pending g_pending[kPendingCount];
static LegacyPending g_legacy_pending[kPendingCount];
static bool g_legacy_carry_valid, g_legacy_space_carry_valid;
static LegacyIdentity g_legacy_carry_identity, g_legacy_space_carry_identity;
static double g_legacy_carry, g_legacy_space_carry;
static bool g_carry_valid;
static Identity g_carry_identity;
static double g_carry;
static BwDialogueSpeedStats g_stats;

static uint32_t canonical(uint32_t address) { return address & ~0x40000000u; }
static bool span(const CPUState* cpu, uint32_t address, uint32_t size) {
    if (cpu == NULL || cpu->ram == NULL || address < 0x80000000u)
        return false;
    const uint32_t offset = address - 0x80000000u;
    return size <= 0x01800000u && offset <= 0x01800000u - size &&
           size <= cpu->ram_size && offset <= cpu->ram_size - size;
}
static bool object(const CPUState* cpu, uint32_t address, uint32_t size) {
    return (address & 3u) == 0 && span(cpu, address, size);
}
static uint8_t read8(const CPUState* cpu, uint32_t address) {
    return cpu->ram[address - 0x80000000u];
}
static uint32_t read32(const CPUState* cpu, uint32_t address) {
    const uint8_t* bytes = cpu->ram + (address - 0x80000000u);
    return (uint32_t)bytes[0] << 24u | (uint32_t)bytes[1] << 16u |
           (uint32_t)bytes[2] << 8u | bytes[3];
}
static uint16_t read16(const CPUState* cpu, uint32_t address) {
    return (uint16_t)((uint16_t)read8(cpu, address) << 8u | read8(cpu, address + 1u));
}
static bool same_identity(const Identity* a, const Identity* b) {
    return a->processor == b->processor && a->control == b->control &&
           a->message == b->message && a->entry == b->entry && a->code == b->code &&
           a->epoch == b->epoch && a->generation == b->generation &&
           a->local_epoch == b->local_epoch && a->stay_room == b->stay_room &&
           memcmp(a->stage, b->stage, sizeof a->stage) == 0;
}
static void cancel_pending(void) {
    for (unsigned i = 0; i < kPendingCount; ++i) {
        if (g_pending[i].active) ++g_stats.cancelled;
        g_pending[i].active = false;
        if (g_legacy_pending[i].active) {
            ++g_stats.cancelled;
            ++g_stats.legacy_cancelled;
        }
        g_legacy_pending[i].active = false;
    }
    g_carry_valid = false;
    g_carry = 0;
    g_legacy_carry_valid = g_legacy_space_carry_valid = false;
    g_legacy_carry = g_legacy_space_carry = 0;
    ++g_local_epoch;
}
bool bluewake_dialogue_speed_configure(float multiplier) {
    if (!isfinite(multiplier) || multiplier < 1.0f || multiplier > 10.0f)
        return false;
    uint32_t bits;
    memcpy(&bits, &multiplier, sizeof bits);
    uint64_t before = atomic_load_explicit(&g_configuration, memory_order_acquire);
    for (;;) {
        if ((uint32_t)before == bits) return true;
        const uint64_t generation = (uint64_t)((uint32_t)(before >> 32u) + 1u);
        const uint64_t after = generation << 32u | bits;
        if (atomic_compare_exchange_weak_explicit(&g_configuration, &before, after,
                                                 memory_order_acq_rel, memory_order_acquire))
            return true;
    }
}
float bluewake_dialogue_speed_multiplier(void) {
    const uint32_t bits = (uint32_t)atomic_load_explicit(&g_configuration, memory_order_acquire);
    float multiplier;
    memcpy(&multiplier, &bits, sizeof multiplier);
    return multiplier;
}
static void accept_configuration(void) {
    const uint64_t value = atomic_load_explicit(&g_configuration, memory_order_acquire);
    if (value == g_accepted_configuration) return;
    g_accepted_configuration = value;
    const uint32_t bits = (uint32_t)value;
    memcpy(&g_multiplier, &bits, sizeof g_multiplier);
    cancel_pending();
}
void bluewake_dialogue_speed_reset(CPUState* cpu) {
    accept_configuration();
    cancel_pending();
    g_cpu = cpu;
    g_ram = cpu != NULL ? cpu->ram : NULL;
    g_ram_size = cpu != NULL ? cpu->ram_size : 0;
}
void bluewake_dialogue_speed_attach(CPUState* cpu) {
    bluewake_dialogue_speed_reset(cpu);
    memset(&g_stats, 0, sizeof g_stats);
}
void bluewake_dialogue_speed_stats(BwDialogueSpeedStats* stats) {
    if (stats != NULL) *stats = g_stats;
}

static bool identity(CPUState* cpu, uint32_t processor, Identity* result) {
    if (!span(cpu, kSequenceSlot, 4) || !span(cpu, kControlSlot, 4) ||
        !span(cpu, kMessageCode, 4) || !span(cpu, kHeaderFlag, 5) ||
        !span(cpu, kStage, 12) || !span(cpu, kNextStageEnabled, 1) ||
        !span(cpu, kStayRoom, 1) || !span(cpu, kOverlap, 4) ||
        !span(cpu, kPause, 1) || read8(cpu, kPause) != 0 ||
        read8(cpu, kNextStageEnabled) != 0 || read32(cpu, kOverlap) != 0 ||
        !object(cpu, processor, 0x164u) || read32(cpu, kSequenceSlot) != processor ||
        read32(cpu, processor) != kSequenceVtable)
        return false;
    const uint32_t control = read32(cpu, processor + 4u);
    const uint32_t message = read32(cpu, processor + 0x38u);
    if (!object(cpu, control, 0x74u) || read32(cpu, kControlSlot) != control ||
        read32(cpu, control) != kControlVtable ||
        read32(cpu, control + 0xCu) != processor || !object(cpu, message, 0x168u) ||
        read8(cpu, message + 0x164u) != 6u)
        return false;
    const uint32_t entry = read32(cpu, control + 0x18u);
    if (!object(cpu, entry, 0x18u) || read8(cpu, entry + 0xDu) != 0u ||
        !span(cpu, read32(cpu, processor + 8u), 1))
        return false;
    memset(result, 0, sizeof *result);
    result->processor = processor;
    result->control = control;
    result->message = message;
    result->entry = entry;
    result->code = read32(cpu, kMessageCode);
    bluewake_game_events_scene(NULL, &result->epoch, &result->generation);
    result->local_epoch = g_local_epoch;
    result->stay_room = read8(cpu, kStayRoom);
    memcpy(result->stage, cpu->ram + (kStage - 0x80000000u), sizeof result->stage);
    return true;
}
static bool ordinary(const CPUState* cpu, uint32_t processor) {
    const uint32_t base = read32(cpu, processor + 0x8Cu);
    return base > 0 && base <= kMaxWait &&
           read8(cpu, processor + 0x15Fu) == 0 &&
           read8(cpu, processor + 0x160u) == 0 &&
           read8(cpu, processor + 0x161u) == 0;
}
bool bluewake_dialogue_speed_observes(uint32_t address) {
    if (g_cpu == NULL) return false;
    address = canonical(address);
    if (address == kCharacter || address == kLegacyString)
        return bluewake_dialogue_speed_multiplier() > 1.0f;
    if (address == kCharacterReturn) {
        for (unsigned i = 0; i < kPendingCount; ++i)
            if (g_pending[i].active) return true;
    }
    if (address == kLegacyReturn || address == kLegacyCharacterFirst ||
        address == kLegacyCharacterNext) {
        for (unsigned i = 0; i < kPendingCount; ++i)
            if (g_legacy_pending[i].active) return true;
    }
    return false;
}
static void enter(CPUState* cpu) {
    Identity current;
    const uint32_t stack = cpu->gpr[1];
    if (canonical(cpu->lr) != kCharacterReturn || !object(cpu, stack, 16) ||
        !identity(cpu, cpu->gpr[3], &current) || !ordinary(cpu, current.processor))
        return;
    const uint32_t cursor = read32(cpu, current.processor + 8u);
    /* Budget-deferred entry replay has not executed the guest prologue. A
     * genuinely nested character callback has a different guest stack. */
    for (unsigned i = 0; i < kPendingCount; ++i) {
        Pending* pending = &g_pending[i];
        if (pending->active && !same_identity(&pending->identity, &current)) {
            pending->active = false;
            ++g_stats.cancelled;
        }
        if (!pending->active || pending->stack != stack) continue;
        if (same_identity(&pending->identity, &current) && pending->cursor == cursor)
            return;
        pending->active = false;
        ++g_stats.cancelled;
    }
    for (unsigned i = 0; i < kPendingCount; ++i) {
        if (g_pending[i].active) continue;
        g_pending[i] = (Pending){true, current, stack, cursor,
                                read32(cpu, current.processor + 0x8Cu),
                                read8(cpu, kHeaderFlag)};
        ++g_stats.entries;
        return;
    }
    ++g_stats.pending_overflow;
}
static void returned(CPUState* cpu) {
    for (unsigned i = 0; i < kPendingCount; ++i) {
        Pending* pending = &g_pending[i];
        if (!pending->active || pending->stack != cpu->gpr[1]) continue;
        const Pending call = *pending;
        pending->active = false; /* Consume before a first-PC/edge replay. */
        Identity current;
        const uint32_t processor = call.identity.processor;
        if (canonical(cpu->lr) != kCharacterReturn || cpu->gpr[30] != processor ||
            !identity(cpu, processor, &current) || !same_identity(&current, &call.identity) ||
            read32(cpu, processor + 8u) != call.cursor || !ordinary(cpu, processor) ||
            read32(cpu, processor + 0x8Cu) != call.base_wait ||
            read8(cpu, kHeaderFlag) != call.header ||
            (call.header != 0 && read8(cpu, kWidePending) != 0)) {
            ++g_stats.cancelled;
            return;
        }
        const uint32_t wait = read32(cpu, processor + 0x90u);
        const uint32_t character = read32(cpu, current.control + 0x68u);
        const uint32_t expected = character == 0x8140u || character == 0x8141u ||
                                  character == 0x20u || character == 0xAu ? 1u : call.base_wait;
        /* Positive waits matching the actual native character-store result
         * exclude early exits and arbitrary stale/scripted delay contents. */
        if (wait == 0 || wait != expected || wait > kMaxWait) {
            ++g_stats.cancelled;
            return;
        }
        ++g_stats.completed;
        if (!g_carry_valid || !same_identity(&g_carry_identity, &current)) {
            g_carry_identity = current;
            g_carry_valid = true;
            g_carry = 0;
        }
        const double delay = (double)wait / (double)g_multiplier + g_carry;
        /* Exact integer ratios such as ten ordinary one-tick waits at 10x
         * must not require an eleventh character because of binary rounding. */
        const uint32_t scaled = (uint32_t)floor(delay + 1e-12);
        g_carry = fmax(0.0, delay - (double)scaled);
        if (scaled != wait) {
            mem_write32(cpu, processor + 0x90u, scaled);
            ++g_stats.scaled;
        }
        return;
    }
}
static bool same_legacy(const LegacyIdentity* a, const LegacyIdentity* b) {
    return same_identity(&a->scene, &b->scene) && a->process_id == b->process_id &&
           a->message_no == b->message_no && a->text == b->text &&
           a->message_id == b->message_id && a->processor_vtable == b->processor_vtable &&
           memcmp(a->entry, b->entry, sizeof a->entry) == 0;
}
static bool legacy_identity(CPUState* cpu, uint32_t processor, LegacyIdentity* result) {
    if (processor < 0x80000E04u || !object(cpu, processor, 0x2A0u) ||
        !span(cpu, kStage, 12) || !span(cpu, kStayRoom, 1) || !span(cpu, kPause, 1) ||
        !span(cpu, kNextStageEnabled, 1) || !span(cpu, kOverlap, 4) ||
        !span(cpu, kLegacyAllowance, 2) || read8(cpu, kPause) != 0 ||
        read8(cpu, kNextStageEnabled) != 0 || read32(cpu, kOverlap) != 0)
        return false;
    const uint32_t message = processor - 0xE04u;
    const uint32_t processor_vtable = read32(cpu, processor);
    /* fpcBs_Create allocates flat process storage; dMsg_Create initializes the
     * embedded parser through dataInit, without its C++ constructor. A real
     * fresh MSG therefore has a zero parser vtable. The C++ constructor's
     * audited vtable is also valid, but neither substitutes for its owning
     * MSG profile, leaf/message methods, status and embedded backreferences.
     * Bind this value to the invocation so reinitialization cancels it. */
    if (!object(cpu, message, 0x116Cu) || read16(cpu, message + 8u) != 0x1E7u ||
        read32(cpu, message + 0x10u) != kLegacyProfile || read16(cpu, message + 0xF8u) != 6u ||
        read32(cpu, message + 0xB8u) != kLegacyLeafMethods ||
        read32(cpu, message + 0xD8u) != kLegacyMethods ||
        (processor_vtable != 0u && processor_vtable != kLegacyVtable) ||
        read32(cpu, processor + 0xCu) != message + 0x100u)
        return false;
    const uint32_t text = read32(cpu, processor + 0x3Cu);
    const uint32_t cursor = read32(cpu, processor + 0x118u);
    /* Bound the source lookup, including a possible Shift-JIS second byte.
     * Only the scheduled ordinary typing call is admitted, never menu strings. */
    if (text != read32(cpu, message + 0x111Cu) || cursor > 0x10000u ||
        !span(cpu, text, cursor + 2u) || read8(cpu, message + 0x10Du) != 0u ||
        read8(cpu, message + 0x10Cu) == 5u || read8(cpu, message + 0x10Cu) == 0xEu ||
        read8(cpu, message + 0x116u) == 0u || read8(cpu, message + 0x116u) > 4u)
        return false;
    memset(result, 0, sizeof *result);
    result->scene.processor = processor;
    result->scene.message = message;
    result->scene.entry = message + 0x100u;
    bluewake_game_events_scene(NULL, &result->scene.epoch, &result->scene.generation);
    result->scene.local_epoch = g_local_epoch;
    result->scene.stay_room = read8(cpu, kStayRoom);
    memcpy(result->scene.stage, cpu->ram + (kStage - 0x80000000u), sizeof result->scene.stage);
    result->process_id = read32(cpu, message + 4u);
    result->message_no = read32(cpu, message + 0xECu);
    result->message_id = read32(cpu, message + 0x1140u);
    result->processor_vtable = processor_vtable;
    result->text = text;
    memcpy(result->entry, cpu->ram + (message + 0x100u - 0x80000000u), sizeof result->entry);
    return true;
}
static bool legacy_ordinary(const CPUState* cpu, uint32_t processor) {
    /* 299 is native A/B shortcut, 29A is a native fast span; 297/298 are
     * automatic/manual scripted send. 296 only marks a forced wait: a zero
     * wait has already completed naturally and can resume ordinary letters. */
    return read8(cpu, processor + 0x294u) == 0 && read8(cpu, processor + 0x297u) == 0 &&
           read8(cpu, processor + 0x298u) == 0 && read8(cpu, processor + 0x299u) == 0 &&
           read8(cpu, processor + 0x29Au) == 0 && read32(cpu, processor + 0x130u) < 4u;
}
static void legacy_cancel(LegacyPending* pending) {
    pending->active = false;
    ++g_stats.cancelled;
    ++g_stats.legacy_cancelled;
    g_legacy_carry_valid = g_legacy_space_carry_valid = false;
}
static void legacy_enter(CPUState* cpu) {
    LegacyIdentity current;
    const uint32_t stack = cpu->gpr[1];
    if (canonical(cpu->lr) != kLegacyReturn || !object(cpu, stack, 16u) ||
        stack < 0x80000150u || !object(cpu, stack - 0x150u, 0x158u) ||
        !legacy_identity(cpu, cpu->gpr[3], &current) || cpu->gpr[30] != current.scene.message ||
        !legacy_ordinary(cpu, current.scene.processor) ||
        read32(cpu, current.scene.processor + 0x158u) != 0 ||
        (int8_t)read16(cpu, kLegacyAllowance) != 0)
        return;
    const uint32_t cursor = read32(cpu, current.scene.processor + 0x118u);
    for (unsigned i = 0; i < kPendingCount; ++i) {
        LegacyPending* pending = &g_legacy_pending[i];
        if (!pending->active || pending->stack != stack) continue;
        if (same_legacy(&pending->identity, &current) && pending->cursor == cursor) return;
        legacy_cancel(pending);
    }
    for (unsigned i = 0; i < kPendingCount; ++i) {
        if (g_legacy_pending[i].active) continue;
        g_legacy_pending[i] = (LegacyPending){true, false, current, stack, cursor,
            read32(cpu, current.scene.processor + 0x15Cu)};
        ++g_stats.entries;
        ++g_stats.legacy_entries;
        return;
    }
    ++g_stats.pending_overflow;
}
static void legacy_budget(CPUState* cpu, uint32_t address) {
    for (unsigned i = 0; i < kPendingCount; ++i) {
        LegacyPending* pending = &g_legacy_pending[i];
        if (!pending->active || pending->stack - 0x150u != cpu->gpr[1]) continue;
        if (pending->budget_applied) return; /* Return-edge/first-PC and later letters. */
        LegacyIdentity current;
        const uint32_t processor = pending->identity.scene.processor;
        if (canonical(cpu->lr) != address || cpu->gpr[31] != processor ||
            read32(cpu, cpu->gpr[1]) != pending->stack ||
            canonical(read32(cpu, cpu->gpr[1] + 0x154u)) != kLegacyReturn ||
            !legacy_identity(cpu, processor, &current) || !same_legacy(&current, &pending->identity) ||
            !legacy_ordinary(cpu, processor) || read8(cpu, processor + 0x27Cu) != 6u ||
            cpu->gpr[30] != 0u || (int8_t)read16(cpu, kLegacyAllowance) != 0) {
            legacy_cancel(pending);
            return;
        }
        const uint32_t cursor = read32(cpu, processor + 0x118u);
        if (cursor < pending->cursor || cursor - pending->cursor > 4096u) {
            legacy_cancel(pending); return;
        }
        const uint8_t first = read8(cpu, current.text + cursor);
        const uint32_t character = (first >> 4u == 8u || first >> 4u == 9u) ?
            (uint32_t)first << 8u | read8(cpu, current.text + cursor + 1u) : first;
        if (character == 0u || character == 0x1Au || character == 0xAu ||
            character != cpu->gpr[28]) { legacy_cancel(pending); return; }
        pending->budget_applied = true; /* Consume allowance before publishing r30. */
        if (!g_legacy_carry_valid || !same_legacy(&g_legacy_carry_identity, &current)) {
            g_legacy_carry_identity = current; g_legacy_carry_valid = true; g_legacy_carry = 0;
        }
        const double allowance = (double)g_multiplier + g_legacy_carry;
        const uint32_t characters = (uint32_t)floor(allowance + 1e-12);
        g_legacy_carry = fmax(0.0, allowance - characters);
        cpu->gpr[30] = characters - 1u; /* At most nine: native signed-byte budget. */
        ++g_stats.scaled;
        ++g_stats.legacy_budget_scaled;
        return;
    }
}
static void legacy_returned(CPUState* cpu) {
    for (unsigned i = 0; i < kPendingCount; ++i) {
        LegacyPending* pending = &g_legacy_pending[i];
        if (!pending->active || pending->stack != cpu->gpr[1]) continue;
        const LegacyPending call = *pending;
        pending->active = false;
        LegacyIdentity current;
        const uint32_t processor = call.identity.scene.processor;
        if (canonical(cpu->lr) != kLegacyReturn || cpu->gpr[30] != call.identity.scene.message ||
            !legacy_identity(cpu, processor, &current) || !same_legacy(&current, &call.identity)) {
            legacy_cancel(pending); return;
        }
        ++g_stats.completed;
        ++g_stats.legacy_completed;
        /* The same wait field holds page/script delays. Admit only the exact
         * native spacing result from a control-tag-free consumed source span. */
        const uint32_t cursor = read32(cpu, processor + 0x118u);
        const uint32_t wait = read32(cpu, processor + 0x158u);
        if (!legacy_ordinary(cpu, processor) || read8(cpu, processor + 0x27Cu) != 6u ||
            read8(cpu, processor + 0x296u) != 0 || wait == 0 || wait > kMaxWait ||
            wait != call.space_wait || read32(cpu, processor + 0x15Cu) != call.space_wait ||
            cursor <= call.cursor || cursor - call.cursor > 4096u)
            return;
        uint32_t last = 0;
        for (uint32_t at = call.cursor; at < cursor;) {
            const uint8_t first = read8(cpu, current.text + at++);
            if (first == 0 || first == 0x1A) return;
            if (first >> 4u == 8u || first >> 4u == 9u) {
                if (at >= cursor) return;
                last = (uint32_t)first << 8u | read8(cpu, current.text + at++);
            } else last = first;
        }
        if (last != 0xAu && last != 0x8140u && last != 0x8141u && last != 0x878Cu &&
            !(last == 0x20u && read8(cpu, processor + 0x29Cu) != 0)) return;
        if (!g_legacy_space_carry_valid || !same_legacy(&g_legacy_space_carry_identity, &current)) {
            g_legacy_space_carry_identity = current; g_legacy_space_carry_valid = true;
            g_legacy_space_carry = 0;
        }
        const double delay = wait / (double)g_multiplier + g_legacy_space_carry;
        const uint32_t scaled = (uint32_t)floor(delay + 1e-12);
        g_legacy_space_carry = fmax(0.0, delay - scaled);
        if (scaled != wait) {
            mem_write32(cpu, processor + 0x158u, scaled);
            ++g_stats.scaled; ++g_stats.legacy_spacing_scaled;
        }
        return;
    }
}
void bluewake_dialogue_speed_dispatch(CPUState* cpu, uint32_t address) {
    if (cpu == NULL || cpu != g_cpu) return;
    accept_configuration();
    if (g_multiplier == 1.0f) return;
    if (cpu->ram != g_ram || cpu->ram_size != g_ram_size) {
        bluewake_dialogue_speed_reset(cpu);
        return;
    }
    if (cpu->exception != 0) {
        cancel_pending();
        return;
    }
    address = canonical(address);
    if (address == kCharacter) enter(cpu);
    else if (address == kCharacterReturn) returned(cpu);
    else if (address == kLegacyString) legacy_enter(cpu);
    else if (address == kLegacyCharacterFirst || address == kLegacyCharacterNext) legacy_budget(cpu, address);
    else if (address == kLegacyReturn) legacy_returned(cpu);
}
