/* Private GPU raw-pulling prototype: exact StripeCross FIFO emission tails.
 * All guest FP math remains; the renderer separately moves vertex decoding to
 * the GPU. No flush or callback may occur within an admitted tail. */
#include "native_stripe.h"
#include "native_inline_fp.h"
#include "gather_pipe.h"
#include <stdio.h>

#if defined(_WIN32)
#define BW_STRIPE_EXPORT __declspec(dllexport)
#else
#define BW_STRIPE_EXPORT __attribute__((visibility("default")))
#endif
static BluewakeGpuStripeReady s_ready;
static BwGatherPipeBytes s_write_bytes;
static void* s_user;
static unsigned long long s_runs[2], s_declines[2];

BW_STRIPE_EXPORT int bluewake_composite_gpu_stripe_tails_v1(
    bool enabled, BluewakeGpuStripeReady ready, BwGatherPipeBytes write_bytes, void* user) {
    s_ready = enabled && write_bytes != NULL ? ready : NULL;
    s_write_bytes = s_ready != NULL ? write_bytes : NULL;
    s_user = s_ready != NULL ? user : NULL;
    return s_ready != NULL;
}

BW_STRIPE_EXPORT void bluewake_gpu_stripe_tails_report(void) {
    fprintf(stderr, "[native-gpu-stripe] first=%llu/%llu second=%llu/%llu (native/declined)\n",
            s_runs[0], s_declines[0], s_runs[1], s_declines[1]);
}

static inline void stripe_lfs(CPUState* cpu, unsigned reg, u32 word) {
    const f64 value = f64_value(convert_to_double(word));
    cpu->fpr[reg] = value;
    cpu->ps1[reg] = value;
}
static inline void stripe_store(u8* bytes, unsigned slot, f64 value) {
    write_be32(bytes + 4u * slot, convert_to_single(f64_bits(value)));
}

static void first_tail(CPUState* cpu, u8* bytes, u32 u0, u32 u1) {
    ppc_fmuls(cpu, 9, 29, 0);
    ppc_frsp(cpu, 8, 5);
    ppc_fmuls(cpu, 5, 24, 8);
    ppc_fmuls(cpu, 7, 20, 7);
    ppc_fadds(cpu, 5, 5, 7);
    ppc_fadds(cpu, 5, 9, 5);
    ppc_fmuls(cpu, 11, 29, 1);
    ppc_frsp(cpu, 10, 4);
    ppc_fmuls(cpu, 4, 24, 10);
    ppc_fmuls(cpu, 9, 20, 12);
    ppc_fadds(cpu, 4, 4, 9);
    ppc_fadds(cpu, 4, 11, 4);
    ppc_fmuls(cpu, 12, 29, 2);
    ppc_frsp(cpu, 6, 6);
    ppc_fmuls(cpu, 11, 24, 6);
    ppc_fmuls(cpu, 3, 20, 3);
    ppc_fadds(cpu, 11, 11, 3);
    ppc_fadds(cpu, 11, 12, 11);
    ppc_fmuls(cpu, 12, 28, 0);
    ppc_fmuls(cpu, 0, 23, 8);
    ppc_fadds(cpu, 0, 0, 7);
    ppc_fadds(cpu, 0, 12, 0);
    ppc_fmuls(cpu, 7, 28, 1);
    ppc_fmuls(cpu, 1, 23, 10);
    ppc_fadds(cpu, 1, 1, 9);
    ppc_fadds(cpu, 7, 7, 1);
    ppc_fmuls(cpu, 2, 28, 2);
    ppc_fmuls(cpu, 1, 23, 6);
    ppc_fadds(cpu, 1, 1, 3);
    ppc_fadds(cpu, 2, 2, 1);
    ppc_fadds(cpu, 1, 11, 27);
    stripe_store(bytes, 0, cpu->fpr[1]);
    ppc_fadds(cpu, 1, 4, 26);
    stripe_store(bytes, 1, cpu->fpr[1]);
    ppc_fadds(cpu, 1, 5, 25);
    stripe_store(bytes, 2, cpu->fpr[1]);
    stripe_lfs(cpu, 1, u0);
    stripe_store(bytes, 3, cpu->fpr[1]);
    stripe_store(bytes, 4, cpu->fpr[21]);
    ppc_fadds(cpu, 1, 2, 27);
    stripe_store(bytes, 5, cpu->fpr[1]);
    ppc_fadds(cpu, 1, 7, 26);
    stripe_store(bytes, 6, cpu->fpr[1]);
    ppc_fadds(cpu, 0, 0, 25);
    stripe_store(bytes, 7, cpu->fpr[0]);
    stripe_lfs(cpu, 0, u1);
    stripe_store(bytes, 8, cpu->fpr[0]);
    stripe_store(bytes, 9, cpu->fpr[21]);
    cpu->pc = 0x80263F38u;
    cpu->cycle_observation_suffix = 5u;
}

