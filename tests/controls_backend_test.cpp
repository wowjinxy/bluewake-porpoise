// Real controller profiles, Aurora PAD output and fast camera input, without a GPU.
#include "controls_bindings.h"
#include <SDL3/SDL.h>
#include <aurora/aurora.h>
#include <dolphin/pad.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace aurora { AuroraConfig g_config{}; }
extern "C" void PADLatchKeyEvent(int scancode, int down);

namespace {
struct VirtualPad {
    SDL_JoystickID id = 0;
    SDL_Joystick* joystick = nullptr;
    explicit VirtualPad(const char* name, uint16_t product) {
        SDL_VirtualJoystickDesc desc{};
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.vendor_id = 0xf00d;
        desc.product_id = product;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1u;
        desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1u;
        desc.name = name;
        id = SDL_AttachVirtualJoystick(&desc);
        assert(id && SDL_IsGamepad(id));
        joystick = SDL_OpenJoystick(id);
        assert(joystick);
        neutral();
    }
    void axis(SDL_GamepadAxis source, Sint16 value) {
        assert(SDL_SetJoystickVirtualAxis(joystick, source, value));
        SDL_UpdateJoysticks();
    }
    void button(SDL_GamepadButton source, bool down) {
        assert(SDL_SetJoystickVirtualButton(joystick, source, down));
        SDL_UpdateJoysticks();
    }
    void neutral() {
        for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i)
            axis(static_cast<SDL_GamepadAxis>(i), i >= SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? -32768 : 0);
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
            button(static_cast<SDL_GamepadButton>(i), false);
    }
    void detach() {
        if (!id) return;
        SDL_CloseJoystick(joystick);
        joystick = nullptr;
        assert(SDL_DetachVirtualJoystick(id));
        id = 0;
    }
    ~VirtualPad() { detach(); }
    std::string guid() const {
        char value[33]{};
        SDL_GUIDToString(SDL_GetGamepadGUIDForID(id), value, sizeof value);
        return value;
    }
};

void refresh_without_events() {
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_GAMEPAD_ADDED, SDL_EVENT_GAMEPAD_REMOVED);
    SDL_FlushEvents(SDL_EVENT_JOYSTICK_ADDED, SDL_EVENT_JOYSTICK_REMOVED);
    bluewake_controls_refresh();
}
PADStatus pad_read() {
    PADStatus result[PAD_CHANMAX]{};
    PADRead(result);
    assert(result[0].err == PAD_ERR_NONE);
    return result[0];
}
BluewakeControlsInput camera_read() {
    bluewake_controls_retrace();
    BluewakeControlsInput input{};
    assert(bluewake_controls_read_controller(&input));
    return input;
}
void assert_neutral(const PADStatus& status) {
    assert(status.button == 0 && status.extButton == 0);
    assert(status.stickX == 0 && status.stickY == 0 && status.substickX == 0 && status.substickY == 0);
    assert(status.triggerLeft == 0 && status.triggerRight == 0 && status.analogA == 0 && status.analogB == 0);
}
void assert_neutral(const BluewakeControlsInput& input) {
    assert(input.stick_x == 0 && input.stick_y == 0 && input.camera_x == 0 && input.camera_y == 0);
    assert(!input.camera_click && input.zoom == 0);
}
bool near(float actual, float expected) { return std::abs(actual - expected) < .001f; }
BluewakeControlsSnapshot snapshot() {
    BluewakeControlsSnapshot state{};
    bluewake_controls_snapshot(&state);
    return state;
}
std::string read_file(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    assert(stream);
    std::ostringstream contents;
    contents << stream.rdbuf();
    assert(!stream.bad());
    return contents.str();
}
void write_file(const std::filesystem::path& file, const std::string& contents) {
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    assert(stream);
    stream << contents;
    stream.close();
    assert(stream);
}
void menu_open(bool open) {
    // These are the two production gates used by controls_menu_set_open.
    bluewake_controls_set_input_blocked(open);
    PADBlockInput(open);
}
void add_serial_profile(std::string& contents, const std::string& guid,
                        const char* serial_hex, int native_a) {
    const auto begin = contents.find("[controller " + guid + ":]");
    assert(begin != std::string::npos);
    const auto end = contents.find("[controller ", begin + 1);
    auto record = contents.substr(begin, end == std::string::npos ? end : end - begin);
    const auto header_end = record.find(']');
    record.insert(header_end, serial_hex);
    const auto buttons_begin = record.find("buttons=");
    const auto buttons_end = record.find('\n', buttons_begin);
    assert(buttons_begin != std::string::npos && buttons_end != std::string::npos);
    auto state = snapshot();
    state.controller_buttons[7] = native_a;
    std::ostringstream buttons;
    buttons << "buttons=";
    for (unsigned i = 0; i < BLUEWAKE_CONTROLS_BUTTONS; ++i)
        buttons << (i ? "," : "") << state.controller_buttons[i];
    record.replace(buttons_begin, buttons_end - buttons_begin, buttons.str());
    contents += "\n" + record;
}
}

