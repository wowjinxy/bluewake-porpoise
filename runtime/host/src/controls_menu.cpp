// SPDX-License-Identifier: GPL-3.0-or-later
// Local input remapping shared by the Windows and Linux F1 settings menus.
#include "controls_menu.h"
#include "controls_bindings.h"

#include <SDL3/SDL.h>
#include <dolphin/pad.h>
#include <imgui.h>
#include <cstdarg>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {
bool menu_open;
bool capturing;
bool capture_armed;
BluewakeControlsCapture capture_kind;
unsigned capture_slot;
uint32_t capture_device;
std::string message;
std::string save_error;
bool retry_save;
bool retry_clock_armed;
uint64_t retry_at;

void saved_bindings() {
    retry_save = false;
    retry_clock_armed = false;
    save_error.clear();
}

bool save_bindings() {
    // Serialize the current profiles on every attempt. A queued retry never
    // retains old bytes that could overwrite a newer edit or explicit reload.
    const bool saved = bluewake_controls_save();
    if (saved) {
        saved_bindings();
        message = "Bindings saved.";
    } else {
        save_error = bluewake_controls_error();
        if (save_error.empty()) save_error = "Bindings could not be saved.";
        retry_save = bluewake_controls_dirty();
        retry_clock_armed = false;
    }
    return saved;
}

bool keyboard_capture() {
    return capture_kind == BLUEWAKE_CAPTURE_KEY_BUTTON || capture_kind == BLUEWAKE_CAPTURE_KEY_AXIS ||
        capture_kind == BLUEWAKE_CAPTURE_KEY_ACTION;
}

const char* action_name(bool axis, unsigned slot) {
    return axis ? bluewake_controls_axis_name(slot) : bluewake_controls_button_name(slot);
}

const char* capture_name() {
    if (capture_kind == BLUEWAKE_CAPTURE_KEY_ACTION) return bluewake_controls_action_name(capture_slot / 2);
    if (capture_kind == BLUEWAKE_CAPTURE_CONTROLLER_ACTION) return bluewake_controls_action_name(capture_slot);
    return action_name(capture_kind == BLUEWAKE_CAPTURE_KEY_AXIS || capture_kind == BLUEWAKE_CAPTURE_CONTROLLER_AXIS,capture_slot);
}

bool bind_key(int32_t key) {
    if (capture_kind == BLUEWAKE_CAPTURE_KEY_ACTION)
        return bluewake_controls_set_action_key(capture_slot / 2,capture_slot % 2,key);
    return bluewake_controls_set_key(capture_kind == BLUEWAKE_CAPTURE_KEY_AXIS,capture_slot,key);
}

const char* key_name(int32_t scancode) {
    switch (scancode) {
    case PAD_KEY_MOUSE_LEFT: return "Mouse left";
    case PAD_KEY_MOUSE_MIDDLE: return "Mouse middle";
    case PAD_KEY_MOUSE_RIGHT: return "Mouse right";
    case PAD_KEY_MOUSE_X1: return "Mouse button 4";
    case PAD_KEY_MOUSE_X2: return "Mouse button 5";
    }
    if (scancode < 0) return "Unbound";
    const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode));
    return name && *name ? name : "Unknown key";
}

const char* button_name(int32_t button) {
    if (button < 0) return "Unbound";
    const char* name = SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(button));
    return name && *name ? name : "Unknown button";
}

std::string axis_name(BluewakeControlsAxis binding) {
    if (binding.button >= 0) return std::string("Button: ") + button_name(binding.button);
    if (binding.axis < 0) return "Unbound";
    constexpr const char* names[] = {"Left stick X", "Left stick Y", "Right stick X",
                                   "Right stick Y", "Left trigger", "Right trigger"};
    const char* name = binding.axis < SDL_GAMEPAD_AXIS_COUNT ? names[binding.axis] : "Unknown axis";
    return std::string(name) + (binding.sign < 0 ? " -" : " +");
}

