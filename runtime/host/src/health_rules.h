// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_HEALTH_RULES_H
#define BLUEWAKE_HEALTH_RULES_H

#include "core/cpu.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Experimental bounded GZLE01 USA rev0 adapter. All APIs belong to the game thread;
 * UI must queue copied configuration. Attach only after the host verifies the
 * audited translated module. Reset on STATE/native CARD load, machine/module/
 * RAM replacement and scene changes, before freeing the old RAM.
 *
 * Rules change only an observed native contribution to mItemLifeCount AFTER
 * its real native call, never life/max-life, native results, damage reactions,
 * item awards, selectors, death/revival or scripted resets. Native meter
 * quantization, clamps and animation remain in charge. Initial coverage is
 * ordinary collision/fall/lava damage and controlled heart/fairy pickups.
 * Potions/soup, NPC/script healing, Bo/ReDead REL damage, scripted throw/restart,
 * event/cutscene/recollection and automatic death-revival remain native.
 * No gameplay or optimized native execution qualification is claimed here. */
enum {
    BW_HEALTH_RULES_ABI_GZLE01 = 1,
    /* The audited module expands the host allocation to 32MiB for linked REL
     * data. Health fields and actors still belong to the original 24MiB MEM1. */
    BW_HEALTH_RULES_MEM1_SIZE = 0x01800000u,
    BW_HEALTH_RULES_HOST_RAM_SIZE = 0x02000000u,
    BW_HEALTH_RULES_NATIVE_RATE = 256,
    BW_HEALTH_RULES_MAX_RATE = 4096,
    BW_HEALTH_RULES_PENDING = 8
};
#define BW_HEALTH_RULES_COLLISION UINT32_C(0x80110654)
#define BW_HEALTH_RULES_COLLISION_RETURN UINT32_C(0x80121F58)
#define BW_HEALTH_RULES_DAMAGE UINT32_C(0x8011029C)
#define BW_HEALTH_RULES_HEART UINT32_C(0x800C2E7C)
#define BW_HEALTH_RULES_FAIRY UINT32_C(0x800C31C8)
#define BW_HEALTH_RULES_ITEM_RETURN UINT32_C(0x800C2E20)

typedef struct BwHealthRulesConfig {
    uint16_t damage_q8;             /* 0..16x, native=256. */
    uint16_t healing_q8;            /* Independent ordinary pickup rate. */
} BwHealthRulesConfig;
typedef struct BwHealthRulesLifetime {
    uint64_t epoch, scene_generation, tick;
} BwHealthRulesLifetime;
typedef struct BwHealthRulesStats {
    uint64_t damage_entries, healing_entries, completed, adjusted;
    uint64_t cancelled, rejected, pending_overflow, nested_damage_ignored;
} BwHealthRulesStats;

/* Pure bounded rule; identity returns the exact observed native bits. Values
 * outside the documented native accumulator range fail without changing out.
 * Scaling the delta separately preserves an already pending opposite effect.
 * Fractions remain native f32 pending units; there is no hidden host carry. */
bool bw_health_rules_adjust(float before, float after, uint16_t rate_q8, float* out);
BwHealthRulesConfig bw_health_rules_native_config(void);

/* Fixed, allocation-free runtime. Internal records are public only to permit
 * stack/static allocation; callers must not inspect or mutate them. */
typedef struct BwHealthRulesIdentity {
    uint32_t player, player_id, player_type, stack, backchain, saved_lr, alias_generation;
    uint16_t life, max_life;
    uint8_t stage[12], room;
    uint64_t epoch, generation;
} BwHealthRulesIdentity;
typedef struct BwHealthRulesPending {
    bool active;
    uint8_t kind;
    uint16_t rate;
    uint32_t entry, return_pc;
    float before, amount;
    uint64_t tick;
    BwHealthRulesIdentity identity;
} BwHealthRulesPending;
typedef struct BwHealthRulesRuntime {
    CPUState* cpu;
    uint8_t* ram;
    uint32_t ram_size, abi;
    uint32_t read_alias_generation;
    bool read_failed;
    BwHealthRulesLifetime lifetime;
    BwHealthRulesConfig config;
    BwHealthRulesStats stats;
    BwHealthRulesPending pending[BW_HEALTH_RULES_PENDING];
} BwHealthRulesRuntime;

void bw_health_rules_init(BwHealthRulesRuntime* runtime);
bool bw_health_rules_configure(BwHealthRulesRuntime* runtime, const BwHealthRulesConfig* config);
bool bw_health_rules_attach(BwHealthRulesRuntime* runtime, CPUState* cpu,
                            const BwHealthRulesLifetime* lifetime, uint32_t abi);
void bw_health_rules_reset(BwHealthRulesRuntime* runtime, const BwHealthRulesLifetime* lifetime);
void bw_health_rules_detach(BwHealthRulesRuntime* runtime);
void bw_health_rules_retrace(BwHealthRulesRuntime* runtime, CPUState* cpu,
                             const BwHealthRulesLifetime* lifetime);
/* Must be consulted by host_can_skip_observation using its SUPPLIED CPU, and
 * by first-PC/edge service outside the geometry interval. Native identity has
 * zero watches/read/write work. Never add intrachunk leaf labels as if watching
 * made them reachable: common combat uses the audited COLLISION outer call. */
bool bw_health_rules_observes(const BwHealthRulesRuntime* runtime, const CPUState* cpu,
                             uint32_t address);
void bw_health_rules_dispatch(BwHealthRulesRuntime* runtime, CPUState* cpu,
                              uint32_t address, const BwHealthRulesLifetime* lifetime);
void bw_health_rules_stats(const BwHealthRulesRuntime* runtime, BwHealthRulesStats* stats);

#ifdef __cplusplus
}
#endif
#endif
