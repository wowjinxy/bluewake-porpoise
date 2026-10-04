#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gather_pipe.h"
extern void ppc_set_mem_write_journal(PPCMemWriteJournal, void*);

/* Standard translator entry/precise-charge contract; all memory operations
 * below use the maintained runtime and module wrappers, not fixture stubs. */
#define DOLRECOMP_C_LOOP_CYCLE_BUDGET (ctx->cycle_budget > 0 ? ctx->cycle_budget : 256)
static bool dolrecomp_block_can_precharge(const CPUState* ctx, u32 cycles) {
    const s64 remaining = ctx->cycle_deadline_budget + ctx->downcount;
    return ctx->cycle_deadline_budget <= 0 || (remaining >= 0 && (u64)remaining >= cycles);
}
static bool dolrecomp_charge_precise(CPUState* ctx, u32 cycles, u32 resume) {
    if (ctx->downcount <= -(s64)DOLRECOMP_C_LOOP_CYCLE_BUDGET) {
        ctx->pc = resume;
        return false;
    }
    ctx->downcount -= cycles;
    return true;
}
static CPUState* active;
static u32 mode, event_count;
static u64 events[128];
static void observe(CPUState* cpu) {
    assert(event_count + 4 < sizeof events / sizeof events[0]);
    events[event_count++] = cpu->pc;
    events[event_count++] = cpu->cycle_observation_suffix;
    events[event_count++] = (u64)cpu->downcount;
    events[event_count++] = cpu->gpr[3];
    if (mode & 1u) cpu->cycle_deadline_budget = 1 + mode % 6;
    /* Slow callbacks are allowed to update observation metadata; a rewrite
     * must retain it and use the updated suffix for its refund decision. */
    if (mode & 16u) {
        cpu->pc ^= 0x104u;
        cpu->cycle_observation_suffix ^= 0x15u;
    }
}
static u64 io_read(CPUState* cpu, u32 address, u8 size) {
    observe(cpu);
    return 0x123456789ABCDEF0ull ^ address ^ size;
}
static void io_write(CPUState* cpu, u32 address, u64 value, u8 size) {
    observe(cpu);
    events[event_count++] = address;
    events[event_count++] = value;
    events[event_count++] = size;
}
static void journal(u32 offset, u32 size, void* user) {
    assert(user == active);
    observe(active);
    events[event_count++] = offset;
    events[event_count++] = size;
}
#define mem_read8 bw_mem_read8
#define mem_read16 bw_mem_read16
#define mem_read32 bw_mem_read32
#define mem_read64 bw_mem_read64
#define mem_write8 bw_mem_write8
#define mem_write16 bw_mem_write16
#define mem_write32 bw_mem_write32
#define mem_write64 bw_mem_write64
typedef void (*Run)(CPUState*);
#include "rewritten_cases.h"

static u8 ram[4096], alias[64], initial_ram[4096], initial_alias[64];
static u8 wanted_ram[4096], wanted_alias[64];
static u64 wanted_events[128];
static u32 seed = 0x9348AA31u;
static u32 random32(void) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}
int main(void) {
    unsigned runs = 0;
    for (unsigned c = 0; c < sizeof cases / sizeof cases[0]; ++c) {
        for (unsigned scenario = 0; scenario < 1000; ++scenario) {
            CPUState initial = {0}, wanted = {0};
            initial.ram = ram; initial.ram_size = sizeof ram;
            initial.external_read = io_read; initial.external_write = io_write;
            for (unsigned r = 0; r < 32; ++r) initial.gpr[r] = random32();
            initial.pc = random32(); initial.lr = random32();
            initial.cr = random32(); initial.xer = random32();
            initial.cycle_observation_suffix = random32();
            initial.downcount = (s64)(random32() % 16) - 9;
            initial.cycle_budget = scenario % 5 ? 1 + random32() % 16 : 0;
            initial.cycle_deadline_budget = scenario % 4 ? 1 + random32() % 32 : 0;
            initial.reserve_valid = scenario % 3 == 0;
            initial.reserve_addr = 0x80000100u;
            const u32 addresses[] = {0x80000100u, 0xC0000100u, 0xCC006C00u, 0x80000FFFu};
            initial.gpr[4] = addresses[scenario % 4];
            initial.gpr[5] = addresses[(scenario / 4) % 4];
            initial.gpr[6] = addresses[(scenario / 16) % 4];
            for (unsigned i = 0; i < sizeof ram; ++i) initial_ram[i] = (u8)random32();
            for (unsigned i = 0; i < sizeof alias; ++i) initial_alias[i] = (u8)random32();
            u32 wanted_count = 0;
            for (unsigned variant = 0; variant < 3; ++variant) {
                CPUState cpu = initial;
                active = &cpu; mode = scenario;
                event_count = 0; memset(events, 0, sizeof events);
                memcpy(ram, initial_ram, sizeof ram);
                memcpy(alias, initial_alias, sizeof alias);
                ppc_guest_alias_clear();
                if (scenario & 4u) assert(ppc_guest_alias_add_shared(0x80000100u, sizeof alias, alias));
                ppc_set_mem_write_journal((scenario & 8u) ? journal : NULL, &cpu);
                cases[c][variant](&cpu);
                ++runs;
                if (variant == 0) {
                    wanted = cpu; wanted_count = event_count;
                    memcpy(wanted_ram, ram, sizeof ram);
                    memcpy(wanted_alias, alias, sizeof alias);
                    memcpy(wanted_events, events, sizeof events);
                } else if (memcmp(&wanted, &cpu, sizeof cpu) || wanted_count != event_count ||
                           memcmp(wanted_ram, ram, sizeof ram) || memcmp(wanted_alias, alias, sizeof alias) ||
                           memcmp(wanted_events, events, sizeof events)) {
                    fprintf(stderr, "case=%u scenario=%u variant=%u pc=%08X/%08X suffix=%u/%u events=%u/%u\n",
                            c, scenario, variant, wanted.pc, cpu.pc,
                            wanted.cycle_observation_suffix, cpu.cycle_observation_suffix, wanted_count, event_count);
                    return 1;
                }
            }
        }
    }
    assert(runs == 96000);
    puts("96000 equivalent runs: complete CPU/RAM/alias state and observation traces");
    return 0;
}
