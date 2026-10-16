// Native host input for held LB/Tab equipment shortcuts. The game adapter
// performs ownership and procedure checks; this file never changes inventory.
#include "pc/pc_quick_items.h"

#if defined(__SWITCH__)
// This first migration targets the Windows SDL provider. Keep the
// Switch build linkable without extending its narrower SDL compatibility API.
extern "C" {
void pc_quick_items_filter(PADStatus*) {}
int pc_quick_items_pressed(PcQuickItemAction) { return 0; }
int pc_quick_items_held(PcQuickItemAction) { return 0; }
int pc_quick_items_enabled(void) { return 0; }
void pc_quick_items_set_enabled(int) {}
}
#else

#include "pc_internal.h"
#include "pc_quick_items_gate.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <cstdlib>
#include <cstring>

namespace {
static_assert(PC_QUICK_ITEM_TACT == 1 && PC_QUICK_ITEM_CANNON == 2 && PC_QUICK_ITEM_CRANE == 3);
static_assert(PAD_BUTTON_LEFT == pc::quick_items::left && PAD_BUTTON_RIGHT == pc::quick_items::right &&
              PAD_BUTTON_DOWN == pc::quick_items::down && PAD_BUTTON_UP == pc::quick_items::up);
pc::quick_items::Gate sGate;
pc::quick_items::Intent sIntent;
int sEnabled = -1;
uint64_t sEpoch = 1;
uint64_t sMappingFingerprint = 0;
uint64_t sKeyboardFingerprint = 0;
SDL_JoystickID sDevice = 0;
bool sSynthetic = false, sSourceKnown = false;
unsigned int sInputFrame = 0;

bool diagnosticInput() {
    return pc::gConfig.input != nullptr ||
           (pc::gConfig.smoke != nullptr && std::strcmp(pc::gConfig.smoke, "quick-items") == 0);
}

uint16_t rawDirections(SDL_Gamepad* pad) {
    if (pad == nullptr) return 0;
    uint16_t value = 0;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP)) value |= PAD_BUTTON_UP;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) value |= PAD_BUTTON_LEFT;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) value |= PAD_BUTTON_RIGHT;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) value |= PAD_BUTTON_DOWN;
    return value;
}

bool currentIntent() {
    if (!pc_quick_items_enabled() || sInputFrame != pc_frame_count() || sIntent.context == 0 ||
        pc::menuOpen()) return false;
    if (!sSynthetic && SDL_GetKeyboardFocus() == nullptr) return false;
    return pc_quick_items_context() == sIntent.context;
}
} // namespace

