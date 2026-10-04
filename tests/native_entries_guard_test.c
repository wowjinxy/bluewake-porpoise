/* Invented RAM fixtures, no disc/source: all nine entries, observers, aliases,
 * budgets, state preservation and FIFO batching/word order. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_entries.h"
#include "native_fifo.h"
#include "native_bg.h"
#include "native_vec.h"
#include "native_mtxcalc.h"
#include "direct_calls.h"
#include "gather_pipe.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static u8 ram[GC_MAIN_RAM_SIZE], saved[GC_MAIN_RAM_SIZE], stream[8192];
static unsigned stream_size, queries;
static bool allow = true, dirty, pending;
static u32 cause, mask, refused, initial_sp, expected_save_lr;
static const u32 entries[] = {BLUEWAKE_J3D_FIFO_POS_MTX, BLUEWAKE_J3D_FIFO_NRM_MTX,
    BLUEWAKE_J3D_FIFO_NRM_MTX33, BLUEWAKE_BG_CHK_SAME_ACTOR_PID, BLUEWAKE_BG_CHK_GRP_THROUGH,
    BLUEWAKE_PSMTX_MULT_VEC_SR, BLUEWAKE_MTXCALC_BASIC, BLUEWAKE_MTXCALC_SOFTIMAGE, BLUEWAKE_MTXCALC_MAYA};

BwChunkFn bw_find_chunk(u32 address) { (void)address; return NULL; }
BwChunkFn* const bw_chunk_fns = NULL;
static void put(u32 at, u32 value) { write_be32(ram + at - GC_RAM_BASE, value); }
static u32 word(u32 at) { return read_be32(ram + at - GC_RAM_BASE); }
static bool ready(void* user, const CPUState* cpu, u32 address) {
    assert(user == &allow && cpu != NULL); ++queries;
    if (address == 0x80328F40u || address == 0x80328F38u) {
        assert(cpu->pc == address && cpu->lr == expected_save_lr);
        assert(cpu->gpr[11] == initial_sp);
        assert(cpu->gpr[1] == initial_sp - (address == 0x80328F38u ? 80u : 96u));
    }
    return allow && address != refused;
}
static void pipe_word(u64 value, u8 size) {
    for (u8 i = size; i > 0; --i) stream[stream_size++] = (u8)(value >> (8u * (i - 1u)));
}
static void pipe_bytes(const u8* data, u32 size) {
    memcpy(stream + stream_size, data, size); stream_size += size;
}
static void journal(u32 a, u32 n, void* user) { (void)a; (void)n; (void)user; assert(0); }

static CPUState state(u32 entry) {
    memset(ram, 0, sizeof ram);
    bw_gather_pipe_length = 0; stream_size = 0; queries = 0;
    allow = true; refused = 0; dirty = pending = false; cause = mask = 0;
    bw_host_sources_dirty = &dirty; bw_host_decrementer_pending = &pending;
    bw_host_pi_cause = &cause; bw_host_pi_mask = &mask;
    bw_host_can_skip = ready; bw_host_can_skip_user = &allow;
    bw_edge_filter_enabled = bw_edge_watch_ready = true;
    memset(bw_edge_watch_table, 0, sizeof bw_edge_watch_table);
    bw_gather_pipe_write = pipe_word; bw_gather_pipe_bytes = pipe_bytes;
    CPUState cpu = {0}; cpu.ram = ram; cpu.ram_size = sizeof ram;
    cpu.pc = entry; cpu.lr = 0x80001003u;
    cpu.msr = PPC_MSR_FP; cpu.hid2 = PPC_HID2_LSQE;
    cpu.cycle_budget = cpu.cycle_deadline_budget = 4096;
    cpu.gpr[1] = 0x80108000u; initial_sp = cpu.gpr[1];
    cpu.gpr[3] = 0x80100000u; cpu.gpr[4] = 0x80100100u; cpu.gpr[5] = 0x80100200u;
    if (entry == BLUEWAKE_BG_CHK_SAME_ACTOR_PID) {
        cpu.gpr[4] = 42; put(cpu.gpr[3] + 8, 42); ram[cpu.gpr[3] + 12 - GC_RAM_BASE] = 1;
    } else if (entry == BLUEWAKE_BG_CHK_GRP_THROUGH) {
        cpu.gpr[4] = 0; cpu.gpr[6] = 2;
        put(cpu.gpr[3] + 148, 0x80100300u); put(0x80100324u, 0x80100400u);
        put(0x80100430u, 0x200u); put(cpu.gpr[5] + 4, 4);
    } else if (entry == BLUEWAKE_PSMTX_MULT_VEC_SR) {
        for (unsigned i = 0; i < 12; ++i) put(cpu.gpr[3] + 4*i, i % 5 == 0 ? 0x3F800000u : 0);
        put(cpu.gpr[4], 0x40000000u); put(cpu.gpr[4] + 4, 0x40400000u); put(cpu.gpr[4] + 8, 0x40800000u);
    } else if (entry >= BLUEWAKE_MTXCALC_BASIC && entry <= BLUEWAKE_MTXCALC_MAYA) {
        cpu.gpr[4] = 0; cpu.gpr[2] = 0x80103000u + 13072u; cpu.gpr[13] = 0x80103100u + 26460u;
        put(0x803EDA90u, 0x80101000u); put(0x80101084u, 0x80101100u); put(0x8010108Cu, 0x80101200u);
        put(0x80101004u, 0x80101300u); put(0x8010132Cu, 0x80101400u); put(0x80101400u, 0x80101500u);
        put(0x80103000u, 0x3F800000u); put(0x803F66F0u, 0); put(0x803F66F4u, 0x3F800000u);
        put(0x80103100u, 0); put(0x80103104u, 0x80103200u); put(0x80103108u, 0x80103300u);
        put(0x80103200u, 0); put(0x80103300u, 0x3F800000u);
        for (unsigned i = 0; i < 12; ++i) put(0x803EDB80u + 4*i, i % 5 == 0 ? 0x3F800000u : 0);
        for (unsigned i = 0; i < 3; ++i) {
            put(cpu.gpr[5] + 4*i, 0x3F800000u);
            put(0x803EDBB0u + 4*i, 0x3F800000u); put(0x803EDBBCu + 4*i, 0x3F800000u);
        }
        put(cpu.gpr[5] + 20, 0x40000000u); put(cpu.gpr[5] + 24, 0x40400000u); put(cpu.gpr[5] + 28, 0x40800000u);
        expected_save_lr = entry == BLUEWAKE_MTXCALC_BASIC ? 0x802F50A4u :
                           entry == BLUEWAKE_MTXCALC_SOFTIMAGE ? 0x802F52D0u : 0x802F551Cu;
    } else {
        cpu.gpr[4] = 3;
        for (unsigned i = 0; i < 12; ++i) put(cpu.gpr[3] + 4*i, 0x01020300u + i);
    }
    ppc_fpscr_updated(&cpu);
    return cpu;
}
static void decline(CPUState cpu, u32 entry) {
    CPUState before = cpu; memcpy(saved, ram, sizeof ram);
    u8 batch[sizeof bw_gather_pipe_buffer]; memcpy(batch, bw_gather_pipe_buffer, sizeof batch);
    const unsigned length = bw_gather_pipe_length, bytes = stream_size;
    assert(!bluewake_native_entries_try(&cpu, entry));
    assert(!memcmp(&cpu, &before, sizeof cpu) && !memcmp(ram, saved, sizeof ram));
    assert(length == bw_gather_pipe_length && bytes == stream_size);
    assert(!memcmp(batch, bw_gather_pipe_buffer, sizeof batch));
}
static void check_result(const CPUState* cpu, u32 entry) {
    assert(cpu->pc == 0x80001000u);
    if (entry == BLUEWAKE_BG_CHK_SAME_ACTOR_PID) assert(cpu->gpr[3] == 1);
    else if (entry == BLUEWAKE_BG_CHK_GRP_THROUGH) assert(cpu->gpr[3] == 0);
    else if (entry == BLUEWAKE_PSMTX_MULT_VEC_SR) {
        assert(word(0x80100200u) == 0x40000000u && word(0x80100204u) == 0x40400000u && word(0x80100208u) == 0x40800000u);
        assert(cpu->downcount == -21 && cpu->cycle_observation_suffix == 1);
    } else if (entry >= BLUEWAKE_MTXCALC_BASIC && entry <= BLUEWAKE_MTXCALC_MAYA) {
        const u32 result[12] = {0x3F800000u, 0, 0, 0x40000000u, 0, 0x3F800000u, 0, 0x40400000u, 0, 0, 0x3F800000u, 0x40800000u};
        for (unsigned i = 0; i < 12; ++i) {
            assert(word(0x803EDB80u + 4*i) == result[i]); assert(word(0x80101200u + 4*i) == result[i]);
        }
        assert(cpu->gpr[1] == initial_sp && cpu->lr == 0x80001003u && cpu->cycle_observation_suffix == 2);
    } else {
        bw_gather_pipe_flush();
        const unsigned words = entry == BLUEWAKE_J3D_FIFO_POS_MTX ? 12u : 9u;
        assert(stream_size == 5 + 4*words && stream[0] == 16 && stream[1] == 0 && stream[2] == words-1);
        assert(read_be16(stream + 3) == (entry == BLUEWAKE_J3D_FIFO_POS_MTX ? 12 : 1033));
        for (unsigned i = 0; i < words; ++i) {
            const unsigned offset = entry == BLUEWAKE_J3D_FIFO_NRM_MTX ? (i/3)*4 + i%3 : i;
            assert(read_be32(stream + 5 + 4*i) == 0x01020300u + offset);
        }
        assert(cpu->cycle_observation_suffix == 1);
    }
}
int main(void) {
    for (unsigned i = 0; i < sizeof entries / sizeof entries[0]; ++i) {
        const u32 entry = entries[i]; CPUState cpu = state(entry);
        assert(!bluewake_composite_native_entries_v1(false, ready, &allow)); decline(cpu, entry);
        assert(!bluewake_composite_native_entries_v1(true, NULL, &allow)); decline(cpu, entry);
        assert(bluewake_composite_native_entries_v1(true, ready, &allow));
        allow = false; decline(cpu, entry); allow = true;
        assert(bluewake_native_entries_try(&cpu, entry)); check_result(&cpu, entry);
        cpu = state(entry); cpu.ram = NULL; decline(cpu, entry);
        cpu = state(entry); cpu.ram_size = 1; decline(cpu, entry);
        cpu = state(entry); cpu.exception = 1; decline(cpu, entry);
        cpu = state(entry); cpu.cycle_budget = 0; decline(cpu, entry);
        cpu = state(entry); cpu.downcount = -4096; decline(cpu, entry);
        cpu = state(entry); cpu.downcount = LLONG_MIN; decline(cpu, entry);
        cpu = state(entry); cpu.downcount = LLONG_MAX; decline(cpu, entry);
        cpu = state(entry); cpu.cycle_deadline_budget = 1; decline(cpu, entry);
        cpu = state(entry); g_mem_write_journal = journal; decline(cpu, entry); g_mem_write_journal = NULL;
        cpu = state(entry); g_ppc_guest_aliases_overlap_mem1 = true; decline(cpu, entry); g_ppc_guest_aliases_overlap_mem1 = false;
        cpu = state(entry); decline(cpu, 0);
        assert(!bluewake_native_entries_try(NULL, entry));
        if (entry >= BLUEWAKE_MTXCALC_BASIC && entry <= BLUEWAKE_MTXCALC_MAYA) {
            const u32 addresses[] = {entry == BLUEWAKE_MTXCALC_MAYA ? 0x80328F38u : 0x80328F40u,
                entry == BLUEWAKE_MTXCALC_MAYA ? 0x80328F84u : 0x80328F8Cu, 0x8030D0FCu, 0x8030D0C8u};
            for (unsigned n = 0; n < 4; ++n) { cpu = state(entry); refused = addresses[n]; decline(cpu, entry); }
            cpu = state(entry); cpu.msr = 0; decline(cpu, entry);
            cpu = state(entry); cpu.gqr[0] = 7; decline(cpu, entry);
            cpu = state(entry); put(0x8010108Cu, 0x803EDB80u); decline(cpu, entry);
            /* Nonzero double products below the single normal range must
             * decline before a host FTZ setting can hide them as zero. */
            cpu = state(entry); put(cpu.gpr[5], 0x00800000u); put(0x803EDBB0u, 0x00800000u);
            cpu.fpscr |= 4u; ppc_fpscr_updated(&cpu); decline(cpu, entry);
        }
        if (entry == BLUEWAKE_BG_CHK_GRP_THROUGH) {
            cpu = state(entry); refused = 0x800A96F0u; decline(cpu, entry);
            cpu = state(entry); dirty = true; decline(cpu, entry);
            cpu = state(entry); bw_host_can_skip = NULL; decline(cpu, entry);
        }
        if (i < 3) {
            cpu = state(entry); bw_gather_pipe_bytes = NULL;
            assert(bluewake_native_entries_try(&cpu, entry)); check_result(&cpu, entry);
            cpu = state(entry); bw_gather_pipe_length = BW_GATHER_PIPE_BATCH - 4;
            memset(bw_gather_pipe_buffer, 0xA5, bw_gather_pipe_length);
            assert(bluewake_native_entries_try(&cpu, entry)); bw_gather_pipe_flush();
            assert(stream_size == BW_GATHER_PIPE_BATCH - 4 + (i == 0 ? 53u : 41u));
        }
    }
    puts("Second native batch: all nine entries, unchanged declines, observer probes and FIFO ordering pass");
    return 0;
}
