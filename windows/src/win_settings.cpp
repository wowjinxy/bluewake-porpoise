// BlueWake for Windows: the settings, the in-game settings menu (F1 or Esc),
// the desktop hotkeys (F11 and Alt+Enter fullscreen, F10 Smooth Motion, F9 the
// frame rate) and the window's placement.
//
// The settings live in %APPDATA%\BlueWake\settings.ini. At launch they become
// the host's own variables (BLUEWAKE_*, DOL_*) unless the command line already
// set one, so the command line wins for that session. In the menu, display and
// control settings apply at once; the mods, Better Wind Waker's options, the
// picture's shape and the audio mode apply when BlueWake restarts (Restart now
// starts it again with the environment it began with).
//
// The menu is ImGui, drawn through the host overlay hook inside Aurora's frame
// (gxruntime/aurora_backend.h). The game keeps running under it; while it is
// open the game's pad is neutral and the mouse camera stands aside. Closing
// waits for held inputs to be released before they can control Link again.
#include "win_settings.h"
#include "../../runtime/host/src/noninteractive.h"
#include "settings_state.h"
#include "smooth_rate.h"
#include "controls_bindings.h"
#include "sprint_input.h"
#include "controls_menu.h"
#include "quick_items.h"
#include "dialogue_speed.h"
#include "autosave.h"
#include "enhancement_hooks.h"
#include "audio_customization.h"
#include "audio_preview_host_bridge.h"
#include "settings_catalog.h"
#include "hud_host.h"
#include "health_host.h"
#include "fps_watch.h"
#include "settings_presets.h"
#include "card_menu.h"
#include "asset_pack_menu.h"
#include "network_menu.h"
#if defined(BLUEWAKE_SETTINGS_UI_TEST)
#include "settings_ui_test_api.h"
#endif
#include "restart_request.h"
#include "launch_marker.h"
#include "atomic_file.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>

#include <SDL3/SDL.h>
#include <aurora/aurora.h>
#include <aurora/imgui.h>
#include <dolphin/pad.h>
#include <imgui.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gxruntime/aurora_backend.h"
#include "save_state.h"
#include "desktop_theme.h"
#include <aurora/gfx.h>

extern "C" {
// runtime/host/src/mouse_camera.h and game_options.h, declared here with plain
// types: those headers bring in the guest CPU's, which this file has no use for.
void bluewake_mouse_camera_configure(bool enabled, double sensitivity, bool invert_y);
void bluewake_mouse_camera_block(bool blocked);
bool bluewake_mouse_camera_captured(void);
void bluewake_haptics_reload(void);
void bluewake_mouse_camera_reload(void);
void bluewake_climb_reload(void);
void bluewake_haptics_block(bool blocked);
const char* bluewake_game_options_describe(uint32_t position, const char** title, bool* default_on, bool* on);
bool bluewake_game_mod_available(const char* name);
// climb.h: the stamina wheel's state for the HUD.
bool bluewake_climb_hud(float* fraction, bool* exhausted, float* x, float* y, float* aspect, float* alpha);
}

// Aurora's frame counters (lib/gfx/common.hpp, linked in statically), for the
// session log: every present, and the game's own frames.
namespace aurora::gfx {
float calculate_fps() noexcept;
float calculate_game_fps() noexcept;
}  // namespace aurora::gfx

namespace {

Settings g_saved;     // as in the file, changed by the menu and hotkeys
Settings g_session, g_before_edit;
Settings g_launched;  // as this session started (what a restart would change)
std::string g_data_dir, g_path;
std::vector<wchar_t> g_environment;  // as BlueWake was started, for Restart

RestartRequest g_restart;
bool g_safe_mode;
bool g_menu_open;
bool g_toggle_menu, g_toggle_fullscreen;  // from the hotkeys, done in the frame
bool g_dirty;
bool g_preset_popup, g_cancel_preset;
bool g_preview_page_visible;
std::string g_preview_path, g_preview_message;
Uint64 g_dirty_at, g_first_frame_at;
Uint64 g_slowdown_notice_until;
bool g_placed;
ImFont* g_menu_font = nullptr;
ImFont* g_menu_large_font = nullptr;
ImFontAtlas* g_menu_atlas = nullptr;
#if defined(BLUEWAKE_SETTINGS_UI_TEST)
bool g_menu_test_default_font = false, g_menu_test_large_failure = false;
#endif
float g_font_scale = 1.0f;  // the scale the UI font was drawn at (see load_font)

// --- the file ---------------------------------------------------------------

void load_file() {
    FILE* f = std::fopen(g_path.c_str(), "r");
    if (f == nullptr)
        return;
    char line[512];
    while (std::fgets(line, sizeof line, f) != nullptr) {
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
            s.pop_back();
        const size_t eq = s.find('=');
        if (s.empty() || s[0] == '#' || eq == std::string::npos)
            continue;
        const std::string k = s.substr(0, eq), v = s.substr(eq + 1);
        if (k == "window") {
            int width = 0, height = 0;
            if (std::sscanf(v.c_str(), "%dx%d", &width, &height) == 2 && width >= 320 && height >= 240 &&
                width <= 32768 && height <= 32768) { g_saved.window_w = width; g_saved.window_h = height; }
            continue;
        }
        if (k == "window_position") {
            std::sscanf(v.c_str(), "%d,%d", &g_saved.window_x, &g_saved.window_y);
            continue;
        }
        const BwSettingDefinition* definition = bw_setting_find(k.c_str());
        if (definition && !(definition->flags & BW_SETTING_SESSION_ONLY)) {
            std::string value = v, error;
            if (definition->type == BW_SETTING_BOOL) {
                if (v == "true" || v == "on" || v == "yes") value = "1";
                else if (v == "false" || v == "off" || v == "no") value = "0";
            }
            if (!bw_setting_assign(g_saved, k, value, &error))
                std::fprintf(stderr, "[settings] ignored %s: %s\n", k.c_str(), error.c_str());
        } else if (!definition && k.rfind("option.", 0) == 0 && (v == "0" || v == "1")) {
            g_saved.options[k.substr(7)] = v == "1";
        }
    }
    std::fclose(f);
}

void save_file() {
    g_dirty_at = SDL_GetTicks(); // back off after failures rather than retry every frame
    char* pending = bw_atomic_path(g_path.c_str());
    FILE* f = pending != nullptr ? std::fopen(pending, "w") : nullptr;
    if (f == nullptr) { free(pending); return; }
    const Settings& d = g_saved;
    std::fprintf(f, "# BlueWake settings (the in-game menu, F1, writes this file)\n");
    std::fprintf(f, "fullscreen=%d\n", d.fullscreen);
    if (d.window_w > 0 && d.window_h > 0)
        std::fprintf(f, "window=%dx%d\n", d.window_w, d.window_h);
    if (d.window_x != INT_MIN && d.window_y != INT_MIN)
        std::fprintf(f, "window_position=%d,%d\n", d.window_x, d.window_y);
    size_t count = 0;
    const BwSettingDefinition* definitions = bw_setting_definitions(&count);
    for (size_t i = 0; i < count; ++i) {
        const auto& definition = definitions[i];
        if (definition.flags & (BW_SETTING_HIDDEN | BW_SETTING_SESSION_ONLY)) continue;
        if (std::strcmp(definition.id, "fullscreen") == 0) continue;
        if (definition.option_name && d.options.find(definition.option_name) == d.options.end()) continue;
        std::fprintf(f, "%s=%s\n", definition.id, bw_setting_value(d, definition).c_str());
    }
    for (const auto& [name, on] : d.options)
        if (!bw_setting_find(("option." + name).c_str())) std::fprintf(f, "option.%s=%d\n", name.c_str(), on);
    const bool ok = bw_atomic_finish_dirty(f, pending, g_path.c_str(), &g_dirty);
    free(pending);
    if (!ok) std::fprintf(stderr, "[settings] save failed; previous file kept, retry pending\n");
}

void changed() {
    bw_settings_keep_edits(g_saved, g_before_edit, g_session);
    g_before_edit = g_session;
    g_dirty = true;
    g_dirty_at = SDL_GetTicks();
}

// --- the launch -------------------------------------------------------------

bool env_set(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && v[0] != '\0';
}

void env_default(const char* name, const std::string& value) {
    if (!env_set(name))
        _putenv_s(name, value.c_str());
}

double aspect_ratio(const std::string& aspect) {
    return aspect == "16:9" ? 16.0 / 9.0 : aspect == "16:10" ? 1.6 : 4.0 / 3.0;
}

// A window a good size for this screen: the tallest multiple of 240 lines that
// fits in 80 percent of the primary screen's work area, at the picture's shape.
void default_window(double ratio, int* w, int* h) {
    MONITORINFO info{};
    info.cbSize = sizeof info;
    int work_w = 1280, work_h = 1024;
    if (GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &info)) {
        work_w = info.rcWork.right - info.rcWork.left;
        work_h = info.rcWork.bottom - info.rcWork.top;
    }
    int height = std::max(480, static_cast<int>(work_h * 0.8) / 240 * 240);
    int width = static_cast<int>(std::lround(height * ratio));
    if (width > work_w * 9 / 10) {
        width = work_w * 9 / 10;
        height = static_cast<int>(width / ratio);
    }
    *w = width;
    *h = height;
}

std::string texture_folder() { return g_data_dir + "Load\\Textures\\GZLE01"; }

// --- the window -------------------------------------------------------------

SDL_Window* game_window() {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Window* window = windows != nullptr && count > 0 ? windows[0] : nullptr;
    SDL_free(windows);
    return window;
}

bool is_fullscreen(SDL_Window* w) { return (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) != 0; }

void set_fullscreen(SDL_Window* w, bool on) {
    if (w == nullptr)
        return;
    SDL_SetWindowFullscreen(w, on);
    g_session.fullscreen = on;
    changed();
    std::fprintf(stderr, "[windows] fullscreen %s\n", on ? "on" : "off");
}