int main() {
    const char* hints[] = {SDL_HINT_JOYSTICK_HIDAPI, SDL_HINT_JOYSTICK_DIRECTINPUT,
        SDL_HINT_XINPUT_ENABLED, SDL_HINT_JOYSTICK_RAWINPUT, SDL_HINT_JOYSTICK_WGI,
        SDL_HINT_JOYSTICK_GAMEINPUT, SDL_HINT_JOYSTICK_LINUX_CLASSIC,
        SDL_HINT_JOYSTICK_IOKIT, SDL_HINT_JOYSTICK_MFI};
    for (auto* hint : hints) SDL_SetHint(hint, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    assert(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMEPAD));
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto dir = std::filesystem::temp_directory_path() /
        ("bluewake-controls-backend-" + std::to_string(unique));
    assert(std::filesystem::create_directory(dir));
    const auto utf8path = dir.u8string();
    const std::string path(reinterpret_cast<const char*>(utf8path.c_str()));
    aurora::g_config.userPath = path.c_str();
    aurora::g_config.logLevel = LOG_INFO;
    assert(PADInit());
    PADStatus initial[PAD_CHANMAX]{};
    PADRead(initial);
    assert(bluewake_controls_init(path.c_str()));
    const auto file = dir / "controls.ini";

    VirtualPad first("BlueWake controls A", 11);
    refresh_without_events();
    assert(bluewake_controls_selected() == first.id && bluewake_controls_automatic());
    assert(bluewake_controls_devices(nullptr, 0) == 1);
    assert(bluewake_controls_select(first.id));
    assert(!bluewake_controls_automatic() && !bluewake_controls_using_fallback());
    assert_neutral(camera_read()); // Release the new-device camera guard.
    assert(bluewake_controls_set_key(false, 7, SDL_SCANCODE_SPACE));
    assert(bluewake_controls_set_button(7, SDL_GAMEPAD_BUTTON_NORTH));
    assert(bluewake_controls_set_button(3, SDL_GAMEPAD_BUTTON_WEST));
    assert(bluewake_controls_set_axis(0, {SDL_GAMEPAD_AXIS_RIGHTX, 1, -1}));
    assert(bluewake_controls_set_axis(4, {SDL_GAMEPAD_AXIS_LEFTX, 1, -1}));
    assert(bluewake_controls_set_axis(5, {SDL_GAMEPAD_AXIS_LEFTX, -1, -1}));
    assert(bluewake_controls_set_dead_zones({true, true, 1000, 12000, 25000, 26000}));
    first.button(SDL_GAMEPAD_BUTTON_NORTH, true);
    assert((pad_read().button & PAD_BUTTON_A) != 0);
    first.button(SDL_GAMEPAD_BUTTON_NORTH, false);
    first.axis(SDL_GAMEPAD_AXIS_LEFTX, 6000);
    assert(camera_read().camera_x == 0 && pad_read().substickX == 0);
    first.axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
    assert(near(camera_read().camera_x, 1) && pad_read().substickX >= 126);
    first.neutral();
    assert_neutral(camera_read());

    VirtualPad second("BlueWake controls B", 22);
    const auto second_guid = second.guid();
    assert(first.guid() != second_guid);
    refresh_without_events();
    assert(bluewake_controls_selected() == first.id && bluewake_controls_devices(nullptr, 0) == 2);
    assert(bluewake_controls_select(second.id));
    assert_neutral(camera_read());
    assert(bluewake_controls_set_button(7, SDL_GAMEPAD_BUTTON_WEST));
    assert(bluewake_controls_set_axis(0, {SDL_GAMEPAD_AXIS_RIGHTY, -1, -1}));
    assert(bluewake_controls_set_dead_zones({true, true, 2200, 3300, 24000, 27000}));
    second.button(SDL_GAMEPAD_BUTTON_WEST, true);
    second.axis(SDL_GAMEPAD_AXIS_RIGHTY, -32768);
    assert((pad_read().button & PAD_BUTTON_A) != 0 && pad_read().stickX >= 126);
    second.neutral();
    assert(bluewake_controls_save());
    const auto good = read_file(file);
    assert(!std::filesystem::exists(dir / "controller_ports.dat"));
    assert(bluewake_controls_set_button(7, SDL_GAMEPAD_BUTTON_EAST));
    assert(bluewake_controls_load());
    assert(snapshot().controller_buttons[7] == SDL_GAMEPAD_BUTTON_WEST);
    assert(snapshot().dead_zones.camera == 3300 && bluewake_controls_selected() == second.id);
    assert_neutral(camera_read());

    // Legacy settings overlay the custom profile without replacing or saving it.
    bluewake_controls_set_legacy_preferences(true, false, true, false);
    assert(!bluewake_controls_dirty());
    second.button(SDL_GAMEPAD_BUTTON_WEST, true);
    second.axis(SDL_GAMEPAD_AXIS_RIGHTX, 32767);
    auto status = pad_read();
    assert((status.button & PAD_BUTTON_B) != 0 && (status.button & PAD_BUTTON_A) == 0);
    assert(status.substickX <= -126 && near(camera_read().camera_x, -1));
    assert(snapshot().controller_buttons[7] == SDL_GAMEPAD_BUTTON_WEST && !snapshot().invert_camera_x);
    assert(read_file(file) == good);
    bluewake_controls_set_legacy_preferences(false, false, false, false);
    status = pad_read();
    assert((status.button & PAD_BUTTON_A) != 0 && status.substickX >= 126);
    assert(snapshot().controller_axes[0].axis == SDL_GAMEPAD_AXIS_RIGHTY);
    assert(snapshot().dead_zones.stick == 2200);
    assert(bluewake_controls_set_invert(false, false, true, false));
    bluewake_controls_set_legacy_preferences(false, false, true, false);
    assert(near(camera_read().camera_x, 1) && pad_read().substickX >= 126); // XOR two inversions.
    bluewake_controls_set_legacy_preferences(false, false, false, false);
    assert(near(camera_read().camera_x, -1) && pad_read().substickX <= -126);
    second.neutral();
    assert(bluewake_controls_load());
    assert_neutral(camera_read());

    const auto old_second_id = second.id;
    second.detach();
    refresh_without_events();
    assert(bluewake_controls_selected() == first.id && bluewake_controls_using_fallback());
    assert(snapshot().controller_buttons[7] == SDL_GAMEPAD_BUTTON_NORTH && snapshot().dead_zones.camera == 12000);
    assert_neutral(camera_read());
    VirtualPad returned("BlueWake controls B", 22);
    assert(returned.id != old_second_id && returned.guid() == second_guid);
    refresh_without_events();
    assert(bluewake_controls_selected() == returned.id && !bluewake_controls_using_fallback());
    assert(snapshot().controller_buttons[7] == SDL_GAMEPAD_BUTTON_WEST);
    assert(snapshot().controller_axes[0].axis == SDL_GAMEPAD_AXIS_RIGHTY);
    assert_neutral(camera_read());
    assert(bluewake_controls_init(path.c_str()));
    assert(bluewake_controls_selected() == returned.id && snapshot().key_buttons[7] == SDL_SCANCODE_SPACE);
    assert_neutral(camera_read());
    returned.button(SDL_GAMEPAD_BUTTON_WEST, true);
    assert((pad_read().button & PAD_BUTTON_A) != 0);
    returned.neutral();

    // SDL virtual pads cannot expose serials. Load distinct serial records for
    // this real pad's GUID and ensure they never bleed into its serialless map.
    const char* serial = SDL_GetGamepadSerial(SDL_GetGamepadFromID(returned.id));
    assert(serial == nullptr || *serial == '\0');
    auto serial_records = good;
    add_serial_profile(serial_records, second_guid, "73657269616c2d41", SDL_GAMEPAD_BUTTON_NORTH);
    add_serial_profile(serial_records, second_guid, "73657269616c2d42", SDL_GAMEPAD_BUTTON_EAST);
    const auto preferred = serial_records.find("preferred_serial=\n");
    assert(preferred != std::string::npos);
    serial_records.insert(preferred + std::string("preferred_serial=").size(), "73657269616c2d41");
    write_file(file, serial_records);
    assert(bluewake_controls_load() && bluewake_controls_using_fallback());
    assert(snapshot().controller_buttons[7] == SDL_GAMEPAD_BUTTON_WEST);
    assert(bluewake_controls_save());
    const auto serial_saved = read_file(file);
    assert(serial_saved.find("[controller " + second_guid + ":73657269616c2d41]") != std::string::npos);
    assert(serial_saved.find("[controller " + second_guid + ":73657269616c2d42]") != std::string::npos);
    write_file(file, good);
    assert(bluewake_controls_load());
    assert_neutral(camera_read());

    // Only the selected device can drive the fast camera, with effective maps.
    first.axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
    assert_neutral(camera_read()); // The unselected pad's held stick is ignored.
    assert(bluewake_controls_select(first.id));
    assert_neutral(camera_read()); // Held input from a replacement is gated.
    first.neutral();
    assert_neutral(camera_read());
    first.axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
    first.axis(SDL_GAMEPAD_AXIS_RIGHTX, 32767);
    first.button(SDL_GAMEPAD_BUTTON_NORTH, true);
    first.button(SDL_GAMEPAD_BUTTON_WEST, true);
    first.button(SDL_GAMEPAD_BUTTON_RIGHT_STICK, true);
    first.button(SDL_GAMEPAD_BUTTON_BACK, true);
    first.axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767);
    first.axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
    auto fast = camera_read();
    assert(near(fast.stick_x, 1) && near(fast.camera_x, 1) && fast.camera_click && fast.zoom == 1);
    const auto keys = std::array{SDL_SCANCODE_SPACE, SDL_SCANCODE_W, SDL_SCANCODE_H, SDL_SCANCODE_E, SDL_SCANCODE_R};
    auto held_pad = [&] {
        for (auto key : keys) PADLatchKeyEvent(key, 1);
        return pad_read();
    };
    status = held_pad();
    assert((status.button & PAD_BUTTON_A) != 0 && (status.extButton & PAD_BUTTON_BACK) != 0);
    assert(status.stickX >= 126 && status.stickY == 127 && status.substickX >= 126);
    assert(status.triggerLeft > 30 && status.triggerRight > 30);
    menu_open(true);
    assert_neutral(held_pad());
    assert_neutral(camera_read());
    menu_open(false);
    for (unsigned i = 0; i < 4; ++i) {
        assert_neutral(held_pad());
        assert_neutral(camera_read());
    }
    first.neutral();
    for (auto key : keys) PADLatchKeyEvent(key, 0);
    SDL_Delay(2);
    assert_neutral(pad_read());
    assert_neutral(camera_read());
    first.axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
    first.axis(SDL_GAMEPAD_AXIS_RIGHTX, 32767);
    first.button(SDL_GAMEPAD_BUTTON_NORTH, true);
    first.button(SDL_GAMEPAD_BUTTON_WEST, true);
    first.button(SDL_GAMEPAD_BUTTON_RIGHT_STICK, true);
    first.button(SDL_GAMEPAD_BUTTON_BACK, true);
    first.axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767);
    first.axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
    status = held_pad();
    assert((status.button & PAD_BUTTON_A) != 0 && (status.extButton & PAD_BUTTON_BACK) != 0);
    assert(status.stickX >= 126 && status.stickY == 127 && status.substickX >= 126);
    assert(status.triggerLeft > 30 && status.triggerRight > 30);
    fast = camera_read();
    assert(near(fast.stick_x, 1) && near(fast.camera_x, 1) && fast.camera_click && fast.zoom == 1);
    first.neutral();
    returned.neutral();

    // Import the old schema while preserving the existing 12/10 game bindings.
    auto legacy = good;
    const auto legacy_version = legacy.find("version=3");
    assert(legacy_version != std::string::npos);
    legacy.replace(legacy_version,9,"version=1");
    for (const auto* field : {"host_keys=","host_buttons="})
        for (size_t at = legacy.find(field); at != std::string::npos; at = legacy.find(field))
            legacy.erase(at,legacy.find('\n',at)-at+1);
    write_file(file,legacy);
    assert(bluewake_controls_load());
    assert(snapshot().controller_buttons[7] == SDL_GAMEPAD_BUTTON_WEST);
    assert(snapshot().controller_axes[0].axis == SDL_GAMEPAD_AXIS_RIGHTY);
    assert(snapshot().action_keys[BLUEWAKE_ACTION_JUMP][0] == SDL_SCANCODE_SPACE);
    assert(snapshot().action_keys[BLUEWAKE_ACTION_SPRINT][1] == SDL_SCANCODE_RSHIFT);
    assert(snapshot().action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][0] == SDL_SCANCODE_TAB);
    assert(snapshot().action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS] == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    assert(bluewake_controls_save() && read_file(file).find("version=3") != std::string::npos);
    for (auto key : keys) PADLatchKeyEvent(key,0);
    SDL_Delay(2);
    assert(bluewake_controls_select(first.id));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_JUMP,0,SDL_SCANCODE_V));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_JUMP,1,PAD_KEY_MOUSE_X1));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_FIRST_PERSON,0,SDL_SCANCODE_G));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_JUMP,SDL_GAMEPAD_BUTTON_NORTH));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_SPRINT,SDL_GAMEPAD_BUTTON_WEST));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_FIRST_PERSON,SDL_GAMEPAD_BUTTON_EAST));
    bluewake_controls_retrace();
    BluewakeControlsActions actions{};
    assert(bluewake_controls_read_actions(&actions) && !actions.jump_pressed);
    // A complete keyboard tap between samples still delivers one Jump press.
    SDL_Event event{}; event.type = SDL_EVENT_KEY_DOWN; event.key.scancode = SDL_SCANCODE_V;
    bluewake_controls_action_event(&event);
    event.type = SDL_EVENT_KEY_UP; bluewake_controls_action_event(&event);
    bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions) && actions.jump_pressed);
    bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions) && !actions.jump_pressed);
    event.type = SDL_EVENT_KEY_DOWN; event.key.scancode = SDL_SCANCODE_SPACE;
    bluewake_controls_action_event(&event);
    bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions) && !actions.jump_pressed); // Old fixed key is inactive.
    event={}; event.type=SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.button=SDL_BUTTON_X1;
    bluewake_controls_action_event(&event); event.type=SDL_EVENT_MOUSE_BUTTON_UP; bluewake_controls_action_event(&event);
    bluewake_controls_retrace(); assert(bluewake_controls_read_actions(&actions) && actions.jump_pressed);
    bluewake_controls_retrace(); assert(bluewake_controls_read_actions(&actions) && !actions.jump_pressed);
    event={}; event.type=SDL_EVENT_KEY_DOWN; event.key.scancode=SDL_SCANCODE_G;
    bluewake_controls_action_event(&event); event.type=SDL_EVENT_KEY_UP; bluewake_controls_action_event(&event);
    bluewake_controls_retrace(); assert(bluewake_controls_read_controller(&fast) && fast.camera_click);
    bluewake_controls_retrace(); assert(bluewake_controls_read_controller(&fast) && !fast.camera_click);
    // Actual virtual-controller events latch a complete click between VIs.
    SDL_FlushEvents(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_EVENT_GAMEPAD_BUTTON_UP);
    first.button(SDL_GAMEPAD_BUTTON_WEST,true); first.button(SDL_GAMEPAD_BUTTON_WEST,false);
    SDL_PumpEvents();
    while (SDL_PollEvent(&event)) bluewake_controls_action_event(&event);
    bluewake_controls_retrace(); assert(bluewake_controls_read_actions(&actions) && actions.sprint_pressed);
    bluewake_controls_retrace(); assert(bluewake_controls_read_actions(&actions) && !actions.sprint_pressed);
    first.button(SDL_GAMEPAD_BUTTON_NORTH,true);
    first.button(SDL_GAMEPAD_BUTTON_WEST,true);
    first.button(SDL_GAMEPAD_BUTTON_EAST,true);
    first.axis(SDL_GAMEPAD_AXIS_RIGHTX,32767); // Remapped movement, not the raw left stick.
    bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions) && actions.jump_pressed && actions.sprint_pressed && actions.first_person_down);
    assert(near(actions.movement_tilt,1));
    // Action output remains the same for every reader in this VI.
    assert(bluewake_controls_read_controller(&fast) && fast.camera_click);
    assert(bluewake_controls_read_actions(&actions) && actions.sprint_pressed);
    bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions) && !actions.jump_pressed && !actions.sprint_pressed && actions.first_person_down);
    menu_open(true);
    assert(bluewake_controls_read_actions(&actions) && actions.blocked && !actions.jump_pressed);
    assert_neutral(pad_read()); assert_neutral(camera_read());
    menu_open(false); bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions) && !actions.jump_pressed && !actions.sprint_pressed && !actions.first_person_down);
    first.neutral(); bluewake_controls_retrace();
    first.button(SDL_GAMEPAD_BUTTON_NORTH,true); first.button(SDL_GAMEPAD_BUTTON_WEST,true); first.button(SDL_GAMEPAD_BUTTON_EAST,true);
    bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions) && actions.jump_pressed && actions.sprint_pressed && actions.first_person_down);
    first.neutral(); bluewake_controls_retrace();
    assert(bluewake_controls_save());
    assert(bluewake_controls_select(returned.id));
    assert(snapshot().action_buttons[BLUEWAKE_ACTION_JUMP] == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_JUMP,SDL_GAMEPAD_BUTTON_EAST));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_FIRST_PERSON,SDL_GAMEPAD_BUTTON_WEST));
    assert(bluewake_controls_save() && bluewake_controls_load());
    assert(snapshot().action_buttons[BLUEWAKE_ACTION_JUMP] == SDL_GAMEPAD_BUTTON_EAST);
    assert(bluewake_controls_select(first.id));
    assert(snapshot().action_keys[BLUEWAKE_ACTION_JUMP][0] == SDL_SCANCODE_V);
    assert(snapshot().action_buttons[BLUEWAKE_ACTION_JUMP] == SDL_GAMEPAD_BUTTON_NORTH);
    assert(snapshot().action_buttons[BLUEWAKE_ACTION_FIRST_PERSON] == SDL_GAMEPAD_BUTTON_EAST);
    bluewake_controls_set_legacy_preferences(true,true,true,true);
    assert(snapshot().action_buttons[BLUEWAKE_ACTION_JUMP] == SDL_GAMEPAD_BUTTON_NORTH);
    assert(snapshot().action_keys[BLUEWAKE_ACTION_FIRST_PERSON][0] == SDL_SCANCODE_G);
    bluewake_controls_set_legacy_preferences(false,false,false,false);
    bluewake_controls_retrace();
    returned.button(SDL_GAMEPAD_BUTTON_EAST,true); returned.button(SDL_GAMEPAD_BUTTON_LEFT_STICK,true);
    bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions) && !actions.jump_pressed && !actions.sprint_pressed); // Unselected ignored.
    returned.neutral();
    assert(bluewake_controls_set_keyboard_enabled(false));
    event.type = SDL_EVENT_KEY_DOWN; event.key.scancode = SDL_SCANCODE_V;
    bluewake_controls_action_event(&event); bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions) && !actions.jump_pressed && !actions.sprint_held);
    first.detach();
    returned.detach();
    refresh_without_events();
    assert(bluewake_controls_devices(nullptr, 0) == 0 && bluewake_controls_selected() == 0);
    assert_neutral(camera_read());
    // Preserve the original NSO GameCube exception through schema migration;
    // an explicit binding can still use that shoulder if the player chooses.
    VirtualPad nso("BlueWake controls NSO",0x2073);
    refresh_without_events();
    assert(snapshot().action_buttons[BLUEWAKE_ACTION_JUMP] == -1);
    assert(bluewake_controls_save());
    auto nso_legacy = read_file(file);
    const auto nso_version = nso_legacy.find("version=3");
    assert(nso_version != std::string::npos);
    nso_legacy.replace(nso_version,9,"version=1");
    for (const auto* field : {"host_keys=","host_buttons="})
        for (size_t at=nso_legacy.find(field);at!=std::string::npos;at=nso_legacy.find(field))
            nso_legacy.erase(at,nso_legacy.find('\n',at)-at+1);
    write_file(file,nso_legacy);assert(bluewake_controls_load());
    assert(snapshot().action_buttons[BLUEWAKE_ACTION_JUMP] == -1);
    assert(snapshot().action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][0] == SDL_SCANCODE_TAB);
    assert(snapshot().action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS] == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    bluewake_controls_retrace();
    nso.button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,true);
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions) && !actions.jump_pressed);
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_JUMP,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER));
    nso.neutral();bluewake_controls_retrace();
    nso.button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,true);
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions) && actions.jump_pressed);
    nso.detach();refresh_without_events();
    assert(!std::filesystem::exists(dir / "controller_ports.dat"));
    assert(!std::filesystem::exists(dir / "controls.ini.tmp"));
    std::filesystem::remove_all(dir);
    SDL_Quit();
    std::puts("Controls backend: real-device profiles, reconnect, persistence, legacy overlays, selected mapped camera and menu-release gates passed");
    return 0;
}
