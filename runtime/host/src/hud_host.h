// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_HUD_HOST_H
#define BLUEWAKE_HUD_HOST_H
#include "hud_customization.h"
typedef struct CPUState CPUState;
typedef struct StaticRecompModuleDesc StaticRecompModuleDesc;
#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t (*BwHudAliasGeneration)(void*);
typedef enum BwHudAvailability {
    BW_HUD_UNATTACHED, BW_HUD_AVAILABLE, BW_HUD_BAD_MODULE,
    BW_HUD_UNSUPPORTED_ASPECT, BW_HUD_UNSUPPORTED_RENDERER,
    BW_HUD_SUSPENDED, BW_HUD_WAITING_SCENE, BW_HUD_SUBSCRIBER_FULL
} BwHudAvailability;
typedef struct BwHudHostStatus {
    BwHudAvailability availability;
    bool requested, configured, bound;
    uint64_t requested_revision, consumed_revision;
    uint64_t epoch, generation, resolver_reads, rejected_reads;
    BwHudStats native;
} BwHudHostStatus;
/* UI-safe copied mailbox. false means busy or invalid; caller retains and
 * retries desired preferences. No CPU/RAM/events/FIFO/settings side effects. */
bool bw_hud_host_configure(const BwHudConfig*);
bool bw_hud_host_snapshot(BwHudHostStatus*);
const char* bw_hud_host_availability_name(BwHudAvailability);
/* Game-thread only. Renderer filter is installed before Aurora starts; true
 * capability requires actual GXCore. View is24MiB, actual allocation32MiB.
 * Alias-generation callback is the native GXR registry, not host bookkeeping. */
bool bw_hud_host_attach(CPUState*,const StaticRecompModuleDesc*,BwHudAliasGeneration,
                         void* alias_user,BwHudEmitBp,void* emit_user,
                         bool renderer_available,bool aspect_four_thirds);
void bw_hud_host_retrace(CPUState*,uint64_t frame,bool native_save_active);
/* Ordinary retrace/budget yield never ends a native draw. A new capture begins
 * only at the audited dMeter_Draw entry; scene/STATE/config invalidation ends
 * it immediately, before any old borrowed guest backing can be freed. */
bool bw_hud_host_observes(const CPUState*,uint32_t address);
void bw_hud_host_dispatch(CPUState*,uint32_t address);
/* Suspend BEFORE STATE/aliases/module/CPU replacement and borrowed RAM free.
 * A failed restore stays suspended. Resume verifies current module again. */
void bw_hud_host_suspend(void);
bool bw_hud_host_resume(CPUState*,const StaticRecompModuleDesc*);
void bw_hud_host_detach(void);
uint64_t bw_hud_module_fingerprint(const StaticRecompModuleDesc*);
#ifdef __cplusplus
}
#endif
#endif
