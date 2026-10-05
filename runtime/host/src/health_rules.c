// SPDX-License-Identifier: GPL-3.0-or-later
#include "health_rules.h"

#include <math.h>
#include <string.h>

/* Primary source pinned zeldaret/tww49f2e3484e5814cbafde68525128669589e86bb2:
 * d_a_player_main.cpp4984/5060, d_item.cpp548/670, d_meter.cpp1405;
 * d_com_inf_game.h793/802 and d_save.h41. Outer changeDamageProc contains
 * same-chunk direct gotos to setDamagePoint; observe its genuine cross-chunk
 * return, not those bypassed entries. It contains only damage life additions;
 * children (including cross-chunk procLargeDamage) are not scaled twice.
 * Supplied CPU LR is required on admission, including cached/uncached DOL.
 * REL PCs are never generally canonicalized/admitted by this prototype. */
static const uint32_t kInfo = 0x803C4C08u, kPlay = 0x803C5EA8u;
static const uint32_t kLifeCount = 0x803CA764u, kMaxCount = 0x803CA77Eu;
static const uint32_t kPlayer = 0x803CA74Cu, kLink = 0x803CA754u;
static const uint32_t kStage = 0x803C9D3Cu, kNext = 0x803C9D54u;
static const uint32_t kRoom = 0x803F6A78u, kPause = 0x803F7097u;
static const uint32_t kOverlap = 0x803F6160u, kEvent = 0x803C9EA2u;
static const uint32_t kBaseType = 0x803F6A18u, kActorType = 0x803F69D0u;
static const uint32_t kProfile = 0x8038FD8Cu, kMethods = 0x8038FD68u;
static const uint32_t kItemTable = 0x803888C8u;
/* Other DOL damage origins, audited against native/source and slow+fast
 * generated callers. Explicitly omit startRestartRoom8012086C and scripted
 * setThrowDamage80128B18, and all four same-chunk changeDamageProc calls. */
static const uint32_t kDamageReturns[] = {
    0x801165F4u, 0x80116644u, 0x80117984u, 0x80118BACu,
    0x8013F744u, 0x8013F764u, 0x8015967Cu, 0x801596A0u
};
enum { kCollision = 1, kDamage = 2, kHeart = 3, kFairy = 4, kMaxTicks = 4 };

static uint32_t dol(uint32_t address) {
    if (address >= 0xC0003100u && address < 0xC0400000u)
        return address & ~0x40000000u;
    return address;
}
static bool damage_return(uint32_t address);
static bool candidate(uint32_t address) {
    return address == BW_HEALTH_RULES_COLLISION || address == BW_HEALTH_RULES_DAMAGE ||
        address == BW_HEALTH_RULES_HEART || address == BW_HEALTH_RULES_FAIRY ||
        address == BW_HEALTH_RULES_COLLISION_RETURN || address == BW_HEALTH_RULES_ITEM_RETURN ||
        damage_return(address);
}
static bool damage_return(uint32_t address) {
    for (unsigned i = 0; i < sizeof kDamageReturns / sizeof kDamageReturns[0]; ++i)
        if (kDamageReturns[i] == address) return true;
    return false;
}
static bool span(const CPUState* cpu, uint32_t address, uint32_t size) {
    if (cpu == NULL || cpu->ram == NULL || address < 0x80000000u) return false;
    const uint32_t offset = address - 0x80000000u;
    return size <= BW_HEALTH_RULES_MEM1_SIZE && offset <= BW_HEALTH_RULES_MEM1_SIZE - size &&
           size <= cpu->ram_size && offset <= cpu->ram_size - size;
}
static bool object(const CPUState* cpu, uint32_t address, uint32_t size) {
    return (address & 3u) == 0 && span(cpu, address, size);
}
static bool attached(const BwHealthRulesRuntime* r, const CPUState* cpu);
/* Non-faulting read-only projection. All fields remain in native24MiB even
 * though the CPU owns32MiB. Each wider field must agree with every native
 * byte's alias backing; a partial alias cannot authorize stale flat RAM.
 * No external_read callback, guest register/exception write or pointer cache. */
