/* Complete-state qualification of the libPorpoise SDK constructors.
 * Private translated bodies are supplied in an ignored build directory by
 * run_porpoise_math_oracle.py; this test contains no game instructions. */
#include "native_math.h"
#include "module_cpu_contract.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*Run)(CPUState*);
extern void porpoise_raw_0(CPUState*), porpoise_raw_1(CPUState*), porpoise_raw_2(CPUState*);
extern void porpoise_prepared_0(CPUState*), porpoise_prepared_1(CPUState*), porpoise_prepared_2(CPUState*);
static Run raw[] = {porpoise_raw_0, porpoise_raw_1, porpoise_raw_2};
static Run prepared[] = {porpoise_prepared_0, porpoise_prepared_1, porpoise_prepared_2};
static const u32 entries[] = {0x8030D09Cu, 0x8030D618u, 0x8030D698u};
static const unsigned cycles[] = {11, 13, 10};
enum { AREA_OFFSET = 0x100000, AREA_BYTES = 0x4000, PAGE_BYTES = 0x1000 };
static u32 seed = 0x706F7270u;
static unsigned ready_calls;
static bool ready_answer = true;
static volatile u32 sink;
static u32 next(void) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static u64 bits64(void) { return ((u64)next() << 32) | next(); }
static bool ready_callback(void* user, const CPUState* cpu, u32 entry) {
    (void)user; (void)cpu; (void)entry; ++ready_calls; return ready_answer;
}
static bool unexpected_host_call(CPUState* cpu, u32 address) {
    (void)cpu; (void)address; abort();
}
static void unexpected_journal(u32 offset, u32 bytes, void* user) {
    (void)offset; (void)bytes; (void)user; abort();
}
static u64 unexpected_read(CPUState* cpu, u32 address, u8 size) {
    (void)cpu; (void)address; (void)size; abort();
}
static void unexpected_write(CPUState* cpu, u32 address, u64 value, u8 size) {
    (void)cpu; (void)address; (void)value; (void)size; abort();
}
static int writable(u8* ram, u32 size, bool protect) {
    DWORD old;
    if (!VirtualProtect(ram, size, protect ? PAGE_READONLY : PAGE_READWRITE, &old)) return 0;
    if (!protect) return 1;
    return VirtualProtect(ram, PAGE_BYTES, PAGE_READWRITE, &old) &&
           VirtualProtect(ram + AREA_OFFSET, AREA_BYTES, PAGE_READWRITE, &old) &&
           VirtualProtect(ram + size - PAGE_BYTES, PAGE_BYTES, PAGE_READWRITE, &old);
}
static void copy_pages(u8* out, const u8* in, u32 size) {
    memcpy(out, in, PAGE_BYTES);
    memcpy(out + AREA_OFFSET, in + AREA_OFFSET, AREA_BYTES);
    memcpy(out + size - PAGE_BYTES, in + size - PAGE_BYTES, PAGE_BYTES);
}
static int equal_pages(const u8* a, const u8* b, u32 size) {
    return memcmp(a, b, PAGE_BYTES) == 0 &&
           memcmp(a + AREA_OFFSET, b + AREA_OFFSET, AREA_BYTES) == 0 &&
           memcmp(a + size - PAGE_BYTES, b + size - PAGE_BYTES, PAGE_BYTES) == 0;
}
static void random_pages(u8* ram, u32 size) {
    const u32 starts[] = {0, AREA_OFFSET, size - PAGE_BYTES};
    const u32 lengths[] = {PAGE_BYTES, AREA_BYTES, PAGE_BYTES};
    for (unsigned page = 0; page < 3; ++page)
        for (u32 i = 0; i < lengths[page]; i += 4) write_be32(ram + starts[page] + i, next());
}
typedef struct Case { CPUState cpu; bool journal, aliases, reject_ready, must_decline; } Case;
static Case build(u8* ram, u32 size, unsigned which, unsigned scenario) {
    Case k = {0}; CPUState* c = &k.cpu;
    random_pages(ram, size);
    c->ram = ram; c->ram_size = size;
    for (unsigned r = 0; r < 32; ++r) {
        c->gpr[r] = next(); c->fpr[r] = f64_value(bits64()); c->ps1[r] = f64_value(bits64());
    }
    c->pc = entries[which]; c->lr = 0xFFFFFFFCu | (next() & 3u);
    c->ctr = next(); c->cr = next(); c->xer = next(); c->fpscr = next() & ~3u;
    c->msr = next() | PPC_MSR_FP; c->hid2 = next() | PPC_HID2_LSQE;
    c->srr0 = next(); c->srr1 = next(); c->dar = next(); c->dsisr = next(); c->ear = next();
    c->timebase = bits64(); c->program_exception = next(); c->tlb_last_vps = next();
    c->tlb_last_index = next(); c->tlb_invalidate_count = next();
    c->external_addr = next(); c->external_value = next(); c->external_rid = (u8)next();
    c->external_read_count = (u8)next(); c->external_write_count = (u8)next();
    c->external_read = unexpected_read; c->external_write = unexpected_write;
    for (unsigned r = 0; r < 16; ++r) c->sr[r] = next();
    for (unsigned r = 1; r < 8; ++r) c->gqr[r] = next();
    for (unsigned r = 0; r < 512; ++r) {
        c->locked_cache_tag[r] = next(); c->locked_cache_valid[r] = (next() & 1u) != 0;
    }
    c->gpr[2] = GC_RAM_BASE + AREA_OFFSET + 0x100u + 12968u;
    c->gpr[3] = GC_RAM_BASE + AREA_OFFSET + 0x400u + 4u * (next() % 128u);
    c->cycle_budget = 16384; c->downcount = -(s64)(next() % 64u);
    c->cycle_deadline_budget = 1000; c->cycle_deadline_active = next();
    c->cycle_observation_suffix = next();
    write_be32(ram + AREA_OFFSET + 0x100u, 0x3F800000u);
    write_be32(ram + AREA_OFFSET + 0x104u, 0u);
    switch (scenario % 40u) {
    case 1: c->gpr[3]++; k.must_decline = true; break;
    case 2: c->gpr[3] = 0xCC008000u; k.must_decline = true; break;
    case 3: c->gpr[3] |= 0x40000000u; k.must_decline = true; break;
    case 4: c->gpr[3] = 0x90000000u; k.must_decline = true; break;
    case 5: c->gpr[3] = GC_RAM_BASE + size - 44u; k.must_decline = true; break;
    case 6: c->gpr[2]++; k.must_decline = true; break;
    case 7: c->gpr[2] = 0xCC0032A8u; k.must_decline = true; break;
    case 8: write_be32(ram + AREA_OFFSET + 0x104u, 0x80000000u); k.must_decline = true; break;
    case 9: write_be32(ram + AREA_OFFSET + 0x100u, next() & 0x7FFFFFFFu); k.must_decline = which != 2; break;
    case 10: c->exception = PPC_EXC_DSI; k.must_decline = true; break;
    case 11: c->msr &= ~PPC_MSR_FP; k.must_decline = true; break;
    case 12: c->hid2 &= ~PPC_HID2_LSQE; k.must_decline = true; break;
    case 13: c->gqr[0] = 4u; k.must_decline = true; break;
    case 14: c->fpscr |= 1u + next() % 3u; k.must_decline = true; break;
    case 15: c->cycle_budget = 0; k.must_decline = true; break;
    case 16: c->downcount = -c->cycle_budget; k.must_decline = true; break;
    case 17: c->cycle_deadline_budget = -c->downcount + cycles[which] - 1u; k.must_decline = true; break;
    case 18: k.journal = true; k.must_decline = true; break;
    case 19: k.aliases = true; k.must_decline = true; break;
    case 20: k.reject_ready = true; k.must_decline = true; break;
    case 21: c->cycle_deadline_budget = -c->downcount + cycles[which]; break;
    case 22: c->downcount = -c->cycle_budget + 1; c->cycle_deadline_budget = 0; break;
    case 23: c->cycle_deadline_budget = -3; break;
    case 24: c->gpr[3] = GC_RAM_BASE; break;
    case 25: c->gpr[3] = GC_RAM_BASE + size - 48u; break;
    case 26: c->gpr[3] = GC_RAM_BASE + AREA_OFFSET + 0xF0u + 4u * (next() % 8u); break;
    case 27: c->gpr[2] |= 0x40000000u; k.must_decline = true; break;
    case 28: c->host_call = unexpected_host_call; k.must_decline = true; break;
    case 29: c->fpr[1] = f64_value(0x7FF0000000000001ull); c->fpr[2] = f64_value(0xFFF8000012345678ull); c->fpr[3] = f64_value(0x8000000000000000ull); break;
    case 30: c->fpr[1] = f64_value(0x36A0000000000000ull); c->fpr[2] = f64_value(0x381ABCDE12345678ull); c->fpr[3] = f64_value(0x36B0000000000001ull); break;
    case 31: c->fpr[1] = 1.00000000000001; c->fpr[2] = -2.9999999999999; c->fpr[3] = 0.0; break;
    default: break;
    }
    c->reserve_valid = (next() & 3u) != 0;
    c->reserve_addr = c->gpr[3] + (next() % 5u) * 32u;
    if (scenario & 1u) c->reserve_addr |= 0x40000000u;
    return k;
}
static void report_cpu(const CPUState* got, const CPUState* want) {
    for (unsigned i = 0; i < sizeof *got; ++i)
        if (((const u8*)got)[i] != ((const u8*)want)[i])
            fprintf(stderr, "CPU byte %u: got %02X want %02X\n", i, ((const u8*)got)[i], ((const u8*)want)[i]);
}
static double now_ns(void) {
    LARGE_INTEGER f, n; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&n);
    return (double)n.QuadPart * 1e9 / (double)f.QuadPart;
}
static int compare(CPUState got, CPUState want, const u8* a, const u8* b, u32 size, const char* label, unsigned i) {
    want.ram = got.ram;
    if (memcmp(&got, &want, sizeof got) != 0 || !equal_pages(a, b, size)) {
        fprintf(stderr, "%s mismatch entry=%08X case=%u seed=%08X\n", label, got.pc, i, seed);
        report_cpu(&got, &want); return 0;
    }
    return 1;
}
int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: porpoise_math_test ORIGINAL.dll [CASES] [BENCH_CALLS]\n"); return 2; }
    const unsigned cases = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 60000u;
    const unsigned bench_calls = argc > 3 ? (unsigned)strtoul(argv[3], NULL, 10) : 1000000u;
    HMODULE lib = LoadLibraryA(argv[1]);
    if (!lib) { fprintf(stderr, "cannot load oracle: %lu\n", GetLastError()); return 1; }