// Once the window exists: where it was last time, if that is still on a
// screen with its title bar showing, else centred. Aurora would put its client
// area at the screen's corner, the title bar above the top edge.
void place_window(SDL_Window* w) {
    if (is_fullscreen(w))
        return;
    const Settings& d = g_session;
    bool restored = false;
    if (d.window_x != INT_MIN && d.window_y != INT_MIN) {
        const SDL_Point title{d.window_x + 60, d.window_y - 16};
        const SDL_Point corner{d.window_x + 60, d.window_y + 60};
        if (SDL_GetDisplayForPoint(&title) != 0 && SDL_GetDisplayForPoint(&corner) != 0) {
            SDL_SetWindowPosition(w, d.window_x, d.window_y);
            restored = true;
        }
    }
    if (!restored)
        SDL_SetWindowPosition(w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

// Remember the window's place and size as the player leaves them.
void track_window(SDL_Window* w) {
    const SDL_WindowFlags flags = SDL_GetWindowFlags(w);
    if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED))
        return;
    int x = 0, y = 0, width = 0, height = 0;
    if (!SDL_GetWindowPosition(w, &x, &y) || !SDL_GetWindowSize(w, &width, &height))
        return;
    Settings& d = g_session;
    static bool first = true;
    if (first) {
        first = false;
        d.window_x = g_before_edit.window_x = x;
        d.window_y = g_before_edit.window_y = y;
        d.window_w = g_before_edit.window_w = width;
        d.window_h = g_before_edit.window_h = height;
        return;
    }
    if (x != d.window_x || y != d.window_y || width != d.window_w || height != d.window_h) {
        d.window_x = x;
        d.window_y = y;
        d.window_w = width;
        d.window_h = height;
        changed();
    }
}

void reset_window(SDL_Window* w) {
    if (w == nullptr)
        return;
    if (is_fullscreen(w))
        set_fullscreen(w, false);
    int width = 0, height = 0;
    default_window(aspect_ratio(g_launched.aspect), &width, &height);
    SDL_SetWindowSize(w, width, height);
    SDL_SetWindowPosition(w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

// --- settings that apply at once ------------------------------------------------

// Bindings own the PAD mapping. Legacy preferences overlay the selected
// profile rather than restoring defaults over the player's bindings.
void apply_controller() {
    const Settings& d = g_session;
    bluewake_controls_set_legacy_preferences(d.controller_swap_ab, d.controller_swap_xy,
                                             d.pad_invert_x, d.pad_invert_y);
}

// Adapted from Elliott Tate's display-rate selection; resolve the current
// session so a command-line override never changes a saved preference.
float display_refresh(SDL_Window* window) {
    const SDL_DisplayMode* mode = window != nullptr ? SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window)) : nullptr;
    return mode != nullptr ? mode->refresh_rate : 0.f;
}

void apply_smooth_rate(SDL_Window* window) {
    aurora_set_frame_interp_steps(bw_smooth_steps(g_session.smooth_steps, display_refresh(window)));
}

const char* haptics_name(int mode) { return mode == 0 ? "off" : mode == 1 ? "classic" : "enhanced"; }

void apply_haptics() {
    const Settings& d = g_session;
    _putenv_s("BLUEWAKE_HAPTICS", haptics_name(d.haptics));
    _putenv_s("BLUEWAKE_HAPTICS_STRENGTH", std::to_string(d.haptics_strength).c_str());
    _putenv_s("BLUEWAKE_HAPTICS_TRIGGERS", d.haptics_triggers ? "1" : "0");
    bluewake_haptics_reload();
}

// The fast right-stick camera reads the controller through SDL itself, so the
// controller's inversion (apply_controller, for the game's own C-stick) is its
// BLUEWAKE_STICK_CAMERA_INVERT_X and _Y too (as in Elliott Tate's Windows menu).
void apply_stick() {
    const Settings& d = g_session;
    _putenv_s("BLUEWAKE_STICK_CAMERA", d.stick_camera ? "1" : "0");
    _putenv_s("BLUEWAKE_STICK_CAMERA_SPEED", std::to_string(d.stick_speed).c_str());
    _putenv_s("BLUEWAKE_STICK_AIM_SPEED", std::to_string(d.stick_aim_speed).c_str());
    _putenv_s("BLUEWAKE_STICK_CAMERA_INVERT_X", d.pad_invert_x ? "1" : "0");
    _putenv_s("BLUEWAKE_STICK_CAMERA_INVERT_Y", d.pad_invert_y ? "1" : "0");
    bluewake_mouse_camera_reload();
}

void apply_climb() {
    const Settings& d = g_session;
    _putenv_s("BLUEWAKE_CLIMB", d.climb ? "1" : "0");
    _putenv_s("BLUEWAKE_CLIMB_STAMINA", std::to_string(d.climb_stamina).c_str());
    bluewake_climb_reload();
}

void apply_sprint_modes() {
    (void)bluewake_sprint_configure_modes(g_session.sprint_keyboard_mode, g_session.sprint_controller_mode);
}

void apply_quick_items() {
    const Settings& d = g_session;
    bluewake_quick_items_configure(d.quick_items);
}
void apply_dialogue_speed() {
    bluewake_dialogue_speed_configure(static_cast<float>(g_session.dialogue_speed));
}
void apply_autosave() {
    bluewake_autosave_configure(g_session.autosave, static_cast<unsigned>(g_session.autosave_interval));
}
void apply_health() {
    if(!bw_health_host_configure(static_cast<unsigned>(g_session.damage_rate_q8),
                                  static_cast<unsigned>(g_session.healing_rate_q8))) {
        const auto native=bw_health_host_configuration();
        g_session.damage_rate_q8=native.damage_q8;g_session.healing_rate_q8=native.healing_q8;
    }
}
void apply_equipment() {
    bluewake_enhancement_faster_wind(g_session.faster_wind);
    bluewake_enhancement_faster_boots(g_session.faster_boots);
}
bool g_hud_pending=false;
void apply_hud() {
    BwHudConfig desired=g_session.hud;
    if(!g_session.hud_enabled)bw_hud_config_identity(&desired);
    g_hud_pending=!bw_hud_host_configure(&desired);
}
void apply_audio() {
    (void)bluewake_audio_configure(static_cast<unsigned>(g_session.audio_master),
                                 static_cast<unsigned>(g_session.audio_music),
                                 static_cast<unsigned>(g_session.audio_sfx), g_session.audio_muted);
}

void apply_live() {
    const Settings& d = g_session;
    aurora_set_frame_buffer_scale(static_cast<float>(d.render_scale));
    aurora_set_forced_anisotropy(static_cast<unsigned>(d.anisotropy));
    apply_smooth_rate(game_window());
    aurora_set_frame_interpolation(d.smooth_motion);
    aurora_set_fps_overlay(d.show_fps);
    aurora_set_pause_on_focus_lost(d.pause_unfocused);
    _putenv_s("BLUEWAKE_MOUSE_CAMERA", d.mouse_camera ? "1" : "0");
    _putenv_s("BLUEWAKE_MOUSE_SENSITIVITY", std::to_string(d.mouse_sensitivity).c_str());
    _putenv_s("BLUEWAKE_MOUSE_INVERT_Y", d.mouse_invert_y ? "1" : "0");
    bluewake_mouse_camera_configure(d.mouse_camera, d.mouse_sensitivity, d.mouse_invert_y);
    apply_controller();
    apply_stick();
    apply_climb();
    apply_sprint_modes();
    apply_quick_items();
    apply_dialogue_speed();
    apply_autosave();
    apply_equipment();
    apply_health();
    apply_audio();
    apply_hud();
}

void set_menu_open(bool open) {
    if (open == g_menu_open)
        return;
    g_menu_open = open;
    bluewake_controls_menu_set_open(open);
    bluewake_mouse_camera_block(open);
    bluewake_haptics_block(open);
    if (!open) {
        bluewake_audio_preview_cancel(bluewake_host_audio_preview_get());
        g_preview_page_visible = false;
    }
    std::fprintf(stderr, "[windows] settings menu %s\n", open ? "open" : "closed");
    if (!open && g_dirty)
        save_file();
}

void open_folder(const std::string& path) {
    CreateDirectoryA(path.c_str(), nullptr);
    wchar_t wide[MAX_PATH * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide, MAX_PATH * 2) > 0)
        ShellExecuteW(nullptr, L"open", wide, nullptr, nullptr, SW_SHOWNORMAL);
}

void restart() {
    if (g_dirty) save_file();
    bool ok = g_restart.request(!g_dirty, [] {
        SDL_Event quit{};
        quit.type = SDL_EVENT_QUIT;
        return SDL_PushEvent(&quit);
    });
    if (!ok) std::fprintf(stderr, "[windows] restart refused: settings save or quit request failed\n");
}

// Start BlueWake again, with the command line and environment it began with,
// so the settings file decides what the new session is.
bool relaunch() {
    wchar_t exe[MAX_PATH * 2];
    if (GetModuleFileNameW(nullptr, exe, MAX_PATH * 2) == 0)
        return false;
    std::vector<wchar_t> command(GetCommandLineW(), GetCommandLineW() + wcslen(GetCommandLineW()) + 1);
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe, command.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT,
                        g_environment.empty() ? nullptr : g_environment.data(), nullptr, &startup, &process)) {
        std::fprintf(stderr, "[windows] restart failed (error %lu)\n", GetLastError());
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    std::fprintf(stderr, "[windows] restarting with the new settings\n");
    return true;
}

// --- the menu ---------------------------------------------------------------

bool needs_restart() {
    if (bw_card_menu_restart_needed() || bw_asset_pack_menu_restart_needed() || bw_network_menu_restart_needed()) return true;
    size_t count = 0;
    const BwSettingDefinition* definitions = bw_setting_definitions(&count);
    for (size_t i = 0; i < count; ++i)
        if (definitions[i].apply == BW_SETTING_RESTART && !(definitions[i].flags & BW_SETTING_SESSION_ONLY) &&
            bw_setting_value(g_session, definitions[i]) != bw_setting_value(g_launched, definitions[i])) return true;
    return false;
}

void restart_note(bool differs) {
    if (differs) {
        ImGui::SameLine();
        ImGui::TextDisabled("*");
    }
}

bool compiled_option(const char* wanted) {
    if (wanted == nullptr) return true;
    for (uint32_t i = 0;; ++i) {
        const char* name = bluewake_game_options_describe(i, nullptr, nullptr, nullptr);
        if (name == nullptr) return false;
        if (std::strcmp(name, wanted) == 0) return true;
    }
}

