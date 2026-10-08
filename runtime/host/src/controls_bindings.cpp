// SPDX-License-Identifier: GPL-3.0-or-later
#include "controls_bindings.h"

#ifdef _WIN32
// The host's forced POSIX shim replaces C rename; it must not rewrite
// std::filesystem overloads. Windows profiles use MoveFileExW below.
#ifdef rename
#undef rename
#endif
#endif

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <dolphin/pad.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
constexpr std::array<PADButton, 12> buttons = {PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT,
    PAD_BUTTON_DOWN, PAD_BUTTON_UP, PAD_TRIGGER_Z, PAD_TRIGGER_R, PAD_TRIGGER_L,
    PAD_BUTTON_A, PAD_BUTTON_B, PAD_BUTTON_X, PAD_BUTTON_Y, PAD_BUTTON_START};
constexpr const char* button_names[] = {"D-pad Left", "D-pad Right", "D-pad Down",
    "D-pad Up", "Z", "R", "L", "A", "B", "X", "Y", "Start"};
constexpr const char* axis_names[] = {"Stick Right", "Stick Left", "Stick Up", "Stick Down",
    "Camera Right", "Camera Left", "Camera Up", "Camera Down", "Analog L", "Analog R"};
constexpr const char* action_names[] = {"Jump", "Sprint", "First-person view", "D-pad shortcut modifier (hold)"};
constexpr uint16_t cpad_buttons[] = {0x8000,0x4000,0x2000,0x1000,0x0800,0x0400,
                                    0x0200,0x0100,0x0080,0x0040,0x0020,0x0010};
struct Profile {
    std::string guid, serial;
    BluewakeControlsSnapshot state{};
};
struct Device {
    BluewakeControlsDevice info{};
    std::string guid, serial;
    SDL_Gamepad* pad = nullptr;
};
struct Settings {
    bool automatic = true;
    std::string preferred_guid, preferred_serial;
    BluewakeControlsSnapshot keyboard{};
    std::vector<Profile> profiles;
};
Settings saved;
std::vector<Device> devices;
std::filesystem::path path;
uint32_t selected;
size_t profile_index;
bool ready, dirty, apply_keyboard = true, apply_controller = true;
bool legacy_swap_ab, legacy_swap_xy, legacy_invert_x, legacy_invert_y;
std::string error, status;

struct InputSnapshot {
    bool initialized = false;
    uint32_t device = 0;
    BluewakeControlsAxis axes[8]{};
    int32_t zoom_up = -1, zoom_down = -1;
    BluewakeControlsDeadZones zones{};
    bool keyboard_enabled = false;
    int32_t action_keys[BLUEWAKE_CONTROLS_ACTIONS][2]{};
    int32_t action_buttons[BLUEWAKE_CONTROLS_ACTIONS]{-1, -1, -1, -1};
    int32_t quick_items_trigger = -1;
    BluewakeControlsAxis triggers[2]{};
    int32_t native_keys[BLUEWAKE_CONTROLS_BUTTONS]{};
    int32_t native_buttons[BLUEWAKE_CONTROLS_BUTTONS]{};
};
std::mutex input_mutex;
InputSnapshot input_snapshot;
bool input_blocked, capture_held;
bool suppress_stick, suppress_camera, suppress_zoom_up, suppress_zoom_down;
BluewakeControlsActions actions;
bool capture_actions_held = true;
bool modifier_trigger_held;
bool suppressed_keys[BLUEWAKE_CONTROLS_ACTIONS][2]{}, previous_keys[BLUEWAKE_CONTROLS_ACTIONS][2]{}, pending_keys[BLUEWAKE_CONTROLS_ACTIONS][2]{};
bool suppressed_buttons[BLUEWAKE_CONTROLS_ACTIONS]{}, previous_buttons[BLUEWAKE_CONTROLS_ACTIONS]{}, pending_buttons[BLUEWAKE_CONTROLS_ACTIONS]{};

// Called with input_mutex held. The generation also invalidates a pending jump
// before guest dispatch resumes after a menu or profile change.
void invalidate_actions() {
    const auto generation = actions.generation + 1;
    actions = {};
    actions.generation = generation;
    actions.blocked = input_blocked;
    capture_actions_held = true;
    modifier_trigger_held = false;
    std::memset(previous_keys, 0, sizeof previous_keys);
    std::memset(pending_keys, 0, sizeof pending_keys);
    std::memset(previous_buttons, 0, sizeof previous_buttons);
    std::memset(pending_buttons, 0, sizeof pending_buttons);
    std::memset(suppressed_keys, 0, sizeof suppressed_keys);
    std::memset(suppressed_buttons, 0, sizeof suppressed_buttons);
}

void publish_input(const BluewakeControlsSnapshot* state) {
    std::lock_guard<std::mutex> lock(input_mutex);
    const auto old_device = input_snapshot.device;
    const auto old = input_snapshot;
    input_snapshot = {};
    input_snapshot.initialized = ready;
    input_snapshot.keyboard_enabled = saved.keyboard.keyboard_enabled;
    std::memcpy(input_snapshot.action_keys, saved.keyboard.action_keys, sizeof input_snapshot.action_keys);
    std::memcpy(input_snapshot.native_keys, saved.keyboard.key_buttons, sizeof input_snapshot.native_keys);
    if (state) {
        input_snapshot.device = selected;
        input_snapshot.zones = state->dead_zones;
        input_snapshot.zoom_up = state->controller_buttons[3];
        input_snapshot.zoom_down = state->controller_buttons[2];
        std::copy(std::begin(state->action_buttons), std::end(state->action_buttons), input_snapshot.action_buttons);
        input_snapshot.quick_items_trigger = state->quick_items_trigger;
        std::copy(state->controller_axes + 8, state->controller_axes + 10, input_snapshot.triggers);
        std::copy(std::begin(state->controller_buttons), std::end(state->controller_buttons), input_snapshot.native_buttons);
        if (legacy_swap_ab) std::swap(input_snapshot.native_buttons[7],input_snapshot.native_buttons[8]);
        if (legacy_swap_xy) std::swap(input_snapshot.native_buttons[9],input_snapshot.native_buttons[10]);
        const bool inverted[] = {state->invert_stick_x, state->invert_stick_y,
            state->invert_camera_x != legacy_invert_x, state->invert_camera_y != legacy_invert_y};
        for (unsigned i = 0; i < 8; ++i)
            input_snapshot.axes[i] = state->controller_axes[inverted[i / 2] ? i ^ 1u : i];
    }
    if (old.initialized != input_snapshot.initialized || old_device != input_snapshot.device ||
        old.keyboard_enabled != input_snapshot.keyboard_enabled ||
        std::memcmp(old.action_keys, input_snapshot.action_keys, sizeof old.action_keys) != 0 ||
        std::memcmp(old.action_buttons, input_snapshot.action_buttons, sizeof old.action_buttons) != 0 ||
        old.quick_items_trigger != input_snapshot.quick_items_trigger ||
        std::memcmp(old.triggers, input_snapshot.triggers, sizeof old.triggers) != 0 ||
        std::memcmp(old.native_keys, input_snapshot.native_keys, sizeof old.native_keys) != 0 ||
        std::memcmp(old.native_buttons, input_snapshot.native_buttons, sizeof old.native_buttons) != 0 ||
        std::memcmp(old.axes, input_snapshot.axes, sizeof old.axes) != 0 ||
        old.zones.enabled != input_snapshot.zones.enabled ||
        old.zones.stick != input_snapshot.zones.stick ||
        old.zones.emulate_triggers != input_snapshot.zones.emulate_triggers ||
        old.zones.trigger_left != input_snapshot.zones.trigger_left ||
        old.zones.trigger_right != input_snapshot.zones.trigger_right) {
        invalidate_actions();
    }
    // A replacement controller must release held input before taking over.
    if (old_device != input_snapshot.device) capture_held = true;
}

