// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "hud_host.h"
struct BwHudSettingsUiEffects {
    unsigned queued_calls = 0;
    BwHudConfig queued{};
};
BwHudSettingsUiEffects bw_hud_settings_ui_effects();
void bw_hud_settings_ui_availability(BwHudAvailability);