bool controller_neutral(SDL_Gamepad* pad) {
    if (!pad) return false;
    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
        if (SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(i))) return false;
    for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i) {
        const int value = SDL_GetGamepadAxis(pad, static_cast<SDL_GamepadAxis>(i));
        // Some device mappings expose trigger rest as negative; only their
        // pressed, positive half is a capture source.
        if (i >= SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? value > 12000 : std::abs(value) > 12000)
            return false;
    }
    return true;
}

void arm_capture_if_released() {
    if (!capturing || capture_armed) return;
    if (keyboard_capture()) {
        int count = 0;
        const bool* keys = SDL_GetKeyboardState(&count);
        if (!keys) return;
        for (int i = 1; i < count; ++i)
            if (keys[i] && !bluewake_controls_key_reserved(i)) return;
        if (SDL_GetMouseState(nullptr, nullptr) != 0) return;
    } else if (!controller_neutral(SDL_GetGamepadFromID(capture_device))) {
        return;
    }
    capture_armed = true;
}

void complete_capture(bool accepted) {
    if (accepted) {
        capturing = false;
        message = "Binding changed. Saved when you close settings.";
    } else {
        const char* error = bluewake_controls_error();
        message = error && *error ? error : "This binding could not be applied.";
    }
}

bool compact_rows = false;
void row_text(const char* format, ...) {
    va_list args; va_start(args, format);
    if (compact_rows) ImGui::PushTextWrapPos();
    ImGui::TextV(format, args);
    if (compact_rows) ImGui::PopTextWrapPos();
    va_end(args);
}
void row_plain_text(const char* text) {
    if (compact_rows) ImGui::TextWrapped("%s", text);
    else ImGui::TextUnformatted(text);
}
bool row_checkbox(const char* label, bool* value) {
    if (!compact_rows) return ImGui::Checkbox(label, value);
    ImGui::PushID(label); ImGui::TextWrapped("%s", label);
    const bool changed = ImGui::Checkbox("##toggle", value);
    ImGui::PopID(); return changed;
}

