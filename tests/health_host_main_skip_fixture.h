// SPDX-License-Identifier: GPL-3.0-or-later
/* Pure stand-ins for unrelated observer policies. Actual projected main skip
 * predicate is source-extracted: its supplied-CPU health call is NOT copied. */
#define BLUEWAKE_ENABLE_DEVELOPER_TRACING 0
#define BLUEWAKE_EDGE_CENSUS 0
#define BW_SEARCH_JUDGE_FILTER 0x80001234u
static bool g_deadline_census_enabled,g_delivery_safety_census_enabled;
static bool g_guest_state_trace_enabled,bluewake_jump_button_armed,g_actor_search_native;
static bool health_other_observer;
static bool bluewake_feature_observes(u32 a){(void)a;return health_other_observer;}
static bool bluewake_game_events_observes(u32 a){(void)a;return false;}
static bool bw_hud_host_observes(const CPUState* c,u32 a){(void)c;(void)a;return false;}
static bool bluewake_quick_items_observes(u32 a){(void)a;return false;}
static bool bluewake_dialogue_speed_observes(u32 a){(void)a;return false;}
static bool bluewake_enhancement_hooks_observes_context(const CPUState* c,u32 a){(void)c;(void)a;return false;}
static bool bluewake_autosave_observes(u32 a){(void)a;return false;}
static bool host_chassis_requires_full(const CPUState* c,u32 a){(void)c;(void)a;return false;}
#include "health_main_skip_under_test.inc"