static const uint8_t* read_pointer(BwHealthRulesRuntime* r, const CPUState* cpu,
                                   uint32_t address, uint32_t size) {
    if (!attached(r,cpu) || r->read_alias_generation != g_ppc_guest_alias_generation ||
        !span(cpu,address,size)) { r->read_failed=true; return NULL; }
    const uint8_t* p=get_ram_ptr((CPUState*)cpu,address,size,NULL);
    if (!p) { r->read_failed=true; return NULL; }
    for (uint32_t i=0;i<size;++i) {
        if (!attached(r,cpu) || r->read_alias_generation != g_ppc_guest_alias_generation ||
            get_ram_ptr((CPUState*)cpu,address+i,1,NULL)!=p+i) {
            r->read_failed=true; return NULL;
        }
    }
    return p;
}
static uint32_t r32(BwHealthRulesRuntime* r,const CPUState* cpu,uint32_t address) {
    const uint8_t* p=read_pointer(r,cpu,address,4); return p?read_be32(p):0;
}
static uint16_t r16(BwHealthRulesRuntime* r,const CPUState* cpu,uint32_t address) {
    const uint8_t* p=read_pointer(r,cpu,address,2); return p?read_be16(p):0;
}
static uint8_t r8(BwHealthRulesRuntime* r,const CPUState* cpu,uint32_t address) {
    const uint8_t* p=read_pointer(r,cpu,address,1); return p?*p:0;
}
static float rf(BwHealthRulesRuntime* r,const CPUState* cpu,uint32_t address) {
    const uint32_t bits=r32(r,cpu,address);float value;memcpy(&value,&bits,sizeof value);return value;
}
static bool attached(const BwHealthRulesRuntime* r, const CPUState* cpu) {
    return r != NULL && cpu != NULL && cpu == r->cpu && cpu->ram == r->ram &&
        cpu->ram != NULL && cpu->ram_size == r->ram_size &&
        r->ram_size == BW_HEALTH_RULES_HOST_RAM_SIZE && r->abi == BW_HEALTH_RULES_ABI_GZLE01;
}
static void cancel(BwHealthRulesRuntime* r, BwHealthRulesPending* p) {
    if (p->active) ++r->stats.cancelled;
    p->active = false;
}
static void cancel_all(BwHealthRulesRuntime* r) {
    for (unsigned i = 0; i < BW_HEALTH_RULES_PENDING; ++i) cancel(r, &r->pending[i]);
}
BwHealthRulesConfig bw_health_rules_native_config(void) {
    BwHealthRulesConfig result = { BW_HEALTH_RULES_NATIVE_RATE, BW_HEALTH_RULES_NATIVE_RATE };
    return result;
}
bool bw_health_rules_adjust(float before, float after, uint16_t rate, float* out) {
    if (out == NULL || rate > BW_HEALTH_RULES_MAX_RATE || !isfinite(before) || !isfinite(after) ||
        fabsf(before) > 256.0f || fabsf(after) > 256.0f) return false;
    const double delta = (double)after - before;
    if (fabs(delta) > 80.0) return false;
    if (rate == BW_HEALTH_RULES_NATIVE_RATE) { *out = after; return true; }
    const double adjusted = (double)before + delta * ((double)rate / 256.0);
    if (!isfinite(adjusted) || fabs(adjusted) > 2048.0) return false;
    *out = (float)adjusted;
    return true;
}
void bw_health_rules_init(BwHealthRulesRuntime* r) {
    if (r == NULL) return;
    memset(r, 0, sizeof *r); r->config = bw_health_rules_native_config();
}
bool bw_health_rules_configure(BwHealthRulesRuntime* r, const BwHealthRulesConfig* config) {
    if (r == NULL || config == NULL || config->damage_q8 > BW_HEALTH_RULES_MAX_RATE ||
        config->healing_q8 > BW_HEALTH_RULES_MAX_RATE) return false;
    if (memcmp(&r->config, config, sizeof *config) != 0) {
        cancel_all(r); r->config = *config;
    }
    return true;
}
bool bw_health_rules_attach(BwHealthRulesRuntime* r, CPUState* cpu,
                            const BwHealthRulesLifetime* lifetime, uint32_t abi) {
    if (r == NULL) return false;
    cancel_all(r); r->cpu = NULL; r->ram = NULL; r->ram_size = r->abi = 0;
    if (cpu == NULL || cpu->ram == NULL || cpu->ram_size != BW_HEALTH_RULES_HOST_RAM_SIZE || lifetime == NULL ||
        abi != BW_HEALTH_RULES_ABI_GZLE01 || lifetime->epoch == 0 || lifetime->scene_generation == 0)
        return false;
    r->cpu = cpu; r->ram = cpu->ram; r->ram_size = cpu->ram_size; r->abi = abi;
    r->lifetime = *lifetime;
    return true;
}
void bw_health_rules_reset(BwHealthRulesRuntime* r, const BwHealthRulesLifetime* lifetime) {
    if (r == NULL) return;
    cancel_all(r);
    if (lifetime != NULL) r->lifetime = *lifetime;
    else bw_health_rules_detach(r);
}
void bw_health_rules_detach(BwHealthRulesRuntime* r) {
    if (r == NULL) return;
    cancel_all(r); r->cpu = NULL; r->ram = NULL; r->ram_size = r->abi = 0;
    memset(&r->lifetime, 0, sizeof r->lifetime);
}
static bool lifetime_update(BwHealthRulesRuntime* r, CPUState* cpu,
                            const BwHealthRulesLifetime* lifetime) {
    if (!attached(r, cpu)) { if (r != NULL) bw_health_rules_detach(r); return false; }
    if (lifetime == NULL || lifetime->epoch == 0 || lifetime->scene_generation == 0) {
        cancel_all(r); return false;
    }
    if (lifetime->epoch != r->lifetime.epoch || lifetime->scene_generation != r->lifetime.scene_generation ||
        lifetime->tick < r->lifetime.tick) cancel_all(r);
    r->lifetime = *lifetime;
    for (unsigned i = 0; i < BW_HEALTH_RULES_PENDING; ++i)
        if (r->pending[i].active && lifetime->tick - r->pending[i].tick > kMaxTicks)
            cancel(r, &r->pending[i]);
    return true;
}
void bw_health_rules_retrace(BwHealthRulesRuntime* r, CPUState* cpu,
                             const BwHealthRulesLifetime* lifetime) {
    (void)lifetime_update(r, cpu, lifetime);
}
static bool outer_active(const BwHealthRulesRuntime* r) {
    for (unsigned i = 0; i < BW_HEALTH_RULES_PENDING; ++i)
        if (r->pending[i].active && r->pending[i].kind == kCollision) return true;
    return false;
}
bool bw_health_rules_observes(const BwHealthRulesRuntime* r, const CPUState* cpu, uint32_t address) {
    address = dol(address);
    if (!candidate(address) || !attached(r, cpu)) return false;
    for (unsigned i = 0; i < BW_HEALTH_RULES_PENDING; ++i)
        if (r->pending[i].active && r->pending[i].return_pc == address) return true;
    if (address == BW_HEALTH_RULES_COLLISION)
        return r->config.damage_q8 != BW_HEALTH_RULES_NATIVE_RATE &&
            dol(cpu->lr) == BW_HEALTH_RULES_COLLISION_RETURN;
    if (address == BW_HEALTH_RULES_DAMAGE)
        return r->config.damage_q8 != BW_HEALTH_RULES_NATIVE_RATE && !outer_active(r) &&
            damage_return(dol(cpu->lr));
    if (address == BW_HEALTH_RULES_HEART || address == BW_HEALTH_RULES_FAIRY)
        return r->config.healing_q8 != BW_HEALTH_RULES_NATIVE_RATE &&
            dol(cpu->lr) == BW_HEALTH_RULES_ITEM_RETURN;
    return false;
}
static bool actor(BwHealthRulesRuntime* r,const CPUState* cpu, uint32_t p) {
    if (!object(cpu, p, 0x4C28u) || r32(r,cpu, kBaseType) == 0 || r32(r,cpu, kActorType) == 0 ||
        r32(r,cpu, p) != r32(r,cpu, kBaseType) || r32(r,cpu, p + 0xC0u) != r32(r,cpu, kActorType) ||
        r32(r,cpu, p + 4u) == 0 || r32(r,cpu, p + 4u) == UINT32_MAX ||
        r16(r,cpu, p + 8u) != 0xA9u || r16(r,cpu, p + 0xEu) != 0xA9u || r8(r,cpu, p + 0xBu) != 0 ||
        dol(r32(r,cpu, p + 0x10u)) != kProfile || dol(r32(r,cpu, p + 0xECu)) != kMethods ||
        r8(r,cpu, p + 0x1BEu) != 1 || (r32(r,cpu, p + 0x1C8u) & 2u) != 0 ||
        r16(r,cpu, kProfile + 8u) != 0xA9u || r32(r,cpu, kProfile + 0x10u) != 0x4C28u ||
        dol(r32(r,cpu, kProfile + 0x24u)) != kMethods || dol(r32(r,cpu, kMethods + 8u)) != 0x80122D30u ||
        r32(r,cpu, p + 0x498u) != p + 0x1F8u) return false;
    for (unsigned i = 0; i < 3; ++i)
        if (!isfinite(rf(r,cpu, p + 0x1F8u + 4u * i))) return false;
    return true;
}
static bool identity(BwHealthRulesRuntime* r, CPUState* cpu, BwHealthRulesIdentity* out) {
    if (!span(cpu, kInfo, 0x5CF0u) || !span(cpu, kRoom, 1) || !span(cpu, kPause, 1) ||
        !span(cpu, kOverlap, 4) || !span(cpu, kBaseType, 4) || !span(cpu, kActorType, 4) ||
        !span(cpu, kProfile, 0x34u) || !span(cpu, kMethods, 0x24u) ||
        cpu->exception != 0 || (cpu->fpscr & 3u) != 0 ||
        r8(r,cpu, kPause) != 0 || r8(r,cpu, kNext) != 0 || r32(r,cpu, kOverlap) != 0 ||
        r8(r,cpu, kEvent) != 0 || r8(r,cpu, kRoom) >= 0x80u || r8(r,cpu, kStage) == 0 ||
        r8(r,cpu, kPlay + 0x4A20u) != 0 || r8(r,cpu, kPlay + 0x4959u) != 0 ||
        r8(r,cpu, kPlay + 0x4A3Au) != 0 || r16(r,cpu, kMaxCount) != 0 ||
        (cpu->gpr[1] & 15u) != 0 || !object(cpu, cpu->gpr[1], 0x18u)) return false;
    /* Read-only native MEM1 validation must agree with the actual memory helper
     * used for the sole write. An alias over the fixed counter fails closed. */
    if (get_ram_ptr(cpu, kLifeCount, 4u, NULL) != cpu->ram + (kLifeCount - 0x80000000u)) return false;
    uint8_t stage[12];
    for (unsigned i = 0; i < sizeof stage; ++i) stage[i] = r8(r,cpu, kStage + i);
    bool ended = false;
    for (unsigned i = 0; i < 8; ++i) {
        const uint8_t ch = stage[i];
        if (ch == 0) { ended = true; continue; }
        if (ended || (!(ch >= 'a' && ch <= 'z') && !(ch >= 'A' && ch <= 'Z') &&
                      !(ch >= '0' && ch <= '9') && ch != '_')) return false;
    }
    if (memcmp(stage, "Xboss", 5u) == 0) return false;
    const uint32_t p = r32(r,cpu, kPlayer);
    const uint16_t life = r16(r,cpu, kInfo + 2u), max_life = r16(r,cpu, kInfo);
    if (p != r32(r,cpu, kLink) || !actor(r,cpu, p) || r16(r,cpu, p + 0x304u) != 0 ||
        r32(r,cpu, p + 0x314u) != 0 || life == 0 || max_life < 4u || max_life > 80u ||
        life > (max_life / 4u) * 4u) return false;
    memset(out, 0, sizeof *out);
    out->player = p; out->player_id = r32(r,cpu, p + 4u); out->player_type = r32(r,cpu, p);
    out->stack = cpu->gpr[1]; out->backchain = r32(r,cpu, out->stack); out->saved_lr = r32(r,cpu, out->stack + 4u);
    if (out->backchain <= out->stack || out->backchain - out->stack > 0x10000u ||
        (out->backchain & 15u) != 0 || !object(cpu, out->backchain, 8u)) return false;
    out->life = life; out->max_life = max_life; out->room = r8(r,cpu, kRoom);
    memcpy(out->stage, stage, sizeof out->stage);
    out->epoch = r->lifetime.epoch; out->generation = r->lifetime.scene_generation;
    out->alias_generation = g_ppc_guest_alias_generation;
    return !r->read_failed;
}
void bw_health_rules_dispatch(BwHealthRulesRuntime* r, CPUState* cpu, uint32_t address,
                              const BwHealthRulesLifetime* lifetime) {
    address = dol(address);
    if (!candidate(address) || r == NULL ||
        (r->config.damage_q8==256 && r->config.healing_q8==256)) return;
    if (!lifetime_update(r, cpu, lifetime)) return;
    r->read_failed=false;r->read_alias_generation=g_ppc_guest_alias_generation;
    /* Child entry can be presented after a budget yield even though normal
     * same-chunk execution bypasses the host. The outer owns all such damage. */
    if (address == BW_HEALTH_RULES_DAMAGE && outer_active(r)) {
        ++r->stats.nested_damage_ignored; return;
    }
    for (unsigned i = 0; i < BW_HEALTH_RULES_PENDING; ++i) {
        BwHealthRulesPending* p = &r->pending[i];
        if (!p->active || p->return_pc != address || p->identity.stack != cpu->gpr[1]) continue;
        BwHealthRulesPending call = *p; p->active = false;
        BwHealthRulesIdentity now;
        if (dol(cpu->lr) != address || !identity(r, cpu, &now)) { ++r->stats.cancelled; return; }
        /* Native framed callees save LR into the caller's linkage area at
         * entry-SP+4, overwriting its previous contents. The unframed item
         * leaves preserve that word. Never require a stale linkage value. */
        if ((call.kind <= kDamage && dol(now.saved_lr) != address) ||
            (call.kind >= kHeart && now.saved_lr != call.identity.saved_lr)) {
            ++r->stats.cancelled; return;
        }
        now.saved_lr = call.identity.saved_lr;
        if (memcmp(&now, &call.identity, sizeof now) != 0) { ++r->stats.cancelled; return; }
        const float after = rf(r,cpu, kLifeCount);
        const double delta = (double)after - call.before;
        float adjusted;
        if (r->read_failed || !isfinite(after) || fabs(delta) > 80.0 ||
            (call.kind == kCollision && delta > 0.0) ||
            (call.kind == kDamage && (cpu->gpr[3] != 1u ||
               after != (float)((double)call.before + call.amount))) ||
            (call.kind >= kHeart && after != (float)((double)call.before + call.amount)) ||
            !bw_health_rules_adjust(call.before, after, call.rate, &adjusted)) {
            ++r->stats.cancelled; return;
        }
        ++r->stats.completed;
        uint32_t before_bits, after_bits;
        memcpy(&before_bits, &after, sizeof before_bits); memcpy(&after_bits, &adjusted, sizeof after_bits);
        if (before_bits != after_bits) {
            /* Preserve write-journal/reservation semantics of actual GXRuntime.
             * All validation is complete before this sole permitted write. */
            /* Revalidate the only writable field immediately before preserving
             * native journal/reservation semantics. No borrowed pointer write. */
            const uint8_t* target=read_pointer(r,cpu,kLifeCount,4);
            if(r->read_failed || target!=r->ram+(kLifeCount-0x80000000u)) {
                ++r->stats.cancelled;return;
            }
            mem_write32(cpu, kLifeCount, after_bits); ++r->stats.adjusted;
        }
        return;
    }
    if (!bw_health_rules_observes(r, cpu, address)) return;
    unsigned kind;
    float amount = 0;
    if (address == BW_HEALTH_RULES_COLLISION) kind = kCollision;
    else if (address == BW_HEALTH_RULES_DAMAGE) {
        kind = kDamage; amount = (float)cpu->fpr[1];
        if (!isfinite(cpu->fpr[1]) || cpu->fpr[1] != amount || amount >= 0 || amount < -80) {
            ++r->stats.rejected; return;
        }
    } else if (address == BW_HEALTH_RULES_HEART || address == BW_HEALTH_RULES_FAIRY) {
        kind = address == BW_HEALTH_RULES_HEART ? kHeart : kFairy;
        amount = kind == kHeart ? 4.0f : 40.0f;
        const unsigned item = kind == kHeart ? 0u : 0x16u;
        if (!span(cpu, kItemTable + item * 4u, 4u) ||
            dol(r32(r,cpu, kItemTable + item * 4u)) != address ||
            dol(cpu->ctr) != address || dol(cpu->gpr[12]) != address) { ++r->stats.rejected; return; }
    } else return;
    BwHealthRulesIdentity now;
    const float before = rf(r,cpu, kLifeCount);
    if (!identity(r, cpu, &now) || r->read_failed || !isfinite(before) || fabsf(before) > 256.0f ||
        (kind <= kDamage && cpu->gpr[3] != now.player)) { ++r->stats.rejected; return; }
    BwHealthRulesPending* free_slot = NULL;
    for (unsigned i = 0; i < BW_HEALTH_RULES_PENDING; ++i) {
        BwHealthRulesPending* p = &r->pending[i];
        if (p->active && p->entry == address && p->identity.stack == now.stack) return;
        if (!p->active && free_slot == NULL) free_slot = p;
        /* A healing invocation nested inside a collision would invalidate its
         * source-audited aggregate. Cancel rather than folding mixed awards. */
        if (p->active && p->kind == kCollision && kind >= kHeart) cancel(r, p);
    }
    if (free_slot == NULL) { ++r->stats.pending_overflow; return; }
    memset(free_slot, 0, sizeof *free_slot); free_slot->active = true;
    free_slot->kind = (uint8_t)kind; free_slot->entry = address; free_slot->return_pc = dol(cpu->lr);
    free_slot->identity = now; free_slot->before = before; free_slot->amount = amount;
    free_slot->tick = r->lifetime.tick;
    free_slot->rate = kind <= kDamage ? r->config.damage_q8 : r->config.healing_q8;
    if (kind <= kDamage) ++r->stats.damage_entries; else ++r->stats.healing_entries;
}
void bw_health_rules_stats(const BwHealthRulesRuntime* r, BwHealthRulesStats* stats) {
    if (r != NULL && stats != NULL) *stats = r->stats;
}
