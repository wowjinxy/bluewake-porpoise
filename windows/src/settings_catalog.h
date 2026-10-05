// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "settings_state.h"
#include "../../runtime/host/src/setting_definitions.h"
#include <string>
#include <vector>

// Pure adapters: no SDL, environment access, disk writes, or running-game state.
std::string bw_setting_value(const Settings& settings, const BwSettingDefinition& definition);
bool bw_setting_validate(const BwSettingDefinition& definition, const std::string& value,
                         std::string* canonical = nullptr, std::string* error = nullptr);
bool bw_setting_assign(Settings& settings, const std::string& id, const std::string& value,
                       std::string* error = nullptr);
bool bw_setting_enabled(const Settings& settings, const BwSettingDefinition& definition);
std::vector<const BwSettingDefinition*> bw_settings_search(const std::string& query);