void apply_menu_settings() {
    SDL_Window* w = game_window();
    const Settings& d = g_session;
    const Settings& before = g_before_edit;
    if (w != nullptr && is_fullscreen(w) != d.fullscreen) SDL_SetWindowFullscreen(w, d.fullscreen);
    if (d.render_scale != before.render_scale) aurora_set_frame_buffer_scale(static_cast<float>(d.render_scale));
    if (d.anisotropy != before.anisotropy) aurora_set_forced_anisotropy(static_cast<unsigned>(d.anisotropy));
    if (d.smooth_steps != before.smooth_steps) apply_smooth_rate(w);
    if (d.smooth_motion != before.smooth_motion) aurora_set_frame_interpolation(d.smooth_motion);
    if (d.show_fps != before.show_fps) aurora_set_fps_overlay(d.show_fps);
    if (d.pause_unfocused != before.pause_unfocused) aurora_set_pause_on_focus_lost(d.pause_unfocused);
    if (d.mouse_camera != before.mouse_camera || d.mouse_sensitivity != before.mouse_sensitivity ||
        d.mouse_invert_y != before.mouse_invert_y) {
        _putenv_s("BLUEWAKE_MOUSE_CAMERA", d.mouse_camera ? "1" : "0");
        _putenv_s("BLUEWAKE_MOUSE_SENSITIVITY", std::to_string(d.mouse_sensitivity).c_str());
        _putenv_s("BLUEWAKE_MOUSE_INVERT_Y", d.mouse_invert_y ? "1" : "0");
        bluewake_mouse_camera_configure(d.mouse_camera, d.mouse_sensitivity, d.mouse_invert_y);
    }
    if (d.controller_swap_ab != before.controller_swap_ab || d.controller_swap_xy != before.controller_swap_xy ||
        d.pad_invert_x != before.pad_invert_x || d.pad_invert_y != before.pad_invert_y) apply_controller();
    if (d.stick_camera != before.stick_camera || d.stick_speed != before.stick_speed ||
        d.stick_aim_speed != before.stick_aim_speed || d.pad_invert_x != before.pad_invert_x ||
        d.pad_invert_y != before.pad_invert_y) apply_stick();
    if (d.climb != before.climb || d.climb_stamina != before.climb_stamina) apply_climb();
    if (d.sprint_keyboard_mode != before.sprint_keyboard_mode ||
        d.sprint_controller_mode != before.sprint_controller_mode)
        apply_sprint_modes();
    if (d.quick_items != before.quick_items) apply_quick_items();
    if (d.dialogue_speed != before.dialogue_speed) apply_dialogue_speed();
    if (d.autosave != before.autosave || d.autosave_interval != before.autosave_interval) apply_autosave();
    if (d.faster_wind != before.faster_wind) bluewake_enhancement_faster_wind(d.faster_wind);
    if (d.faster_boots != before.faster_boots) bluewake_enhancement_faster_boots(d.faster_boots);
    if (d.damage_rate_q8 != before.damage_rate_q8 || d.healing_rate_q8 != before.healing_rate_q8) apply_health();
    if (d.audio_master != before.audio_master || d.audio_music != before.audio_music ||
        d.audio_sfx != before.audio_sfx || d.audio_muted != before.audio_muted) apply_audio();
    if (d.haptics != before.haptics || d.haptics_strength != before.haptics_strength ||
        d.haptics_triggers != before.haptics_triggers) apply_haptics();
    apply_hud();
    changed();
}

bool compact_menu_rows() {
    return g_session.menu_size != 100 &&
        ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 38.0f;
}

