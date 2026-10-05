// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <climits>
#include <map>
#include <string>
#include "hud_customization.h"
inline BwHudConfig bw_settings_native_hud() {
    BwHudConfig c{};
    for(auto& g:c.groups){g.scale=g.opacity=1.f;g.visible=true;for(auto& t:g.tint)t=255;}
    return c;
}
#include "sprint_input.h"

struct Settings {
    bool hud_enabled=false;
    BwHudConfig hud=bw_settings_native_hud();
    // Display: apply at once.
    int menu_size = 100; // Percent of automatic DPI/window-fit sizing, applied live.
    bool fullscreen = false;
    int window_w = 0, window_h = 0;  // 0: sized from the screen
    int window_x = INT_MIN, window_y = INT_MIN;
    int render_scale = 0;  // 0: the window's own pixels; 1-4: x 480 lines
    int anisotropy = 1;    // 1: the game's own filtering; 2-16 forced
    bool smooth_motion = false;  // experimental: off unless the player turns it on
    int smooth_steps = 1; // 60 FPS; 3: 120; -1: match the display, up to 240.
    bool show_fps = false;
    bool pause_unfocused = false;
    // At start, the game waits until the saved and bundled pipelines are compiled.
    bool shaders_first = false;
    // Controls: apply at once.
    bool mouse_camera = true;
    double mouse_sensitivity = 1.0;
    bool mouse_invert_y = false;
    bool controller_swap_ab = false, controller_swap_xy = false;
    bool pad_invert_x = false, pad_invert_y = false;
    // The fast right-stick camera (mouse_camera.h): the stick turns the view
    // and aims directly, instead of the game's eased C-stick camera.
    bool stick_camera = true;
    int stick_speed = 360;      // degrees a second at full tilt
    int stick_aim_speed = 180;  // the same when aiming
    bool climb = false;         // climb any wall on a stamina wheel (climb.h)
    int climb_stamina = 12;     // seconds of climbing on a full wheel
    bool quick_items = false;   // Fixed native Wind Waker / boat shortcuts.
    int damage_rate_q8 = 256, healing_rate_q8 = 256;
    bool faster_wind = false;
    bool faster_boots = false;
    double dialogue_speed = 1.0;
    bool autosave = false;
    int autosave_interval = 300; // Seconds; native game saves, safe contexts only.
    int audio_master = 100;
    int audio_music = 100, audio_sfx = 100;
    bool audio_muted = false;
    // Elliott Tate's controller feedback: off, classic, enhanced.
    int haptics = 2;
    int haptics_strength = 80;
    bool haptics_triggers = true;
    // At the next launch.
    std::string aspect = "4:3";
    bool keep_aspect = true;
    bool betterww = false;
    std::map<std::string, bool> options;  // only those changed from their default
    bool option_defaults_off = false; // Session-only --options none baseline.
    bool hd_textures = false;
    bool lle_audio = false;
    BwSprintMode sprint_keyboard_mode = BW_SPRINT_HOLD;
    BwSprintMode sprint_controller_mode = BW_SPRINT_TOGGLE;
    bool movement_extras = false;
    bool fast_transitions = false;
    bool quick_doors = false;
};

inline bool bw_settings_option_value(const Settings& settings, const std::string& name, bool default_on) {
    auto chosen = settings.options.find(name);
    return chosen != settings.options.end() ? chosen->second : default_on && !settings.option_defaults_off;
}

