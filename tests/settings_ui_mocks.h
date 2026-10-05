// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include "audio_preview.h"
struct BwSettingsUiEffects {
    unsigned render_scale_calls = 0, anisotropy_calls = 0, smooth_calls = 0;
    unsigned fps_calls = 0, camera_calls = 0, controller_writes = 0;
    unsigned native_window_operations = 0, quit_requests = 0, state_requests = 0;
    unsigned texture_uploads = 0;
    unsigned quick_items_calls = 0;
    unsigned faster_wind_calls = 0, faster_boots_calls = 0;
    bool faster_wind_enabled = false, faster_boots_enabled = false;
    unsigned audio_calls = 0, audio_master = 100, audio_music = 100, audio_sfx = 100;
    bool audio_muted = false;
    unsigned dialogue_speed_calls = 0;
    unsigned autosave_calls = 0, autosave_interval = 300;
    bool autosave_enabled = false;
    float dialogue_speed = 1.0f;
    float render_scale = 0;
    unsigned anisotropy = 0;
    bool smooth = false, fps = false, blocked = false;
    bool quick_items_enabled = false;
};
void bw_settings_ui_mock_initialize();
void bw_settings_ui_mock_frame();
void bw_settings_ui_mock_preview_bind(BwAudioPreview* preview);
void bw_settings_ui_mock_preview_selection(const std::string& utf8);
unsigned bw_settings_ui_mock_preview_picker_calls();
BwSettingsUiEffects bw_settings_ui_mock_effects();

// Fixture-only UI shell policy; no native game/module is attached here.
void bw_settings_ui_health_policy(bool available,bool room);
unsigned bw_settings_ui_health_calls();