#define SYM(name) ((void*)GetProcAddress(lib, name))
    StaticRecompGetModuleFn get = (StaticRecompGetModuleFn)SYM(STATICRECOMP_GET_MODULE_SYMBOL);
    if (!get) return 1;
    const StaticRecompModuleDesc* module = get();
    /* The supplied reference DLL may predate the fixed-storage ABI declaration.
     * This test borrows its storage only after checking the complete CPU ABI;
     * production loading continues to require module_cpu_contract.h unchanged. */
    if (!module || !module->dispatch || !module->on_state_loaded ||
        module->cpu_abi_version != GXRUNTIME_CPU_ABI_VERSION ||
        module->cpu_state_size != sizeof(CPUState) || strcmp(module->game_id, "GZLE01") != 0 ||
        (module->abi_version != STATICRECOMP_ABI_VERSION &&
         module->abi_version != BLUEWAKE_FIXED_CPU_ABI_VERSION &&
         module->abi_version != BLUEWAKE_FIXED_MEM1_ABI_VERSION)) {
        fprintf(stderr, "oracle CPU/module layout mismatch\n"); return 1;
    }
    BlueWakeModuleCPUFn guest_cpu = (BlueWakeModuleCPUFn)SYM("bluewake_composite_guest_cpu");
    BlueWakeModuleMEM1Fn guest_mem1 = (BlueWakeModuleMEM1Fn)SYM("bluewake_composite_guest_mem1");
    CPUState fallback;
    BlueWakeModuleStorage storage = {0};
    storage.cpu = guest_cpu ? guest_cpu() : &fallback;
    storage.mem1 = guest_mem1 ? guest_mem1(&storage.mem1_size) : NULL;
    if (!storage.cpu || (guest_mem1 && (!storage.mem1 || storage.mem1_size < GC_MAIN_RAM_SIZE))) return 1;
    printf("Oracle ABI: module=%u CPU=%u size=%u fixed_cpu=%u fixed_mem1=%u\n",
           module->abi_version, module->cpu_abi_version, module->cpu_state_size,
           guest_cpu != NULL, guest_mem1 != NULL);
    const u32 size = storage.mem1 ? storage.mem1_size : GC_MAIN_RAM_SIZE;
    u8* oracle = storage.mem1 ? storage.mem1 : VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    u8* native = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    u8* baseline = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    u8* snapshot = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!oracle || !native || !baseline || !snapshot) return 1;
    memset(oracle, 0, size);
    if (!writable(oracle, size, true) || !writable(native, size, true) || !writable(baseline, size, true)) return 1;
    int (*old_native)(bool, BluewakeNativeMathReady, void*) = (int (*)(bool, BluewakeNativeMathReady, void*))SYM("bluewake_composite_native_math_v1");
    if (old_native) old_native(false, NULL, NULL);
    bluewake_composite_native_math_v1(true, ready_callback, NULL);
    for (unsigned which = 0; which < 3; ++which) {
        unsigned accepted = 0, declined = 0;
        for (unsigned i = 0; i < cases; ++i) {
            Case k = build(native, size, which, i);
            /* Module on_state_loaded refreshes derived FPSCR flags. Begin both
             * executions with that same valid CPU state; never normalize a
             * completed result before the complete-state comparison. */
            ppc_fpscr_updated(&k.cpu);
            copy_pages(snapshot, native, size); copy_pages(oracle, native, size); copy_pages(baseline, native, size);
            CPUState result = k.cpu; const CPUState before = result;
            g_mem_write_journal = k.journal ? unexpected_journal : NULL;
            g_ppc_guest_aliases_overlap_mem1 = k.aliases;
            ready_answer = !k.reject_ready;
            const int handled = bluewake_native_math_try(&result, entries[which]);
            g_mem_write_journal = NULL; g_ppc_guest_aliases_overlap_mem1 = false; ready_answer = true;
            if (!handled) {
                ++declined;
                if (!compare(result, before, native, snapshot, size, "declined", i)) return 1;
                continue;
            }
            if (k.must_decline) { fprintf(stderr, "accepted forbidden guard scenario %u\n", i % 40u); return 1; }
            ++accepted;
            CPUState reference = k.cpu; reference.ram = oracle;
            *storage.cpu = reference; module->on_state_loaded(storage.cpu);
            if (!module->dispatch(storage.cpu, entries[which])) return 1;
            if (!compare(result, *storage.cpu, native, oracle, size, "oracle", i)) return 1;
            reference = k.cpu; reference.ram = baseline;
            ppc_fpscr_updated(&reference); prepared[which](&reference);
            if (!compare(result, reference, native, baseline, size, "matched prepared", i)) return 1;
            copy_pages(baseline, snapshot, size); reference = k.cpu; reference.ram = baseline;
            ppc_fpscr_updated(&reference); raw[which](&reference);
            if (!compare(result, reference, native, baseline, size, "matched raw", i)) return 1;
        }
        printf("%08X: %u cases, %u identical against oracle/raw/prepared, %u declined unchanged, 0 mismatches\n", entries[which], cases, accepted, declined);
        if (accepted == 0 || declined == 0 || (cases >= 60000u && accepted < 20000u)) return 1;
    }
    if (bench_calls) for (unsigned which = 0; which < 3; ++which) {
        Case fixture = build(native, size, which, 0);
        fixture.cpu.cycle_budget = (s64)bench_calls * 20 + 1000;
        fixture.cpu.downcount = 0; fixture.cpu.cycle_deadline_budget = 0;
        fixture.cpu.fpr[1] = 3.25; fixture.cpu.fpr[2] = -17.5; fixture.cpu.fpr[3] = 0.0;
        double timings[5][3];
        for (unsigned round = 0; round < 5; ++round) {
            for (unsigned pass = 0; pass < 3; ++pass) {
                const unsigned kind = (pass + round) % 3u;
                CPUState c = fixture.cpu; ppc_fpscr_updated(&c);
                const double start = now_ns();
                for (unsigned i = 0; i < bench_calls; ++i) {
                    c.pc = entries[which];
                    if (kind == 0) raw[which](&c);
                    else if (kind == 1) prepared[which](&c);
                    else if (!bluewake_native_math_try(&c, entries[which])) return 1;
                }
                timings[round][kind] = (now_ns() - start) / bench_calls;
                sink ^= read_be32(native + (c.gpr[3] - GC_RAM_BASE)) ^ (u32)c.downcount;
            }
        }
        for (unsigned round = 0; round < 5; ++round)
            printf("BENCH %08X round=%u raw=%.3f prepared=%.3f guarded=%.3f ns/call calls=%u\n",
                   entries[which], round, timings[round][0], timings[round][1], timings[round][2], bench_calls);
    }
    printf("Readiness queries: %u; checksum: %u\n", ready_calls, sink);
    writable(oracle, size, false);
    if (!storage.mem1) VirtualFree(oracle, 0, MEM_RELEASE);
    VirtualFree(native, 0, MEM_RELEASE); VirtualFree(baseline, 0, MEM_RELEASE); VirtualFree(snapshot, 0, MEM_RELEASE);
    FreeLibrary(lib); return 0;
}
