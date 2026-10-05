// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "settings_ui_mocks.h"
#include "autosave.h"
#include "enhancement_hooks.h"
#include "health_host.h"
#include "audio_customization.h"
#include "audio_preview_host_bridge.h"
#include "card_menu.h"
#include "asset_pack_menu.h"
#include "network_menu.h"
#include "controls_test_support.h"
#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <aurora/imgui.h>
#include <gxruntime/aurora_backend.h>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <string>
#ifdef rename
#undef rename
#endif
#include <filesystem>

namespace {
BwSettingsUiEffects effects;
Uint64 ticks;
SDL_Gamepad controller{101, 0, {}, "ui-fixture-controller"};
BwAudioPreview* preview_handle;
std::string preview_selection;
unsigned preview_picker_calls;
std::atomic<uint32_t> audio_configuration{100u | (100u << 7u) | (100u << 14u)};
}
void bw_settings_ui_mock_initialize() {
    controller.guid = SDL_StringToGUID("00112233445566778899aabbccddeeff");
    defaults(controller);
    pads = {&controller};
    for (unsigned i = 0; i < 12; ++i) keys[i] = {SDL_SCANCODE_J, pad_buttons[i]};
    for (unsigned i = 0; i < 10; ++i) key_axes[i] = {SDL_SCANCODE_D, static_cast<PADAxis>(i), 32767};
    effects = {};
    ticks = 0;
    preview_handle = nullptr; preview_selection.clear(); preview_picker_calls = 0;
    audio_configuration.store(100u | (100u << 7u) | (100u << 14u));
}
void bw_settings_ui_mock_preview_bind(BwAudioPreview* preview) { preview_handle = preview; }
void bw_settings_ui_mock_preview_selection(const std::string& utf8) { preview_selection = utf8; }
unsigned bw_settings_ui_mock_preview_picker_calls() { return preview_picker_calls; }
std::wstring bw_settings_ui_test_preview_file() {
    ++preview_picker_calls;
    return preview_selection.empty() ? std::wstring() : std::filesystem::u8path(preview_selection).wstring();
}
void bw_settings_ui_mock_frame() { ticks += 17; }
BwSettingsUiEffects bw_settings_ui_mock_effects() {
    auto result = effects;
    result.controller_writes = writes;
    return result;
}

