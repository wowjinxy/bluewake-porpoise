// SPDX-License-Identifier: GPL-3.0-or-later
#include "enhancement_hooks.h"
#include "game_events.h"

#include <math.h>
#include <stdatomic.h>
#include <string.h>

/* Primary source: zeldaret/tww GZLE01 d_a_wbird.cpp actionMove and
 * d_a_player_main.cpp procBootsEquip_init/procBootsEquip/setSingleMoveAnime.
 * The real optimized Wbird caller preserves this in r30 and Link in r28.
 * r31 is REL rodata, NOT Link. At the wind commit return, native timer==70
 * and the actual wind flags have already been published by 8008A870.
 * Retain the last 15 native ticks, including custom-wind fade, cutEnd,
 * angle restoration, actionEnd, sound, event reset and deletion.
 * Boots only changes the animation call's rate argument. Native checkPass
 * at 11 toggles the boots and at 15 vibrates; no frame/flag/proc writes. */
enum { kWind = 1u, kBoots = 2u, kMaxCallRetraces = 4u };
static const uint32_t kGame = 0x803C4C08u;
static const uint32_t kPlayer = 0x803CA74Cu, kLink = 0x803CA754u;
static const uint32_t kStage = 0x803C9D3Cu, kRoom = 0x803F6A78u;
static const uint32_t kPause = 0x803F7097u, kNext = 0x803C9D54u;
static const uint32_t kOverlap = 0x803F6160u, kFrame = 0x803E8140u;
static const uint32_t kControl = 0x803C9DE0u, kManager = 0x803C9ED4u;
static const uint32_t kBaseType = 0x803F6A18u, kActorType = 0x803F69D0u;
static const uint32_t kPlayerProfile = 0x8038FD8Cu, kPlayerMethods = 0x8038FD68u;
static const uint32_t kWbirdProfile = 0xC1F10A98u, kWbirdMethods = 0xC1F10A78u;
static const uint32_t kBootsHio = 0x8035DD14u;
static const uint32_t kWindFlags = 0x803E5460u;

typedef struct Identity {
    uintptr_t profile_storage, method_storage;
    uint32_t owner, owner_id, owner_type, profile, methods;
    uint32_t player, player_id, player_type, stack, backchain, saved_lr;
    uint32_t frame, alias_generation, event, event_header, event_table;
    uint64_t epoch, scene_generation;
    uint8_t stage[12], room;
} Identity;
typedef struct Pending {
    bool active;
    Identity identity;
    uint64_t retrace;
    double rate, start, blend;
    uint32_t end, item;
} Pending;

static CPUState* g_cpu;
static uint8_t* g_ram;
static uint32_t g_ram_size;
static uint64_t g_retrace;
/* Generation and both feature bits are one atomic publication, allowing
 * concurrent independent UI setters without losing either setting. */
static _Atomic(uint64_t) g_configuration;
static uint64_t g_seen;
static Pending g_wind, g_boots;
static BwEnhancementHooksStats g_stats;