void setting_widget(const BwSettingDefinition& definition) {
    const std::string before = bw_setting_value(g_session, definition);
    std::string value = before;
    static BwHudHostStatus hud_status{};
    (void)bw_hud_host_snapshot(&hud_status);
    const bool hud_option=std::strncmp(definition.id,"hud.",4)==0;
    const bool hud_available=hud_status.availability==BW_HUD_AVAILABLE ||
        hud_status.availability==BW_HUD_WAITING_SCENE;
    const bool health_option=std::strcmp(definition.id,"damage_rate_q8")==0 || std::strcmp(definition.id,"healing_rate_q8")==0;
    const bool healing_option=std::strcmp(definition.id,"healing_rate_q8")==0;
    const bool health_available=healing_option?bw_health_host_healing_available():bw_health_host_available();
    const bool available = (!health_option||(health_available&&!bw_health_host_room_locked())) && (!hud_option||hud_available) && compiled_option(definition.option_name) &&
        (std::strcmp(definition.id, "betterww") != 0 || bluewake_game_mod_available("betterww"));
    const bool enabled = available && bw_setting_enabled(g_session, definition);
    ImGui::PushID(definition.id);
    ImGui::BeginDisabled(!enabled);
    const bool reflow = g_session.menu_size != 100 &&
        ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 38.0f;
    const std::string compact_label = std::string("##") + definition.id;
    const char* label = reflow ? compact_label.c_str() : definition.label;
    if (reflow) ImGui::TextWrapped("%s", definition.label);
    ImGui::SetNextItemWidth(reflow ? -1.0f : ImGui::GetFontSize() * 19.0f);
    bool edit = false;
    switch (definition.type) {
    case BW_SETTING_BOOL: {
        bool on = before == "1";
        edit = ImGui::Checkbox(label, &on);
        value = on ? "1" : "0";
        break;
    }
    case BW_SETTING_INT: {
        int n = std::stoi(before);
        edit = ImGui::SliderInt(label, &n, static_cast<int>(definition.minimum),
                               static_cast<int>(definition.maximum));
        value = std::to_string(n);
        break;
    }
    case BW_SETTING_REAL: {
        float n = std::stof(before);
        edit = ImGui::SliderFloat(label, &n, static_cast<float>(definition.minimum),
                                 static_cast<float>(definition.maximum), "%.2f");
        value = std::to_string(n);
        break;
    }
    case BW_SETTING_CHOICE: {
        const char* preview = before.c_str();
        for (size_t i = 0; i < definition.choice_count; ++i)
            if (before == definition.choices[i].value) preview = definition.choices[i].label;
        if (ImGui::BeginCombo(label, preview)) {
            for (size_t i = 0; i < definition.choice_count; ++i) {
                const BwSettingChoice& choice = definition.choices[i];
                bool choice_available = true;
                if (std::strcmp(definition.id, "aspect") == 0 && std::strcmp(choice.value, "4:3") != 0)
                    choice_available = bluewake_game_mod_available(std::strcmp(choice.value, "16:9") == 0 ? "widescreen" : "widescreen1610");
                ImGui::BeginDisabled(!choice_available);
                if (ImGui::Selectable(choice.label, before == choice.value)) {
                    value = choice.value;
                    edit = true;
                }
                ImGui::EndDisabled();
                if (!choice_available && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("This game module does not contain the required widescreen mod.");
            }
            ImGui::EndCombo();
        }
        break;
    }
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
        ImGui::TextUnformatted(definition.help);
        if (!available) ImGui::TextUnformatted(hud_option?
            bw_hud_host_availability_name(hud_status.availability):"This game module does not contain this option.");
        else if (!enabled && definition.dependency_id != nullptr) {
            const BwSettingDefinition* dependency = bw_setting_find(definition.dependency_id);
            ImGui::Text("Requires %s = %s.", dependency ? dependency->label : definition.dependency_id,
                        definition.dependency_value);
        }
        if (definition.apply == BW_SETTING_RESTART) ImGui::TextUnformatted("Applies after restarting BlueWake.");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    if (edit && bw_setting_assign(g_session, definition.id, value)) apply_menu_settings();
    if (definition.apply == BW_SETTING_RESTART)
        restart_note(bw_setting_value(g_session, definition) != bw_setting_value(g_launched, definition));
    if (std::strcmp(definition.id, "quick_items") == 0)
        ImGui::TextWrapped("Hold LB (controller) or Tab (keyboard): Up plays the Wind Waker; Left deploys the cannon at sea, then a fresh Left fires; hold Right to deploy/lower the salvage crane, release to raise it. Change the modifier in Controls. Down and plain D-pad keep their native actions. X/Y/Z item assignments stay unchanged.");
    if (std::strcmp(definition.id, "dialogue_speed") == 0)
        ImGui::TextWrapped("Speeds up supported ordinary NPC and cutscene messages. Scripted waits, page stops, choices and unskippable text stay unchanged. BetterWW Instant text takes priority. Disable Instant text and restart to restore already-patched text.");
    if(health_option && !available) ImGui::TextWrapped(bw_health_host_room_locked()?"Challenge settings are unavailable for mounted room saves.":
        healing_option?"This game translation lacks certified healing returns. Pickups keep their native healing rate.":"This game translation has not been qualified for health rules.");
    ImGui::PopID();
}

void settings_page(BwSettingPage page) {
    size_t count = 0;
    const BwSettingDefinition* definitions = bw_setting_definitions(&count);
    for (size_t i = 0; i < count; ++i)
        if (definitions[i].page == page && !(definitions[i].flags & BW_SETTING_HIDDEN))
            setting_widget(definitions[i]);
}

void tab_display(SDL_Window* w) {
    settings_page(BW_PAGE_DISPLAY);
    const float refresh = display_refresh(w);
    ImGui::TextDisabled("Target %d FPS / display %.0f Hz. Game logic runs at 30 FPS.",
                       g_session.smooth_motion ? (bw_smooth_steps(g_session.smooth_steps, refresh) + 1) * 30 : 30, refresh);
    if (ImGui::Button("Reset the window")) reset_window(w);
}

void tab_controls() {
    settings_page(BW_PAGE_CONTROLS);
    ImGui::SeparatorText("Bindings and device profiles");
    if (g_session.menu_size != 100 && ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 38.0f)
        bluewake_controls_menu_draw_compact();
    else bluewake_controls_menu_draw();
    ImGui::SeparatorText("Additional shortcuts");
    ImGui::TextWrapped("Jump, Sprint and first-person camera bindings are editable above. Camera zoom: mouse wheel. "
                       "Settings: F1 or Esc / Back. Fullscreen: F11 or Alt+Enter. Smooth Motion: F10. Frame rate: F9.");
}

void tab_enhancements() {
    settings_page(BW_PAGE_ENHANCEMENTS);
    ImGui::TextDisabled("Compiled gameplay options marked * apply after restarting BlueWake.");
}

void tab_mods() {
    ImGui::SeparatorText("Asset packs");
    settings_page(BW_PAGE_MODS);
    if (ImGui::Button("Open the texture folder")) open_folder(texture_folder());
    ImGui::TextWrapped("The legacy texture folder accepts Dolphin-format GZLE01 PNG or DDS textures. "
                       "Gameplay mods are compiled into the game module and configured in Enhancements.");
    bw_asset_pack_menu_draw();
}

const char* slowdown_marker_text(unsigned status) {
    switch (status) {
    case BLUEWAKE_FPS_MARKER_PENDING: return "Slowdown marked; waiting for the game.";
    case BLUEWAKE_FPS_MARKER_CAPTURING: return "Slowdown marked; recording the next 5 seconds.";
    case BLUEWAKE_FPS_MARKER_COMPLETE: return "Slowdown capture saved in the session log.";
    case BLUEWAKE_FPS_MARKER_UNAVAILABLE: return "Slowdown capture is unavailable.";
    default: return "Press F7 during a slowdown to mark it.";
    }
}

void mark_slowdown() {
    const bool accepted = bluewake_fps_watch_mark_slowdown();
    g_slowdown_notice_until = SDL_GetTicks() + 8000;
    if (accepted) set_menu_open(false);
}

void tab_developer() {
    ImGui::SeparatorText("Diagnostics");
    const AuroraStats* stats = aurora_get_stats();
    ImGui::Text("Rendered %.1f FPS / game %.1f FPS", aurora::gfx::calculate_fps(), aurora::gfx::calculate_game_fps());
    if (stats != nullptr) ImGui::Text("Queued shader pipelines: %u", stats->queuedPipelines);
    const unsigned marker_status = bluewake_fps_watch_marker_status();
    const bool marker_busy = marker_status == BLUEWAKE_FPS_MARKER_PENDING || marker_status == BLUEWAKE_FPS_MARKER_CAPTURING;
    ImGui::BeginDisabled(marker_busy);
    if (ImGui::Button("Mark slowdown (F7)")) mark_slowdown();
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s", slowdown_marker_text(marker_status));
    ImGui::TextWrapped("Records up to 15 seconds before the mark and 5 seconds after it in the session log. The menu closes so the game can continue.");
    ImGui::Text("Markers this session: %u", bluewake_fps_watch_marker_count());
    ImGui::TextWrapped("Logs: %slogs", g_data_dir.c_str());
    if (ImGui::Button("Open the session logs")) open_folder(g_data_dir + "logs");
    ImGui::Spacing();
    ImGui::TextUnformatted("Experimental debug save states");
    if (ImGui::Button("Save state (F6)")) {
        bluewake_save_state_hotkey(false);
        set_menu_open(false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load latest state (F8)")) {
        bluewake_save_state_hotkey(true);
        set_menu_open(false);
    }
    ImGui::TextDisabled("Keep normal memory-card saves. States depend on this game translation.");
}

void tab_network() {
    bw_network_menu_draw();
}

std::string preview_file_dialog() {
#if defined(BLUEWAKE_SETTINGS_UI_TEST)
    // The local ImGui fixture supplies a path; it must never open a native picker.
    const std::wstring selected = bw_settings_ui_test_preview_file();
#else
    wchar_t file[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof dialog;
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = L"Stereo PCM WAV (*.wav)\0*.wav\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = static_cast<DWORD>(std::size(file));
    dialog.lpstrDefExt = L"wav";
    dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameW(&dialog)) return {};
    const std::wstring selected = file;
#endif
    if (selected.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, selected.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) return {};
    std::string path(static_cast<size_t>(length), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, selected.c_str(), -1, path.data(), length, nullptr, nullptr)) return {};
    path.pop_back();
    return path;
}

const char* preview_state_name(BwAudioPreviewState state) {
    switch (state) {
    case BW_PREVIEW_IDLE: return "Off";
    case BW_PREVIEW_LOADING: return "Loading";
    case BW_PREVIEW_READY: return "Ready; waiting for game audio";
    case BW_PREVIEW_PLAYING: return "Playing";
    case BW_PREVIEW_FINISHED: return "Finished";
    case BW_PREVIEW_STOPPED: return "Stopped";
    case BW_PREVIEW_CANCELLED: return "Cancelled";
    case BW_PREVIEW_RESET: return "Stopped after game reset";
    case BW_PREVIEW_FAILED: return "Unable to play";
    case BW_PREVIEW_RATE_MISMATCH: return "Sample rate mismatch";
    case BW_PREVIEW_SHUTDOWN: return "Unavailable";
    }
    return "Unavailable";
}

void audio_preview_menu() {
    ImGui::SeparatorText("External WAV preview");
    ImGui::TextWrapped("Preview mixes with game audio. Choose a 16-bit stereo PCM WAV at 32 or 48 kHz matching the game's output rate. Music and Master/mute controls apply.");
    ImGui::TextDisabled("One pass; leaving this page stops the preview. No file is selected or played automatically.");
    auto* preview = bluewake_host_audio_preview_get();
    BwAudioPreviewStatus status{};
    const bool available = preview && bluewake_audio_preview_status(preview, &status) && status.accepting_requests;
    ImGui::BeginDisabled(!available);
    if (ImGui::Button("Choose WAV")) {
        const auto selected = preview_file_dialog();
        if (!selected.empty()) {
            if (selected.size() >= BW_PREVIEW_PATH_BYTES) {
                g_preview_message = "The selected path is too long for the WAV preview.";
            } else {
                if (selected != g_preview_path) bluewake_audio_preview_cancel(preview);
                g_preview_path = selected;
                g_preview_message.clear();
            }
        }
    }
    ImGui::EndDisabled();
    if (g_preview_path.empty()) ImGui::TextDisabled("No WAV selected.");
    else ImGui::TextWrapped("Selected: %s", g_preview_path.c_str());
    ImGui::BeginDisabled(!available || g_preview_path.empty());
    if (ImGui::Button("Play WAV")) {
        g_preview_message.clear();
        if (!bluewake_audio_preview_play_utf8(preview, g_preview_path.c_str()))
            g_preview_message = "The preview request was not accepted.";
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!available);
    if (ImGui::Button("Stop preview")) {
        bluewake_audio_preview_stop(preview);
        g_preview_message.clear();
    }
    ImGui::EndDisabled();
    if (!available) ImGui::TextWrapped("WAV preview is unavailable. Game audio remains available.");
    else {
        ImGui::Text("Preview: %s", preview_state_name(status.state));
        if (status.path_utf8[0]) ImGui::TextWrapped("Preview file: %s", status.path_utf8);
        ImGui::Text("File: %u Hz / output: %u Hz", status.sample_rate, status.observed_output_rate);
        if (status.total_frames) {
            const float progress = static_cast<float>(std::min(status.cursor_frame, status.total_frames)) / static_cast<float>(status.total_frames);
            ImGui::ProgressBar(progress, ImVec2(-1.f, 0.f), "Playback progress");
            if (status.sample_rate)
                ImGui::Text("Time: %.1f / %.1f seconds", static_cast<double>(status.cursor_frame) / status.sample_rate,
                            static_cast<double>(status.total_frames) / status.sample_rate);
        }
        if (status.error[0]) ImGui::TextWrapped("%s", status.error);
    }
    if (!g_preview_message.empty()) ImGui::TextWrapped("%s", g_preview_message.c_str());
    ImGui::Spacing();
}

void tab_sound_saves() {
    audio_preview_menu();
    settings_page(BW_PAGE_SOUND_SAVES);
    BluewakeAutosaveStatus autosave{};
    bluewake_autosave_status(&autosave);
    ImGui::TextWrapped("Autosave: %s%s", autosave.active ? "Saving; " : "",
                       bluewake_autosave_reason_text(autosave.reason));
    ImGui::TextDisabled("Completed %llu / failed %llu", static_cast<unsigned long long>(autosave.completed),
                        static_cast<unsigned long long>(autosave.failed));
    ImGui::Spacing();
    ImGui::SeparatorText("Your files");
    ImGui::TextDisabled("Saves, settings and logs are kept apart from the build:");
    ImGui::TextUnformatted(g_data_dir.c_str());
    if (ImGui::Button("Open that folder"))
        open_folder(g_data_dir);
    ImGui::Spacing();
    bw_card_menu_draw();
}

std::string preset_file_dialog(bool save) {
    wchar_t file[32768]{};
    if (save) std::wcscpy(file, L"BlueWake.bwpreset");
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof dialog;
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = L"BlueWake preset (*.bwpreset)\0*.bwpreset\0All files\0*.*\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = static_cast<DWORD>(std::size(file));
    dialog.lpstrDefExt = L"bwpreset";
    dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, file, -1, result.data(), length, nullptr, nullptr);
    result.pop_back();
    return result;
}

void preset_menu() {
    static std::vector<BwSettingsPreset> presets;
    static int selected;
    static uint32_t sections;
    static char name[97] = "My settings";
    static std::string message;
    const std::string folder = g_data_dir + "presets";
    auto reload = [&] {
        presets = bw_settings_builtin_presets();
        WIN32_FIND_DATAA entry{};
        HANDLE find = FindFirstFileA((folder + "\\*.bwpreset").c_str(), &entry);
        std::vector<std::string> files;
        if (find != INVALID_HANDLE_VALUE) {
            do { if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) files.push_back(entry.cFileName); }
            while (FindNextFileA(find, &entry));
            FindClose(find);
        }
        std::sort(files.begin(), files.end());
        for (const auto& file : files) {
            BwSettingsPreset preset;
            std::string error;
            if (bw_settings_preset_read(folder + "\\" + file, preset, &error)) presets.push_back(std::move(preset));
            else std::fprintf(stderr, "[presets] %s: %s\n", file.c_str(), error.c_str());
        }
        selected = 0;
        sections = presets.front().sections;
    };
    if (presets.empty()) reload();
    if (!ImGui::CollapsingHeader("Presets")) return;
    const bool narrow = compact_menu_rows();
    if (narrow) ImGui::TextWrapped("Preset");
    ImGui::SetNextItemWidth(narrow ? -1.0f : ImGui::GetFontSize() * 20);
    if (ImGui::BeginCombo(narrow ? "##preset-choice" : "Preset", presets[selected].name.c_str())) {
        for (size_t i = 0; i < presets.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(presets[i].name.c_str(), selected == static_cast<int>(i))) {
                selected = static_cast<int>(i);
                sections = presets[i].sections;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::TextDisabled("Sections to apply or capture:");
    if (ImGui::BeginTable("preset-sections", narrow ? 1 : 3)) {
        for (unsigned i = 0; i < BW_PAGE_COUNT; ++i) {
            if (i == BW_PAGE_NETWORK || i == BW_PAGE_DEVELOPER) continue;
            ImGui::TableNextColumn();
            bool on = (sections & (1u << i)) != 0;
            if (ImGui::Checkbox(bw_setting_page_name(static_cast<BwSettingPage>(i)), &on)) {
                if (on) sections |= 1u << i; else sections &= ~(1u << i);
            }
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("Preview changes")) {
        bluewake_controls_menu_cancel_capture();
        g_preset_popup = true;
        ImGui::OpenPopup("Apply preset");
    }
    if (!narrow) ImGui::SameLine();
    if (ImGui::Button("Import preset")) {
        bluewake_controls_menu_cancel_capture();
        const auto path = preset_file_dialog(false);
        if (!path.empty()) {
            BwSettingsPreset imported;
            if (bw_settings_preset_read(path, imported, &message)) {
                CreateDirectoryA(folder.c_str(), nullptr);
                const auto destination = folder + "\\import-" + std::to_string(SDL_GetPerformanceCounter()) + ".bwpreset";
                if (bw_settings_preset_write(destination, imported, &message)) { reload(); message = "Preset imported."; }
            }
        }
    }
    if (!narrow) ImGui::SameLine();
    if (ImGui::Button("Export selected")) {
        bluewake_controls_menu_cancel_capture();
        const auto path = preset_file_dialog(true);
        if (!path.empty() && bw_settings_preset_write(path, presets[selected], &message)) message = "Preset exported.";
    }
    if (narrow) ImGui::TextWrapped("Name");
    ImGui::SetNextItemWidth(narrow ? -1.0f : ImGui::GetFontSize() * 20);
    ImGui::InputText(narrow ? "##preset-name" : "Name", name, sizeof name);
    if (!narrow) ImGui::SameLine();
    if (ImGui::Button(narrow ? "Save preset" : "Save current settings")) {
        bluewake_controls_menu_cancel_capture();
        CreateDirectoryA(folder.c_str(), nullptr);
        const auto preset = bw_settings_capture_preset(name, g_session, sections);
        const auto path = folder + "\\settings-" + std::to_string(SDL_GetPerformanceCounter()) + ".bwpreset";
        if (bw_settings_preset_write(path, preset, &message)) { reload(); message = "Preset saved."; }
    }
    ImGui::TextDisabled("Presets cover selected settings. Button bindings stay in your device profiles.");
    if (!message.empty()) ImGui::TextWrapped("%s", message.c_str());
    // The original popup's fixed 36-font-wide child also overflowed tiny
    // windows at 100%. This viewport-only popup fix leaves the main menu's
    // original sizing/metrics untouched at 100%.
    const ImVec2 popup_display = ImGui::GetIO().DisplaySize;
    const bool fit_popup = narrow || popup_display.x < ImGui::GetFontSize() * 40.0f ||
        popup_display.y < ImGui::GetFontSize() * 18.0f;
    if (fit_popup) {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(display.x * 0.9f, display.y * 0.85f), ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), display);
    }
    if (ImGui::BeginPopupModal("Apply preset", nullptr, fit_popup ? 0 : ImGuiWindowFlags_AlwaysAutoResize)) {
        if (g_cancel_preset) {
            g_cancel_preset = g_preset_popup = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        BwPresetPreview preview;
        if (bw_settings_preset_preview(presets[selected], g_session, sections, preview, &message)) {
            ImGui::Text("%s: %zu setting changes", presets[selected].name.c_str(), preview.changes.size());
            ImGui::BeginChild("changes", fit_popup ? ImVec2(0, ImGui::GetIO().DisplaySize.y * 0.25f) :
                ImVec2(ImGui::GetFontSize() * 36, ImGui::GetFontSize() * 12), true);
            for (const auto& change : preview.changes)
                ImGui::TextWrapped("%s: %s -> %s%s", change.definition->label, change.before.c_str(), change.after.c_str(),
                                   change.definition->apply == BW_SETTING_RESTART ? " (after restart)" : "");
            for (const auto& warning : preview.warnings) ImGui::TextWrapped("%s", warning.c_str());
            ImGui::EndChild();
            if (ImGui::Button("Apply")) {
                g_session = std::move(preview.candidate);
                apply_menu_settings();
                message = "Preset applied.";
                g_preset_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
        } else ImGui::TextWrapped("%s", message.c_str());
        if (ImGui::Button("Cancel")) { g_preset_popup = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

// The UI's size for this window: the display's scale (Windows' own, or more
// on a big screen at 100 percent), smaller when the window is too small for
// the menu. No larger than the font was drawn at.
float ui_scale(SDL_Window* w) {
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    const float display = w != nullptr ? SDL_GetWindowDisplayScale(w) : 1.0f;
    const float wanted = std::max({1.0f, display, size.y / 900.0f});
    const float fits = std::min(size.x / 660.0f, size.y / 560.0f);
    return std::clamp(std::min(wanted, fits), 0.75f, g_font_scale);
}

// On the first frame: a clear font drawn at the screen's scale, the default
// for everything ImGui draws from the next frame on (the menu and Aurora's
// frame rate). ImGui's own is a 13-pixel bitmap font, tiny on a 4K screen and
// blurred when scaled.
//
// The font has an atlas of its own, handed to Aurora as an ImGui texture
// (aurora_imgui_add_texture copies it on the render worker), so it can be
// made inside the frame. Adding it to ImGui's atlas instead would have the
// WebGPU backend rebuild and upload that from the main thread while the
// render worker submits: Dawn's device is not thread-safe here, and the bigger
// upload crashed early frames.
// Keep the original font at its original metrics. A second face at twice
// the raster size shares this private atlas, so live enlargement needs no
// atlas rebuild or render-worker upload. The default ImGui atlas stays intact.
void load_font(SDL_Window* w) {
    float scale = std::max(1.0f, SDL_GetWindowDisplayScale(w));
    if (const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(w)))
        scale = std::max(scale, mode->h / 900.0f);
    scale = std::min(scale, 4.0f);
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2010, 0x205E, 0x2190, 0x2193, 0};
    char fonts[MAX_PATH];
    const UINT length = GetWindowsDirectoryA(fonts, MAX_PATH);
    std::string path;
    if (length > 0 && length < MAX_PATH - 32) {
        path = std::string(fonts) + "\\Fonts\\segoeui.ttf";
        if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) path.clear();
    }
#if defined(BLUEWAKE_SETTINGS_UI_TEST)
    if (g_menu_test_default_font) path.clear();
#endif
    std::unique_ptr<ImFontAtlas> atlas;
    ImFont* font = nullptr;
    ImFont* large = nullptr;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    // One retry without the large face retains the original fallback if the
    // combined atlas cannot be built. Neither attempt touches the GPU.
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        atlas = std::make_unique<ImFontAtlas>();
        atlas->Flags |= ImFontAtlasFlags_NoMouseCursors;
        float pixels_per_face = std::round(15.0f * scale);
        font = path.empty() ? nullptr : atlas->AddFontFromFileTTF(path.c_str(), pixels_per_face, nullptr, ranges);
        if (font == nullptr) {
            scale = std::max(1.0f, std::round(scale));
            ImFontConfig pixel;
            pixel.SizePixels = 13.0f * scale;
            pixels_per_face = pixel.SizePixels;
            font = atlas->AddFontDefault(&pixel);
        }
        large = nullptr;
        if (attempt == 0) {
            if (!path.empty()) large = atlas->AddFontFromFileTTF(path.c_str(), pixels_per_face * 2.0f, nullptr, ranges);
            if (!large) {
                ImFontConfig pixel;
                pixel.SizePixels = pixels_per_face * 2.0f;
                large = atlas->AddFontDefault(&pixel);
            }
        }
        pixels = nullptr; width = height = 0;
        atlas->GetTexDataAsRGBA32(&pixels, &width, &height);
#if defined(BLUEWAKE_SETTINGS_UI_TEST)
        if (attempt == 0 && g_menu_test_large_failure) { pixels = nullptr; width = height = 0; }
#endif
        if (pixels && width > 0 && height > 0) break;
    }
    if (pixels == nullptr || width <= 0 || height <= 0 || font == nullptr) {
        std::fprintf(stderr, "[windows] the UI font did not build; keeping ImGui's\n");
        return;
    }
    // Aurora copies this data and uploads on its existing render-worker seam.
    atlas->SetTexID(aurora_imgui_add_texture(static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixels));
    atlas->ClearTexData();
    ImGui::GetIO().FontDefault = font;
    ImGui::GetStyle().ScaleAllSizes(scale);
    g_menu_font = font;
    g_menu_large_font = large;
    g_menu_atlas = atlas.release(); // Stable until process shutdown, like the original UI atlas.
    g_font_scale = scale;
    std::fprintf(stderr, "[windows] UI scale %.2f (font atlas %dx%d, menu enlargement %s)\n", scale, width, height, large ? "ready" : "uses original font");
}

void draw_menu(SDL_Window* w) {
    bool sound_visible = false;
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    const float multiplier = std::clamp(g_session.menu_size, 75, 200) / 100.0f;
    const float scale = ui_scale(w) * multiplier;
    const bool large_font = multiplier > 1.0f && g_menu_large_font != nullptr;
    ImFont* menu_font = large_font ? g_menu_large_font : g_menu_font;
    if (menu_font) ImGui::PushFont(menu_font);
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, style.ScrollbarSize * multiplier);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, style.GrabMinSize * multiplier);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(style.CellPadding.x * multiplier, style.CellPadding.y * multiplier));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, style.IndentSpacing * multiplier);
    bluewake_ui::begin_theme(scale);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(io.DisplaySize.x * 0.95f, io.DisplaySize.y * 0.9f));
    ImGui::SetNextWindowSize(ImVec2(std::min(io.DisplaySize.x * 0.95f, 1050.f * scale),
                                  std::min(io.DisplaySize.y * 0.9f, 750.f * scale)), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.94f);
    bool open = true;
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("BlueWake settings", &open, flags)) {
        ImGui::SetWindowFontScale(scale / (g_font_scale * (large_font ? 2.0f : 1.0f)));
        bluewake_ui::heading("Play your way", "Display, controls and enhancements, all in one place.");
        // Esc normally closes it in the keyboard hook (bw_settings_key), which
        // keeps the key from SDL; one that reaches ImGui instead closes it too.
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !bluewake_controls_menu_capturing() &&
            !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
            open = false;
        static char search[160]{};
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * 5);
        if (ImGui::InputTextWithHint("##search", "Search settings", search, sizeof search))
            bluewake_controls_menu_cancel_capture();
        ImGui::SameLine();
        if (ImGui::Button("Clear")) search[0] = '\0';
        if (search[0] == '\0') preset_menu();
        ImGui::Separator();
        bool controls_visible = false;
        if (search[0] != '\0') {
            const auto results = bw_settings_search(search);
            BwSettingPage previous = BW_PAGE_COUNT;
            for (const auto* definition : results) {
                if (definition->page != previous) {
                    ImGui::SeparatorText(bw_setting_page_name(definition->page));
                    previous = definition->page;
                }
                setting_widget(*definition);
            }
            if (results.empty()) ImGui::TextDisabled("No matching settings.");
        } else if (ImGui::BeginTabBar("tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
            if (ImGui::BeginTabItem("Display")) {
                tab_display(w);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Controls")) {
                controls_visible = true;
                tab_controls();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Enhancements")) {
                tab_enhancements();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Mods")) {
                tab_mods();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Network")) {
                tab_network();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Sound & Saves")) {
                sound_visible = true;
                tab_sound_saves();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("HUD")) {
                settings_page(BW_PAGE_HUD);
                if(ImGui::Button("Reset native HUD")) {
                    g_session.hud_enabled=false;bw_hud_config_identity(&g_session.hud);apply_menu_settings();
                }
                ImGui::TextWrapped("Experimental native pane customization. Separate rupee glow, particles, minimap, compass and timers keep their native presentation.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Developer")) {
                tab_developer();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (!controls_visible) bluewake_controls_menu_cancel_capture();
        ImGui::Separator();
        if (needs_restart()) {
            if (compact_menu_rows()) ImGui::TextWrapped("* Takes effect when BlueWake starts again.");
            else ImGui::TextUnformatted("* Takes effect when BlueWake starts again.");
            if (!compact_menu_rows()) ImGui::SameLine();
            if (ImGui::Button("Restart now"))
                restart();
            ImGui::SameLine();
        }
        if (ImGui::Button("Close   (F1 or Esc)"))
            open = false;
    }
    ImGui::End();
    bluewake_ui::end_theme();
    ImGui::PopStyleVar(4);
    if (menu_font) ImGui::PopFont();
    if (g_preview_page_visible && !sound_visible)
        bluewake_audio_preview_cancel(bluewake_host_audio_preview_get());
    g_preview_page_visible = sound_visible;
    if (!open)
        set_menu_open(false);
}

// The climbing stamina wheel (climb.c), beside Link in the game's picture: the
// picture is the window's middle at the game's shape, or the whole window when
// "keep the picture's shape" is off (DOL_AURORA_ASPECT_FIT=0, set at launch).
void draw_climb_wheel() {
    float fraction, x, y, aspect, alpha;
    bool exhausted;
    if (!bluewake_climb_hud(&fraction, &exhausted, &x, &y, &aspect, &alpha))
        return;
    static const bool fit = [] {
        const char* v = std::getenv("DOL_AURORA_ASPECT_FIT");
        return v == nullptr || v[0] != '0';
    }();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    float w = display.x, h = display.y, x0 = 0.f, y0 = 0.f;
    if (h <= 0.f || aspect <= 0.f)
        return;
    if (fit && w / h > aspect) {
        w = h * aspect;
        x0 = (display.x - w) * 0.5f;
    } else if (fit) {
        h = w / aspect;
        y0 = (display.y - h) * 0.5f;
    }
    const float radius = h * 0.03f, thick = radius * 0.45f, pi = 3.14159265f;
    const ImVec2 center(x0 + x * w + radius * 2.4f, y0 + y * h - radius * 0.6f);
    ImDrawList* list = ImGui::GetForegroundDrawList();
    const auto a = [alpha](float v) { return static_cast<int>(v * alpha); };
    list->PathArcTo(center, radius, 0.f, 2.f * pi, 48);
    list->PathStroke(IM_COL32(20, 30, 20, a(150.f)), 0, thick + 3.f);
    if (fraction <= 0.002f)
        return;
    ImU32 color = IM_COL32(120, 230, 90, a(245.f));  // green
    if (exhausted) {
        const float pulse = 0.65f + 0.35f * std::sin(static_cast<float>(ImGui::GetTime()) * 8.f);
        color = IM_COL32(235, 70, 50, a(245.f * pulse));  // refilling after running out
    } else if (fraction < 0.25f) {
        color = IM_COL32(245, 190, 60, a(245.f));  // nearly out
    }
    list->PathArcTo(center, radius, -0.5f * pi, -0.5f * pi + 2.f * pi * fraction, 48);
    list->PathStroke(color, 0, thick);
}

// Compile shaders before playing: the game is held at its first present
// (dol_aurora_set_hold, the picture redrawn under this overlay) until every
// pipeline the cache queued at start is compiled, or two minutes have passed.
// They compile on several threads (Aurora's pipeline cache), and while the
// game is held it asks for nothing new. Otherwise they compile while the game
// runs, and a draw whose pipeline is not ready yet is left out of its frame.
bool g_shader_wait;
Uint64 g_shader_wait_since;
uint32_t g_shader_wait_first;

bool shader_wait_hold(void*) {
    if (!g_shader_wait)
        return false;
    const AuroraStats* stats = aurora_get_stats();
    const uint32_t left = stats != nullptr ? stats->queuedPipelines : 0u;
    if (g_shader_wait_since == 0) {
        g_shader_wait_since = SDL_GetTicks();
        g_shader_wait_first = left;
    }
    const Uint64 waited = SDL_GetTicks() - g_shader_wait_since;
    if (left == 0u || waited > 120000) {
        g_shader_wait = false;
        std::fprintf(stderr, "[windows] shaders compiled before play: %u in %.1f s%s\n", g_shader_wait_first,
                     waited / 1000.0, left != 0u ? " (stopped waiting)" : "");
        return false;
    }
    return true;
}

void draw_shader_wait(SDL_Window* w) {
    if (!g_shader_wait)
        return;
    const AuroraStats* stats = aurora_get_stats();
    const uint32_t left = stats != nullptr ? stats->queuedPipelines : 0u;
    ImGuiIO& io = ImGui::GetIO();
    const float scale = ui_scale(w);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.75f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##bluewake-shaders", nullptr, flags)) {
        ImGui::SetWindowFontScale(scale / g_font_scale);
        ImGui::Text("Compiling shaders: %u to go", left);
        if (g_shader_wait_first > 0u)
            ImGui::ProgressBar(1.f - static_cast<float>(left) / static_cast<float>(g_shader_wait_first),
                               ImVec2(260.f * scale, 0.f), "");
    }
    ImGui::End();
}

