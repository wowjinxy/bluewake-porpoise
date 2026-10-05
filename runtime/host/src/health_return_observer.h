// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_HEALTH_RETURN_OBSERVER_H
#define BLUEWAKE_HEALTH_RETURN_OBSERVER_H
#include "core/cpu.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional module ABI, independent of StaticRecompModuleDesc. The module
 * calls this read-only predicate after a real healing blr, before the shared
 * execItemGet epilogue. False yields to the ordinary host edge service; the
 * callback must never dispatch, change guest state, or retain the supplied CPU.
 * A NULL callback preserves the native path without a host call. Registration
 * and removal belong to the game thread, before borrowed storage is replaced. */
typedef bool (*BwHealingReturnCanContinueFn)(void*, const CPUState*, u32);
typedef unsigned (*BwHealingReturnSetterFn)(u32 cpu_abi, u32 cpu_size,
                                          BwHealingReturnCanContinueFn, void*);
enum { BW_HEALING_RETURN_OBSERVATION_V1 = 1u };
#ifdef __cplusplus
}
#endif
#endif
