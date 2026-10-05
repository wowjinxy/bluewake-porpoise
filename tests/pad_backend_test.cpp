// Exercise production SDL discovery and PAD mapping without a renderer or game.
#include <SDL3/SDL.h>
#include <aurora/aurora.h>
#include <dolphin/pad.h>
#include "input.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
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
        assert(id != 0 && SDL_IsGamepad(id));
        joystick = SDL_OpenJoystick(id);
        assert(joystick != nullptr);
        axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
        axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
    }
    void axis(SDL_GamepadAxis axis, Sint16 value) {
        assert(SDL_SetJoystickVirtualAxis(joystick, axis, value));
        SDL_UpdateJoysticks();
    }
    void button(SDL_GamepadButton button, bool down) {
        assert(SDL_SetJoystickVirtualButton(joystick, button, down));
        SDL_UpdateJoysticks();
    }
    void detach() {
        if (!id) return;
        SDL_CloseJoystick(joystick);
        joystick = nullptr;
        assert(SDL_DetachVirtualJoystick(id));
        id = 0;
    }
    ~VirtualPad() { detach(); }
};

void discard_hotplug_events() {
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_GAMEPAD_ADDED, SDL_EVENT_GAMEPAD_REMOVED);
    SDL_FlushEvents(SDL_EVENT_JOYSTICK_ADDED, SDL_EVENT_JOYSTICK_REMOVED);
}

u32 index_for(SDL_JoystickID id) {
    for (u32 i = 0; i < PADCount(); ++i) {
        auto* pad = PADGetSDLGamepadForIndex(i);
        if (pad && SDL_GetGamepadID(pad) == id) return i;
    }
    assert(false);
    return 0;
}

PADStatus read(unsigned port = 0) {
    PADStatus result[PAD_CHANMAX]{};
    PADRead(result);
    assert(result[port].err == PAD_ERR_NONE);
    return result[port];
}

void assert_neutral(const PADStatus& status) {
    assert(status.button == 0 && status.extButton == 0);
    assert(status.stickX == 0 && status.stickY == 0);
    assert(status.substickX == 0 && status.substickY == 0);
    assert(status.triggerLeft == 0 && status.triggerRight == 0);
    assert(status.analogA == 0 && status.analogB == 0);
}

void expect_default_ranges(VirtualPad& pad) {
    const std::array axes{SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
                         SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY};
    for (size_t i = 0; i < axes.size(); ++i) {
        pad.axis(axes[i], 32767);
        auto status = read();
        const std::array values{status.stickX, status.stickY, status.substickX, status.substickY};
        assert(i % 2 == 0 ? values[i] >= 126 : values[i] <= -126);
        pad.axis(axes[i], -32768);
        status = read();
        const std::array negative{status.stickX, status.stickY, status.substickX, status.substickY};
        assert(i % 2 == 0 ? negative[i] <= -126 : negative[i] >= 126);
        pad.axis(axes[i], 0);
    }
    assert_neutral(read());
}
}

