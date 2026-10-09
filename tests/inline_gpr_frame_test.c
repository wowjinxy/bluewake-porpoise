/* SPDX-License-Identifier: GPL-3.0-or-later
 * Differential eligibility check against the previous per-word predicate.
 * This exercises the real memory resolver; queries must not mutate state. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "inline_gpr.h"
#include <assert.h>
#include <stdio.h>

#if defined(BW_GUEST_MEM1)
u8 BW_GUEST_MEM1[BW_GUEST_MEM1_SIZE];
#endif
static u8 ordinary_ram[1024], other_ram[1024], alias[128], exram[1024];
static CPUState cpu;
static unsigned checks;
static u32 random_state = 0x76543210u;

static u32 random_word(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
static bool old_ready(CPUState* state, unsigned first) {
    if (first < 14 || first > 31 || state->ram == NULL || g_mem_write_journal != NULL)
        return false;
    for (unsigned reg = first; reg < 32; ++reg) {
        const u32 address = state->gpr[11] + (u32)(4 * reg - 128);
        const u32 offset = (address & ~0x40000000u) - GC_RAM_BASE;
        if ((address & 3u) != 0u || state->ram_size < 4u || offset > state->ram_size - 4u ||
            get_ram_ptr(state, address, 4u, NULL) != state->ram + offset)
            return false;
    }
    return true;
}
static void check(u32 frame) {
    cpu.gpr[11] = frame;
    const CPUState before = cpu;
    for (unsigned first = 0; first <= 40; ++first) {
        const bool got = bw_inline_gpr_memory_ready(&cpu, first);
        const bool expected = old_ready(&cpu, first);
        if (got != expected) {
            fprintf(stderr, "readiness mismatch: frame=%08X first=%u got=%u expected=%u\n",
                    frame, first, got ? 1u : 0u, expected ? 1u : 0u);
            assert(got == expected);
        }
        assert(memcmp(&cpu, &before, sizeof cpu) == 0);
        ++checks;
    }
}
static void sweep(void) {
    static const u32 edges[] = {0u, 1u, 3u, 4u, 71u, 72u, 127u, 128u,
        0x7FFFFFFCu, 0x80000000u, 0x80000004u, 0x80000048u, 0x80000080u,
        0x800003FCu, 0x80000400u, 0x80000404u, 0x8FFFFFFCu, 0x90000000u,
        0xC0000000u, 0xC0000048u, 0xC0000400u, 0xCC008004u, 0xFFFFFFFCu};
    for (unsigned i = 0; i < sizeof edges / sizeof edges[0]; ++i) check(edges[i]);
    for (u32 address = 0x80000000u; address < 0x80000480u; ++address) {
        check(address);
        check(address | 0x40000000u);
    }
    for (unsigned i = 0; i < 4096; ++i) check(random_word());
}
static void journal(u32 offset, u32 size, void* user) {
    (void)offset; (void)size; (void)user;
    assert(!"readiness query must not write memory");
}
int main(void) {
    cpu.ram = ordinary_ram;
#if defined(BW_GUEST_MEM1)
    cpu.ram = BW_GUEST_MEM1;
#endif
    cpu.ram_size = 1024;
    sweep();
    cpu.reserve_valid = true; sweep();
    /* Aliases inside only one frame word must retain the original rejection. */
    assert(ppc_guest_alias_add_shared(0x800000ECu, 4u, alias)); sweep();
    assert(ppc_guest_alias_remove(0x800000ECu, 4u));
    assert(ppc_guest_alias_add_shared(0xC00000ECu, 4u, alias)); sweep();
    assert(ppc_guest_alias_remove(0xC00000ECu, 4u));
    assert(ppc_guest_alias_add_shared(0x800003F0u, 32u, alias)); sweep();
    assert(ppc_guest_alias_remove(0x800003F0u, 32u));
    cpu.exram = exram; cpu.exram_size = sizeof exram; sweep();
    cpu.ram_size = 3u; sweep();
    cpu.ram_size = 68u; sweep();
    cpu.ram_size = 1024u; cpu.ram = other_ram; sweep();
    g_mem_write_journal = journal; sweep(); g_mem_write_journal = NULL;
    cpu.ram = NULL; sweep();
    for (unsigned i = 0; i < sizeof ordinary_ram; ++i) {
        assert(ordinary_ram[i] == 0 && other_ram[i] == 0 && exram[i] == 0);
    }
    for (unsigned i = 0; i < sizeof alias; ++i) assert(alias[i] == 0);
    printf("Inline GPR frame: %u equivalent readiness queries\n", checks);
    return 0;
}