inline void bw_settings_keep_edits(Settings& saved, const Settings& before, const Settings& session) {
    if(before.hud_enabled!=session.hud_enabled)saved.hud_enabled=session.hud_enabled;
    for(unsigned i=0;i<BW_HUD_GROUP_COUNT;++i) {
        const auto &a=before.hud.groups[i],&b=session.hud.groups[i];auto& d=saved.hud.groups[i];
        if(a.offset_x!=b.offset_x)d.offset_x=b.offset_x;
        if(a.offset_y!=b.offset_y)d.offset_y=b.offset_y;
        if(a.scale!=b.scale)d.scale=b.scale;
        if(a.opacity!=b.opacity)d.opacity=b.opacity;
        if(a.anchor_x!=b.anchor_x)d.anchor_x=b.anchor_x;
        if(a.anchor_y!=b.anchor_y)d.anchor_y=b.anchor_y;
        if(a.visible!=b.visible)d.visible=b.visible;
        for(unsigned c=0;c<4;++c)if(a.tint[c]!=b.tint[c])d.tint[c]=b.tint[c];
    }

    if (before.sprint_keyboard_mode != session.sprint_keyboard_mode) saved.sprint_keyboard_mode = session.sprint_keyboard_mode;
    if (before.sprint_controller_mode != session.sprint_controller_mode) saved.sprint_controller_mode = session.sprint_controller_mode;
    if (before.menu_size != session.menu_size) saved.menu_size = session.menu_size;
    if (before.fullscreen != session.fullscreen) saved.fullscreen = session.fullscreen;
    if (before.window_w != session.window_w) saved.window_w = session.window_w;
    if (before.window_h != session.window_h) saved.window_h = session.window_h;
    if (before.window_x != session.window_x) saved.window_x = session.window_x;
    if (before.window_y != session.window_y) saved.window_y = session.window_y;
    if (before.render_scale != session.render_scale) saved.render_scale = session.render_scale;
    if (before.anisotropy != session.anisotropy) saved.anisotropy = session.anisotropy;
    if (before.smooth_motion != session.smooth_motion) saved.smooth_motion = session.smooth_motion;
    if (before.smooth_steps != session.smooth_steps) saved.smooth_steps = session.smooth_steps;
    if (before.show_fps != session.show_fps) saved.show_fps = session.show_fps;
    if (before.pause_unfocused != session.pause_unfocused) saved.pause_unfocused = session.pause_unfocused;
    if (before.shaders_first != session.shaders_first) saved.shaders_first = session.shaders_first;
    if (before.mouse_camera != session.mouse_camera) saved.mouse_camera = session.mouse_camera;
    if (before.mouse_sensitivity != session.mouse_sensitivity) saved.mouse_sensitivity = session.mouse_sensitivity;
    if (before.mouse_invert_y != session.mouse_invert_y) saved.mouse_invert_y = session.mouse_invert_y;
    if (before.pad_invert_x != session.pad_invert_x) saved.pad_invert_x = session.pad_invert_x;
    if (before.pad_invert_y != session.pad_invert_y) saved.pad_invert_y = session.pad_invert_y;
    if (before.stick_camera != session.stick_camera) saved.stick_camera = session.stick_camera;
    if (before.stick_speed != session.stick_speed) saved.stick_speed = session.stick_speed;
    if (before.stick_aim_speed != session.stick_aim_speed) saved.stick_aim_speed = session.stick_aim_speed;
    if (before.climb != session.climb) saved.climb = session.climb;
    if (before.climb_stamina != session.climb_stamina) saved.climb_stamina = session.climb_stamina;
    if (before.quick_items != session.quick_items) saved.quick_items = session.quick_items;
    if (before.damage_rate_q8 != session.damage_rate_q8) saved.damage_rate_q8 = session.damage_rate_q8;
    if (before.healing_rate_q8 != session.healing_rate_q8) saved.healing_rate_q8 = session.healing_rate_q8;
    if (before.faster_wind != session.faster_wind) saved.faster_wind = session.faster_wind;
    if (before.faster_boots != session.faster_boots) saved.faster_boots = session.faster_boots;
    if (before.dialogue_speed != session.dialogue_speed) saved.dialogue_speed = session.dialogue_speed;
    if (before.autosave != session.autosave) saved.autosave = session.autosave;
    if (before.autosave_interval != session.autosave_interval) saved.autosave_interval = session.autosave_interval;
    if (before.audio_master != session.audio_master) saved.audio_master = session.audio_master;
    if (before.audio_music != session.audio_music) saved.audio_music = session.audio_music;
    if (before.audio_sfx != session.audio_sfx) saved.audio_sfx = session.audio_sfx;
    if (before.audio_muted != session.audio_muted) saved.audio_muted = session.audio_muted;
    if (before.haptics != session.haptics) saved.haptics = session.haptics;
    if (before.haptics_strength != session.haptics_strength) saved.haptics_strength = session.haptics_strength;
    if (before.haptics_triggers != session.haptics_triggers) saved.haptics_triggers = session.haptics_triggers;
    if (before.aspect != session.aspect) saved.aspect = session.aspect;
    if (before.keep_aspect != session.keep_aspect) saved.keep_aspect = session.keep_aspect;
    if (before.betterww != session.betterww) saved.betterww = session.betterww;
    if (before.hd_textures != session.hd_textures) saved.hd_textures = session.hd_textures;
    if (before.lle_audio != session.lle_audio) saved.lle_audio = session.lle_audio;
    if (before.movement_extras != session.movement_extras) saved.movement_extras = session.movement_extras;
    if (before.fast_transitions != session.fast_transitions) saved.fast_transitions = session.fast_transitions;
    if (before.quick_doors != session.quick_doors) saved.quick_doors = session.quick_doors;
    if (before.controller_swap_ab != session.controller_swap_ab) saved.controller_swap_ab = session.controller_swap_ab;
    if (before.controller_swap_xy != session.controller_swap_xy) saved.controller_swap_xy = session.controller_swap_xy;
    for (const auto& [key, value] : session.options) {
        auto old = before.options.find(key);
        if (old == before.options.end() || old->second != value) saved.options[key] = value;
    }
    for (const auto& [key, value] : before.options) {
        if (!session.options.count(key)) saved.options.erase(key);
    }
}