bool fail(const char* message) { error = message; return false; }
bool valid_key(int value) { return value >= PAD_KEY_MOUSE_X2 && value < SDL_SCANCODE_COUNT; }
bool valid_button(int value) { return value >= -1 && value < SDL_GAMEPAD_BUTTON_COUNT; }
bool valid_modifier_trigger(int value) {
    return value == -1 || value == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || value == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
}
bool valid_axis(const BluewakeControlsAxis& value) {
    return value.axis >= -1 && value.axis < SDL_GAMEPAD_AXIS_COUNT &&
        valid_button(value.button) && (value.sign == -1 || value.sign == 1) &&
        !(value.axis >= 0 && value.button >= 0) &&
        !((value.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || value.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) && value.sign < 0);
}
bool valid_zones(const BluewakeControlsDeadZones& value) {
    return value.stick <= 32767 && value.camera <= 32767 &&
        value.trigger_left <= 32767 && value.trigger_right <= 32767;
}
void keyboard_defaults(BluewakeControlsSnapshot& state) {
    state.keyboard_enabled = true;
    const int key_buttons[] = {SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_DOWN,
        SDL_SCANCODE_UP, SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_E, SDL_SCANCODE_J,
        SDL_SCANCODE_K, SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_RETURN};
    const int key_axes[] = {SDL_SCANCODE_D, SDL_SCANCODE_A, SDL_SCANCODE_W, SDL_SCANCODE_S,
        SDL_SCANCODE_H, SDL_SCANCODE_F, SDL_SCANCODE_T, SDL_SCANCODE_G, SDL_SCANCODE_E, SDL_SCANCODE_R};
    std::copy(std::begin(key_buttons), std::end(key_buttons), state.key_buttons);
    std::copy(std::begin(key_axes), std::end(key_axes), state.key_axes);
    state.action_keys[BLUEWAKE_ACTION_JUMP][0] = SDL_SCANCODE_SPACE;
    state.action_keys[BLUEWAKE_ACTION_JUMP][1] = PAD_KEY_INVALID;
    state.action_keys[BLUEWAKE_ACTION_SPRINT][0] = SDL_SCANCODE_LSHIFT;
    state.action_keys[BLUEWAKE_ACTION_SPRINT][1] = SDL_SCANCODE_RSHIFT;
    state.action_keys[BLUEWAKE_ACTION_FIRST_PERSON][0] = PAD_KEY_INVALID;
    state.action_keys[BLUEWAKE_ACTION_FIRST_PERSON][1] = PAD_KEY_INVALID;
    state.action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][0] = SDL_SCANCODE_TAB;
    state.action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][1] = PAD_KEY_INVALID;
    state.quick_items_trigger = -1;
}
void controller_defaults(BluewakeControlsSnapshot& state, uint16_t product = 0) {
    std::fill(std::begin(state.controller_buttons), std::end(state.controller_buttons), -1);
    for (auto& axis : state.controller_axes) axis = {-1, 1, -1};
    state.dead_zones = {true, true, 8000, 8000, 31150, 31150};
    state.invert_stick_x = state.invert_stick_y = false;
    state.invert_camera_x = state.invert_camera_y = false;
    state.action_buttons[BLUEWAKE_ACTION_JUMP] = product == 0x2073 ? -1 : SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    state.action_buttons[BLUEWAKE_ACTION_SPRINT] = SDL_GAMEPAD_BUTTON_LEFT_STICK;
    state.action_buttons[BLUEWAKE_ACTION_FIRST_PERSON] = SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS] = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    state.quick_items_trigger = -1;
}
void capture_keyboard() {
    u32 count = 0;
    if (const auto* maps = PADGetKeyButtonBindings(0, &count); maps && count == 12) {
        for (const auto& button : buttons) {
            auto at = std::find_if(maps, maps + count, [button](const auto& m) { return m.padButton == button; });
            if (at != maps + count && valid_key(at->scancode)) saved.keyboard.key_buttons[&button - buttons.data()] = at->scancode;
        }
    }
    if (const auto* maps = PADGetKeyAxisBindings(0, &count); maps && count == 10) {
        for (unsigned i = 0; i < 10; ++i) {
            auto at = std::find_if(maps, maps + count, [i](const auto& m) { return m.padAxis == i; });
            if (at != maps + count && valid_key(at->scancode)) saved.keyboard.key_axes[i] = at->scancode;
        }
    }
}
void capture_controller(BluewakeControlsSnapshot& state) {
    auto* pad = SDL_GetGamepadFromID(selected);
    controller_defaults(state, pad ? SDL_GetGamepadProduct(pad) : 0);
    u32 count = 0;
    if (const auto* maps = PADGetButtonMappings(0, &count); maps && count == 12) {
        for (unsigned i = 0; i < 12; ++i) {
            auto at = std::find_if(maps, maps + count, [i](const auto& m) { return m.padButton == buttons[i]; });
            if (at != maps + count) {
                int native = at->nativeButton == PAD_NATIVE_BUTTON_INVALID ? -1 : static_cast<int>(at->nativeButton);
                if (valid_button(native)) state.controller_buttons[i] = native;
            }
        }
    }
    if (const auto* maps = PADGetAxisMappings(0, &count); maps && count == 10) {
        for (unsigned i = 0; i < 10; ++i) {
            auto at = std::find_if(maps, maps + count, [i](const auto& m) { return m.padAxis == i; });
            if (at != maps + count) {
                BluewakeControlsAxis axis{at->nativeAxis.nativeAxis, static_cast<int>(at->nativeAxis.sign), at->nativeButton};
                if (valid_axis(axis)) state.controller_axes[i] = axis;
            }
        }
    }
    if (const auto* zone = PADGetDeadZones(0)) {
        BluewakeControlsDeadZones value{zone->useDeadzones, zone->emulateTriggers, zone->stickDeadZone,
            zone->substickDeadZone, zone->leftTriggerActivationZone, zone->rightTriggerActivationZone};
        if (valid_zones(value)) state.dead_zones = value;
    }
}
Profile* current_profile() { return selected && profile_index < saved.profiles.size() ? &saved.profiles[profile_index] : nullptr; }
void apply() {
    const bool keyboard_changed = apply_keyboard;
    if (apply_keyboard) {
        // These arrays remain valid even when the Controls panel blocks PADRead.
        PADSetKeyboardActive(0, TRUE);
        u32 count = 0;
        auto* keys = PADGetKeyButtonBindings(0, &count);
        if (keys && count == 12) for (unsigned i = 0; i < 12; ++i)
            keys[i] = {saved.keyboard.key_buttons[i], buttons[i]};
        auto* axes = PADGetKeyAxisBindings(0, &count);
        if (axes && count == 10) for (unsigned i = 0; i < 10; ++i)
            axes[i] = {saved.keyboard.key_axes[i], static_cast<PADAxis>(i), 32767};
        PADSetKeyboardActive(0, saved.keyboard.keyboard_enabled ? TRUE : FALSE);
        apply_keyboard = false;
    }
    if (apply_controller) if (const auto* profile = current_profile()) {
        const auto& state = profile->state;
        // Getters load Aurora's existing profile before applying custom changes.
        u32 count;
        (void)PADGetButtonMappings(0, &count);
        (void)PADGetAxisMappings(0, &count);
        for (unsigned i = 0; i < 12; ++i) {
            unsigned source = i;
            if (legacy_swap_ab && (i == 7 || i == 8)) source = i == 7 ? 8 : 7;
            if (legacy_swap_xy && (i == 9 || i == 10)) source = i == 9 ? 10 : 9;
            const int native = state.controller_buttons[source];
            PADSetButtonMapping(0, {native < 0 ? PAD_NATIVE_BUTTON_INVALID : static_cast<u32>(native), buttons[i]});
        }
        const bool invert[] = {state.invert_stick_x, state.invert_stick_y,
            state.invert_camera_x != legacy_invert_x, state.invert_camera_y != legacy_invert_y};
        for (unsigned i = 0; i < 10; ++i) {
            unsigned source = i < 8 && invert[i / 2] ? i ^ 1u : i;
            const auto& axis = state.controller_axes[source];
            PADSetAxisMapping(0, {{axis.axis, static_cast<PADAxisSign>(axis.sign)}, axis.button, static_cast<PADAxis>(i)});
        }
        if (auto* zones = PADGetDeadZones(0)) *zones = {state.dead_zones.emulate_triggers, state.dead_zones.enabled,
            state.dead_zones.stick, state.dead_zones.camera, state.dead_zones.trigger_left, state.dead_zones.trigger_right};
        publish_input(&state);
        apply_controller = false;
    }
    if (keyboard_changed) {
        const auto* profile = current_profile();
        publish_input(profile ? &profile->state : nullptr);
    }
}
std::string hex(const std::string& source) {
    constexpr char chars[] = "0123456789abcdef";
    std::string result;
    for (unsigned char c : source) { result += chars[c >> 4]; result += chars[c & 15]; }
    return result;
}
bool unhex(const std::string& text, std::string& result) {
    if (text.size() > 256 || text.size() % 2) return false;
    result.clear();
    for (size_t i = 0; i < text.size(); i += 2) {
        unsigned value;
        const auto parsed = std::from_chars(text.data() + i, text.data() + i + 2, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + i + 2 || value == 0) return false;
        result += static_cast<char>(value);
    }
    return true;
}
bool guid_valid(const std::string& text) {
    return text.size() == 32 && std::all_of(text.begin(), text.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
bool numbers(const std::string& text, int* values, size_t count) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    for (size_t i = 0; i < count; ++i) {
        const auto result = std::from_chars(begin, end, values[i]);
        if (result.ec != std::errc{} || result.ptr == begin) return false;
        begin = result.ptr;
        if (i + 1 < count) { if (begin == end || *begin++ != ',') return false; }
    }
    return begin == end;
}
template<size_t N> void write_numbers(std::ostream& out, const char* name, const int32_t (&values)[N]) {
    out << name << '=';
    for (size_t i = 0; i < N; ++i) out << (i ? "," : "") << values[i];
    out << '\n';
}
bool parse(std::istream& in, Settings& candidate, unsigned& migrated_f7) {
    unsigned global_seen = 0;
    std::vector<unsigned> profile_seen;
    int section = -2;
    int version = 0;
    migrated_f7 = 0;
    // F7 became a host diagnostic key after profiles already allowed it. Only
    // retire those slots; rejecting the file would discard unrelated mappings.
    auto load_key = [&](int value, int32_t& destination) {
        if (value == SDL_SCANCODE_F7) {
            destination = PAD_KEY_INVALID;
            ++migrated_f7;
            return true;
        }
        if (bluewake_controls_key_reserved(value)) return false;
        destination = value;
        return true;
    };
    keyboard_defaults(candidate.keyboard);
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() > 2048) return false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line == "[controls]") { if (section != -2) return false; section = -1; continue; }
        if (line.rfind("[controller ", 0) == 0 && line.back() == ']') {
            if (global_seen != (version >= 2 ? 255u : 127u) || candidate.profiles.size() >= 32) return false;
            const auto identity = line.substr(12, line.size() - 13);
            const auto separator = identity.find(':');
            if (separator == std::string::npos) return false;
            Profile profile;
            profile.guid = identity.substr(0, separator);
            if (!guid_valid(profile.guid) || !unhex(identity.substr(separator + 1), profile.serial)) return false;
            uint16_t product = 0;
            SDL_GetJoystickGUIDInfo(SDL_StringToGUID(profile.guid.c_str()), nullptr, &product, nullptr, nullptr);
            controller_defaults(profile.state, product);
            if (std::any_of(candidate.profiles.begin(), candidate.profiles.end(), [&](const auto& p) { return p.guid == profile.guid && p.serial == profile.serial; })) return false;
            candidate.profiles.push_back(profile); profile_seen.push_back(0);
            section = static_cast<int>(candidate.profiles.size() - 1);
            continue;
        }
        const auto equal = line.find('=');
        if (section == -2 || equal == std::string::npos) return false;
        const auto name = line.substr(0, equal), value = line.substr(equal + 1);
        int values[12]{};
        unsigned bit = 0;
        if (section == -1) {
            if (name == "version") { bit = 1; if (value != "1" && value != "2" && value != "3" && value != "4") return false; version = value[0] - '0'; }
            else if (name == "keyboard_enabled") { bit = 2; if (!numbers(value, values, 1) || (values[0] != 0 && values[0] != 1)) return false; candidate.keyboard.keyboard_enabled = values[0] != 0; }
            else if (name == "key_buttons") { bit = 4; if (!numbers(value, values, 12)) return false; for (unsigned i=0;i<12;++i) if (!load_key(values[i], candidate.keyboard.key_buttons[i])) return false; }
            else if (name == "key_axes") { bit = 8; if (!numbers(value, values, 10)) return false; for (unsigned i=0;i<10;++i) if (!load_key(values[i], candidate.keyboard.key_axes[i])) return false; }
            else if (name == "automatic") { bit = 16; if (!numbers(value, values, 1) || (values[0] != 0 && values[0] != 1)) return false; candidate.automatic = values[0] != 0; }
            else if (name == "preferred_guid") { bit = 32; if (!value.empty() && !guid_valid(value)) return false; candidate.preferred_guid=value; }
            else if (name == "preferred_serial") { bit = 64; if (!unhex(value, candidate.preferred_serial)) return false; }
            else if (name == "host_keys") {
                const unsigned count = version >= 3 ? BLUEWAKE_CONTROLS_ACTIONS * 2u : 6u;
                bit = 128; if (version < 2 || !numbers(value, values, count)) return false;
                for (unsigned i = 0; i < count; ++i) {
                    if (!load_key(values[i], candidate.keyboard.action_keys[i / 2][i % 2])) return false;
                }
            }
            if (!bit || global_seen & bit) return false;
            global_seen |= bit;
        } else {
            auto& state = candidate.profiles[section].state;
            if (name == "buttons") { bit = 1; if (!numbers(value, values, 12)) return false; for (unsigned i=0;i<12;++i) { if (!valid_button(values[i]) || values[i] == SDL_GAMEPAD_BUTTON_BACK) return false; state.controller_buttons[i]=values[i]; } }
            else if (name.rfind("axis", 0) == 0) {
                int slot;
                if (!numbers(name.substr(4), &slot, 1) || slot < 0 || slot >= 10 || !numbers(value, values, 3)) return false;
                BluewakeControlsAxis axis{values[0],values[1],values[2]}; if (!valid_axis(axis) || axis.button == SDL_GAMEPAD_BUTTON_BACK) return false;
                state.controller_axes[slot]=axis; bit=2u << slot;
            } else if (name == "dead_zones") {
                bit = 1u << 11;
                if (!numbers(value, values, 6) || (values[0]!=0 && values[0]!=1) || (values[1]!=0 && values[1]!=1)) return false;
                for (unsigned i=2;i<6;++i) if (values[i]<0 || values[i]>32767) return false;
                state.dead_zones={values[0]!=0,values[1]!=0,static_cast<uint16_t>(values[2]),static_cast<uint16_t>(values[3]),static_cast<uint16_t>(values[4]),static_cast<uint16_t>(values[5])};
            } else if (name == "invert") {
                bit=1u << 12; if (!numbers(value, values, 4)) return false;
                for (unsigned i=0;i<4;++i) if (values[i]!=0 && values[i]!=1) return false;
                state.invert_stick_x=values[0]!=0; state.invert_stick_y=values[1]!=0;
                state.invert_camera_x=values[2]!=0; state.invert_camera_y=values[3]!=0;
            } else if (name == "host_buttons") {
                const unsigned count = version >= 3 ? BLUEWAKE_CONTROLS_ACTIONS : 3u;
                bit=1u << 13; if (version < 2 || !numbers(value,values,count)) return false;
                for (unsigned i=0;i<count;++i) {
                    if (!valid_button(values[i]) || values[i] == SDL_GAMEPAD_BUTTON_BACK) return false;
                    state.action_buttons[i] = values[i];
                }
            } else if (name == "quick_items_trigger") {
                bit = 1u << 14;
                if (version < 4 || !numbers(value, values, 1) || !valid_modifier_trigger(values[0])) return false;
                state.quick_items_trigger = values[0];
            }
            if (!bit || profile_seen[section] & bit) return false;
            profile_seen[section] |= bit;
        }
    }
    return !in.bad() && version != 0 && global_seen == (version >= 2 ? 255u : 127u) && (candidate.automatic || !candidate.preferred_guid.empty()) &&
        std::all_of(profile_seen.begin(), profile_seen.end(), [version](unsigned seen) { return seen == (version >= 4 ? 32767u : version >= 2 ? 16383u : 8191u); }) &&
        std::all_of(candidate.profiles.begin(), candidate.profiles.end(), [](const auto& profile) {
            return profile.state.quick_items_trigger == -1 || profile.state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS] == -1;
        });
}
void changed(bool keyboard = false) {
    dirty = true; error.clear(); status = "Unsaved control changes";
    if (keyboard) apply_keyboard = true; else apply_controller = true;
    apply();
}

