/* Invented RAM qualification for all six routed hooks: complete CPU/RAM
 * preservation on observer, alias, journal, budget and deadline refusal.
 * Real translation equivalence is exercised by run_native_search_oracle.py. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_search.h"
#include "native_entries.h"
#include "direct_calls.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static u8 ram[GC_MAIN_RAM_SIZE], before_ram[GC_MAIN_RAM_SIZE];
static bool dirty, pending, observe_all;
static u32 cause, mask, refused, initial_sp, initial_lr, active_entry;
static unsigned queried, helpers;
static const u32 entries[] = {BLUEWAKE_SEARCH_STRCMP, BLUEWAKE_SEARCH_STAGE_NAME,
    BLUEWAKE_SEARCH_NAME_LOOP, BLUEWAKE_SEARCH_NAME_RESULT, BLUEWAKE_SEARCH_NAME_STEP,
    BLUEWAKE_SEARCH_JUDGE_FILTER};
#define TABLE 0x80372818u
#define NAME 0x80100100u
#define NODE 0x80100200u
#define FILTER 0x80100300u
#define PRM 0x80100400u
#define ACTOR 0x80100800u

BwChunkFn bw_find_chunk(u32 address) { (void)address; return NULL; }
BwChunkFn* const bw_chunk_fns = NULL;
static void put(u32 address, u32 value) { write_be32(ram + address - GC_RAM_BASE, value); }
static void journal(u32 address, u32 bytes, void* user) { (void)address; (void)bytes; (void)user; abort(); }

static bool ready(void* user, const CPUState* cpu, u32 address) {
    assert(user == &observe_all && cpu != NULL); queried++;
    if (address == 0x80328F40u || address == 0x80328F8Cu ||
        address == 0x80041558u || address == 0x800415A4u) {
        const bool judge = active_entry == BLUEWAKE_SEARCH_JUDGE_FILTER;
        const u32 top = initial_sp - (judge ? 32u : 0u);
        assert(cpu->pc == address && cpu->gpr[1] == top - 32u && cpu->gpr[11] == top);
        assert(cpu->lr == (address == 0x80328F40u || address == 0x80041558u ?
                          0x80041558u : 0x800415A4u));
        if (address == 0x80328F40u)
            assert(cpu->gpr[0] == (judge ? 0x80028394u : initial_lr));
        helpers++;
    }
    return !observe_all && address != refused;
}

static CPUState state(u32 entry) {
    memset(ram, 0xAB, sizeof ram);
    for (unsigned i = 0; i < 825u; ++i) {
        u8* p = ram + (TABLE - GC_RAM_BASE) + 12u * i;
        memset(p, 0, 12u); p[0] = 'x'; p[8] = 0x01u; p[9] = 0x23u;
    }
    ram[NAME - GC_RAM_BASE] = 'a'; ram[NAME + 1u - GC_RAM_BASE] = 0u;
    ram[TABLE - GC_RAM_BASE] = 'a';
    ram[TABLE + 12u - GC_RAM_BASE] = 'a';
    CPUState cpu = {0};
    for (unsigned i = 0; i < 32u; ++i) cpu.gpr[i] = 0xBEE00000u + 0x0101u * i;
    cpu.ram = ram; cpu.ram_size = sizeof ram; cpu.pc = entry; cpu.lr = 0xFFFFFFFCu;
    cpu.gpr[1] = 0x80110000u; cpu.gpr[3] = NAME; cpu.gpr[4] = TABLE;
    cpu.cycle_budget = 20000; cpu.cycle_observation_suffix = 0xEDu;
    cpu.xer = 0xA0000000u; cpu.cr = 0x89ABCDEFu;
    initial_sp = cpu.gpr[1]; initial_lr = cpu.lr; active_entry = entry;
    if (entry == BLUEWAKE_SEARCH_NAME_LOOP || entry == BLUEWAKE_SEARCH_NAME_RESULT || entry == BLUEWAKE_SEARCH_NAME_STEP) {
        put(initial_sp - 12u, cpu.gpr[29]); put(initial_sp - 8u, cpu.gpr[30]);
        put(initial_sp - 4u, cpu.gpr[31]); put(initial_sp + 4u, cpu.lr);
        cpu.gpr[1] -= 32u; cpu.gpr[29] = NAME; cpu.gpr[30] = 0u; cpu.gpr[31] = TABLE;
        cpu.gpr[3] = entry == BLUEWAKE_SEARCH_NAME_RESULT ? 0u : 0x80100100u;
    }
    if (entry == BLUEWAKE_SEARCH_JUDGE_FILTER) {
        put(NODE + 12u, ACTOR); put(FILTER, 0x8002833Cu); put(FILTER + 4u, PRM);
        put(PRM, NAME); put(PRM + 4u, 0u); put(PRM + 8u, 0u);
        ram[ACTOR + 14u - GC_RAM_BASE] = 0x01u; ram[ACTOR + 15u - GC_RAM_BASE] = 0x23u;
        ram[ACTOR + 449u - GC_RAM_BASE] = 0u;
        cpu.gpr[3] = NODE; cpu.gpr[4] = FILTER;
    }
    dirty = pending = observe_all = false; cause = mask = refused = queried = helpers = 0;
    bw_host_sources_dirty = &dirty; bw_host_decrementer_pending = &pending;
    bw_host_pi_cause = &cause; bw_host_pi_mask = &mask;
    bw_host_can_skip = ready; bw_host_can_skip_user = &observe_all;
    bw_edge_filter_enabled = bw_edge_watch_ready = true;
    memset(bw_edge_watch_table, 0, sizeof bw_edge_watch_table);
    g_mem_write_journal = NULL; g_ppc_guest_aliases_overlap_mem1 = false;
    bluewake_composite_native_entries_v1(true, ready, &observe_all);
    return cpu;
}

static void decline(CPUState cpu, u32 entry) {
    const CPUState before = cpu;
    memcpy(before_ram, ram, sizeof ram);
    assert(!bluewake_native_entries_try(&cpu, entry));
    assert(!memcmp(&cpu, &before, sizeof cpu));
    assert(!memcmp(ram, before_ram, sizeof ram));
}

int main(void) {
    unsigned accepted = 0, declined = 0;
    for (unsigned i = 0; i < sizeof entries / sizeof entries[0]; ++i) {
        const u32 entry = entries[i]; CPUState cpu = state(entry);
        assert(bluewake_native_entries_try(&cpu, entry));
        assert(cpu.pc == 0xFFFFFFFCu && queried > 0u); accepted++;
        if (entry != BLUEWAKE_SEARCH_STRCMP) assert(helpers >= 2u);
        cpu = state(entry); bluewake_composite_native_entries_v1(false, ready, &observe_all); decline(cpu, entry); declined++;
        cpu = state(entry); bluewake_composite_native_entries_v1(true, NULL, NULL); decline(cpu, entry); declined++;
        cpu = state(entry); observe_all = true; decline(cpu, entry); declined++;
        cpu = state(entry); refused = entry; decline(cpu, entry); declined++;
        cpu = state(entry); cpu.exception = 1u; decline(cpu, entry); declined++;
        cpu = state(entry); g_ppc_guest_aliases_overlap_mem1 = true; decline(cpu, entry); declined++;
        cpu = state(entry); cpu.downcount = -cpu.cycle_budget; decline(cpu, entry); declined++;
        cpu = state(entry); cpu.downcount = 1; decline(cpu, entry); declined++;
        for (unsigned deadline = 1; deadline < 8u; ++deadline) {
            cpu = state(entry); cpu.cycle_deadline_budget = deadline; decline(cpu, entry); declined++;
        }
        cpu = state(entry); cpu.cycle_budget = 1; decline(cpu, entry); declined++;
        if (entry != BLUEWAKE_SEARCH_STRCMP) {
            const u32 boundaries[] = {BLUEWAKE_SEARCH_STRCMP, BLUEWAKE_SEARCH_NAME_RESULT, 0x80328F8Cu, 0x800415A4u};
            for (unsigned b = 0; b < sizeof boundaries / sizeof boundaries[0]; ++b) {
                cpu = state(entry); refused = boundaries[b]; decline(cpu, entry); declined++;
            }
            cpu = state(entry); bw_host_can_skip = NULL; decline(cpu, entry); declined++;
            cpu = state(entry); bw_host_sources_dirty = NULL; decline(cpu, entry); declined++;
            cpu = state(entry); dirty = true; decline(cpu, entry); declined++;
            cpu = state(entry); pending = true; cpu.msr |= PPC_MSR_EE; decline(cpu, entry); declined++;
            cpu = state(entry); cause = mask = 1u; cpu.msr |= PPC_MSR_EE; decline(cpu, entry); declined++;
        }
        if (entry == BLUEWAKE_SEARCH_STAGE_NAME || entry == BLUEWAKE_SEARCH_JUDGE_FILTER) {
            cpu = state(entry); refused = 0x80328F40u; decline(cpu, entry); declined++;
            cpu = state(entry); refused = 0x80041558u; decline(cpu, entry); declined++;
            cpu = state(entry); g_mem_write_journal = journal; decline(cpu, entry); declined++;
        }
        if (entry == BLUEWAKE_SEARCH_STAGE_NAME) {
            cpu = state(entry);
            const u32 alias = 0xC1F00000u;
            assert(ppc_guest_alias_add_shared(alias, 16u, ram + initial_sp - GC_RAM_BASE - 12u));
            ram[initial_sp - GC_RAM_BASE - 12u] = 'a';
            ram[initial_sp - GC_RAM_BASE - 11u] = 0u;
            cpu.gpr[3] = alias; decline(cpu, entry); declined++;
        }
        if (entry == BLUEWAKE_SEARCH_STRCMP) {
            cpu = state(entry); cpu.gpr[3] = 0xCC008000u; decline(cpu, entry); declined++;
            cpu = state(entry); cpu.gpr[4] = 0u; decline(cpu, entry); declined++;
        }
    }
    g_mem_write_journal = NULL; g_ppc_guest_aliases_overlap_mem1 = false;
    printf("Native search routing guards: %u accepted; %u refused with every CPU/RAM byte unchanged\n", accepted, declined);
    return 0;
}
