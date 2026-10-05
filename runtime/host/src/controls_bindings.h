// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_CONTROLS_BINDINGS_H
#define BLUEWAKE_CONTROLS_BINDINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLUEWAKE_CONTROLS_BUTTONS 12u
#define BLUEWAKE_CONTROLS_AXES 10u
#define BLUEWAKE_CONTROLS_ACTIONS 4u
#define BLUEWAKE_CONTROLS_ACTION_KEYS 2u

typedef enum BluewakeControlsAction {
    BLUEWAKE_ACTION_JUMP,
    BLUEWAKE_ACTION_SPRINT,
    BLUEWAKE_ACTION_FIRST_PERSON,
    BLUEWAKE_ACTION_QUICK_ITEMS
} BluewakeControlsAction;

// Canonical button order: D-pad Left/Right/Down/Up, Z, R, L, A, B, X, Y, Start.
// Canonical axes: stick X+/X-/Y+/Y-, camera X+/X-/Y+/Y-, trigger L/R.
typedef struct BluewakeControlsAxis {
    int32_t axis;       // SDL_GamepadAxis, or -1 for a button/unbound source.
    int32_t sign;       // +1 or -1; ignored for button/unbound sources.
    int32_t button;     // SDL_GamepadButton, or -1 for an axis/unbound source.
} BluewakeControlsAxis;

typedef struct BluewakeControlsDeadZones {
    bool enabled;
    bool emulate_triggers;
    uint16_t stick;
    uint16_t camera;
    uint16_t trigger_left;
    uint16_t trigger_right;
} BluewakeControlsDeadZones;

typedef struct BluewakeControlsSnapshot {
    bool keyboard_enabled;
    int32_t key_buttons[BLUEWAKE_CONTROLS_BUTTONS];
    int32_t key_axes[BLUEWAKE_CONTROLS_AXES];
    int32_t controller_buttons[BLUEWAKE_CONTROLS_BUTTONS];
    BluewakeControlsAxis controller_axes[BLUEWAKE_CONTROLS_AXES];
    BluewakeControlsDeadZones dead_zones;
    bool invert_stick_x;
    bool invert_stick_y;
    bool invert_camera_x;
    bool invert_camera_y;
    int32_t action_keys[BLUEWAKE_CONTROLS_ACTIONS][BLUEWAKE_CONTROLS_ACTION_KEYS];
    int32_t action_buttons[BLUEWAKE_CONTROLS_ACTIONS];
} BluewakeControlsSnapshot;

typedef struct BluewakeControlsDevice {
    uint32_t id; // SDL instance ID, valid only for this connection.
    char name[160];
} BluewakeControlsDevice;

typedef struct BluewakeControlsInput {
    float stick_x, stick_y;   // -1..1, positive Y is up.
    float camera_x, camera_y; // Effective mappings, inversions and dead zones.
    bool camera_click;
    int zoom;                // Mapped D-pad Up/Down, +1/-1.
} BluewakeControlsInput;

typedef struct BluewakeControlsActions {
    uint64_t generation; // Changes on input blocking, rebinding or device replacement.
    bool blocked;
    bool jump_pressed;
    bool sprint_held;     // Gated keyboard/mouse level (legacy field retained).
    bool sprint_keyboard_pressed; // Gated keyboard/mouse edge, including completed taps.
    bool sprint_controller_down;  // Selected controller level after held-release gates.
    bool sprint_pressed;  // Controller: toggle sprint on a press.
    bool first_person_down;
    bool quick_items_down; // Hold modifier; brief completed taps are never latched.
    uint16_t quick_items_native_buttons; // cpad bits sharing a held modifier's physical source.
    bool jump_modifier_conflict; // This Jump press uses the same source as the modifier.
    float movement_tilt;  // Selected controller's effective movement axes, 0..1.
} BluewakeControlsActions;

// Call on the input/UI thread after Aurora's first PADRead initializes mappings.
// Profiles live only in data_dir/controls.ini. Mutations and refresh use this thread.
bool bluewake_controls_init(const char* data_dir);
void bluewake_controls_refresh(void);
void bluewake_controls_snapshot(BluewakeControlsSnapshot* out);
// Returns total connected devices; writes at most capacity entries.
size_t bluewake_controls_devices(BluewakeControlsDevice* out, size_t capacity);
uint32_t bluewake_controls_selected(void);
bool bluewake_controls_automatic(void);
bool bluewake_controls_using_fallback(void);
// id0 selects Automatic. A manual choice remembers GUID+serial across hotplug.
bool bluewake_controls_select(uint32_t id);
bool bluewake_controls_set_keyboard_enabled(bool enabled);
bool bluewake_controls_set_key(bool axis, unsigned slot, int32_t scancode);
bool bluewake_controls_set_button(unsigned slot, int32_t native_button);
bool bluewake_controls_set_axis(unsigned slot, BluewakeControlsAxis binding);
bool bluewake_controls_set_dead_zones(BluewakeControlsDeadZones dead_zones);
bool bluewake_controls_set_invert(bool stick_x, bool stick_y, bool camera_x, bool camera_y);
bool bluewake_controls_set_action_key(unsigned action, unsigned source, int32_t scancode);
bool bluewake_controls_set_action_button(unsigned action, int32_t native_button);
const char* bluewake_controls_action_name(unsigned action);
// Brief action presses latch on the event thread and are sampled once per VI.
void bluewake_controls_action_event(const void* sdl_event);
void bluewake_controls_retrace(void);
bool bluewake_controls_read_actions(BluewakeControlsActions* out);
// Existing settings are overlays, never replacements for a device's mappings.
// Call on the input/UI thread. Profile inversions combine with these by XOR.
void bluewake_controls_set_legacy_preferences(bool swap_ab, bool swap_xy,
                                             bool invert_camera_x, bool invert_camera_y);
bool bluewake_controls_has_legacy_preferences(void);
// Race-free snapshot for the game thread's fast camera, which bypasses PADRead.
// False until initialized. True with zero input if no controller or blocked.
// A held stick/button stays suppressed after closing until released.
bool bluewake_controls_read_controller(BluewakeControlsInput* out);
void bluewake_controls_set_input_blocked(bool blocked);
// Cancels pending actions and suppresses current held sources until release.
void bluewake_controls_cancel_actions(void);
void bluewake_controls_reset_keyboard(void);
bool bluewake_controls_reset_controller(void);
bool bluewake_controls_save(void);
// Invalid/truncated settings are rejected as a whole; current maps stay intact.
bool bluewake_controls_load(void);
bool bluewake_controls_dirty(void);
const char* bluewake_controls_error(void);
const char* bluewake_controls_status(void);
const char* bluewake_controls_button_name(unsigned slot);
const char* bluewake_controls_axis_name(unsigned slot);
bool bluewake_controls_key_reserved(int32_t scancode);

#ifdef __cplusplus
}
#endif
#endif