static uint32_t canonical(uint32_t address) { return address & ~0x40000000u; }
static bool span(CPUState* cpu, uint32_t address, uint32_t bytes) {
    if (cpu == NULL || cpu->ram == NULL || address < 0x80000000u) return false;
    const uint32_t offset = address - 0x80000000u;
    return bytes <= 0x01800000u && offset <= 0x01800000u - bytes &&
           bytes <= cpu->ram_size && offset <= cpu->ram_size - bytes;
}
static bool object(CPUState* cpu, uint32_t address, uint32_t bytes) {
    return (address & 3u) == 0 && span(cpu, address, bytes);
}
static bool data(CPUState* cpu, uint32_t address, uint32_t bytes) {
    /* Only the two fixed audited REL data ranges use alias resolution. */
    return (address & 3u) == 0 && get_ram_ptr(cpu, address, bytes, NULL) != NULL;
}
static double read_float(CPUState* cpu, uint32_t address) {
    const uint32_t bits = mem_read32(cpu, address);
    float value; memcpy(&value, &bits, sizeof value); return value;
}
static void cancel(Pending* pending) {
    if (pending->active) ++g_stats.cancelled;
    pending->active = false;
}
static void publish(uint32_t mask, bool enabled) {
    uint64_t before = atomic_load_explicit(&g_configuration, memory_order_acquire);
    for (;;) {
        const uint32_t bits = ((uint32_t)before & ~mask) | (enabled ? mask : 0u);
        if ((uint32_t)before == bits) return;
        const uint64_t after = ((before >> 32u) + 1u) << 32u | bits;
        if (atomic_compare_exchange_weak_explicit(&g_configuration, &before, after,
                                                  memory_order_acq_rel, memory_order_acquire)) return;
    }
}
void bluewake_enhancement_faster_wind(bool enabled) { publish(kWind, enabled); }
void bluewake_enhancement_faster_boots(bool enabled) { publish(kBoots, enabled); }
bool bluewake_enhancement_faster_wind_enabled(void) {
    return (atomic_load_explicit(&g_configuration, memory_order_acquire) & kWind) != 0;
}
bool bluewake_enhancement_faster_boots_enabled(void) {
    return (atomic_load_explicit(&g_configuration, memory_order_acquire) & kBoots) != 0;
}
static uint32_t configuration(void) {
    const uint64_t value = atomic_load_explicit(&g_configuration, memory_order_acquire);
    if (value != g_seen) { cancel(&g_wind); cancel(&g_boots); g_seen = value; }
    return (uint32_t)value;
}
void bluewake_enhancement_hooks_reset(CPUState* cpu) {
    cancel(&g_wind); cancel(&g_boots);
    g_cpu = cpu; g_ram = cpu ? cpu->ram : NULL; g_ram_size = cpu ? cpu->ram_size : 0;
    g_retrace = 0;
    g_seen = atomic_load_explicit(&g_configuration, memory_order_acquire);
}
void bluewake_enhancement_hooks_attach(CPUState* cpu) {
    bluewake_enhancement_hooks_reset(cpu);
    memset(&g_stats, 0, sizeof g_stats);
}
static bool attached(CPUState* cpu) {
    return cpu != NULL && cpu == g_cpu && cpu->ram == g_ram && cpu->ram_size == g_ram_size;
}
void bluewake_enhancement_hooks_retrace(CPUState* cpu) {
    if (!attached(cpu)) { bluewake_enhancement_hooks_reset(cpu); return; }
    configuration();
    ++g_retrace;
    if (g_wind.active && g_retrace - g_wind.retrace > kMaxCallRetraces) cancel(&g_wind);
    if (g_boots.active && g_retrace - g_boots.retrace > kMaxCallRetraces) cancel(&g_boots);
}
void bluewake_enhancement_hooks_stats(BwEnhancementHooksStats* result) {
    if (result != NULL) *result = g_stats;
}
bool bluewake_enhancement_hooks_observes_context(const CPUState* cpu, uint32_t address) {
    address = canonical(address);
    if (address != BLUEWAKE_ENHANCEMENT_WIND_COMMIT &&
        address != BLUEWAKE_ENHANCEMENT_BOOTS_ANIMATION &&
        address != BLUEWAKE_ENHANCEMENT_WIND_RETURN &&
        address != BLUEWAKE_ENHANCEMENT_BOOTS_RETURN)
        return false;
    if (g_cpu == NULL || cpu == NULL) return false;
    if (address == BLUEWAKE_ENHANCEMENT_WIND_COMMIT)
        return bluewake_enhancement_faster_wind_enabled() &&
            canonical(cpu->lr) == BLUEWAKE_ENHANCEMENT_WIND_RETURN;
    if (address == BLUEWAKE_ENHANCEMENT_BOOTS_ANIMATION)
        return bluewake_enhancement_faster_boots_enabled() &&
            canonical(cpu->lr) == BLUEWAKE_ENHANCEMENT_BOOTS_RETURN;
    if (address == BLUEWAKE_ENHANCEMENT_WIND_RETURN) return g_wind.active;
    if (address == BLUEWAKE_ENHANCEMENT_BOOTS_RETURN) return g_boots.active;
    return false;
}
bool bluewake_enhancement_hooks_observes(uint32_t address) {
    return bluewake_enhancement_hooks_observes_context(g_cpu, address);
}

