// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_HEALTH_HOST_H
#define BLUEWAKE_HEALTH_HOST_H
#include "health_rules.h"
#ifdef __cplusplus
extern "C" {
#endif
struct StaticRecompModuleDesc;
/* Copied atomic mailbox only. No UI thread may call the remaining lifecycle
 * APIs or access guest state. Rates are Q8, native=256, range0..4096. */
bool bw_health_host_configure(unsigned damage_q8,unsigned healing_q8);
BwHealthRulesConfig bw_health_host_configuration(void);
/* Before CARD/backend open, with no attached CPU: a mounted room accepts
 * only native rates. Once locked, configure cannot introduce nonnative rates.
 * This prototype makes no network challenge-compatibility claim. */
bool bw_health_host_prepare_room(bool room_mode);
bool bw_health_host_available(void); /* UI safe; audited module attached. */
bool bw_health_host_room_locked(void); /* UI safe, mounted native-only room. */
bool bw_health_host_attach(CPUState* cpu,const struct StaticRecompModuleDesc* module);
void bw_health_host_retrace(CPUState* cpu,bool saving);
bool bw_health_host_observes(const CPUState* cpu,uint32_t address);
void bw_health_host_dispatch(CPUState* cpu,uint32_t address,bool saving);
/* Reset cancels calls but retains the audited borrowed storage binding. Use
 * suspend BEFORE STATE/module/CPU/RAM replacement; attach after valid restore.
 * An ordinary VI/budget yield retains an armed call within its bounded native
 * return window; stale calls expire after four retraces. */
void bw_health_host_reset(void);
void bw_health_host_suspend(void);
void bw_health_host_detach(void);
/* Game-thread diagnostic only; no copied UI stats are required by this slice. */
void bw_health_host_stats(BwHealthRulesStats* out);
#ifdef __cplusplus
}
#endif
#endif