// For the first few seconds, where the settings and fullscreen are.
void draw_hint(SDL_Window* w) {
    const Uint64 shown = SDL_GetTicks() - g_first_frame_at;
    if (shown > 7000 || g_menu_open)
        return;
    ImGuiIO& io = ImGui::GetIO();
    const float scale = ui_scale(w);
    ImGui::SetNextWindowPos(ImVec2(12 * scale, io.DisplaySize.y - 12 * scale), ImGuiCond_Always, ImVec2(0, 1));
    ImGui::SetNextWindowBgAlpha(shown > 6000 ? 0.6f * (7000 - shown) / 1000.0f : 0.6f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##bluewake-hint", nullptr, flags)) {
        ImGui::SetWindowFontScale(scale / g_font_scale);
        ImGui::TextUnformatted("F1 settings   F11 fullscreen   F9 frame rate   Click the game to steer the camera");
    }
    ImGui::End();
}

void draw_controls_save_notice(SDL_Window* w) {
    if (bluewake_controls_menu_save_error()[0] == '\0')
        return;
    const float scale = ui_scale(w);
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(12 * scale, 12 * scale), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(std::max(1.f, std::min(440 * scale, io.DisplaySize.x - 24 * scale)), 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##bluewake-controls-save", nullptr, flags)) {
        ImGui::SetWindowFontScale(scale / g_font_scale);
        ImGui::TextWrapped("Your controls are active, but could not be saved.");
        ImGui::TextWrapped("BlueWake will retry when settings are closed. F1 > Controls > Save bindings retries immediately.");
    }
    ImGui::End();
}

