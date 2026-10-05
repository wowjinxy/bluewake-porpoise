// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_events.h"
#include <assert.h>
bool bluewake_game_events_scene(BwGameScene* scene,u64* epoch,u64* generation){(void)scene;(void)epoch;(void)generation;assert(!"Network menu fixture attempted guest scene lookup");return false;}
void bluewake_game_events_stats(BwGameEventStats* stats){(void)stats;assert(!"Network menu fixture attempted game stats");}
BwGameEventSubscription bluewake_game_events_subscribe(uint64_t mask,BwGameEventCallback callback,void* user){(void)mask;(void)callback;(void)user;assert(!"Network menu fixture attempted event subscription");return 0;}
bool bluewake_game_events_unsubscribe(BwGameEventSubscription subscription){(void)subscription;assert(!"Network menu fixture attempted unsubscribe");return false;}