int main() {
    PADRefreshControllers(); // Safe before SDL input initialization.
    assert(PADCount() == 0);
    // Isolate discovery from physical devices connected to the test host.
    const char* hints[] = {SDL_HINT_JOYSTICK_HIDAPI, SDL_HINT_JOYSTICK_DIRECTINPUT,
        SDL_HINT_XINPUT_ENABLED, SDL_HINT_JOYSTICK_RAWINPUT, SDL_HINT_JOYSTICK_WGI,
        SDL_HINT_JOYSTICK_GAMEINPUT, SDL_HINT_JOYSTICK_LINUX_CLASSIC,
        SDL_HINT_JOYSTICK_IOKIT, SDL_HINT_JOYSTICK_MFI};
    for (auto* hint : hints) SDL_SetHint(hint, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    assert(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMEPAD));
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto dir = std::filesystem::temp_directory_path() /
        ("bluewake-pad-backend-" + std::to_string(unique));
    assert(std::filesystem::create_directory(dir));
    const auto utf8path = dir.u8string();
    const std::string path(reinterpret_cast<const char*>(utf8path.c_str()));
    aurora::g_config.userPath = path.c_str();
    aurora::g_config.logLevel = LOG_INFO;
    assert(PADInit());
    PADStatus initial[PAD_CHANMAX]{};
    PADRead(initial); // Load keyboard mappings before edits, matching host startup.
    PADSetKeyboardActive(0, FALSE);

    {
        VirtualPad first("BlueWake backend A", 1);
        discard_hotplug_events();
        assert(SDL_GetGamepadFromID(first.id) == nullptr);
        PADRefreshControllers(); // Recover a deliberately discarded ADDED event.
        assert(PADCount() == 1);
        PADSetPortForIndex(index_for(first.id), 0);
        expect_default_ranges(first);

        u32 count = 0;
        assert(PADGetButtonMappings(0, &count) && count == PAD_BUTTON_COUNT);
        assert(PADGetAxisMappings(0, &count) && count == PAD_AXIS_COUNT);
        PADSetButtonMapping(0, {SDL_GAMEPAD_BUTTON_EAST, PAD_BUTTON_A});
        // A cleared half is neutral; the other half still has its full range.
        PADSetAxisMapping(0, {{-1, AXIS_SIGN_POSITIVE}, -1, PAD_AXIS_LEFT_X_POS});
        first.axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
        assert(read().stickX == 0);
        first.axis(SDL_GAMEPAD_AXIS_LEFTX, -32768);
        assert(read().stickX <= -126);
        first.axis(SDL_GAMEPAD_AXIS_LEFTX, 0);
        PADSetAxisMapping(0, {{-1, AXIS_SIGN_POSITIVE}, SDL_GAMEPAD_BUTTON_WEST, PAD_AXIS_LEFT_X_POS});
        first.button(SDL_GAMEPAD_BUTTON_WEST, true);
        assert(read().stickX >= 126);
        first.axis(SDL_GAMEPAD_AXIS_LEFTX, -32768);
        assert(read().stickX == 0); // Opposing full inputs cancel.
        first.button(SDL_GAMEPAD_BUTTON_WEST, false);
        first.axis(SDL_GAMEPAD_AXIS_LEFTX, 0);
        PADSetAxisMapping(0, {{SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_POSITIVE}, -1, PAD_AXIS_LEFT_X_POS});
        first.axis(SDL_GAMEPAD_AXIS_RIGHTX, -32768);
        assert(read().stickX == 0);
        first.axis(SDL_GAMEPAD_AXIS_RIGHTX, 32767);
        assert(read().stickX >= 126);
        first.axis(SDL_GAMEPAD_AXIS_RIGHTX, 0);
        PADSetAxisMapping(0, {{SDL_GAMEPAD_AXIS_COUNT, AXIS_SIGN_POSITIVE}, -1, PAD_AXIS_LEFT_X_POS});
        assert(read().stickX == 0); // Malformed source remains neutral.
        PADSetAxisMapping(0, {{SDL_GAMEPAD_AXIS_RIGHTX, static_cast<PADAxisSign>(0)}, -1, PAD_AXIS_LEFT_X_POS});
        assert(read().stickX == 0);
        PADSetAxisMapping(0, {{SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_POSITIVE}, -1, PAD_AXIS_LEFT_X_POS});
        PADGetDeadZones(0)->stickDeadZone = 12345;
        auto* opened = SDL_GetGamepadFromID(first.id);
        assert(aurora::input::add_controller(first.id) == first.id); // Duplicate queued ADDED.
        for (unsigned i = 0; i < 20; ++i) PADRefreshControllers();
        assert(PADCount() == 1 && SDL_GetGamepadFromID(first.id) == opened);
        assert(PADGetDeadZones(0)->stickDeadZone == 12345);
        first.button(SDL_GAMEPAD_BUTTON_EAST, true);
        first.button(SDL_GAMEPAD_BUTTON_BACK, true);
        first.axis(SDL_GAMEPAD_AXIS_RIGHTX, 32767);
        first.axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767);
        first.axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
        auto held = read();
        assert((held.button & PAD_BUTTON_A) != 0 && (held.extButton & PAD_BUTTON_BACK) != 0);
        assert(held.stickX >= 126 && held.substickX >= 126);
        assert(held.triggerLeft > 30 && held.triggerRight > 30);
        PADBlockInput(true);
        assert_neutral(read());
        PADBlockInput(false);
        for (unsigned i = 0; i < 4; ++i) assert_neutral(read());
        first.button(SDL_GAMEPAD_BUTTON_EAST, false);
        first.button(SDL_GAMEPAD_BUTTON_BACK, false);
        first.axis(SDL_GAMEPAD_AXIS_RIGHTX, 0);
        first.axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
        first.axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
        assert_neutral(read());
        first.button(SDL_GAMEPAD_BUTTON_EAST, true);
        first.button(SDL_GAMEPAD_BUTTON_BACK, true);
        first.axis(SDL_GAMEPAD_AXIS_RIGHTX, 32767);
        first.axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767);
        first.axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
        held = read();
        assert((held.button & PAD_BUTTON_A) != 0 && (held.extButton & PAD_BUTTON_BACK) != 0);
        assert(held.stickX >= 126 && held.substickX >= 126);
        assert(held.triggerLeft > 30 && held.triggerRight > 30);

        VirtualPad second("BlueWake backend B", 2);
        discard_hotplug_events();
        PADRefreshControllers();
        assert(PADCount() == 2);
        PADSetPortForIndex(index_for(second.id), 1);
        second.button(SDL_GAMEPAD_BUTTON_SOUTH, true);
        assert((read(1).button & PAD_BUTTON_A) != 0);
        assert(PADGetIndexForPort(0) == static_cast<s32>(index_for(first.id)));
        const auto removed = second.id;
        second.detach();
        discard_hotplug_events();
        PADRefreshControllers(); // Recover a deliberately discarded REMOVED event.
        assert(PADCount() == 1 && SDL_GetGamepadFromID(removed) == nullptr);
        assert(aurora::input::add_controller(removed) == SDL_JoystickID(-1));
        assert(PADCount() == 1);
        first.detach();
        discard_hotplug_events();
        PADRefreshControllers();
        assert(PADCount() == 0);
    }

    // Keyboard controls use the same gate. Feed the production key latch on
    // each sample to simulate keys held while the settings menu is closing.
    PADSetKeyboardActive(0, TRUE);
    assert(PADSetKeyButtonBinding(0, {SDL_SCANCODE_J, PAD_BUTTON_A}));
    assert(PADSetKeyAxisBinding(0, {SDL_SCANCODE_W, PAD_AXIS_LEFT_Y_POS, 1}));
    assert(PADSetKeyAxisBinding(0, {SDL_SCANCODE_H, PAD_AXIS_RIGHT_X_POS, 1}));
    assert(PADSetKeyAxisBinding(0, {SDL_SCANCODE_E, PAD_AXIS_TRIGGER_L, 1}));
    const auto keys = std::array{SDL_SCANCODE_J, SDL_SCANCODE_W, SDL_SCANCODE_H, SDL_SCANCODE_E};
    auto held_keyboard = [&] {
        for (auto key : keys) PADLatchKeyEvent(key, 1);
        return read();
    };
    auto keyboard = held_keyboard();
    assert((keyboard.button & PAD_BUTTON_A) != 0 && keyboard.stickY == 127 && keyboard.substickX == 127);
    assert(keyboard.triggerLeft == 255);
    PADBlockInput(true);
    assert_neutral(held_keyboard());
    PADBlockInput(false);
    for (unsigned i = 0; i < 4; ++i) assert_neutral(held_keyboard());
    for (auto key : keys) PADLatchKeyEvent(key, 0);
    SDL_Delay(2);
    assert_neutral(read());
    keyboard = held_keyboard();
    assert((keyboard.button & PAD_BUTTON_A) != 0 && keyboard.stickY == 127 && keyboard.substickX == 127);
    for (auto key : keys) PADLatchKeyEvent(key, 0);
    SDL_Delay(2);
    (void)read();
    PADSetKeyboardActive(0, FALSE);

    PADStatus virtual_status{};
    virtual_status.button = PAD_BUTTON_A;
    virtual_status.extButton = PAD_BUTTON_BACK;
    virtual_status.stickX = 127;
    virtual_status.substickY = -127;
    virtual_status.triggerLeft = 255;
    virtual_status.triggerRight = 255;
    virtual_status.analogA = 127;
    virtual_status.analogB = 64;
    PADSetVirtualStatus(0, &virtual_status);
    assert(read().analogA == 127);
    PADBlockInput(true);
    assert_neutral(read());
    PADBlockInput(false);
    for (unsigned i = 0; i < 4; ++i) assert_neutral(read());
    PADStatus released{};
    PADSetVirtualStatus(0, &released);
    assert_neutral(read());
    PADSetVirtualStatus(0, &virtual_status);
    const auto resumed = read();
    assert(resumed.button == virtual_status.button && resumed.extButton == virtual_status.extButton);
    assert(resumed.stickX == 127 && resumed.substickY == -127);
    assert(resumed.triggerLeft == 255 && resumed.triggerRight == 255);
    assert(resumed.analogA == 127 && resumed.analogB == 64);
    PADClearAllVirtualStatus();
    std::filesystem::remove_all(dir);
    SDL_Quit();
    std::puts("PAD backend: lost/duplicate hotplug, independent/unbound mappings and complete menu-close suppression passed");
    return 0;
}
