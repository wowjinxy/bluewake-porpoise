// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_HUD_RENDERER_H
#define BLUEWAKE_HUD_RENDERER_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BwHudRendererStats {uint64_t tagged,applied,suppressed,unsupported,malformed;} BwHudRendererStats;
/* Install once BEFORE Aurora/its FIFO worker starts. Lifetime is static.
 * Filter consumes immutable pending metadata; it never accesses CPU or UI. */
bool bw_hud_renderer_install(void);
void bw_hud_renderer_stats(BwHudRendererStats*);
bool bw_hud_renderer_filter(void* copied_plan,const void* pending_state,void*);
#ifdef __cplusplus
}
#endif
#endif
