#ifndef BLUEWAKE_NATIVE_GPU_STRIPE_H
#define BLUEWAKE_NATIVE_GPU_STRIPE_H
#include "core/cpu.h"
#include "gather_pipe_batch.h"

#define BLUEWAKE_GPU_STRIPE_FIRST 0x80263E7Cu
#define BLUEWAKE_GPU_STRIPE_SECOND 0x80264260u

typedef bool (*BluewakeGpuStripeReady)(void*, const CPUState*, u32);
int bluewake_composite_gpu_stripe_tails_v1(bool, BluewakeGpuStripeReady, BwGatherPipeBytes, void*);
/* Only from the certified prepaid copies, after the original basis stores.
 * Success preserves the existing prepaid debit and appends exactly 40 bytes;
 * rejection changes neither CPU, RAM nor the module's gather buffer. */
int bluewake_native_stripe_tail(CPUState*, u32);
void bluewake_gpu_stripe_tails_report(void);
#endif