extern "C" {
void bluewake_quick_items_configure(bool enabled) { ++effects.quick_items_calls; effects.quick_items_enabled = enabled; }
void bluewake_enhancement_faster_wind(bool enabled) { ++effects.faster_wind_calls; effects.faster_wind_enabled = enabled; }
void bluewake_enhancement_faster_boots(bool enabled) { ++effects.faster_boots_calls; effects.faster_boots_enabled = enabled; }
bool bluewake_audio_configure(unsigned master, unsigned music, unsigned sfx, bool muted) {
    if (master > 100 || music > 100 || sfx > 100) return false;
    ++effects.audio_calls; effects.audio_master = master; effects.audio_music = music;
    effects.audio_sfx = sfx; effects.audio_muted = muted;
    audio_configuration.store(master | (music << 7u) | (sfx << 14u) | (uint32_t(muted) << 21u)); return true;
}
void bluewake_audio_configuration(BwAudioConfiguration* config) {
    const uint32_t value = audio_configuration.load();
    *config = {value & 127u, (value >> 7u) & 127u, (value >> 14u) & 127u, (value & (1u << 21u)) != 0, 0};
}
void bluewake_host_audio_preview_bind(BwAudioPreview* preview) { preview_handle = preview; }
BwAudioPreview* bluewake_host_audio_preview_get() { return preview_handle; }
bool bluewake_dialogue_speed_configure(float multiplier) { effects.dialogue_speed=multiplier; ++effects.dialogue_speed_calls; return true; }
void bluewake_autosave_configure(bool enabled, unsigned seconds) {
    ++effects.autosave_calls; effects.autosave_enabled = enabled; effects.autosave_interval = seconds;
}
void bluewake_autosave_status(BluewakeAutosaveStatus* status) {
    *status = {}; status->desired = effects.autosave_enabled; status->interval_seconds = effects.autosave_interval;
    status->reason = effects.autosave_enabled ? BW_AUTOSAVE_NEEDS_NATIVE_QUEST : BW_AUTOSAVE_DISABLED;
}
const char* bluewake_autosave_reason_text(BluewakeAutosaveReason reason) {
    return reason == BW_AUTOSAVE_DISABLED ? "Disabled" : "Use the game's Save or load a native card quest first";
}
void PADBlockInput(bool blocked) { effects.blocked = blocked; }
Uint64 SDL_GetTicks() { return ticks; }
Uint64 SDL_GetPerformanceCounter() { return ticks * 1000; }
SDL_Window** SDL_GetWindows(int* count) { *count = 0; return nullptr; }
void SDL_free(void* memory) { std::free(memory); }
SDL_WindowFlags SDL_GetWindowFlags(SDL_Window*) { return 0; }
float SDL_GetWindowDisplayScale(SDL_Window*) { return 1; }
SDL_DisplayID SDL_GetDisplayForWindow(SDL_Window*) { return 0; }
SDL_DisplayID SDL_GetDisplayForPoint(const SDL_Point*) { return 0; }
const SDL_DisplayMode* SDL_GetCurrentDisplayMode(SDL_DisplayID) { return nullptr; }
bool SDL_GetWindowPosition(SDL_Window*, int*, int*) { return false; }
bool SDL_GetWindowSize(SDL_Window*, int*, int*) { return false; }
bool SDL_SetWindowFullscreen(SDL_Window*, bool) { ++effects.native_window_operations; assert(false); return false; }
bool SDL_SetWindowPosition(SDL_Window*, int, int) { ++effects.native_window_operations; assert(false); return false; }
bool SDL_SetWindowSize(SDL_Window*, int, int) { ++effects.native_window_operations; assert(false); return false; }
bool SDL_PushEvent(SDL_Event*) { ++effects.quit_requests; assert(false); return false; }
void aurora_set_frame_buffer_scale(float value) { ++effects.render_scale_calls; effects.render_scale = value; }
void aurora_set_forced_anisotropy(unsigned value) { ++effects.anisotropy_calls; effects.anisotropy = value; }
void aurora_set_frame_interpolation(bool value) { ++effects.smooth_calls; effects.smooth = value; }
bool aurora_get_frame_interpolation() { return effects.smooth; }
void aurora_set_frame_interp_steps(int) {}
void aurora_set_fps_overlay(bool value) { ++effects.fps_calls; effects.fps = value; }
void aurora_set_pause_on_focus_lost(bool) {}
const AuroraStats* aurora_get_stats() { static const AuroraStats stats{}; return &stats; }
ImTextureID aurora_imgui_add_texture(uint32_t, uint32_t, const void*) { ++effects.texture_uploads; assert(false); return {}; }
void dol_aurora_set_overlay(DolAuroraOverlayFn, void*) {}
void dol_aurora_set_hold(DolAuroraHoldFn, void*) {}
void dol_aurora_set_hold_redraw(bool) {}
void dol_aurora_frame_timing(DolAuroraFrameTiming* result) { *result = {}; }
void bluewake_mouse_camera_configure(bool, double, bool) { ++effects.camera_calls; }
void bluewake_mouse_camera_block(bool) {}
bool bluewake_mouse_camera_captured() { return false; }
void bluewake_mouse_camera_reload() {}
void bluewake_haptics_reload() {}
void bluewake_haptics_block(bool) {}
void bluewake_climb_reload() {}
bool bluewake_climb_hud(float*, bool*, float*, float*, float*, float*) { return false; }
void bluewake_save_state_hotkey(bool) { ++effects.state_requests; assert(false); }
bool bluewake_game_mod_available(const char*) { return true; }
const char* bluewake_game_options_describe(uint32_t index, const char** title, bool* default_on, bool* on) {
    static const char* names[] = {"sail_fast", "swift_sail", "swift_sail_no_wind", "instant_text"};
    if (index >= sizeof names / sizeof names[0]) return nullptr;
    if (title) *title = names[index];
    if (default_on) *default_on = true;
    if (on) *on = true;
    return names[index];
}
}
namespace aurora::gfx {
float calculate_fps() noexcept { return 60; }
float calculate_game_fps() noexcept { return 30; }
}
void bw_card_menu_draw(void) {}
bool bw_card_menu_restart_needed(void) { return false; }
bool bw_asset_pack_menu_start(const char*, bool) { return true; }
void bw_asset_pack_menu_draw(void) {}
bool bw_asset_pack_menu_restart_needed(void) { return false; }
const char* bw_asset_pack_menu_error(void) { return ""; }
void bw_network_menu_draw(void) { ImGui::TextUnformatted("Shared progress co-op (experimental)"); }
bool bw_network_menu_restart_needed(void) { return false; }

// Production ImGui's Aurora configuration replaces its file API. This fixture
// uses actual files in its private directory, without initializing SDL at all.
ImFileHandle ImFileOpen(const char* path, const char* mode) {
    return reinterpret_cast<ImFileHandle>(std::fopen(path, mode));
}
bool ImFileClose(ImFileHandle file) { return file == nullptr || std::fclose(reinterpret_cast<FILE*>(file)) == 0; }
ImU64 ImFileGetSize(ImFileHandle file) {
    FILE* stream = reinterpret_cast<FILE*>(file);
    if (!stream) return static_cast<ImU64>(-1);
    const auto at = std::ftell(stream);
    std::fseek(stream, 0, SEEK_END);
    const auto size = std::ftell(stream);
    std::fseek(stream, at, SEEK_SET);
    return size < 0 ? static_cast<ImU64>(-1) : static_cast<ImU64>(size);
}
ImU64 ImFileRead(void* data, ImU64 size, ImU64 count, ImFileHandle file) {
    return std::fread(data, static_cast<size_t>(size), static_cast<size_t>(count), reinterpret_cast<FILE*>(file));
}
ImU64 ImFileWrite(const void* data, ImU64 size, ImU64 count, ImFileHandle file) {
    return std::fwrite(data, static_cast<size_t>(size), static_cast<size_t>(count), reinterpret_cast<FILE*>(file));
}

#include "health_settings_ui_fixture.h"