float mapped_axis(SDL_Gamepad* pad,const BluewakeControlsAxis& source) {
    if (!pad) return 0;
    if (source.button >= 0) return SDL_GetGamepadButton(pad,static_cast<SDL_GamepadButton>(source.button)) ? 1.0f : 0.0f;
    if (source.axis < 0) return 0;
    const int value = SDL_GetGamepadAxis(pad,static_cast<SDL_GamepadAxis>(source.axis));
    return std::clamp(static_cast<float>(value) * source.sign / 32767.0f,0.0f,1.0f);
}
float mapped_pair(SDL_Gamepad* pad,const InputSnapshot& state,unsigned i,uint16_t zone) {
    const float value = mapped_axis(pad,state.axes[i]) - mapped_axis(pad,state.axes[i+1]);
    return state.zones.enabled && std::abs(value) * 32767.0f <= zone ? 0.0f : value;
}
}

extern "C" {
bool bluewake_controls_init(const char* data_dir) {
    saved = {}; devices.clear(); selected=0; profile_index=0;
    ready=false; dirty=false; apply_keyboard=true; apply_controller=true; error.clear(); status.clear();
    if (!data_dir || !*data_dir) return fail("Controls need a private data folder");
    std::error_code ec;
#ifdef __cpp_char8_t
    path=std::filesystem::path(reinterpret_cast<const char8_t*>(data_dir)) / "controls.ini";
#else
    path=std::filesystem::u8path(data_dir) / "controls.ini";
#endif
    if (!std::filesystem::is_directory(path.parent_path(), ec)) return fail("Controls data folder is unavailable");
    keyboard_defaults(saved.keyboard); capture_keyboard(); ready=true;
    publish_input(nullptr);
    bool loaded=true;
    if (std::filesystem::exists(path, ec)) loaded=bluewake_controls_load();
    bluewake_controls_refresh();
    if (loaded && status.empty()) status="Controls ready";
    return loaded;
}
void bluewake_controls_refresh(void) {
    if (!ready) return;
    PADRefreshControllers();
    devices.clear();
    for (u32 i=0;i<PADCount();++i) {
        auto* pad=PADGetSDLGamepadForIndex(i);
        if (!pad || !SDL_GamepadConnected(pad)) continue;
        Device device; device.pad=pad; device.info.id=SDL_GetGamepadID(pad);
        const char* name=SDL_GetGamepadName(pad);
        std::snprintf(device.info.name,sizeof device.info.name,"%s",name ? name : "Controller");
        char guid[33]{}; SDL_GUIDToString(SDL_GetGamepadGUIDForID(device.info.id),guid,sizeof guid);
        device.guid=guid;
        const char* serial=SDL_GetGamepadSerial(pad); if (serial) device.serial=std::string(serial).substr(0,128);
        devices.push_back(device);
    }
    std::sort(devices.begin(),devices.end(),[](const auto& a,const auto& b){return a.info.id<b.info.id;});
    Device* choice=nullptr;
    if (!saved.automatic) {
        auto at=std::find_if(devices.begin(),devices.end(),[](const auto& d){return d.guid==saved.preferred_guid && d.serial==saved.preferred_serial;});
        if (at!=devices.end()) choice=&*at;
    }
    if (!choice) {
        auto at=std::find_if(devices.begin(),devices.end(),[](const auto& d){return d.info.id==selected;});
        if (at!=devices.end()) choice=&*at;
        if (!choice) { at=std::find_if(devices.begin(),devices.end(),[](const auto& d){return SDL_GetGamepadPlayerIndex(d.pad)==0;}); if(at!=devices.end())choice=&*at; }
        if (!choice && !devices.empty()) choice=&devices.front();
    }
    uint32_t next=choice ? choice->info.id : 0;
    if (next!=selected) { selected=next; apply_controller=true; }
    if (!choice && apply_controller) { publish_input(nullptr); apply_controller=false; }
    if (choice) {
        // Assign only runtime SDL ports. Aurora's single-player preference file
        // is untouched; the explicit selection lives in controls.ini.
        for (const auto& d:devices) if (d.info.id!=selected && SDL_GetGamepadPlayerIndex(d.pad)==0) SDL_SetGamepadPlayerIndex(d.pad,-1);
        if (SDL_GetGamepadPlayerIndex(choice->pad)!=0) SDL_SetGamepadPlayerIndex(choice->pad,0);
        auto at=std::find_if(saved.profiles.begin(),saved.profiles.end(),[&](const auto& p){return p.guid==choice->guid && p.serial==choice->serial;});
        if (at==saved.profiles.end()) {
            if (saved.profiles.size()>=32) {
                // A device without a profile must not inherit the previous
                // device's cached host actions, or take over the PAD port.
                SDL_SetGamepadPlayerIndex(choice->pad,-1);
                selected=0; profile_index=saved.profiles.size(); apply_controller=false;
                publish_input(nullptr); apply();
                error="Too many saved controller profiles"; return;
            }
            Profile profile; profile.guid=choice->guid; profile.serial=choice->serial;
            capture_controller(profile.state); saved.profiles.push_back(profile); profile_index=saved.profiles.size()-1;
        } else profile_index=static_cast<size_t>(at-saved.profiles.begin());
    }
    apply();
}
void bluewake_controls_snapshot(BluewakeControlsSnapshot* out) {
    if (!out) return;
    *out=saved.keyboard;
    if (const auto* profile=current_profile()) {
        const auto keyboard=*out; *out=profile->state;
        out->keyboard_enabled=keyboard.keyboard_enabled;
        std::copy(std::begin(keyboard.key_buttons),std::end(keyboard.key_buttons),out->key_buttons);
        std::copy(std::begin(keyboard.key_axes),std::end(keyboard.key_axes),out->key_axes);
        std::memcpy(out->action_keys, keyboard.action_keys, sizeof out->action_keys);
    } else controller_defaults(*out);
}
size_t bluewake_controls_devices(BluewakeControlsDevice* out,size_t capacity) {
    if(out)for(size_t i=0;i<std::min(capacity,devices.size());++i)out[i]=devices[i].info;
    return devices.size();
}
uint32_t bluewake_controls_selected(void){return selected;}
bool bluewake_controls_automatic(void){return saved.automatic;}
bool bluewake_controls_using_fallback(void){
    return !saved.automatic && std::none_of(devices.begin(),devices.end(),[](const auto& device){
        return device.info.id==selected && device.guid==saved.preferred_guid && device.serial==saved.preferred_serial;
    });
}
bool bluewake_controls_select(uint32_t id) {
    if(!ready)return fail("Controls are not initialized");
    const auto previous_automatic=saved.automatic;
    const auto previous_guid=saved.preferred_guid, previous_serial=saved.preferred_serial;
    const auto previous_selected=selected;
    if(id){auto at=std::find_if(devices.begin(),devices.end(),[id](const auto& d){return d.info.id==id;});
        if(at==devices.end())return fail("That controller is disconnected");
        saved.preferred_guid=at->guid;saved.preferred_serial=at->serial;}
    saved.automatic=id==0; selected=0; bluewake_controls_refresh();
    if(id && selected==0){
        const auto selection_error=error;
        saved.automatic=previous_automatic;saved.preferred_guid=previous_guid;saved.preferred_serial=previous_serial;
        selected=previous_selected;bluewake_controls_refresh();
        error=selection_error;return false;
    }
    changed(); return true;
}
bool bluewake_controls_set_keyboard_enabled(bool enabled){if(!ready)return fail("Controls are not initialized");saved.keyboard.keyboard_enabled=enabled;changed(true);return true;}
bool bluewake_controls_set_key(bool axis,unsigned slot,int32_t scancode){
    if(!ready)return fail("Controls are not initialized");
    if(slot>=(axis?10u:12u)||!valid_key(scancode)||bluewake_controls_key_reserved(scancode))return fail("Invalid or reserved key binding");
    (axis?saved.keyboard.key_axes:saved.keyboard.key_buttons)[slot]=scancode;changed(true);return true;
}
bool bluewake_controls_set_button(unsigned slot,int32_t native){
    auto* profile=current_profile();if(!profile)return fail("Connect a controller first");
    if(slot>=12||!valid_button(native)||native==SDL_GAMEPAD_BUTTON_BACK)return fail("Invalid or reserved controller button");
    profile->state.controller_buttons[slot]=native;changed();return true;
}
bool bluewake_controls_set_axis(unsigned slot,BluewakeControlsAxis binding){
    auto* profile=current_profile();if(!profile)return fail("Connect a controller first");
    if(slot>=10||!valid_axis(binding)||binding.button==SDL_GAMEPAD_BUTTON_BACK)return fail("Invalid or reserved controller axis");
    profile->state.controller_axes[slot]=binding;changed();return true;
}
bool bluewake_controls_set_dead_zones(BluewakeControlsDeadZones zones){
    auto* profile=current_profile();if(!profile)return fail("Connect a controller first");
    if(!valid_zones(zones))return fail("Dead zones must be between 0 and 32767");
    profile->state.dead_zones=zones;changed();return true;
}
bool bluewake_controls_set_invert(bool sx,bool sy,bool cx,bool cy){
    auto* profile=current_profile();if(!profile)return fail("Connect a controller first");
    auto& state=profile->state;state.invert_stick_x=sx;state.invert_stick_y=sy;
    state.invert_camera_x=cx;state.invert_camera_y=cy;changed();return true;
}
bool bluewake_controls_set_action_key(unsigned action,unsigned source,int32_t key){
    if (!ready) return fail("Controls are not initialized");
    if (action >= BLUEWAKE_CONTROLS_ACTIONS || source >= 2 || bluewake_controls_key_reserved(key)) return fail("Invalid or reserved action key");
    saved.keyboard.action_keys[action][source] = key; changed(true); return true;
}
bool bluewake_controls_set_action_button(unsigned action,int32_t button){
    auto* profile = current_profile(); if (!profile) return fail("Connect a controller first");
    if (action >= BLUEWAKE_CONTROLS_ACTIONS || !valid_button(button) || button == SDL_GAMEPAD_BUTTON_BACK) return fail("Invalid or reserved action button");
    profile->state.action_buttons[action] = button;
    if (action == BLUEWAKE_ACTION_QUICK_ITEMS) profile->state.quick_items_trigger = -1;
    changed(); return true;
}
bool bluewake_controls_set_quick_items_trigger(int32_t axis){
    auto* profile = current_profile(); if (!profile) return fail("Connect a controller first");
    if (!valid_modifier_trigger(axis)) return fail("Choose the left or right trigger");
    profile->state.quick_items_trigger = axis;
    profile->state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS] = -1;
    changed(); return true;
}
const char* bluewake_controls_action_name(unsigned action){return action < BLUEWAKE_CONTROLS_ACTIONS ? action_names[action] : "";}
void bluewake_controls_set_legacy_preferences(bool ab,bool xy,bool cx,bool cy){
    if(legacy_swap_ab==ab && legacy_swap_xy==xy && legacy_invert_x==cx && legacy_invert_y==cy)return;
    legacy_swap_ab=ab;legacy_swap_xy=xy;legacy_invert_x=cx;legacy_invert_y=cy;
    apply_controller=true;if(ready)apply();
}
bool bluewake_controls_has_legacy_preferences(void){return legacy_swap_ab || legacy_swap_xy || legacy_invert_x || legacy_invert_y;}
void bluewake_controls_set_input_blocked(bool blocked){
    std::lock_guard<std::mutex> lock(input_mutex);
    if (input_blocked == blocked) return;
    if(input_blocked && !blocked)capture_held=true;
    input_blocked=blocked;
    invalidate_actions();
}
void bluewake_controls_cancel_actions(void) {
    std::lock_guard<std::mutex> lock(input_mutex);
    invalidate_actions();
}
void bluewake_controls_action_event(const void* sdl_event){
    if (!sdl_event) return;
    const auto* event = static_cast<const SDL_Event*>(sdl_event);
    std::lock_guard<std::mutex> lock(input_mutex);
    if (!input_snapshot.initialized || input_blocked) return;
    int key = PAD_KEY_INVALID;
    bool key_event = false, down = false;
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
        if (event->key.repeat) return;
        key = event->key.scancode; key_event = true; down = event->type == SDL_EVENT_KEY_DOWN;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        key = -static_cast<int>(event->button.button) - 1; key_event = true;
        down = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    }
    if (key_event && input_snapshot.keyboard_enabled) {
        for (unsigned action = 0; action < BLUEWAKE_CONTROLS_ACTIONS; ++action) for (unsigned source = 0; source < 2; ++source) {
            if (input_snapshot.action_keys[action][source] != key || key == PAD_KEY_INVALID) continue;
            if (!down) { suppressed_keys[action][source] = false; previous_keys[action][source] = false; }
            else if (!suppressed_keys[action][source]) pending_keys[action][source] = true;
        }
    }
    if ((event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) &&
        event->gbutton.which == input_snapshot.device) {
        down = event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        for (unsigned action = 0; action < BLUEWAKE_CONTROLS_ACTIONS; ++action) {
            if (input_snapshot.action_buttons[action] != event->gbutton.button) continue;
            if (!down) { suppressed_buttons[action] = false; previous_buttons[action] = false; }
            else if (!suppressed_buttons[action]) pending_buttons[action] = true;
        }
    }
}
void bluewake_controls_retrace(void){
    std::lock_guard<std::mutex> lock(input_mutex);
    const auto generation = actions.generation;
    actions = {}; actions.generation = generation; actions.blocked = input_blocked;
    const auto& state = input_snapshot;
    if (!state.initialized || input_blocked) return;
    bool key_levels[BLUEWAKE_CONTROLS_ACTIONS][2]{}, pad_levels[BLUEWAKE_CONTROLS_ACTIONS]{};
    uint16_t modifier_trigger_buttons = 0;
    int key_count = 0;
    const bool* keys = SDL_GetKeyboardState(&key_count);
    const auto mouse = SDL_GetMouseState(nullptr,nullptr);
    if (state.keyboard_enabled) for (unsigned a = 0; a < BLUEWAKE_CONTROLS_ACTIONS; ++a) for (unsigned s = 0; s < 2; ++s) {
        const int key = state.action_keys[a][s];
        if (key > PAD_KEY_INVALID) key_levels[a][s] = keys && key < key_count && keys[key];
        else if (key < PAD_KEY_INVALID && key >= PAD_KEY_MOUSE_X2)
            key_levels[a][s] = (mouse & (1u << (-key - 2))) != 0;
    }
    SDL_LockJoysticks();
    auto* pad = state.device ? SDL_GetGamepadFromID(state.device) : nullptr;
    if (pad && SDL_GamepadConnected(pad)) {
        for (unsigned a = 0; a < BLUEWAKE_CONTROLS_ACTIONS; ++a) if (state.action_buttons[a] >= 0)
            pad_levels[a] = SDL_GetGamepadButton(pad,static_cast<SDL_GamepadButton>(state.action_buttons[a]));
        if (state.quick_items_trigger >= 0) {
            const int value = SDL_GetGamepadAxis(pad,static_cast<SDL_GamepadAxis>(state.quick_items_trigger));
            // A source already held on menu close/rebind must release first,
            // including the hysteresis interval below the activation threshold.
            if (capture_actions_held) modifier_trigger_held = value > BLUEWAKE_CONTROLS_TRIGGER_RELEASE;
            else if (value >= BLUEWAKE_CONTROLS_TRIGGER_PRESS) modifier_trigger_held = true;
            else if (value <= BLUEWAKE_CONTROLS_TRIGGER_RELEASE) modifier_trigger_held = false;
            pad_levels[BLUEWAKE_ACTION_QUICK_ITEMS] = modifier_trigger_held;
            if (state.zones.emulate_triggers) for (unsigned side = 0; side < 2; ++side) {
                const auto& native = state.triggers[side];
                const auto zone = side ? state.zones.trigger_right : state.zones.trigger_left;
                // Aurora emulates a digital L/R only when it has no button
                // mapping. Match the actual physical axis and current threshold.
                if (state.native_buttons[side ? 5 : 6] == -1 && native.button == -1 &&
                    native.axis == state.quick_items_trigger && native.sign == 1 && value > zone)
                    modifier_trigger_buttons |= cpad_buttons[side ? 5 : 6];
            }
        } else modifier_trigger_held = false;
        const auto x = mapped_pair(pad,state,0,state.zones.stick);
        const auto y = mapped_pair(pad,state,2,state.zones.stick);
        actions.movement_tilt = std::min(1.0f,std::sqrt(x*x + y*y));
    } else modifier_trigger_held = false;
    SDL_UnlockJoysticks();
    if (capture_actions_held) {
        std::memcpy(suppressed_keys,key_levels,sizeof suppressed_keys);
        std::memcpy(suppressed_buttons,pad_levels,sizeof suppressed_buttons);
        capture_actions_held = false;
    }
    bool key_held[BLUEWAKE_CONTROLS_ACTIONS]{}, key_pressed[BLUEWAKE_CONTROLS_ACTIONS]{}, pad_held[BLUEWAKE_CONTROLS_ACTIONS]{}, pad_pressed[BLUEWAKE_CONTROLS_ACTIONS]{};
    bool key_source_pressed[BLUEWAKE_CONTROLS_ACTIONS][2]{};
    for (unsigned a = 0; a < BLUEWAKE_CONTROLS_ACTIONS; ++a) {
        for (unsigned s = 0; s < 2; ++s) {
            suppressed_keys[a][s] = suppressed_keys[a][s] && key_levels[a][s];
            const bool effective = key_levels[a][s] && !suppressed_keys[a][s];
            key_held[a] = key_held[a] || effective;
            key_source_pressed[a][s] = (!suppressed_keys[a][s] && pending_keys[a][s]) ||
                (effective && !previous_keys[a][s]);
            key_pressed[a] = key_pressed[a] || key_source_pressed[a][s];
            previous_keys[a][s] = effective;
            pending_keys[a][s] = false;
        }
        suppressed_buttons[a] = suppressed_buttons[a] && pad_levels[a];
        pad_held[a] = pad_levels[a] && !suppressed_buttons[a];
        pad_pressed[a] = (!suppressed_buttons[a] && pending_buttons[a]) || (pad_held[a] && !previous_buttons[a]);
        previous_buttons[a] = pad_held[a]; pending_buttons[a] = false;
    }
    actions.jump_pressed = key_pressed[BLUEWAKE_ACTION_JUMP] || pad_pressed[BLUEWAKE_ACTION_JUMP];
    actions.sprint_held = key_held[BLUEWAKE_ACTION_SPRINT];
    actions.sprint_keyboard_pressed = key_pressed[BLUEWAKE_ACTION_SPRINT];
    actions.sprint_controller_down = pad_held[BLUEWAKE_ACTION_SPRINT];
    actions.sprint_pressed = pad_pressed[BLUEWAKE_ACTION_SPRINT];
    actions.first_person_down = key_held[BLUEWAKE_ACTION_FIRST_PERSON] || pad_held[BLUEWAKE_ACTION_FIRST_PERSON] ||
        key_pressed[BLUEWAKE_ACTION_FIRST_PERSON] || pad_pressed[BLUEWAKE_ACTION_FIRST_PERSON];
    actions.quick_items_down = key_held[BLUEWAKE_ACTION_QUICK_ITEMS] || pad_held[BLUEWAKE_ACTION_QUICK_ITEMS];
    if (pad_held[BLUEWAKE_ACTION_QUICK_ITEMS]) {
        const int source = state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS];
        if (source >= 0) for (unsigned i = 0; i < BLUEWAKE_CONTROLS_BUTTONS; ++i)
            if (state.native_buttons[i] == source) actions.quick_items_native_buttons |= cpad_buttons[i];
        actions.quick_items_native_buttons |= modifier_trigger_buttons;
    }
    for (unsigned s = 0; s < 2; ++s) {
        if (!key_levels[BLUEWAKE_ACTION_QUICK_ITEMS][s] || suppressed_keys[BLUEWAKE_ACTION_QUICK_ITEMS][s]) continue;
        const int source = state.action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][s];
        for (unsigned i = 0; i < BLUEWAKE_CONTROLS_BUTTONS; ++i)
            if (state.native_keys[i] == source) actions.quick_items_native_buttons |= cpad_buttons[i];
    }
    bool shared_jump = false, independent_jump = false;
    if (pad_pressed[BLUEWAKE_ACTION_JUMP]) {
        const bool shared = pad_held[BLUEWAKE_ACTION_QUICK_ITEMS] &&
            state.action_buttons[BLUEWAKE_ACTION_JUMP] == state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS];
        shared_jump |= shared; independent_jump |= !shared;
    }
    for (unsigned j = 0; j < 2; ++j) {
        if (!key_source_pressed[BLUEWAKE_ACTION_JUMP][j]) continue;
        bool shared = false;
        for (unsigned s = 0; s < 2; ++s)
            shared |= key_levels[BLUEWAKE_ACTION_QUICK_ITEMS][s] && !suppressed_keys[BLUEWAKE_ACTION_QUICK_ITEMS][s] &&
                state.action_keys[BLUEWAKE_ACTION_JUMP][j] == state.action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][s];
        shared_jump |= shared; independent_jump |= !shared;
    }
    actions.jump_modifier_conflict = shared_jump && !independent_jump;
}
bool bluewake_controls_read_actions(BluewakeControlsActions* out){
    if (!out) return false;
    std::lock_guard<std::mutex> lock(input_mutex);
    *out = actions; return input_snapshot.initialized;
}
bool bluewake_controls_read_controller(BluewakeControlsInput* out){
    if(!out)return false;
    *out={};
    std::lock_guard<std::mutex> lock(input_mutex);
    const auto& snapshot=input_snapshot;
    if(!snapshot.initialized)return false;
    if(input_blocked)return true;
    out->camera_click=actions.first_person_down;
    if(!snapshot.device)return true;
    SDL_LockJoysticks();
    auto* pad=SDL_GetGamepadFromID(snapshot.device);
    if(!pad || !SDL_GamepadConnected(pad)){SDL_UnlockJoysticks();return true;}
    const auto button=[&](int source){return source>=0 && SDL_GetGamepadButton(pad,static_cast<SDL_GamepadButton>(source));};
    out->stick_x=mapped_pair(pad,snapshot,0,snapshot.zones.stick);out->stick_y=mapped_pair(pad,snapshot,2,snapshot.zones.stick);
    out->camera_x=mapped_pair(pad,snapshot,4,snapshot.zones.camera);out->camera_y=mapped_pair(pad,snapshot,6,snapshot.zones.camera);
    const bool zoom_up=button(snapshot.zoom_up),zoom_down=button(snapshot.zoom_down);
    SDL_UnlockJoysticks();
    if(capture_held){
        suppress_stick=suppress_stick || out->stick_x!=0 || out->stick_y!=0;
        suppress_camera=suppress_camera || out->camera_x!=0 || out->camera_y!=0;
        suppress_zoom_up=suppress_zoom_up || zoom_up;suppress_zoom_down=suppress_zoom_down || zoom_down;
        capture_held=false;
    }
    if(suppress_stick){suppress_stick=out->stick_x!=0 || out->stick_y!=0;out->stick_x=out->stick_y=0;}
    if(suppress_camera){suppress_camera=out->camera_x!=0 || out->camera_y!=0;out->camera_x=out->camera_y=0;}
    suppress_zoom_up=suppress_zoom_up && zoom_up;suppress_zoom_down=suppress_zoom_down && zoom_down;
    out->zoom=(zoom_up && !suppress_zoom_up?1:0)-(zoom_down && !suppress_zoom_down?1:0);
    return true;
}
void bluewake_controls_reset_keyboard(void){if(!ready){fail("Controls are not initialized");return;}keyboard_defaults(saved.keyboard);changed(true);}
bool bluewake_controls_reset_controller(void){
    auto* profile=current_profile();if(!profile)return fail("Connect a controller first");
    PADRestoreDefaultMapping(0);capture_controller(profile->state);
    profile->state.dead_zones={true,PADIsGCAdapter(0)==FALSE,8000,8000,31150,31150};
    changed();return true;
}
bool bluewake_controls_save(void){
    if(!ready)return fail("Controls are not initialized");
    std::ostringstream text;
    text<<"[controls]\nversion=4\nkeyboard_enabled="<<saved.keyboard.keyboard_enabled<<'\n';
    write_numbers(text,"key_buttons",saved.keyboard.key_buttons);write_numbers(text,"key_axes",saved.keyboard.key_axes);
    text<<"automatic="<<saved.automatic<<"\npreferred_guid="<<saved.preferred_guid<<"\npreferred_serial="<<hex(saved.preferred_serial)<<'\n';
    text<<"host_keys=";
    for(unsigned i=0;i<BLUEWAKE_CONTROLS_ACTIONS*2u;++i)text<<(i?",":"")<<saved.keyboard.action_keys[i/2][i%2];
    text<<'\n';
    for(const auto& profile:saved.profiles){
        const auto& state=profile.state;text<<"\n[controller "<<profile.guid<<':'<<hex(profile.serial)<<"]\n";
        write_numbers(text,"buttons",state.controller_buttons);
        for(unsigned i=0;i<10;++i){const auto& axis=state.controller_axes[i];text<<"axis"<<i<<'='<<axis.axis<<','<<axis.sign<<','<<axis.button<<'\n';}
        const auto& zone=state.dead_zones;
        text<<"dead_zones="<<zone.enabled<<','<<zone.emulate_triggers<<','<<zone.stick<<','<<zone.camera<<','<<zone.trigger_left<<','<<zone.trigger_right<<'\n';
        text<<"invert="<<state.invert_stick_x<<','<<state.invert_stick_y<<','<<state.invert_camera_x<<','<<state.invert_camera_y<<'\n';
        write_numbers(text,"host_buttons",state.action_buttons);
        text<<"quick_items_trigger="<<state.quick_items_trigger<<'\n';
    }
    const auto bytes=text.str();
    // Each process owns an exclusively created candidate. Two running games
    // may share controls.ini, but cannot truncate or rename each other's bytes.
    static std::atomic<uint64_t> sequence{0};
    std::filesystem::path temporary;
    bool owned=false, written=false, replaced=false;
#ifdef _WIN32
    DWORD io_error=ERROR_SUCCESS;
    HANDLE file=INVALID_HANDLE_VALUE;
    for(unsigned attempt=0;attempt<16;++attempt) {
        temporary=path;
        temporary+=".tmp-"+std::to_string(GetCurrentProcessId())+"-"+
            std::to_string(sequence.fetch_add(1,std::memory_order_relaxed));
        file=CreateFileW(temporary.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,
                         nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE) {owned=true;break;}
        io_error=GetLastError();
        if(io_error!=ERROR_FILE_EXISTS&&io_error!=ERROR_ALREADY_EXISTS)break;
    }
    if(owned) {
        written=true;
        for(size_t offset=0;offset<bytes.size();) {
            DWORD count=0;
            const DWORD remaining=static_cast<DWORD>(std::min<size_t>(bytes.size()-offset,MAXDWORD));
            const BOOL wrote=WriteFile(file,bytes.data()+offset,remaining,&count,nullptr);
            if(!wrote||count==0) {
                io_error=!wrote?GetLastError():ERROR_WRITE_FAULT;written=false;break;
            }
            offset+=count;
        }
        if(written&&!FlushFileBuffers(file)) {io_error=GetLastError();written=false;}
        if(!CloseHandle(file)&&written) {io_error=GetLastError();written=false;}
    }
    if(written) {
        constexpr DWORD delay[]={10,20,40,80,100};
        for(unsigned attempt=0;;++attempt) {
            if(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
                replaced=true;break;
            }
            io_error=GetLastError();
            bool retry=io_error==ERROR_SHARING_VIOLATION||io_error==ERROR_LOCK_VIOLATION;
            if(io_error==ERROR_ACCESS_DENIED) {
                // Windows also reports a target's open reader as ACCESS_DENIED.
                // Do not retry read-only files, directories, or missing paths.
                const DWORD source=GetFileAttributesW(temporary.c_str()), target=GetFileAttributesW(path.c_str());
                retry=source!=INVALID_FILE_ATTRIBUTES&&target!=INVALID_FILE_ATTRIBUTES&&
                    !(source&(FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_DIRECTORY))&&
                    !(target&(FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_DIRECTORY));
            }
            if(!retry||attempt>=std::size(delay))break;
            Sleep(delay[attempt]);
        }
    }
#else
    int io_error=0, descriptor=-1;
    for(unsigned attempt=0;attempt<16;++attempt) {
        temporary=path;
        temporary+=".tmp-"+std::to_string(getpid())+"-"+
            std::to_string(sequence.fetch_add(1,std::memory_order_relaxed));
        descriptor=open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
        if(descriptor>=0) {owned=true;break;}
        io_error=errno;if(io_error!=EEXIST)break;
    }
    if(owned) {
        FILE* file=fdopen(descriptor,"wb");
        if(file) {
            written=fwrite(bytes.data(),1,bytes.size(),file)==bytes.size()&&
                fflush(file)==0&&fsync(descriptor)==0;
            if(!written)io_error=errno;
            if(fclose(file)!=0) {if(written)io_error=errno;written=false;}
        } else {io_error=errno;close(descriptor);}
    }
    if(written) {replaced=std::rename(temporary.c_str(),path.c_str())==0;if(!replaced)io_error=errno;}
#endif
    if(!replaced){
        if(owned){std::error_code ec;std::filesystem::remove(temporary,ec);}
        std::string message=written?"Could not replace controls.ini; previous controls were preserved":
            "Could not write controls.ini; previous controls were preserved";
#ifdef _WIN32
        if(io_error)message+=" (Windows error "+std::to_string(io_error)+")";
#else
        if(io_error)message+=" (errno "+std::to_string(io_error)+")";
#endif
        return fail(message.c_str());
    }
    dirty=false;error.clear();status="Controls saved";return true;
}
bool bluewake_controls_load(void){
    if(!ready)return fail("Controls are not initialized");
#ifdef _WIN32
    // An atomic replacement may already hold delete access. Read one bounded
    // file object with explicit sharing, then close it before parsing/applying
    // bindings. A replacement cannot splice two profiles into this snapshot.
    struct Reader {
        HANDLE handle=INVALID_HANDLE_VALUE;
        ~Reader(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
    } reader;
    DWORD read_error=0;
    constexpr DWORD delay[]={10,20,40,80,100};
    for(unsigned attempt=0;;++attempt) {
        reader.handle=CreateFileW(path.c_str(),GENERIC_READ,
            FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,nullptr);
        if(reader.handle!=INVALID_HANDLE_VALUE)break;
        read_error=GetLastError();
        if((read_error!=ERROR_SHARING_VIOLATION&&read_error!=ERROR_LOCK_VIOLATION&&
            read_error!=ERROR_DELETE_PENDING&&read_error!=ERROR_FILE_NOT_FOUND)||
           attempt>=std::size(delay))break;
        Sleep(delay[attempt]);
    }
    if(reader.handle==INVALID_HANDLE_VALUE) {
        std::string message="Could not read controls.ini";
        if(read_error)message+=" (Windows error "+std::to_string(read_error)+")";
        return fail(message.c_str());
    }
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(reader.handle,&size))return fail("Could not size controls.ini");
    if(size.QuadPart<0||size.QuadPart>262144)return fail("Invalid controls.ini size");
    std::string bytes(static_cast<size_t>(size.QuadPart),'\0');
    size_t offset=0;
    while(offset<bytes.size()) {
        DWORD count=0;
        if(!ReadFile(reader.handle,bytes.data()+offset,static_cast<DWORD>(bytes.size()-offset),&count,nullptr)||
           count==0)return fail("Could not read complete controls.ini; current controls were preserved");
        offset+=count;
    }
    const bool closed=CloseHandle(reader.handle)!=FALSE;
    reader.handle=INVALID_HANDLE_VALUE;
    if(!closed)return fail("Could not close controls.ini; current controls were preserved");
    std::istringstream file(bytes);
#else
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)return fail("Could not read controls.ini");
    const auto size=file.tellg();if(size<0||size>262144)return fail("Invalid controls.ini size");file.seekg(0);