static void second_tail(CPUState* cpu, u8* bytes, u32 u0, u32 u1) {
    ppc_fmuls(cpu, 10, 4, 0);
    ppc_frsp(cpu, 9, 7);
    ppc_fmuls(cpu, 7, 23, 9);
    ppc_fmuls(cpu, 8, 21, 8);
    ppc_fadds(cpu, 7, 7, 8);
    ppc_fadds(cpu, 7, 10, 7);
    ppc_fmuls(cpu, 27, 4, 1);
    ppc_frsp(cpu, 11, 6);
    ppc_fmuls(cpu, 6, 23, 11);
    ppc_fmuls(cpu, 10, 21, 28);
    ppc_fadds(cpu, 6, 6, 10);
    ppc_fadds(cpu, 6, 27, 6);
    ppc_fmuls(cpu, 27, 4, 2);
    ppc_frsp(cpu, 4, 26);
    ppc_fmuls(cpu, 23, 23, 4);
    ppc_fmuls(cpu, 3, 21, 3);
    ppc_fadds(cpu, 23, 23, 3);
    ppc_fadds(cpu, 26, 27, 23);
    ppc_fmuls(cpu, 23, 5, 0);
    ppc_fmuls(cpu, 0, 24, 9);
    ppc_fadds(cpu, 0, 0, 8);
    ppc_fadds(cpu, 9, 23, 0);
    ppc_fmuls(cpu, 1, 5, 1);
    ppc_fmuls(cpu, 0, 24, 11);
    ppc_fadds(cpu, 0, 0, 10);
    ppc_fadds(cpu, 8, 1, 0);
    ppc_fmuls(cpu, 1, 5, 2);
    ppc_fmuls(cpu, 0, 24, 4);
    ppc_fadds(cpu, 0, 0, 3);
    ppc_fadds(cpu, 1, 1, 0);
    ppc_fadds(cpu, 0, 26, 12);
    stripe_store(bytes, 0, cpu->fpr[0]);
    ppc_fadds(cpu, 0, 6, 13);
    stripe_store(bytes, 1, cpu->fpr[0]);
    ppc_fadds(cpu, 0, 7, 20);
    stripe_store(bytes, 2, cpu->fpr[0]);
    stripe_lfs(cpu, 0, u0);
    stripe_store(bytes, 3, cpu->fpr[0]);
    stripe_store(bytes, 4, cpu->fpr[25]);
    ppc_fadds(cpu, 0, 1, 12);
    stripe_store(bytes, 5, cpu->fpr[0]);
    ppc_fadds(cpu, 0, 8, 13);
    stripe_store(bytes, 6, cpu->fpr[0]);
    ppc_fadds(cpu, 0, 9, 20);
    stripe_store(bytes, 7, cpu->fpr[0]);
    stripe_lfs(cpu, 0, u1);
    stripe_store(bytes, 8, cpu->fpr[0]);
    stripe_store(bytes, 9, cpu->fpr[25]);
    cpu->pc = 0x8026431Cu;
    cpu->cycle_observation_suffix = 4u;
}

int bluewake_native_stripe_tail(CPUState* cpu, u32 address) {
    unsigned which;
    if (address == BLUEWAKE_GPU_STRIPE_FIRST) which = 0;
    else if (address == BLUEWAKE_GPU_STRIPE_SECOND) which = 1;
    else return 0;
    const u32 previous_pc = which == 0 ? 0x80263E78u : 0x8026425Cu;
    const u32 previous_suffix = which == 0 ? 53u : 52u;
    const unsigned fifo_reg = which == 0 ? 28u : 29u;
    if (cpu == NULL || s_ready == NULL || cpu->ram == NULL ||
        cpu->pc != previous_pc || cpu->cycle_observation_suffix != previous_suffix ||
        cpu->exception != 0 || (cpu->msr & PPC_MSR_FP) == 0 ||
        g_mem_write_journal != NULL || cpu->cycle_budget <= 0 || cpu->downcount > 0 ||
        (cpu->cycle_deadline_budget > 0 && cpu->cycle_deadline_budget < previous_suffix) ||
        bw_gather_pipe_write == NULL ||
        (bw_gather_pipe_bytes == NULL ? bw_gather_pipe_length != 0u :
         bw_gather_pipe_length >= BW_GATHER_PIPE_BATCH - 40u) ||
        cpu->gpr[fifo_reg] != 0xCC010000u ||
        !ppc_dispatch_poll_read_stable(cpu, cpu->gpr[2] - 15972u, 8u) ||
        ((cpu->gpr[2] - 15972u) & 3u) != 0u ||
        get_ram_ptr(cpu, 0xCC008000u, 4u, NULL) != NULL ||
        !s_ready(s_user, cpu, address)) {
        s_declines[which]++;
        return 0;
    }
    /* Stable plain RAM; no observer/callback/flush occurs between original
     * constant reads or stores. The module retains the exact FIFO bytes. */
    const u8* constants = cpu->ram + (cpu->gpr[2] - 15972u - GC_RAM_BASE);
    const u32 u0 = read_be32(constants), u1 = read_be32(constants + 4u);
    u8 direct_bytes[40];
    u8* bytes = bw_gather_pipe_bytes != NULL ? bw_gather_pipe_buffer + bw_gather_pipe_length : direct_bytes;
    if (which == 0) first_tail(cpu, bytes, u0, u1);
    else second_tail(cpu, bytes, u0, u1);
    if (bw_gather_pipe_bytes != NULL) bw_gather_pipe_length += 40u;
    else s_write_bytes(bytes, 40u);
    s_runs[which]++;
    return 1;
}