void binding_rows(bool keyboard, bool axis, const BluewakeControlsSnapshot& snapshot) {
    const unsigned count = axis ? BLUEWAKE_CONTROLS_AXES : BLUEWAKE_CONTROLS_BUTTONS;
    if (!compact_rows && !ImGui::BeginTable(axis ? "axes" : "buttons", 3,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) return;
    if (!compact_rows) {
        ImGui::TableSetupColumn("Game action", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
    }
    for (unsigned slot = 0; slot < count; ++slot) {
        ImGui::PushID(static_cast<int>(slot));
        if (!compact_rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
        }
        if (compact_rows) ImGui::TextWrapped("%s", action_name(axis, slot));
        else row_plain_text(action_name(axis, slot));
        if (!compact_rows) ImGui::TableNextColumn();
        if (keyboard) row_plain_text(key_name(axis ? snapshot.key_axes[slot] : snapshot.key_buttons[slot]));
        else if (axis) row_plain_text(axis_name(snapshot.controller_axes[slot]).c_str());
        else row_plain_text(button_name(snapshot.controller_buttons[slot]));
        if (!compact_rows) ImGui::TableNextColumn();
        if (ImGui::Button("Bind")) {
            bluewake_controls_menu_begin_capture(keyboard ?
                (axis ? BLUEWAKE_CAPTURE_KEY_AXIS : BLUEWAKE_CAPTURE_KEY_BUTTON) :
                (axis ? BLUEWAKE_CAPTURE_CONTROLLER_AXIS : BLUEWAKE_CAPTURE_CONTROLLER_BUTTON), slot);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            const bool accepted = keyboard ? bluewake_controls_set_key(axis, slot, -1) :
                axis ? bluewake_controls_set_axis(slot, {-1, 1, -1}) : bluewake_controls_set_button(slot, -1);
            if (!accepted) message = bluewake_controls_error();
        }
        ImGui::PopID();
    }
    if (!compact_rows) ImGui::EndTable();
}

void action_rows(bool keyboard,const BluewakeControlsSnapshot& state) {
    ImGui::SeparatorText("BlueWake actions");
    if (!compact_rows && !ImGui::BeginTable("host-actions",3,ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) return;
    if (!compact_rows) {
        ImGui::TableSetupColumn("Action");
        ImGui::TableSetupColumn("Binding");
        ImGui::TableSetupColumn("Change");
        ImGui::TableHeadersRow();
    }
    for (unsigned i = 0; i < (keyboard ? BLUEWAKE_CONTROLS_ACTIONS * 2u : BLUEWAKE_CONTROLS_ACTIONS); ++i) {
        const unsigned action = keyboard ? i / 2 : i;
        ImGui::PushID(static_cast<int>(i));
        if (!compact_rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
        }
        row_text("%s%s",bluewake_controls_action_name(action),keyboard && i % 2 ? " (alternate)" : "");
        if (!compact_rows) ImGui::TableNextColumn();
        row_plain_text(keyboard ? key_name(state.action_keys[action][i % 2]) : button_name(state.action_buttons[action]));
        if (!compact_rows) ImGui::TableNextColumn();
        if (ImGui::Button("Bind")) bluewake_controls_menu_begin_capture(keyboard ? BLUEWAKE_CAPTURE_KEY_ACTION : BLUEWAKE_CAPTURE_CONTROLLER_ACTION,i);
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            const bool okay = keyboard ? bluewake_controls_set_action_key(action,i % 2,PAD_KEY_INVALID) : bluewake_controls_set_action_button(action,-1);
            if (!okay) message = bluewake_controls_error();
        }
        ImGui::PopID();
    }
    if (!compact_rows) ImGui::EndTable();
    ImGui::TextWrapped("Jump and Sprint use the movement enhancements. First-person view uses the fast stick camera. Hold the D-pad shortcut modifier: Up for Wind Waker, Left for the cannon at sea, Right for the salvage crane at sea. Down stays native. Item assignments stay unchanged. Bindings do not enable these options.");
}

void draw_devices() {
    const size_t total = bluewake_controls_devices(nullptr, 0);
    std::vector<BluewakeControlsDevice> devices(std::min(total, size_t{64}));
    bluewake_controls_devices(devices.data(), devices.size());
    const uint32_t selected = bluewake_controls_selected();
    const bool automatic = bluewake_controls_automatic();
    const bool fallback = bluewake_controls_using_fallback();
    std::string current = automatic ? "Automatic" : "Selected controller disconnected";
    for (const auto& device : devices)
        if (device.id == selected) current = (automatic ? "Automatic: " : fallback ? "Fallback: " : "") + std::string(device.name);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##local-controller", current.c_str())) {
        if (ImGui::Selectable("Automatic (first available controller)", automatic)) {
            if (bluewake_controls_select(0)) message.clear();
            else message = bluewake_controls_error();
        }
        for (const auto& device : devices) {
            ImGui::PushID(static_cast<int>(device.id));
            const std::string label = std::string(device.name) + " (" + std::to_string(device.id) + ")";
            if (ImGui::Selectable(label.c_str(), !automatic && !fallback && selected == device.id)) {
                if (bluewake_controls_select(device.id)) message.clear();
                else message = bluewake_controls_error();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::TextWrapped("%s", bluewake_controls_status());
    if (fallback) ImGui::TextWrapped("Your preferred controller is disconnected. It will take over again when it reconnects.");
    if (!selected) ImGui::TextWrapped("No active controller. Keyboard bindings remain available below.");
    if (total > devices.size()) ImGui::TextDisabled("Showing the first %zu controllers.", devices.size());
}

void draw_live_input() {
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    std::string pressed_keys;
    if (keys) for (int i = 1, shown = 0; i < count; ++i) {
        if (!keys[i]) continue;
        if (!pressed_keys.empty()) pressed_keys += ", ";
        if (++shown > 16) { pressed_keys += "..."; break; }
        pressed_keys += key_name(i);
    }
    ImGui::TextWrapped("Live keyboard: %s", pressed_keys.empty() ? "none" : pressed_keys.c_str());
    SDL_Gamepad* pad = SDL_GetGamepadFromID(bluewake_controls_selected());
    if (!pad) return;
    std::string pressed;
    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i) {
        if (!SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(i))) continue;
        if (!pressed.empty()) pressed += ", ";
        pressed += button_name(i);
    }
    ImGui::TextWrapped("Live buttons: %s", pressed.empty() ? "none" : pressed.c_str());
    row_text("Left stick: %d, %d   Right stick: %d, %d",
        SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX), SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY),
        SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX), SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY));
    row_text("Triggers: %d, %d", SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER),
        SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
}