#endif
    Settings candidate;
    unsigned migrated_f7 = 0;
    if(!parse(file,candidate,migrated_f7))return fail("Invalid controls.ini; current controls were preserved");
    saved=std::move(candidate);selected=0;dirty=migrated_f7!=0;apply_keyboard=true;apply_controller=true;
    error.clear();status=migrated_f7 ? "Controls loaded; old F7 bindings are now Unbound (F7 marks slowdowns)" : "Controls loaded";
    bluewake_controls_refresh();
    // Reloading identical mappings must still require held host actions to release.
    bluewake_controls_cancel_actions();return true;
}
bool bluewake_controls_dirty(void){return dirty;}
const char* bluewake_controls_error(void){return error.c_str();}
const char* bluewake_controls_status(void){return status.c_str();}
const char* bluewake_controls_button_name(unsigned slot){return slot<12?button_names[slot]:"";}
const char* bluewake_controls_axis_name(unsigned slot){return slot<10?axis_names[slot]:"";}
bool bluewake_controls_key_reserved(int32_t scancode){return !valid_key(scancode)||scancode==SDL_SCANCODE_UNKNOWN||scancode==SDL_SCANCODE_ESCAPE||scancode==SDL_SCANCODE_F1||scancode==SDL_SCANCODE_F5||scancode==SDL_SCANCODE_F6||scancode==SDL_SCANCODE_F7||scancode==SDL_SCANCODE_F8||scancode==SDL_SCANCODE_F9||scancode==SDL_SCANCODE_F10||scancode==SDL_SCANCODE_F11;}
}
