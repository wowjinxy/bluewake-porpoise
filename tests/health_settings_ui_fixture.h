// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "health_host.h"
// UI shell only; real mailbox/lifetime is independently tested with real core.
static BwHealthRulesConfig health_ui_rates={256,256};
static bool health_ui_room=false,health_ui_available=true,health_ui_healing_available=true;
static unsigned health_ui_calls;
extern "C" bool bw_health_host_configure(unsigned damage,unsigned healing) {
    ++health_ui_calls;
    if(damage>4096||healing>4096||(health_ui_room&&(damage!=256||healing!=256)))return false;
    health_ui_rates=BwHealthRulesConfig{static_cast<uint16_t>(damage),static_cast<uint16_t>(healing)};return true;
}
extern "C" bool bw_health_host_available(void) {return health_ui_available;}
extern "C" bool bw_health_host_healing_available(void) {return health_ui_available&&health_ui_healing_available;}
extern "C" bool bw_health_host_room_locked(void) {return health_ui_room;}
extern "C" BwHealthRulesConfig bw_health_host_configuration(void) {return health_ui_rates;}
void bw_settings_ui_health_policy(bool available,bool room) {
    health_ui_available=available;health_ui_room=room;
    if(room)health_ui_rates=BwHealthRulesConfig{256,256};
}
unsigned bw_settings_ui_health_calls(void) {return health_ui_calls;}
void bw_settings_ui_healing_policy(bool available) {health_ui_healing_available=available;}