bool dead_zone_slider(const char* name, uint16_t& value) {
    int percent = static_cast<int>((static_cast<unsigned>(value) * 100u + 16383u) / 32767u);
    ImGui::SetNextItemWidth(compact_rows ? -1.0f : ImGui::GetFontSize() * 14);
    if (compact_rows) ImGui::TextWrapped("%s", name);
    if (!ImGui::SliderInt(compact_rows ? (std::string("##") + name).c_str() : name, &percent, 0, 95, "%d%%")) return false;
    value = static_cast<uint16_t>((percent * 32767 + 50) / 100);
    return true;
}
} // namespace

extern "C" void bluewake_controls_menu_set_open(bool open) {
    if (menu_open == open) return;
    menu_open = open;
    bluewake_controls_set_input_blocked(open);
    retry_clock_armed = false;
    if (!open) {
        bluewake_controls_menu_cancel_capture();
        if (bluewake_controls_dirty()) save_bindings();
        else saved_bindings();
    }
    PADBlockInput(open);
}

extern "C" void bluewake_controls_menu_tick(uint64_t now_ms) {
    if (!retry_save) return;
    if (!bluewake_controls_dirty()) {
        saved_bindings(); // A separate successful save/reload resolved the failure.
        return;
    }
    if (menu_open || capturing) {
        retry_clock_armed = false;
        return;
    }
    if (!retry_clock_armed || now_ms < retry_at) {
        retry_at = now_ms;
        retry_clock_armed = true;
        return;
    }
    if (now_ms - retry_at < 1000) return;
    save_bindings();
    // Each failed attempt starts a fresh throttle interval, including when the
    // caller's clock rolls back. No retry loop or extra sleep is introduced here.
    if (retry_save) {
        retry_at = now_ms;
        retry_clock_armed = true;
    }
}

extern "C" const char* bluewake_controls_menu_save_error(void) {
    return save_error.c_str();
}

extern "C" bool bluewake_controls_menu_is_open(void) { return menu_open; }
extern "C" bool bluewake_controls_menu_capturing(void) { return capturing; }

extern "C" bool bluewake_controls_menu_begin_capture(BluewakeControlsCapture kind, unsigned slot) {
    if (!menu_open || kind < BLUEWAKE_CAPTURE_KEY_BUTTON || kind > BLUEWAKE_CAPTURE_CONTROLLER_ACTION) return false;
    const bool axis = kind == BLUEWAKE_CAPTURE_KEY_AXIS || kind == BLUEWAKE_CAPTURE_CONTROLLER_AXIS;
    const unsigned count = kind == BLUEWAKE_CAPTURE_KEY_ACTION ? BLUEWAKE_CONTROLS_ACTIONS * 2u :
        kind == BLUEWAKE_CAPTURE_CONTROLLER_ACTION ? BLUEWAKE_CONTROLS_ACTIONS : axis ? BLUEWAKE_CONTROLS_AXES : BLUEWAKE_CONTROLS_BUTTONS;
    if (slot >= count) return false;
    const uint32_t selected = bluewake_controls_selected();
    if ((kind == BLUEWAKE_CAPTURE_CONTROLLER_BUTTON || kind == BLUEWAKE_CAPTURE_CONTROLLER_AXIS ||
         kind == BLUEWAKE_CAPTURE_CONTROLLER_ACTION) && !selected) {
        message = "Connect and select a controller before binding its inputs.";
        return false;
    }
    capture_kind = kind;
    capture_slot = slot;
    capture_device = selected;
    capturing = true;
    capture_armed = false;
    message.clear();
    arm_capture_if_released();
    return true;
}

extern "C" void bluewake_controls_menu_cancel_capture(void) {
    if (!capturing) return;
    capturing = false;
    message = "Binding cancelled.";
}

extern "C" void bluewake_controls_menu_reject_reserved_key(void) {
    if (capturing) message = "This key is reserved for a BlueWake hotkey. Choose another key, or press Escape to cancel.";
}

