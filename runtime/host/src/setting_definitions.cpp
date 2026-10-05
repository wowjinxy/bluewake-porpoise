// SPDX-License-Identifier: GPL-3.0-or-later
#include "setting_definitions.h"
#include <climits>
#include <cstring>

namespace {
const BwSettingChoice kHealthRates[] = {{"0","Off"},{"64","Quarter"},{"128","Half"},{"256","Native"},{"512","Double"},{"1024","4x"},{"2048","8x"},{"4096","16x"}};
const BwSettingChoice kSmooth[] = {{"60", "60 FPS"}, {"120", "120 FPS"}, {"display", "Match display"}};
const BwSettingChoice kHaptics[] = {{"off", "Off"}, {"classic", "Classic"}, {"enhanced", "Enhanced"}};
const BwSettingChoice kSprint[] = {{"hold", "Hold"}, {"toggle", "Toggle"}};
const BwSettingChoice kAspect[] = {{"4:3", "4:3"}, {"16:10", "16:10"}, {"16:9", "16:9"}};
#define SIMPLE(ID, LABEL, HELP, TYPE, PAGE, DEFAULT, LOW, HIGH, APPLY, DEP, WANT, FLAGS) \
    {ID, LABEL, HELP, TYPE, PAGE, DEFAULT, LOW, HIGH, nullptr, 0, APPLY, DEP, WANT, FLAGS, nullptr}
#define BOOL(ID, LABEL, HELP, PAGE, DEFAULT, APPLY) \
    SIMPLE(ID, LABEL, HELP, BW_SETTING_BOOL, PAGE, DEFAULT, 0, 1, APPLY, nullptr, nullptr, 0)
#define OPTION(ID, LABEL, HELP, DEFAULT) \
    {"option." ID, LABEL, HELP, BW_SETTING_BOOL, BW_PAGE_ENHANCEMENTS, DEFAULT, 0, 1, nullptr, 0, \
     BW_SETTING_RESTART, "betterww", "1", 0, ID}
const BwSettingDefinition kDefinitions[] = {
    {"damage_rate_q8", "Ordinary damage (experimental)", "Changes verified ordinary collision, fall and lava damage. Native hit reactions, shields, scripted damage, death and heart-container progression stay in charge. Native is the default; unavailable for room saves.", BW_SETTING_CHOICE, BW_PAGE_ENHANCEMENTS, "256", 0, 4096, kHealthRates, 8, BW_SETTING_LIVE, nullptr, nullptr, 0, nullptr},
    {"healing_rate_q8", "Heart and fairy pickups (experimental)", "Changes verified ordinary heart and fairy pickup healing. Potions, soup, scripted healing, automatic fairy revival and heart-container refills retain native behavior. Unavailable for room saves.", BW_SETTING_CHOICE, BW_PAGE_ENHANCEMENTS, "256", 0, 4096, kHealthRates, 8, BW_SETTING_LIVE, nullptr, nullptr, 0, nullptr},

    {"hud.enabled", "Customize native HUD panes", "Position, scale, fade and tint five audited native pane groups. Native effects, minimap, compass and timers stay separate. Requires an audited module, aspect and GXCore renderer.", BW_SETTING_BOOL, BW_PAGE_HUD, "0", 0, 1, nullptr, 0, BW_SETTING_LIVE, nullptr, nullptr, 0, nullptr},
    {"hud.hearts.offset_x", "Hearts: Horizontal position", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", -2048, 2048, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.hearts.offset_y", "Hearts: Vertical position", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", -2048, 2048, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.hearts.scale", "Hearts: Scale", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "1", 0.25, 4, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.hearts.opacity", "Hearts: Opacity", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_REAL, BW_PAGE_HUD, "1", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.hearts.anchor_x", "Hearts: Horizontal pivot", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.hearts.anchor_y", "Hearts: Vertical pivot", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.hearts.visible", "Hearts: Visible", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_BOOL, BW_PAGE_HUD, "1", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.hearts.tint_r", "Hearts: Red tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.hearts.tint_g", "Hearts: Green tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.hearts.tint_b", "Hearts: Blue tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.hearts.tint_a", "Hearts: Alpha tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.magic.offset_x", "Magic: Horizontal position", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", -2048, 2048, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.magic.offset_y", "Magic: Vertical position", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", -2048, 2048, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.magic.scale", "Magic: Scale", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "1", 0.25, 4, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.magic.opacity", "Magic: Opacity", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_REAL, BW_PAGE_HUD, "1", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.magic.anchor_x", "Magic: Horizontal pivot", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.magic.anchor_y", "Magic: Vertical pivot", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.magic.visible", "Magic: Visible", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_BOOL, BW_PAGE_HUD, "1", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.magic.tint_r", "Magic: Red tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.magic.tint_g", "Magic: Green tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.magic.tint_b", "Magic: Blue tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.magic.tint_a", "Magic: Alpha tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.buttons.offset_x", "Buttons: Horizontal position", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", -2048, 2048, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.buttons.offset_y", "Buttons: Vertical position", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", -2048, 2048, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.buttons.scale", "Buttons: Scale", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "1", 0.25, 4, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.buttons.opacity", "Buttons: Opacity", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_REAL, BW_PAGE_HUD, "1", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.buttons.anchor_x", "Buttons: Horizontal pivot", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.buttons.anchor_y", "Buttons: Vertical pivot", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.buttons.visible", "Buttons: Visible", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_BOOL, BW_PAGE_HUD, "1", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.buttons.tint_r", "Buttons: Red tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.buttons.tint_g", "Buttons: Green tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.buttons.tint_b", "Buttons: Blue tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.buttons.tint_a", "Buttons: Alpha tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.rupees.offset_x", "Rupees: Horizontal position", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", -2048, 2048, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.rupees.offset_y", "Rupees: Vertical position", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", -2048, 2048, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.rupees.scale", "Rupees: Scale", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "1", 0.25, 4, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.rupees.opacity", "Rupees: Opacity", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_REAL, BW_PAGE_HUD, "1", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.rupees.anchor_x", "Rupees: Horizontal pivot", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.rupees.anchor_y", "Rupees: Vertical pivot", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.rupees.visible", "Rupees: Visible", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_BOOL, BW_PAGE_HUD, "1", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.rupees.tint_r", "Rupees: Red tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.rupees.tint_g", "Rupees: Green tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.rupees.tint_b", "Rupees: Blue tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.rupees.tint_a", "Rupees: Alpha tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.keys.offset_x", "Keys: Horizontal position", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", -2048, 2048, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.keys.offset_y", "Keys: Vertical position", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", -2048, 2048, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.keys.scale", "Keys: Scale", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "1", 0.25, 4, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.keys.opacity", "Keys: Opacity", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_REAL, BW_PAGE_HUD, "1", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.keys.anchor_x", "Keys: Horizontal pivot", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.keys.anchor_y", "Keys: Vertical pivot", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_REAL, BW_PAGE_HUD, "0", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.keys.visible", "Keys: Visible", "Native hidden panes stay hidden. Units follow the current native 2D layout; separate effects are outside this pane prototype.", BW_SETTING_BOOL, BW_PAGE_HUD, "1", 0, 1, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.keys.tint_r", "Keys: Red tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.keys.tint_g", "Keys: Green tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.keys.tint_b", "Keys: Blue tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},
    {"hud.keys.tint_a", "Keys: Alpha tint", "Multiplies the native final fragment and preserves native fades/flashes.", BW_SETTING_INT, BW_PAGE_HUD, "255", 0, 255, nullptr, 0, BW_SETTING_LIVE, "hud.enabled", "1", 0, nullptr},

    BOOL("fullscreen", "Fullscreen", "Fill the display with the game window.", BW_PAGE_DISPLAY, "0", BW_SETTING_LIVE),
    SIMPLE("window_w", "Window width", "Saved window placement; excluded from portable presets.", BW_SETTING_INT, BW_PAGE_DISPLAY, "0", 0, 32768, BW_SETTING_LIVE, nullptr, nullptr, BW_SETTING_HIDDEN | BW_SETTING_NO_PRESET),
    SIMPLE("window_h", "Window height", "Saved window placement; excluded from portable presets.", BW_SETTING_INT, BW_PAGE_DISPLAY, "0", 0, 32768, BW_SETTING_LIVE, nullptr, nullptr, BW_SETTING_HIDDEN | BW_SETTING_NO_PRESET),
    SIMPLE("window_x", "Window X", "Saved window placement; excluded from portable presets.", BW_SETTING_INT, BW_PAGE_DISPLAY, "-2147483648", INT_MIN, INT_MAX, BW_SETTING_LIVE, nullptr, nullptr, BW_SETTING_HIDDEN | BW_SETTING_NO_PRESET),
    SIMPLE("window_y", "Window Y", "Saved window placement; excluded from portable presets.", BW_SETTING_INT, BW_PAGE_DISPLAY, "-2147483648", INT_MIN, INT_MAX, BW_SETTING_LIVE, nullptr, nullptr, BW_SETTING_HIDDEN | BW_SETTING_NO_PRESET),
    SIMPLE("render_scale", "Render resolution", "0 follows the window; 1 through 4 multiply the game's 480-line image.", BW_SETTING_INT, BW_PAGE_DISPLAY, "0", 0, 4, BW_SETTING_LIVE, nullptr, nullptr, 0),
    SIMPLE("anisotropy", "Texture filtering", "Anisotropic filtering strength; 1 uses the game's filtering.", BW_SETTING_INT, BW_PAGE_DISPLAY, "1", 1, 16, BW_SETTING_LIVE, nullptr, nullptr, 0),
    BOOL("smooth_motion", "Smooth Motion", "Experimental interpolation adds displayed frames while game logic stays at its normal rate.", BW_PAGE_DISPLAY, "0", BW_SETTING_LIVE),
    {"smooth_motion_fps", "Smooth Motion frame rate", "The display's refresh rate may cap the applied rate without changing this preference.", BW_SETTING_CHOICE, BW_PAGE_DISPLAY, "60", 0, 0, kSmooth, 3, BW_SETTING_LIVE, "smooth_motion", "1", 0, nullptr},
    BOOL("show_fps", "Show frame rate", "Display the frame-rate overlay.", BW_PAGE_DISPLAY, "0", BW_SETTING_LIVE),
    BOOL("pause_unfocused", "Pause when unfocused", "Pause when another window receives focus.", BW_PAGE_DISPLAY, "0", BW_SETTING_LIVE),
    BOOL("compile_shaders_first", "Compile shaders at startup", "Wait for bundled and saved pipelines before starting the game.", BW_PAGE_DISPLAY, "0", BW_SETTING_RESTART),
    SIMPLE("menu_size", "Menu size (%)", "Enlarge or reduce menu text and controls relative to automatic display sizing. Large menus scroll; the preference stays unchanged when the window resizes.", BW_SETTING_INT, BW_PAGE_DISPLAY, "100", 75, 200, BW_SETTING_LIVE, nullptr, nullptr, 0),
    BOOL("mouse_camera", "Mouse camera", "Click the game to control the camera; Escape releases the mouse.", BW_PAGE_CONTROLS, "1", BW_SETTING_LIVE),
    SIMPLE("mouse_sensitivity", "Mouse sensitivity", "Camera and aiming sensitivity.", BW_SETTING_REAL, BW_PAGE_CONTROLS, "1", 0.1, 10, BW_SETTING_LIVE, "mouse_camera", "1", 0),
    SIMPLE("mouse_invert_y", "Mouse vertical inversion", "Moving the mouse forward looks down.", BW_SETTING_BOOL, BW_PAGE_CONTROLS, "0", 0, 1, BW_SETTING_LIVE, "mouse_camera", "1", 0),
    BOOL("controller_swap_ab", "Swap A and B", "Swap the controller's A and B actions.", BW_PAGE_CONTROLS, "0", BW_SETTING_LIVE),
    BOOL("controller_swap_xy", "Swap X and Y", "Swap the controller's X and Y actions.", BW_PAGE_CONTROLS, "0", BW_SETTING_LIVE),
    BOOL("controller_invert_x", "Controller horizontal inversion", "Invert camera-stick left and right.", BW_PAGE_CONTROLS, "0", BW_SETTING_LIVE),
    BOOL("controller_invert_y", "Controller vertical inversion", "Invert camera-stick up and down.", BW_PAGE_CONTROLS, "0", BW_SETTING_LIVE),
    BOOL("stick_camera", "Fast right-stick camera", "Turn and aim directly; use the first-person action to switch views (right-stick click by default).", BW_PAGE_CONTROLS, "1", BW_SETTING_LIVE),
    SIMPLE("stick_camera_speed", "Right-stick turn speed", "Camera degrees per second at full stick tilt.", BW_SETTING_INT, BW_PAGE_CONTROLS, "360", 60, 1080, BW_SETTING_LIVE, "stick_camera", "1", 0),
    SIMPLE("stick_aim_speed", "Right-stick aim speed", "Aiming degrees per second at full stick tilt.", BW_SETTING_INT, BW_PAGE_CONTROLS, "180", 30, 720, BW_SETTING_LIVE, "stick_camera", "1", 0),
    {"haptics", "Controller vibration", "Classic uses the game's motor; Enhanced shapes hits and supports trigger feedback.", BW_SETTING_CHOICE, BW_PAGE_CONTROLS, "enhanced", 0, 0, kHaptics, 3, BW_SETTING_LIVE, nullptr, nullptr, 0, nullptr},
    SIMPLE("haptics_strength", "Vibration strength", "Enhanced vibration strength as a percentage.", BW_SETTING_INT, BW_PAGE_CONTROLS, "80", 0, 100, BW_SETTING_LIVE, "haptics", "enhanced", 0),
    SIMPLE("haptics_triggers", "Trigger feedback", "Enhanced vibration for supported controller triggers.", BW_SETTING_BOOL, BW_PAGE_CONTROLS, "1", 0, 1, BW_SETTING_LIVE, "haptics", "enhanced", 0),
    {"sprint_keyboard_mode", "Keyboard Sprint mode", "Hold requires the mapped Sprint key. Toggle starts or stops Sprint on a fresh key press. Requires Jump and Run. Menu, input changes and resets cancel Sprint; touch remains Hold.", BW_SETTING_CHOICE, BW_PAGE_CONTROLS, "hold", 0, 0, kSprint, 2, BW_SETTING_LIVE, nullptr, nullptr, 0, nullptr},
    {"sprint_controller_mode", "Controller Sprint mode", "Hold requires the selected controller's mapped Sprint button. Toggle starts/stops on a fresh press and also stops after eight idle retraces. Requires Jump and Run; menu, input changes and resets cancel Sprint.", BW_SETTING_CHOICE, BW_PAGE_CONTROLS, "toggle", 0, 0, kSprint, 2, BW_SETTING_LIVE, nullptr, nullptr, 0, nullptr},
    BOOL("movement_extras", "Jump and Run", "Enable the optional Jump and Sprint actions configured in Controls.", BW_PAGE_ENHANCEMENTS, "0", BW_SETTING_RESTART),
    SIMPLE("dialogue_speed", "Dialogue text speed (experimental)", "Speeds up supported ordinary NPC and cutscene messages. 1 is native. Scripted waits, page stops, choices and unskippable text keep their original timing. BetterWW Instant text takes priority; disable it and restart to restore already-patched text.", BW_SETTING_REAL, BW_PAGE_ENHANCEMENTS, "1", 1, 10, BW_SETTING_LIVE, nullptr, nullptr, 0),
    BOOL("quick_items", "D-pad equipment shortcuts", "Hold the shortcut modifier (LB or Tab by default): Up plays the Wind Waker; Left deploys the cannon and Right the salvage crane while riding the boat. Down and plain D-pad retain native actions. Item assignments stay unchanged.", BW_PAGE_ENHANCEMENTS, "0", BW_SETTING_LIVE),
    BOOL("faster_wind", "Faster wind changes (experimental)", "Shortens the final animation after the chosen wind direction has been committed. Conducting, direction selection, story events and native cleanup keep their normal behavior.", BW_PAGE_ENHANCEMENTS, "0", BW_SETTING_LIVE),
    BOOL("faster_boots", "Faster Iron Boots (experimental)", "Doubles the speed of the normal Iron Boots equip animation. Requires owned and assigned boots; preserves the game's equipment toggle and vibration timing within the animation.", BW_PAGE_ENHANCEMENTS, "0", BW_SETTING_LIVE),
    BOOL("fast_transitions", "Fast transitions", "Shorten scene fades and speed through covered loading transitions.", BW_PAGE_ENHANCEMENTS, "0", BW_SETTING_RESTART),
    BOOL("quick_doors", "Quick doors", "Finish knob-door transitions once the game's fade covers the screen.", BW_PAGE_ENHANCEMENTS, "0", BW_SETTING_RESTART),
    BOOL("climb", "Climb any wall (experimental)", "Use native ivy climbing on supported plain walls with stamina; sloped and faceted surfaces up to 30 degrees are supported.", BW_PAGE_ENHANCEMENTS, "0", BW_SETTING_LIVE),
    SIMPLE("climb_stamina", "Climbing stamina", "Seconds of movement on a full stamina wheel; hanging drains more slowly.", BW_SETTING_INT, BW_PAGE_ENHANCEMENTS, "12", 4, 30, BW_SETTING_LIVE, "climb", "1", 0),
    {"aspect", "Aspect ratio", "Widescreen widens the camera and positions the HUD; compiled widescreen variants are required.", BW_SETTING_CHOICE, BW_PAGE_DISPLAY, "4:3", 0, 0, kAspect, 3, BW_SETTING_RESTART, nullptr, nullptr, 0, nullptr},
    BOOL("keep_aspect", "Preserve aspect ratio", "Fit the game image without stretching it to the window.", BW_PAGE_DISPLAY, "1", BW_SETTING_RESTART),
    BOOL("betterww", "Better Wind Waker", "Enable the compiled BetterWW quality-of-life options.", BW_PAGE_ENHANCEMENTS, "0", BW_SETTING_RESTART),
    SIMPLE("option_defaults_off", "Disable option defaults", "Session-only command-line baseline; never exported or stored by a preset.", BW_SETTING_BOOL, BW_PAGE_DEVELOPER, "0", 0, 1, BW_SETTING_RESTART, nullptr, nullptr, BW_SETTING_HIDDEN | BW_SETTING_NO_PRESET | BW_SETTING_SESSION_ONLY),
    BOOL("hd_textures", "HD texture pack", "Load Dolphin-format replacement textures from the texture folder.", BW_PAGE_MODS, "0", BW_SETTING_RESTART),
    BOOL("lle_audio", "Exact audio", "Run the original sound-chip program; slower than high-level Zelda audio.", BW_PAGE_SOUND_SAVES, "0", BW_SETTING_RESTART),
    SIMPLE("audio_master", "Master volume", "Volume of the complete game output, including music, sound effects and fanfares. Works with both audio backends.", BW_SETTING_INT, BW_PAGE_SOUND_SAVES, "100", 0, 100, BW_SETTING_LIVE, nullptr, nullptr, 0),
    SIMPLE("audio_music", "Music volume (experimental)", "Volume of recognized native music voices with high-level Zelda audio. Requires Exact audio off; restart after changing Exact audio to switch the running backend. Unclassified fanfares and streams retain their original category volume.", BW_SETTING_INT, BW_PAGE_SOUND_SAVES, "100", 0, 100, BW_SETTING_LIVE, "lle_audio", "0", 0),
    SIMPLE("audio_sfx", "Sound effects volume (experimental)", "Volume of recognized native sound effects, including voices, with high-level Zelda audio. Requires Exact audio off; restart after changing Exact audio to switch the running backend. Unclassified sounds retain their original category volume.", BW_SETTING_INT, BW_PAGE_SOUND_SAVES, "100", 0, 100, BW_SETTING_LIVE, "lle_audio", "0", 0),
    BOOL("audio_muted", "Mute game audio", "Mute the complete output without pausing the native sound engine. Turning mute off restores the selected volume.", BW_PAGE_SOUND_SAVES, "0", BW_SETTING_LIVE),
    BOOL("autosave", "Autosave (experimental)", "Periodically saves the existing quest using the game's memory-card transaction. First load a normal card quest or use the game's Save. Waits for safe gameplay; defers during menus, events, scene changes and pending host work.", BW_PAGE_SOUND_SAVES, "0", BW_SETTING_LIVE),
    SIMPLE("autosave_interval", "Autosave interval (seconds)", "Minimum time between native saves; busy or unsafe gameplay can delay a save. Disabling autosave lets a save already in progress finish.", BW_SETTING_INT, BW_PAGE_SOUND_SAVES, "300", 60, 3600, BW_SETTING_LIVE, "autosave", "1", 0),
    OPTION("instant_text", "Instant text boxes", "Draw text immediately; holding B advances supported text boxes.", "1"),
    OPTION("swift_sail", "Swift Sail", "Faster sailing with the wind set behind the boat.", "1"),
    OPTION("brisk_sail", "Brisk Sail", "Swift Sail with stronger braking; enables the Swift Sail patches too.", "0"),
    OPTION("faster_movement", "Faster rolling", "Increase the rolling speed.", "1"),
    OPTION("faster_grapple", "Faster grappling hook", "Shorten grappling-hook actions.", "1"),
    OPTION("faster_block_push", "Faster block pushing", "Shorten block push and pull animations.", "1"),
    OPTION("faster_animations", "Faster crawling, climbing and NPC chat zoom", "Speed up the selected native animations.", "1"),
    OPTION("tingle_chests", "Tingle Chests without the Tingle Tuner", "Allow ordinary bombs and show chests after obtaining the compass.", "1"),
    OPTION("unrestricted_boat", "Unrestricted boat", "Remove story-related sailing restrictions; the sea boundary still applies.", "0"),
    OPTION("no_song_replays", "No song replays", "Skip conducting the completed song a second time.", "1"),
    OPTION("swing_turn", "Turn while swinging", "Steer Link while swinging on a rope.", "1"),
    OPTION("skip_intro_movie", "Skip the opening movie", "Skip the movie at startup.", "0"),
    OPTION("faster_ballad", "Faster Ballad of Gales", "Shorten the boat's warp preparation.", "1"),
    OPTION("invert_camera_x", "Invert camera left and right", "Invert the game's native horizontal C-stick camera axis.", "0"),
    OPTION("reveal_sea_chart", "Reveal the full sea chart (new games)", "Reveal sea-chart cells when starting a new game.", "0")
};
#undef OPTION
#undef BOOL
#undef SIMPLE
const char* kPageNames[] = {"Display", "Controls", "Enhancements", "Mods", "Network", "Sound & Saves", "Developer", "HUD"};
const char* kPageIds[] = {"display", "controls", "enhancements", "mods", "network", "sound_saves", "developer", "hud"};
}

const BwSettingDefinition* bw_setting_definitions(size_t* count) {
    if (count) *count = sizeof kDefinitions / sizeof kDefinitions[0];
    return kDefinitions;
}
const BwSettingDefinition* bw_setting_find(const char* id) {
    if (!id) return nullptr;
    for (const auto& definition : kDefinitions)
        if (std::strcmp(id, definition.id) == 0) return &definition;
    return nullptr;
}
const char* bw_setting_page_name(BwSettingPage page) {
    return page >= 0 && page < BW_PAGE_COUNT ? kPageNames[page] : "Unknown";
}
const char* bw_setting_page_id(BwSettingPage page) {
    return page >= 0 && page < BW_PAGE_COUNT ? kPageIds[page] : "unknown";
}
uint32_t bw_setting_all_sections(void) { return (1u << BW_PAGE_COUNT) - 1u; }