void draw_slowdown_notice(SDL_Window* w) {
    if (g_slowdown_notice_until == 0) return;
    const unsigned status = bluewake_fps_watch_marker_status();
    const bool active = status == BLUEWAKE_FPS_MARKER_PENDING || status == BLUEWAKE_FPS_MARKER_CAPTURING;
    if (!active && SDL_GetTicks() > g_slowdown_notice_until) return;
    const float scale = ui_scale(w);
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 12 * scale, io.DisplaySize.y - 12 * scale),
                           ImGuiCond_Always, ImVec2(1, 1));
    ImGui::SetNextWindowSize(ImVec2(std::max(1.f, std::min(440 * scale, io.DisplaySize.x - 24 * scale)), 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##bluewake-slowdown", nullptr, flags)) {
        ImGui::SetWindowFontScale(scale / g_font_scale);
        ImGui::TextWrapped("%s", slowdown_marker_text(status));
        ImGui::TextWrapped("F1 > Developer > Open the session logs");
    }
    ImGui::End();
}

// Every presented frame, on the main thread, inside Aurora's frame.
void frame(void*) {
    SDL_Window* w = game_window();
    if (w == nullptr)
        return;
    if (g_first_frame_at == 0) {
        g_first_frame_at = SDL_GetTicks();
        if (!bluewake_controls_init(g_data_dir.c_str()))
            std::fprintf(stderr, "[controls] %s\n", bluewake_controls_error());
        apply_live();
        load_font(w);
        return;  // the font is ImGui's default from the next frame
    }
    if (!g_placed) {
        g_placed = true;
        place_window(w);
    }
    if (g_toggle_fullscreen) {
        g_toggle_fullscreen = false;
        set_fullscreen(w, !is_fullscreen(w));
    }
    if (g_toggle_menu) {
        g_toggle_menu = false;
        set_menu_open(!g_menu_open);
    }
    if(g_hud_pending)apply_hud();
    track_window(w);
    // The frame rate in the session log, a line a second beside the host's
    // [perf] line: frames shown (60 with Smooth Motion at full speed) and the
    // game's own (30 at full speed).
    // With it, where the game thread waited in that second (GXRuntime's
    // counters): for the GX worker at the game's draw-done barriers, and in
    // the present, of which end_frame is Aurora's submission.
    static Uint64 fps_logged;
    static DolAuroraFrameTiming timing_before;
    const Uint64 now = SDL_GetTicks();
    bluewake_controls_menu_tick(now);
    if (now - fps_logged >= 1000) {
        apply_smooth_rate(w);
        DolAuroraFrameTiming timing{};
        dol_aurora_frame_timing(&timing);
        if (fps_logged != 0)
            std::fprintf(stderr, "[fps] shown=%.1f game=%.1f drain_ms=%.0f present_ms=%.0f end_frame_ms=%.0f\n",
                         aurora::gfx::calculate_fps(), aurora::gfx::calculate_game_fps(),
                         (timing.drain_us - timing_before.drain_us) / 1000.0,
                         (timing.present_us - timing_before.present_us) / 1000.0,
                         (timing.end_frame_us - timing_before.end_frame_us) / 1000.0);
        fps_logged = now;
        timing_before = timing;
    }
    static Uint64 controllers_refreshed;
    if (now - controllers_refreshed >= 500) {
        bluewake_controls_refresh();
        controllers_refreshed = now;
    }
    static unsigned healthy_frames;
    if (!g_menu_open && ++healthy_frames == 600u && !bw_launch_clear((g_data_dir + "launch.pending").c_str()))
        std::fprintf(stderr, "[safe-mode] could not clear launch marker\n");
    if (g_safe_mode && SDL_GetTicks() - g_first_frame_at < 15000) {
        ImGui::SetNextWindowBgAlpha(0.85f);
        ImGui::Begin("Launch recovery", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextWrapped("Safe mode: HLE audio, mods off. Previous settings were kept in a backup when possible.");
        ImGui::End();
    }
    draw_climb_wheel();
    draw_shader_wait(w);
    if (g_menu_open)
        draw_menu(w);
    draw_hint(w);
    draw_controls_save_notice(w);
    draw_slowdown_notice(w);
    if (g_dirty && SDL_GetTicks() - g_dirty_at > 1000 && !g_menu_open)
        save_file();
}

void save_at_exit() {
    if (g_dirty)
        save_file();
    if (bluewake_controls_dirty() && !bluewake_controls_save())
        std::fprintf(stderr, "[controls] %s\n", bluewake_controls_error());
}

}  // namespace