extern "C" bool bluewake_controls_menu_event(const void* sdl_event) {
    if (!menu_open || !sdl_event) return false;
    const auto* event = static_cast<const SDL_Event*>(sdl_event);
    if (!capturing) return false;
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_ESCAPE) {
        bluewake_controls_menu_cancel_capture();
        return true;
    }
    if (!keyboard_capture() && (bluewake_controls_selected() != capture_device ||
        !SDL_GetGamepadFromID(capture_device))) {
        bluewake_controls_menu_cancel_capture();
        message = "The selected controller disconnected or changed. Choose it again to bind.";
        return true;
    }
    arm_capture_if_released();
    if (!capture_armed) return true;
    if (keyboard_capture() && event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        int source = PAD_KEY_INVALID;
        switch (event->button.button) {
        case SDL_BUTTON_LEFT: source = PAD_KEY_MOUSE_LEFT; break;
        case SDL_BUTTON_MIDDLE: source = PAD_KEY_MOUSE_MIDDLE; break;
        case SDL_BUTTON_RIGHT: source = PAD_KEY_MOUSE_RIGHT; break;
        case SDL_BUTTON_X1: source = PAD_KEY_MOUSE_X1; break;
        case SDL_BUTTON_X2: source = PAD_KEY_MOUSE_X2; break;
        }
        if (source != PAD_KEY_INVALID)
            complete_capture(bind_key(source));
    } else if (keyboard_capture() && event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        if (bluewake_controls_key_reserved(event->key.scancode) ||
            (event->key.scancode == SDL_SCANCODE_RETURN && (event->key.mod & SDL_KMOD_ALT))) {
            bluewake_controls_menu_reject_reserved_key();
        } else {
            complete_capture(bind_key(event->key.scancode));
        }
    } else if (!keyboard_capture() && event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
               event->gbutton.which == capture_device) {
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
            message = "Back/Share opens settings. Choose another button.";
        } else if (capture_kind == BLUEWAKE_CAPTURE_CONTROLLER_BUTTON) {
            complete_capture(bluewake_controls_set_button(capture_slot, event->gbutton.button));
        } else if (capture_kind == BLUEWAKE_CAPTURE_CONTROLLER_ACTION) {
            complete_capture(bluewake_controls_set_action_button(capture_slot,event->gbutton.button));
        } else {
            complete_capture(bluewake_controls_set_axis(capture_slot, {-1, 1, event->gbutton.button}));
        }
    } else if (capture_kind == BLUEWAKE_CAPTURE_CONTROLLER_AXIS &&
               event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event->gaxis.which == capture_device) {
        const int value = event->gaxis.value;
        const bool trigger = event->gaxis.axis >= SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        if (value >= 16384 || (!trigger && value <= -16384))
            complete_capture(bluewake_controls_set_axis(capture_slot,
                {event->gaxis.axis, value < 0 ? -1 : 1, -1}));
    }
    return true;
}

