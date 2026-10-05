// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifndef BLUEWAKE_SETTINGS_UI_TEST
#error "Settings UI fixture helpers are unavailable in production builds"
#endif
#include "settings_state.h"

// The fixture creates an ImGui context, initializes mocked controls, and passes
// an isolated directory ending in a separator. No SDL video or native window.
void bw_settings_ui_test_reset(const char* data_dir, const Settings& saved);
void bw_settings_ui_test_draw();
const Settings& bw_settings_ui_test_session();
const Settings& bw_settings_ui_test_saved();
bool bw_settings_ui_test_menu_open();
bool bw_settings_ui_test_preset_open();
void bw_settings_ui_test_flush();
// Mocked picker seam: actual test clicks Choose WAV but opens no native dialog.
std::wstring bw_settings_ui_test_preview_file();