#if defined(BLUEWAKE_SETTINGS_UI_TEST)
void bw_settings_ui_test_reset(const char* data_dir, const Settings& saved) {
    if (g_menu_open) set_menu_open(false);
    g_saved = g_session = g_before_edit = g_launched = saved;
    g_data_dir = data_dir;
    g_path = g_data_dir + "settings.ini";
    g_safe_mode = g_menu_open = g_toggle_menu = g_toggle_fullscreen = g_dirty = false;
    g_preset_popup = g_cancel_preset = g_placed = false;
    g_first_frame_at = g_dirty_at = 0;
    g_slowdown_notice_until = 0;
    g_environment.clear();
    g_restart = RestartRequest{};
    g_font_scale = 1.f;
    g_preview_page_visible = false;
    g_preview_path.clear();
    g_preview_message.clear();
    set_menu_open(true);
}
void bw_settings_ui_test_draw() {
    bluewake_controls_menu_tick(SDL_GetTicks());
    if (g_menu_open) draw_menu(nullptr);
    draw_controls_save_notice(nullptr);
    draw_slowdown_notice(nullptr);
}
const Settings& bw_settings_ui_test_session() { return g_session; }
const Settings& bw_settings_ui_test_saved() { return g_saved; }
bool bw_settings_ui_test_menu_open() { return g_menu_open; }
bool bw_settings_ui_test_preset_open() { return g_preset_popup; }
void bw_settings_ui_test_flush() { save_file(); }
void bw_menu_size_test_load_font() { load_font(nullptr); }
void bw_menu_size_test_release_font() {
    ImGui::GetIO().FontDefault = nullptr;
    delete g_menu_atlas; g_menu_atlas = nullptr;
    g_menu_font = g_menu_large_font = nullptr;
}
bool bw_menu_size_test_restart_needed() { return needs_restart(); }
float bw_menu_size_test_auto_scale() { return ui_scale(nullptr); }
float bw_menu_size_test_raster_scale() { return g_font_scale; }
#endif

extern "C" void bw_settings_capture_environment(void) {
    wchar_t* block = GetEnvironmentStringsW();
    if (block == nullptr)
        return;
    wchar_t* end = block;
    while (*end != L'\0')
        end += wcslen(end) + 1;
    g_environment.assign(block, end + 1);
    FreeEnvironmentStringsW(block);
}

extern "C" void bw_settings_load(const char* data_dir) {
    g_data_dir = data_dir;
    g_path = g_data_dir + "settings.ini";
    load_file();
    g_safe_mode = env_set("BLUEWAKE_SAFE_MODE") || bw_launch_pending((g_data_dir + "launch.pending").c_str());
    if (g_safe_mode) {
        if (!bw_launch_backup(g_path.c_str())) {
            std::fprintf(stderr, "[safe-mode] settings backup failed; refusing to reset preferences\n");
            // Keep preferences intact; session safety still forces HLE/mods off.
        } else {
            Settings defaults;
            g_saved.aspect = defaults.aspect;
            g_saved.keep_aspect = defaults.keep_aspect;
            g_saved.betterww = false;
            g_saved.options.clear();
            g_saved.hd_textures = false;
            g_saved.lle_audio = false;
            g_saved.movement_extras = false;
            g_saved.quick_items = false;
            g_saved.faster_wind = false;
            g_saved.faster_boots = false;
            g_saved.autosave = false;
            g_saved.fast_transitions = false;
            g_saved.quick_doors = false;
            g_dirty = true;
            save_file();
        }
        _putenv_s("BLUEWAKE_DSP_MODE", "hle");
        _putenv_s("BLUEWAKE_MODS", "none");
        _putenv_s("BLUEWAKE_OPTIONS", "none");
        _putenv_s("BLUEWAKE_ASPECT", "4:3");
        _putenv_s("DOL_AURORA_ASPECT_FIT", "1");
        _putenv_s("DOL_AURORA_TEXTURE_PACK", "");
        _putenv_s("BLUEWAKE_JUMP_BUTTON", "0");
        _putenv_s("BLUEWAKE_SPRINT_SPEED", "1");
        _putenv_s("BLUEWAKE_FAST_FORWARD", "0");
        _putenv_s("BLUEWAKE_FADE_FRAMES", "0");
        _putenv_s("BLUEWAKE_QUICK_DOORS", "0");
        std::fprintf(stderr, "[safe-mode] recovering a pending launch; HLE audio and restart defaults\n");
    }
    g_launched = g_saved;
}

