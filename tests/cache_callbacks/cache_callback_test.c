/* Authored CPU/FIFO oracle; no game, GPU, or physical device. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <fenv.h>
#include <stdio.h>
#include <string.h>
#include "core/cpu.h"

/* Instrument only the maintained trampoline's entry. Its actual source is
 * separately compiled under this renamed symbol, with all six real CPU TUs. */
void bw_reference_runtime_fallback(CPUState*, u32, u32);
static unsigned trampoline_calls;
void ppc_fallback_instruction(CPUState* cpu, u32 raw, u32 cia) {
    ++trampoline_calls;
    bw_reference_runtime_fallback(cpu, raw, cia);
}
#include "cache_fallback.h"

typedef struct Observation {
    u32 events[16], event_count, raw, cia, pc, suffix, msr, fpscr;
    s64 downcount, deadline;
    unsigned callbacks, cache_callbacks, flushes, bytes;
    u8 delivered[256];
} Observation;
static Observation observed;
static CPUState* active_cpu;
static unsigned callback_mode, flush_mode;
static u8 ram[4096], initial_ram[4096], reference_ram[4096];
static u32 random_state = 0x29CA6E5u;
static unsigned long long comparisons, original_trampolines, inline_trampolines;
static u32 next_random(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    return random_state ^= random_state << 5;
}
static void event(u32 value) {
    assert(observed.event_count < 16);
    observed.events[observed.event_count++] = value;
}
static void cache_callback(CPUState* cpu, u8 op, u32 ea, u32 cia) {
    (void)cpu; (void)op; (void)ea; (void)cia;
    ++observed.cache_callbacks; /* Translation's fallback never invokes this. */
}
static void callback_b(CPUState* cpu, u32 raw, u32 cia);
static void callback_a(CPUState* cpu, u32 raw, u32 cia) {
    event(2);
    observed.callbacks++;
    observed.raw = raw; observed.cia = cia; observed.pc = cpu->pc;
    observed.suffix = cpu->cycle_observation_suffix;
    observed.downcount = cpu->downcount; observed.deadline = cpu->cycle_deadline_budget;
    observed.msr = cpu->msr; observed.fpscr = cpu->fpscr;
    assert(observed.flushes == (flush_mode != 0));
    if (callback_mode == 1) { cpu->pc = cia + 4u; return; }
    cpu->gpr[(raw >> 16) & 31u] ^= raw;
    cpu->lr += 7; cpu->cr ^= 0x12345678; cpu->xer += 13;
    cpu->cycle_observation_suffix += 19; cpu->downcount -= 23;
    cpu->cycle_deadline_budget -= 31;
    cpu->ram[(raw ^ cia) & 4095] ^= (u8)raw;
    cpu->pc = callback_mode == 3 ? cia : cia + 12u;
    if (callback_mode == 3) cpu->exception |= PPC_EXC_DSI;
    if (callback_mode == 4) cpu->instruction_fallback = callback_b;
}
static void callback_b(CPUState* cpu, u32 raw, u32 cia) {
    event(3); observed.callbacks++;
    observed.raw = raw; observed.cia = cia; observed.pc = cpu->pc;
    observed.suffix = cpu->cycle_observation_suffix;
    observed.downcount = cpu->downcount; observed.deadline = cpu->cycle_deadline_budget;
    observed.msr = cpu->msr; observed.fpscr = cpu->fpscr;
    cpu->pc = cia ^ 0x20u; cpu->fpscr ^= raw;
    cpu->ram[(cia >> 2) & 4095] ^= 0xA5;
}
static void capture(const u8* data, u32 size) {
    event(1); observed.flushes++;
    assert(observed.bytes + size <= sizeof observed.delivered);
    memcpy(observed.delivered + observed.bytes, data, size); observed.bytes += size;
    if (flush_mode == 2) active_cpu->instruction_fallback = callback_b;
    if (flush_mode == 3) active_cpu->instruction_fallback = NULL;
    if (flush_mode == 4) active_cpu->instruction_fallback = callback_a;
    if (flush_mode == 5) {
        /* Real drain resets length before delivering. New bytes must remain
         * pending, including when the callback is NULL (no second drain). */
        bw_gather_pipe_buffer[0] = 0xBA; bw_gather_pipe_length = 1;
    }
}
static void invoke(CPUState* cpu, u32 raw, u32 cia, bool optimized, bool denied) {
    /* Authored instruction-entry stand-in: if a watch/budget refuses it, no
     * cache helper, FIFO drain, or callback can run. This code is identical. */
    event(0);
    if (denied) return;
    cpu->pc = cia; cpu->downcount -= 2;
    cpu->cycle_observation_suffix = 7;
    if (optimized) bw_cache_fallback_instruction(cpu, raw, cia);
    else bw_fallback_instruction(cpu, raw, cia);
    return; /* Generated fallback's control-flow boundary remains intact. */
}
int main(void) {
    const unsigned xo[] = {54, 86, 470, 982};
    const int rounds[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    bw_gather_pipe_bytes = capture;
    for (unsigned round = 0; round < 4; ++round) {
        assert(fesetround(rounds[round]) == 0);
        for (unsigned i = 0; i < 8000; ++i) {
            CPUState initial = {0};
            for (unsigned r = 0; r < 32; ++r) {
                initial.gpr[r] = next_random();
                u64 bits = ((u64)next_random() << 32) | next_random();
                memcpy(&initial.fpr[r], &bits, 8);
                bits ^= UINT64_C(0x7FF8000000000001); memcpy(&initial.ps1[r], &bits, 8);
            }
            initial.pc = next_random(); initial.lr = next_random(); initial.cr = next_random();
            initial.xer = next_random(); initial.msr = next_random(); initial.fpscr = next_random();
            initial.downcount = -(s64)(next_random() & 1023);
            initial.cycle_budget = (s64)(next_random() & 1023);
            initial.cycle_observation_suffix = next_random();
            initial.cycle_deadline_budget = (s64)(next_random() & 1023);
            initial.ram = ram; initial.ram_size = sizeof ram; initial.cache_control = cache_callback;
            for (unsigned b = 0; b < sizeof ram; ++b) initial_ram[b] = (u8)(b ^ i);
            callback_mode = i % 5; flush_mode = (i / 5) % 6;
            initial.instruction_fallback = callback_mode ? callback_a : NULL;
            const bool denied = i % 13 == 0;
            const u32 raw = (31u << 26) | (next_random() & 0x03FFF800u) | (xo[i % 4] << 1);
            const u32 cia = (i % 17 == 0) ? 0xFFFFFFFCu : 0x80001000u + 4u * i;
            CPUState saved; Observation saved_observed; u8 pending[264]; u32 pending_length;
            int original_excepts = 0;
            for (unsigned optimized = 0; optimized < 2; ++optimized) {
                CPUState cpu = initial; active_cpu = &cpu;
                memcpy(ram, initial_ram, sizeof ram); memset(&observed, 0, sizeof observed);
                memset(bw_gather_pipe_buffer, 0xD6, sizeof bw_gather_pipe_buffer);
                bw_gather_pipe_length = flush_mode ? 37 : 0; trampoline_calls = 0;
                assert(feclearexcept(FE_ALL_EXCEPT) == 0);
                assert(feraiseexcept(FE_INEXACT | FE_DIVBYZERO) == 0);
                const int excepts = fetestexcept(FE_ALL_EXCEPT);
                invoke(&cpu, raw, cia, optimized != 0, denied);
                assert(fegetround() == rounds[round]);
                assert(fetestexcept(FE_ALL_EXCEPT) == excepts);
                assert(observed.cache_callbacks == 0);
                if (!optimized) {
                    saved = cpu; saved_observed = observed; memcpy(reference_ram, ram, sizeof ram);
                    memcpy(pending, bw_gather_pipe_buffer, sizeof pending); pending_length = bw_gather_pipe_length;
                    original_excepts = excepts; original_trampolines += trampoline_calls;
                } else {
                    inline_trampolines += trampoline_calls;
                    assert(memcmp(&saved, &cpu, sizeof cpu) == 0);
                    assert(memcmp(&saved_observed, &observed, sizeof observed) == 0);
                    assert(memcmp(reference_ram, ram, sizeof ram) == 0);
                    assert(memcmp(pending, bw_gather_pipe_buffer, sizeof pending) == 0);
                    assert(pending_length == bw_gather_pipe_length && original_excepts == excepts);
                    comparisons++;
                }
            }
        }
    }
    assert(inline_trampolines == 0 && original_trampolines > 29000);
    printf("item29 cpu_ram_fifo_fp_equal=%llu reference_trampolines=%llu inline_trampolines=%llu callback_elision=0\n",
           comparisons, original_trampolines, inline_trampolines);
    return 0;
}