extern "C" {
int pc_quick_items_enabled(void) {
    if (sEnabled < 0) {
        const char* value = std::getenv("COS_QUICK_ITEMS");
        sEnabled = value != nullptr && (std::strcmp(value, "1") == 0 ||
                    std::strcmp(value, "on") == 0 || std::strcmp(value, "true") == 0);
    }
    return sEnabled;
}

void pc_quick_items_set_enabled(int enabled) {
    const int next = enabled ? 1 : 0;
    if (pc_quick_items_enabled() != next) {
        sEnabled = next;
        ++sEpoch;
        sIntent = {};
    }
}

void pc_quick_items_filter(PADStatus* status) {
    sIntent = {};
    if (status == nullptr) return;
    const bool enabled = pc_quick_items_enabled() != 0;
    if (!enabled) {
        // Default-off frames perform no SDL polling or actor/context search.
        // A previously consumed hold still must not leak into the native map.
        if (sGate.claimed() != 0) {
            status[0].button &= ~sGate.release_claims(status[0].button);
        }
        return;
    }
    const bool synthetic = diagnosticInput();
    bool modifier = false, focused = true;
    uint16_t held = 0, protectedDirections = 0;
    uint64_t mappingFingerprint = 0;
    uint64_t keyboardFingerprint = 0;
    SDL_JoystickID device = 0;
    if (synthetic) {
        // Explicit COS_INPUT MOD / quick-items smoke input, never a GC L bit.
        held = status[0].button & pc::quick_items::directions;
        modifier = pc::inputQuickItemModifier();
    } else {
        focused = SDL_GetKeyboardFocus() != nullptr;
        const s32 index = PADGetIndexForPort(0);
        SDL_Gamepad* pad = index >= 0 ? PADGetSDLGamepadForIndex((u32)index) : nullptr;
        if (pad != nullptr && !SDL_GamepadConnected(pad)) pad = nullptr;
        if (pad != nullptr) {
            device = SDL_GetGamepadID(pad);
            u32 mappingCount = 0;
            const PADButtonMapping* mappings = PADGetButtonMappings(0, &mappingCount);
            pc::quick_items::PhysicalProfile profile(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
                SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
                SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
            for (u32 i = 0; mappings != nullptr && i < mappingCount; ++i) {
                profile.observe(mappings[i].nativeButton, mappings[i].padButton);
            }
            u32 axisCount = 0;
            const PADAxisMapping* axes = PADGetAxisMappings(0, &axisCount);
            for (u32 i = 0; axes != nullptr && i < axisCount; ++i) {
                profile.observeAxis(axes[i].nativeAxis.nativeAxis, axes[i].nativeAxis.sign,
                                    axes[i].nativeButton, axes[i].padAxis);
            }
            mappingFingerprint = profile.fingerprint();
            if (profile.canonicalMapping()) {
                held = rawDirections(pad);
                modifier = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
            } else {
                // Custom physical controls remain native. Even a simultaneous
                // Tab+arrow shortcut must not strip their mapped directions.
                for (u32 i = 0; mappings != nullptr && i < mappingCount; ++i) {
                    if (mappings[i].nativeButton < SDL_GAMEPAD_BUTTON_COUNT &&
                        SDL_GetGamepadButton(pad, (SDL_GamepadButton)mappings[i].nativeButton)) {
                        protectedDirections |= mappings[i].padButton & pc::quick_items::directions;
                    }
                }
            }
        }
        int count = 0;
        const bool* keys = SDL_GetKeyboardState(&count);
        pc::quick_items::KeyboardProfile keyboard(SDL_SCANCODE_TAB, SDL_SCANCODE_UP,
            SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_DOWN);
        for (u32 port = 0; port < PAD_CHANMAX; ++port) {
            u32 buttonCount = 0, axisCount = 0;
            const PADKeyButtonBinding* buttons = PADGetKeyButtonBindings(port, &buttonCount);
            const PADKeyAxisBinding* axes = PADGetKeyAxisBindings(port, &axisCount);
            for (u32 i = 0; buttons != nullptr && i < buttonCount; ++i) {
                keyboard.observeButton(port, buttons[i].scancode, buttons[i].padButton);
                if (port == 0 && keys != nullptr && buttons[i].scancode >= 0 &&
                    buttons[i].scancode < count && keys[buttons[i].scancode]) {
                    protectedDirections |= buttons[i].padButton & pc::quick_items::directions;
                }
            }
            for (u32 i = 0; axes != nullptr && i < axisCount; ++i) {
                keyboard.observeAxis(port, axes[i].scancode, axes[i].padAxis, axes[i].influence);
            }
        }
        keyboardFingerprint = keyboard.fingerprint();
        const bool keyboardAvailable = keyboard.available();
        const bool tab = keyboardAvailable && focused && keys != nullptr &&
                         count > SDL_SCANCODE_TAB && keys[SDL_SCANCODE_TAB];
        modifier |= tab;
        if (keyboardAvailable && focused && keys != nullptr && count > SDL_SCANCODE_UP) {
            // Keyboard gameplay bindings are otherwise unchanged. Arrows are
            // always observed for release/edge history; only a held modifier
            // permits claiming them or producing an equipment intention.
            if (keys[SDL_SCANCODE_UP]) held |= PAD_BUTTON_UP;
            if (keys[SDL_SCANCODE_LEFT]) held |= PAD_BUTTON_LEFT;
            if (keys[SDL_SCANCODE_RIGHT]) held |= PAD_BUTTON_RIGHT;
            if (keys[SDL_SCANCODE_DOWN]) held |= PAD_BUTTON_DOWN;
        }
    }
    if (!sSourceKnown || sDevice != device || sSynthetic != synthetic ||
        sMappingFingerprint != mappingFingerprint || sKeyboardFingerprint != keyboardFingerprint) {
        ++sEpoch;
        sDevice = device;
        sMappingFingerprint = mappingFingerprint;
        sKeyboardFingerprint = keyboardFingerprint;
        sSynthetic = synthetic;
        sSourceKnown = true;
    }
    const uint64_t context = pc_quick_items_context();
    sIntent = sGate.sample({context, sEpoch, held, true, modifier,
                           pc::menuOpen() || !focused});
    sInputFrame = pc_frame_count();
    status[0].button &= ~(sIntent.claimed & ~protectedDirections);
}

int pc_quick_items_pressed(PcQuickItemAction action) {
    return action != PC_QUICK_ITEM_NONE && sIntent.pressed == (unsigned)action && currentIntent();
}
int pc_quick_items_held(PcQuickItemAction action) {
    return action != PC_QUICK_ITEM_NONE && sIntent.held == (unsigned)action && currentIntent();
}
} // extern "C"
#endif // __SWITCH__