extern "C" void bw_settings_apply_launch(void) {
    // What the command line chose wins for this session (and is shown).
    g_session = g_saved;
    Settings& d = g_session;
    if (g_safe_mode) { d.quick_items = false; d.autosave = false; d.faster_wind = false; d.faster_boots = false; }
    if (env_set("BLUEWAKE_CLIMB")) d.climb = std::getenv("BLUEWAKE_CLIMB")[0] != '0';
    if (env_set("BLUEWAKE_CLIMB_STAMINA")) d.climb_stamina = std::clamp(std::atoi(std::getenv("BLUEWAKE_CLIMB_STAMINA")), 4, 30);
    if (env_set("BLUEWAKE_STICK_CAMERA")) d.stick_camera = std::getenv("BLUEWAKE_STICK_CAMERA")[0] != '0';
    if (env_set("BLUEWAKE_STICK_CAMERA_SPEED")) d.stick_speed = std::clamp(std::atoi(std::getenv("BLUEWAKE_STICK_CAMERA_SPEED")), 60, 1080);
    if (env_set("BLUEWAKE_STICK_AIM_SPEED")) d.stick_aim_speed = std::clamp(std::atoi(std::getenv("BLUEWAKE_STICK_AIM_SPEED")), 30, 720);
    if (env_set("BLUEWAKE_STICK_CAMERA_INVERT_X")) d.pad_invert_x = std::getenv("BLUEWAKE_STICK_CAMERA_INVERT_X")[0] == '1';
    if (env_set("BLUEWAKE_STICK_CAMERA_INVERT_Y")) d.pad_invert_y = std::getenv("BLUEWAKE_STICK_CAMERA_INVERT_Y")[0] == '1';
    if (env_set("BLUEWAKE_JUMP_BUTTON"))
        d.movement_extras = std::getenv("BLUEWAKE_JUMP_BUTTON")[0] != '0';
    if (env_set("BLUEWAKE_FAST_FORWARD"))
        d.fast_transitions = std::getenv("BLUEWAKE_FAST_FORWARD")[0] != '0';
    if (env_set("BLUEWAKE_QUICK_DOORS"))
        d.quick_doors = std::getenv("BLUEWAKE_QUICK_DOORS")[0] != '0';
    env_default("BLUEWAKE_JUMP_BUTTON", d.movement_extras ? "1" : "0");
    env_default("BLUEWAKE_SPRINT_SPEED", d.movement_extras ? "1.5" : "1");
    env_default("BLUEWAKE_FAST_FORWARD", d.fast_transitions ? "1" : "0");
    env_default("BLUEWAKE_FADE_FRAMES", d.fast_transitions ? "6" : "0");
    env_default("BLUEWAKE_QUICK_DOORS", d.quick_doors ? "1" : "0");
    // Diagnostic routes restore block-level overlap observations in the host.
    env_default("BLUEWAKE_OVERLAP_OBSERVATION", "0");
    if (env_set("DOL_AURORA_RENDER_SCALE"))
        d.render_scale = std::clamp(std::atoi(std::getenv("DOL_AURORA_RENDER_SCALE")), 0, 4);
    if (env_set("DOL_AURORA_FORCE_ANISO"))
        d.anisotropy = std::clamp(std::atoi(std::getenv("DOL_AURORA_FORCE_ANISO")), 1, 16);
    if (env_set("DOL_AURORA_FULLSCREEN"))
        d.fullscreen = std::getenv("DOL_AURORA_FULLSCREEN")[0] != '0';
    if (env_set("BLUEWAKE_MOUSE_CAMERA"))
        d.mouse_camera = std::getenv("BLUEWAKE_MOUSE_CAMERA")[0] != '0';
    if (env_set("BLUEWAKE_MOUSE_SENSITIVITY"))
        d.mouse_sensitivity = std::clamp(std::atof(std::getenv("BLUEWAKE_MOUSE_SENSITIVITY")), 0.1, 10.0);
    if (env_set("BLUEWAKE_MOUSE_INVERT_Y"))
        d.mouse_invert_y = std::getenv("BLUEWAKE_MOUSE_INVERT_Y")[0] == '1';
    if (env_set("BLUEWAKE_HAPTICS")) {
        const char mode = std::getenv("BLUEWAKE_HAPTICS")[0];
        d.haptics = mode == 'e' || mode == '1' ? 2 : mode == 'c' ? 1 : 0;
    }
    if (env_set("BLUEWAKE_HAPTICS_STRENGTH"))
        d.haptics_strength = std::clamp(std::atoi(std::getenv("BLUEWAKE_HAPTICS_STRENGTH")), 0, 100);
    if (env_set("BLUEWAKE_HAPTICS_TRIGGERS"))
        d.haptics_triggers = std::getenv("BLUEWAKE_HAPTICS_TRIGGERS")[0] != '0';
    env_default("BLUEWAKE_HAPTICS", haptics_name(d.haptics));
    env_default("BLUEWAKE_HAPTICS_STRENGTH", std::to_string(d.haptics_strength));
    env_default("BLUEWAKE_HAPTICS_TRIGGERS", d.haptics_triggers ? "1" : "0");
    if (d.aspect != "4:3")
        env_default("BLUEWAKE_ASPECT", d.aspect);
    env_default("DOL_AURORA_ASPECT_FIT", d.keep_aspect ? "1" : "0");
    if (d.betterww)
        env_default("BLUEWAKE_MODS", "betterww");
    if (!d.options.empty()) {
        std::string list;
        for (const auto& [name, on] : d.options)
            list += (list.empty() ? "" : ",") + std::string(on ? "" : "-") + name;
        env_default("BLUEWAKE_OPTIONS", list);
    }
    CreateDirectoryA((g_data_dir + "Load").c_str(), nullptr);
    CreateDirectoryA((g_data_dir + "Load\\Textures").c_str(), nullptr);
    CreateDirectoryA(texture_folder().c_str(), nullptr);
    if (d.hd_textures)
        env_default("DOL_AURORA_TEXTURE_PACK", texture_folder());
    if (d.lle_audio)
        env_default("BLUEWAKE_DSP_MODE", "lle");
    if (d.fullscreen)
        env_default("DOL_AURORA_FULLSCREEN", "1");
    int width = d.window_w, height = d.window_h;
    if (width < 320 || height < 240) {
        const std::string aspect = env_set("BLUEWAKE_ASPECT") ? std::getenv("BLUEWAKE_ASPECT") : d.aspect;
        default_window(aspect_ratio(aspect), &width, &height);
    }
    env_default("DOL_AURORA_WINDOW", std::to_string(width) + "x" + std::to_string(height));
    env_default("DOL_AURORA_RENDER_SCALE", std::to_string(d.render_scale));
    if (d.anisotropy > 1)
        env_default("DOL_AURORA_FORCE_ANISO", std::to_string(d.anisotropy));
    if (!d.mouse_camera)
        env_default("BLUEWAKE_MOUSE_CAMERA", "0");
    char sensitivity[32];
    std::snprintf(sensitivity, sizeof sensitivity, "%.2f", d.mouse_sensitivity);
    env_default("BLUEWAKE_MOUSE_SENSITIVITY", sensitivity);
    if (d.mouse_invert_y)
        env_default("BLUEWAKE_MOUSE_INVERT_Y", "1");
    // Aurora reads these two before main runs; the command line's --smooth
    // and --fps (which set them) win over the file.
    if (!env_set("DOL_AURORA_FRAME_INTERP"))
        aurora_set_frame_interpolation(d.smooth_motion);
    else
        d.smooth_motion = std::getenv("DOL_AURORA_FRAME_INTERP")[0] == '1';
    if (env_set("DOL_AURORA_FRAME_INTERP_STEPS"))
        d.smooth_steps = bw_smooth_requested(std::getenv("DOL_AURORA_FRAME_INTERP_STEPS"));
    apply_smooth_rate(game_window());
    if (!env_set("DOL_AURORA_SHOW_FPS"))
        aurora_set_fps_overlay(d.show_fps);
    else
        d.show_fps = std::getenv("DOL_AURORA_SHOW_FPS")[0] == '1';
    if (env_set("BLUEWAKE_DSP_MODE")) d.lle_audio = std::strcmp(std::getenv("BLUEWAKE_DSP_MODE"), "lle") == 0;
    if (env_set("BLUEWAKE_ASPECT")) d.aspect = std::getenv("BLUEWAKE_ASPECT");
    if (env_set("DOL_AURORA_ASPECT_FIT")) d.keep_aspect = std::getenv("DOL_AURORA_ASPECT_FIT")[0] != '0';
    if (env_set("BLUEWAKE_MODS")) d.betterww = std::string(std::getenv("BLUEWAKE_MODS")).find("betterww") != std::string::npos;
    if (env_set("DOL_AURORA_TEXTURE_PACK")) d.hd_textures = true;
    if (env_set("BLUEWAKE_OPTIONS")) {
        d.options.clear();
        std::string options = std::getenv("BLUEWAKE_OPTIONS");
        d.option_defaults_off = options.rfind("none", 0) == 0;
        size_t start = 0;
        while (start < options.size()) {
            size_t end = options.find(',', start);
            std::string key = options.substr(start, end - start);
            bool enabled = !key.empty() && key[0] != '-';
            if (!enabled) key.erase(0, 1);
            if (!key.empty() && key != "none") d.options[key] = enabled;
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    g_launched = g_before_edit = g_session;
    apply_health();
}

extern "C" void bw_settings_install(void) {
    // These game-thread features use atomic desired settings and must be
    // initialized even before an Aurora frame (including a headless launch).
    apply_sprint_modes();
    apply_quick_items();
    apply_dialogue_speed();
    apply_autosave();
    apply_equipment();
    apply_health();
    apply_audio();
    apply_hud();
    // The diagnostic host keeps game settings but never installs callbacks
    // that initialize controls or resize/show/fullscreen the hidden window.
    if (bluewake_noninteractive_requested()) return;
    dol_aurora_set_overlay(frame, nullptr);
    // BLUEWAKE_SHADERS_FIRST=0/1 overrides the setting (testing).
    const char* first = std::getenv("BLUEWAKE_SHADERS_FIRST");
    g_shader_wait = first != nullptr && first[0] != '\0' ? first[0] != '0' : g_saved.shaders_first;
    if (g_shader_wait) {
        dol_aurora_set_hold(shader_wait_hold, nullptr);
        dol_aurora_set_hold_redraw(true);
    }
    std::atexit(save_at_exit);
}
extern "C" void bw_settings_start_asset_packs(void) {
    if (!bw_asset_pack_menu_start(g_data_dir.c_str(), g_launched.hd_textures))
        std::fprintf(stderr, "[asset-packs] %s\n", bw_asset_pack_menu_error());
}

extern "C" int bw_settings_key(unsigned virtual_key, int alt) {
    if (g_preset_popup && (virtual_key == VK_ESCAPE || virtual_key == VK_F1)) {
        g_cancel_preset = true;
        return 1;
    }
    if (g_menu_open && (virtual_key == VK_F6 || virtual_key == VK_F8)) return 1;
    if (bluewake_controls_menu_capturing()) {
        if (virtual_key == VK_ESCAPE) {
            bluewake_controls_menu_cancel_capture();
            return 1;
        }
        if (virtual_key == VK_F1 || virtual_key == VK_F6 || virtual_key == VK_F7 || virtual_key == VK_F8 ||
            virtual_key == VK_F9 || virtual_key == VK_F10 || virtual_key == VK_F11 ||
            (virtual_key == VK_RETURN && alt)) {
            bluewake_controls_menu_reject_reserved_key();
            return 1;
        }
    }
    switch (virtual_key) {
    case VK_F1:
        g_toggle_menu = true;
        return 1;
    case VK_ESCAPE:
        // Esc first gives the mouse back (the mouse camera sees it); then it
        // opens and closes the menu.
        if (!g_menu_open && bluewake_mouse_camera_captured())
            return 0;
        g_toggle_menu = true;
        return 1;
    case VK_F11:
        g_toggle_fullscreen = true;
        return 1;
    case VK_RETURN:
        if (!alt)
            return 0;
        g_toggle_fullscreen = true;
        return 1;
    case VK_F10:
        g_session.smooth_motion = !aurora_get_frame_interpolation();
        aurora_set_frame_interpolation(g_session.smooth_motion);
        changed();
        std::fprintf(stderr, "[windows] Smooth Motion %s\n", g_session.smooth_motion ? "on" : "off");
        return 1;
    case VK_F6:
        bluewake_save_state_hotkey(false);
        return 1;
    case VK_F7:
        mark_slowdown();
        return 1;
    case VK_F8:
        bluewake_save_state_hotkey(true);
        return 1;
    case VK_F9:
        g_session.show_fps = !g_session.show_fps;
        aurora_set_fps_overlay(g_session.show_fps);
        changed();
        return 1;
    default:
        return 0;
    }
}

extern "C" int bw_settings_relaunch(void) {
    return !g_restart.take() || relaunch() ? 0 : 1;
}

extern "C" bool bluewake_settings_menu_event(const void* sdl_event) {
    const SDL_Event* event = static_cast<const SDL_Event*>(sdl_event);
    if (event->type == SDL_EVENT_GAMEPAD_ADDED || event->type == SDL_EVENT_GAMEPAD_REMOVED)
        bluewake_controls_refresh();
    if (bluewake_controls_menu_capturing()) {
        bluewake_controls_menu_event(sdl_event);
        return true;
    }
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
        if (g_preset_popup) { g_cancel_preset = true; return true; }
        set_menu_open(!g_menu_open);
        return true;
    }
    bluewake_controls_menu_event(sdl_event);
    return g_menu_open;
}

#if defined(BLUEWAKE_SETTINGS_UI_TEST)
void bw_menu_size_test_font_mode(bool fallback, bool failed_large) {
    g_menu_test_default_font = fallback; g_menu_test_large_failure = failed_large;
}
#endif