extern "C" void bluewake_controls_menu_draw(void) {
    bluewake_controls_refresh();
    if (capturing) {
        arm_capture_if_released();
        const bool axis = capture_kind == BLUEWAKE_CAPTURE_KEY_AXIS || capture_kind == BLUEWAKE_CAPTURE_CONTROLLER_AXIS;
        ImGui::TextWrapped("Binding: %s",capture_name());
        if (!capture_armed) ImGui::TextWrapped("Release held keys/buttons and center the stick first.");
        else if (keyboard_capture()) ImGui::TextWrapped("Press a keyboard key or mouse button. Escape cancels.");
        else if (axis) ImGui::TextWrapped("Move a stick, press a trigger or a controller button. Escape cancels.");
        else ImGui::TextWrapped("Press a controller button. Stick and trigger mappings are in the section below.");
        if (ImGui::Button("Cancel binding (Esc)")) bluewake_controls_menu_cancel_capture();
    }
    if (!message.empty()) ImGui::TextWrapped("%s", message.c_str());
    const char* error = save_error.empty() ? bluewake_controls_error() : save_error.c_str();
    if (error && *error) ImGui::TextWrapped("%s", error);
    if (!save_error.empty())
        ImGui::TextWrapped("Bindings are still unsaved. Close settings for automatic retry, or choose Save bindings to retry now.");
    ImGui::BeginDisabled(capturing);
    ImGui::SeparatorText("Your controller");
    draw_devices();
    draw_live_input();
    BluewakeControlsSnapshot snapshot{};
    bluewake_controls_snapshot(&snapshot);
    if (ImGui::BeginTabBar("input-bindings")) {
        if (ImGui::BeginTabItem("Keyboard")) {
            bool enabled = snapshot.keyboard_enabled;
            if (row_checkbox("Keyboard controls enabled", &enabled)) bluewake_controls_set_keyboard_enabled(enabled);
            ImGui::TextWrapped("Keyboard and controller can both control your Link. F1, Escape, F5, F6, F8, F9, F10 and F11 remain reserved.");
            ImGui::SeparatorText("Game buttons");
            binding_rows(true, false, snapshot);
            ImGui::SeparatorText("Sticks and triggers");
            binding_rows(true, true, snapshot);
            action_rows(true,snapshot);
            if (ImGui::Button(compact_rows ? "Reset keyboard" : "Restore keyboard defaults")) bluewake_controls_reset_keyboard();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Controller")) {
            ImGui::BeginDisabled(bluewake_controls_selected() == 0);
            ImGui::TextWrapped("Back/Share opens settings. Use the live readings above to check the selected device.");
            ImGui::SeparatorText("Game buttons");
            binding_rows(false, false, snapshot);
            ImGui::SeparatorText("Sticks and triggers");
            binding_rows(false, true, snapshot);
            action_rows(false,snapshot);
            if (bluewake_controls_has_legacy_preferences())
                ImGui::TextWrapped("The camera inversion and face-button swaps above also apply to this profile. Profile inversion combines with camera inversion.");
            bool inverted = row_checkbox("Invert control stick X", &snapshot.invert_stick_x);
            inverted |= row_checkbox("Invert control stick Y", &snapshot.invert_stick_y);
            inverted |= row_checkbox("Invert camera stick X for this profile", &snapshot.invert_camera_x);
            inverted |= row_checkbox("Invert camera stick Y for this profile", &snapshot.invert_camera_y);
            if (inverted) bluewake_controls_set_invert(snapshot.invert_stick_x, snapshot.invert_stick_y,
                                                       snapshot.invert_camera_x, snapshot.invert_camera_y);
            bool zones = row_checkbox("Use dead zones", &snapshot.dead_zones.enabled);
            ImGui::BeginDisabled(!snapshot.dead_zones.enabled);
            zones |= dead_zone_slider("Control stick dead zone", snapshot.dead_zones.stick);
            zones |= dead_zone_slider("Camera stick dead zone", snapshot.dead_zones.camera);
            ImGui::EndDisabled();
            zones |= row_checkbox("Analog triggers also press L/R", &snapshot.dead_zones.emulate_triggers);
            zones |= dead_zone_slider("L trigger activation", snapshot.dead_zones.trigger_left);
            zones |= dead_zone_slider("R trigger activation", snapshot.dead_zones.trigger_right);
            if (zones) bluewake_controls_set_dead_zones(snapshot.dead_zones);
            if (ImGui::Button(compact_rows ? "Reset controller" : "Restore this controller's defaults")) bluewake_controls_reset_controller();
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    if (ImGui::Button("Save bindings")) {
        save_bindings();
    }
    if (!compact_rows) ImGui::SameLine();
    if (ImGui::Button(compact_rows ? "Reload saved" : "Reload saved bindings")) {
        if (bluewake_controls_load()) {
            saved_bindings();
            message = "Saved bindings loaded.";
        } else message = bluewake_controls_error();
    }
    ImGui::SameLine();
    ImGui::TextDisabled(bluewake_controls_dirty() ? "Unsaved changes" : "Saved");
    ImGui::TextWrapped("Changes apply immediately and are saved when settings closes. Your Link's input is neutral while settings is open.");
    ImGui::EndDisabled();
}

extern "C" void bluewake_controls_menu_draw_compact(void) {
    compact_rows = true;
    bluewake_controls_menu_draw();
    compact_rows = false;
}
