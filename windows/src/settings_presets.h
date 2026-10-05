// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "settings_catalog.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Versioned, portable text files contain setting values, never paths, secrets,
// window placement, or the session-only CLI defaults flag. Unknown fields are
// retained for round trips but are never applied to the game.
struct BwSettingsPreset {
    std::string name;
    uint32_t sections = 0;
    std::map<std::string, std::string> values;
    std::map<std::string, std::string> unknown_fields;
    std::vector<std::string> unknown_sections;
};
struct BwPresetChange {
    const BwSettingDefinition* definition = nullptr;
    std::string before, after;
};
struct BwPresetPreview {
    Settings candidate;
    std::vector<BwPresetChange> changes;
    std::vector<std::string> warnings;
};

std::vector<BwSettingsPreset> bw_settings_builtin_presets();
BwSettingsPreset bw_settings_capture_preset(const std::string& name, const Settings& settings, uint32_t sections);
bool bw_settings_preset_parse(const std::string& text, BwSettingsPreset& preset, std::string* error = nullptr);
std::string bw_settings_preset_serialize(const BwSettingsPreset& preset);
bool bw_settings_preset_read(const std::string& path, BwSettingsPreset& preset, std::string* error = nullptr);
bool bw_settings_preset_write(const std::string& path, const BwSettingsPreset& preset, std::string* error = nullptr);
bool bw_settings_preset_preview(const BwSettingsPreset& preset, const Settings& current, uint32_t selected_sections,
                                BwPresetPreview& preview, std::string* error = nullptr);
bool bw_settings_preset_apply(const BwSettingsPreset& preset, Settings& session, uint32_t selected_sections,
                              std::string* error = nullptr);