static bool actor(CPUState* cpu, uint32_t owner, bool wbird) {
    const uint32_t profile = wbird ? kWbirdProfile : kPlayerProfile;
    const uint32_t methods = wbird ? kWbirdMethods : kPlayerMethods;
    const uint16_t name = wbird ? 0xC5u : 0xA9u;
    if (!object(cpu, owner, wbird ? 0x2A8u : 0x4C28u) ||
        mem_read32(cpu, kBaseType) == 0 || mem_read32(cpu, kActorType) == 0 ||
        mem_read32(cpu, owner) != mem_read32(cpu, kBaseType) ||
        mem_read32(cpu, owner + 0xC0u) != mem_read32(cpu, kActorType) ||
        mem_read32(cpu, owner + 4u) == 0 || mem_read32(cpu, owner + 4u) == UINT32_MAX ||
        mem_read16(cpu, owner + 8u) != name || mem_read16(cpu, owner + 0xEu) != name ||
        mem_read8(cpu, owner + 0xBu) != 0 ||
        canonical(mem_read32(cpu, owner + 0x10u)) != canonical(profile) ||
        canonical(mem_read32(cpu, owner + 0xECu)) != canonical(methods) ||
        mem_read32(cpu, owner + 0xA8u) != 0x803726E8u ||
        mem_read32(cpu, owner + 0xB8u) != 0x80371FF8u ||
        mem_read8(cpu, owner + 0x1BEu) != (wbird ? 0u : 1u) ||
        (mem_read32(cpu, owner + 0x1C8u) & 2u) != 0 ||
        !data(cpu, profile, wbird ? 0x30u : 0x34u) || !data(cpu, methods, 0x14u)) return false;
    if (mem_read16(cpu, profile + 8u) != name ||
        mem_read32(cpu, profile + 0x10u) != (wbird ? 0x2A8u : 0x4C28u) ||
        mem_read32(cpu, profile + 0x14u) != 0 ||
        mem_read32(cpu, profile + 0xCu) != 0x803726E8u ||
        mem_read32(cpu, profile + 0x1Cu) != 0x80371FF8u ||
        canonical(mem_read32(cpu, profile + 0x24u)) != canonical(methods) ||
        canonical(mem_read32(cpu, methods + 8u)) != (wbird ? 0x81F10934u : 0x80122D30u)) return false;
    return wbird || mem_read32(cpu, owner + 0x498u) == owner + 0x1F8u;
}
static bool identity(CPUState* cpu, uint32_t owner, bool wbird, Identity* result) {
    if (!span(cpu, kGame, 0x5CF0u) || !span(cpu, kRoom, 1) || !span(cpu, kPause, 1) ||
        !span(cpu, kOverlap, 4) || !span(cpu, kFrame, 4) || !span(cpu, kBaseType, 4) ||
        !span(cpu, kActorType, 4) || cpu->exception != 0 || mem_read8(cpu, kPause) != 0 ||
        mem_read8(cpu, kNext) != 0 || mem_read32(cpu, kOverlap) != 0 ||
        mem_read8(cpu, kStage) == 0 || mem_read8(cpu, kRoom) >= 0x80u ||
        (cpu->gpr[1] & 15u) != 0 || !object(cpu, cpu->gpr[1], 0x28u) ||
        mem_read32(cpu, cpu->gpr[1]) != cpu->gpr[1] + (wbird ? 32u : 16u) ||
        !actor(cpu, owner, wbird)) return false;
    const uint32_t player = mem_read32(cpu, kPlayer);
    if (player != mem_read32(cpu, kLink) || !actor(cpu, player, false)) return false;
    memset(result, 0, sizeof *result);
    result->owner = owner; result->owner_id = mem_read32(cpu, owner + 4u);
    result->owner_type = mem_read32(cpu, owner);
    result->profile = mem_read32(cpu, owner + 0x10u);
    result->methods = mem_read32(cpu, owner + 0xECu);
    result->profile_storage = (uintptr_t)get_ram_ptr(cpu, result->profile, 0x30u, NULL);
    result->method_storage = (uintptr_t)get_ram_ptr(cpu, result->methods, 0x14u, NULL);
    result->player = player; result->player_id = mem_read32(cpu, player + 4u);
    result->player_type = mem_read32(cpu, player);
    result->stack = cpu->gpr[1]; result->backchain = mem_read32(cpu, result->stack);
    result->saved_lr = mem_read32(cpu, result->stack + (wbird ? 36u : 20u));
    result->frame = mem_read32(cpu, kFrame); result->alias_generation = g_ppc_guest_alias_generation;
    result->room = mem_read8(cpu, kRoom);
    memcpy(result->stage, cpu->ram + (kStage - 0x80000000u), sizeof result->stage);
    bluewake_game_events_scene(NULL, &result->epoch, &result->scene_generation);
    if (!wbird) return owner == player && mem_read8(cpu, kControl + 0xC2u) == 0;
    /* demoCheck/setParam stores the changing actor as pt1, and eventIdx
     * must select the currently PLAYing wind event. Bounds mirror getEventData.
     * Pointer/count consistency additionally rejects malformed event tables. */
    const int16_t index = (int16_t)mem_read16(cpu, owner + 0x2A6u);
    const uint32_t header = mem_read32(cpu, kManager), table = mem_read32(cpu, kManager + 4u);
    if (mem_read8(cpu, kControl + 0xC2u) != 2 ||
        mem_read8(cpu, kControl + 0xC3u) != 0 ||
        (mem_read16(cpu, kControl + 0xE8u) & 8u) != 0 ||
        mem_read32(cpu, kControl + 0xC4u) != result->owner_id ||
        mem_read32(cpu, kControl + 0xC8u) != result->player_id ||
        mem_read16(cpu, kControl + 0xD8u) != (uint16_t)index ||
        mem_read16(cpu, owner + 0xF8u) != 2 || index < 0 || !object(cpu, header, 0x40u)) return false;
    const uint32_t count = mem_read32(cpu, header + 4u), offset = mem_read32(cpu, header);
    if (count == 0 || count > 4096u || (uint32_t)index >= count ||
        offset > UINT32_MAX - header || header + offset != table ||
        !object(cpu, table, count * 0xB0u)) return false;
    result->event_header = header; result->event_table = table;
    result->event = table + (uint32_t)index * 0xB0u;
    const uint8_t* name = cpu->ram + (result->event - 0x80000000u);
    return mem_read32(cpu, result->event + 0xA4u) == 2u &&
        (memcmp(name, "TACT_WINDOW2", sizeof "TACT_WINDOW2") == 0 ||
         memcmp(name, "TACT_WINDOW2_SHIP", sizeof "TACT_WINDOW2_SHIP") == 0);
}
static bool same(const Identity* a, const Identity* b) { return memcmp(a, b, sizeof *a) == 0; }
static bool wind_context(CPUState* cpu, Identity* result) {
    return canonical(cpu->lr) == BLUEWAKE_ENHANCEMENT_WIND_RETURN &&
        identity(cpu, cpu->gpr[30], true, result) && cpu->gpr[28] == result->player &&
        canonical(result->saved_lr) == 0x81F10978u &&
        canonical(cpu->gpr[31]) == 0x81F10A10u &&
        mem_read8(cpu, result->owner + 0x29Cu) == 2 &&
        mem_read8(cpu, result->owner + 0x29Du) == 0 &&
        mem_read16(cpu, result->owner + 0x29Eu) == 70;
}
static bool assigned_boots(CPUState* cpu) {
    if (mem_read8(cpu, kGame + 0x3Cu + 9u) != 0x29u) return false;
    for (unsigned button = 0; button < 3; ++button) {
        if (mem_read8(cpu, kGame + 9u + button) == 9u &&
            mem_read8(cpu, 0x803CA7DBu + button) == 0x29u) return true;
    }
    return false;
}
static bool boots_context(CPUState* cpu, Identity* result) {
    return canonical(cpu->lr) == BLUEWAKE_ENHANCEMENT_BOOTS_RETURN &&
        identity(cpu, cpu->gpr[30], false, result) &&
        mem_read32(cpu, result->player + 0x31D8u) == 0xA1u &&
        mem_read32(cpu, result->player + 0x31E4u) == 0x801198E0u &&
        mem_read16(cpu, result->player + 0x304u) == 0 &&
        mem_read32(cpu, result->player + 0x314u) == 0 &&
        (canonical(result->saved_lr) == 0x8010CE28u || canonical(result->saved_lr) == 0x8010D19Cu) &&
        assigned_boots(cpu);
}
void bluewake_enhancement_hooks_dispatch(CPUState* cpu, uint32_t address) {
    address = canonical(address);
    if (address != BLUEWAKE_ENHANCEMENT_WIND_COMMIT &&
        address != BLUEWAKE_ENHANCEMENT_WIND_RETURN &&
        address != BLUEWAKE_ENHANCEMENT_BOOTS_ANIMATION &&
        address != BLUEWAKE_ENHANCEMENT_BOOTS_RETURN) return;
    if (!attached(cpu)) { bluewake_enhancement_hooks_reset(cpu); return; }
    const uint32_t enabled = configuration();
    if (enabled == 0) return; /* Disabled: no guest reads or writes. */
    if (canonical(cpu->pc) != address) return;
    Identity current;
    if (address == BLUEWAKE_ENHANCEMENT_WIND_COMMIT) {
        if ((enabled & kWind) == 0) return;
        if (!wind_context(cpu, &current)) { cancel(&g_wind); return; }
        if (g_wind.active) {
            if (!same(&g_wind.identity, &current)) cancel(&g_wind);
            return;
        }
        g_wind = (Pending){.active=true, .identity=current, .retrace=g_retrace};
        ++g_stats.wind_entries;
    } else if (address == BLUEWAKE_ENHANCEMENT_WIND_RETURN) {
        if (!g_wind.active) return;
        const Pending pending = g_wind; g_wind.active = false;
        if ((enabled & kWind) == 0 || !wind_context(cpu, &current) ||
            !same(&pending.identity, &current) || !span(cpu, kWindFlags, 2) ||
            (mem_read8(cpu, kWindFlags) & 1u) == 0 || mem_read8(cpu, kWindFlags + 1u) != 0xFFu) {
            ++g_stats.cancelled; return;
        }
        mem_write16(cpu, current.owner + 0x29Eu, 15);
        ++g_stats.wind_completed; ++g_stats.wind_shortened;
    } else if (address == BLUEWAKE_ENHANCEMENT_BOOTS_ANIMATION) {
        if ((enabled & kBoots) == 0) return;
        if (!boots_context(cpu, &current) || cpu->gpr[3] != current.player ||
            cpu->gpr[4] != 0xAEu || cpu->gpr[31] != 0x29u || (cpu->msr & PPC_MSR_FP) == 0) {
            cancel(&g_boots); return;
        }
        if (g_boots.active) {
            if (!same(&g_boots.identity, &current) || cpu->fpr[1] != g_boots.rate * 2.0 ||
                cpu->ps1[1] != cpu->fpr[1] || cpu->fpr[2] != g_boots.start ||
                cpu->fpr[3] != g_boots.blend || cpu->gpr[5] != g_boots.end)
                cancel(&g_boots);
            return;
        }
        if (!span(cpu, kBootsHio, 0x14u)) return;
        const double rate = read_float(cpu, kBootsHio + 4u);
        const double start = read_float(cpu, kBootsHio + 8u);
        const double blend = read_float(cpu, kBootsHio + 0x10u);
        /* Restrict to the audited native forward setup. Modified/reverse/
         * stopped/partial animations are unknown contexts and remain native. */
        if (rate != 1.0 || start != 0 || !isfinite(blend) || blend != 5.0 ||
            mem_read16(cpu, kBootsHio) != 19u ||
            read_float(cpu, kBootsHio + 0xCu) != 19.0 ||
            cpu->fpr[1] != rate || cpu->ps1[1] != rate || cpu->fpr[2] != start ||
            cpu->fpr[3] != blend || cpu->gpr[5] != 19u) return;
        g_boots = (Pending){.active=true, .identity=current, .retrace=g_retrace,
            .rate=rate, .start=start, .blend=blend, .end=cpu->gpr[5], .item=cpu->gpr[31]};
        cpu->fpr[1] = rate * 2.0; cpu->ps1[1] = cpu->fpr[1];
        ++g_stats.boots_entries; ++g_stats.boots_scaled;
    } else {
        if (!g_boots.active) return;
        const Pending pending = g_boots; g_boots.active = false;
        if ((enabled & kBoots) == 0 || !boots_context(cpu, &current) ||
            !same(&pending.identity, &current) || cpu->gpr[31] != pending.item) {
            ++g_stats.cancelled; return;
        }
        ++g_stats.boots_completed;
    }
}
