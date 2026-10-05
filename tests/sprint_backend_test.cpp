#ifdef NDEBUG
#undef NDEBUG
#endif
#include "controls_bindings.h"
#include "sprint_input.h"
#include <SDL3/SDL.h>
#include <aurora/aurora.h>
#include <dolphin/pad.h>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <cstdio>
namespace aurora {AuroraConfig g_config{};}
struct Pad {
 SDL_JoystickID id;SDL_Joystick* joystick;
 Pad(uint16_t product) {
  SDL_VirtualJoystickDesc d{};SDL_INIT_INTERFACE(&d);d.type=SDL_JOYSTICK_TYPE_GAMEPAD;
  d.vendor_id=0xf00d;d.product_id=product;d.naxes=SDL_GAMEPAD_AXIS_COUNT;d.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
  d.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;d.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;d.name="Sprint fixture";
  id=SDL_AttachVirtualJoystick(&d);assert(id&&SDL_IsGamepad(id));joystick=SDL_OpenJoystick(id);assert(joystick);
  for(int i=0;i<SDL_GAMEPAD_AXIS_COUNT;++i)assert(SDL_SetJoystickVirtualAxis(joystick,i,i>=SDL_GAMEPAD_AXIS_LEFT_TRIGGER?-32768:0));
  SDL_UpdateJoysticks();
 }
 void button(bool down){assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_LEFT_STICK,down));SDL_UpdateJoysticks();}
 void axis(int source,Sint16 value){assert(SDL_SetJoystickVirtualAxis(joystick,source,value));SDL_UpdateJoysticks();}
 void detach(){if(!id)return;SDL_CloseJoystick(joystick);assert(SDL_DetachVirtualJoystick(id));id=0;}
 ~Pad(){detach();}
};
static BwSprintInputState state{};
static BluewakeControlsActions actions;
static bool sample() {
 bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions));
 BwSprintInput input{actions.generation,actions.blocked,actions.sprint_held,actions.sprint_keyboard_pressed,
  actions.sprint_controller_down,actions.sprint_pressed,false,actions.movement_tilt};
 return bw_sprint_input_step(&state,&input);
}
static void refresh(){SDL_PumpEvents();bluewake_controls_refresh();}
static void tap(SDL_Scancode key) {
 SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=key;bluewake_controls_action_event(&event);
 event.type=SDL_EVENT_KEY_UP;bluewake_controls_action_event(&event);
}
int main(){
 for(const auto* hint:{SDL_HINT_JOYSTICK_HIDAPI,SDL_HINT_JOYSTICK_DIRECTINPUT,SDL_HINT_XINPUT_ENABLED,
   SDL_HINT_JOYSTICK_RAWINPUT,SDL_HINT_JOYSTICK_WGI,SDL_HINT_JOYSTICK_GAMEINPUT,SDL_HINT_JOYSTICK_LINUX_CLASSIC,
   SDL_HINT_JOYSTICK_IOKIT,SDL_HINT_JOYSTICK_MFI})SDL_SetHint(hint,"0");
 SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");assert(SDL_Init(SDL_INIT_EVENTS|SDL_INIT_GAMEPAD));
 const auto dir=std::filesystem::temp_directory_path()/("sprint-backend-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 assert(std::filesystem::create_directory(dir));const auto path=dir.string();aurora::g_config.userPath=path.c_str();aurora::g_config.logLevel=LOG_INFO;
 assert(PADInit());PADStatus initial[PAD_CHANMAX]{};PADRead(initial);assert(bluewake_controls_init(path.c_str()));
 Pad first(87),other(88);refresh();assert(bluewake_controls_select(first.id));
 assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_SPRINT,SDL_GAMEPAD_BUTTON_LEFT_STICK));
 assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_SPRINT,0,SDL_SCANCODE_C));
 bw_sprint_input_reset(&state);assert(!sample());assert(!sample());
 // Actual selected-controller down+edge; unselected device cannot start Sprint.
 other.axis(SDL_GAMEPAD_AXIS_LEFTX,32767);other.button(true);assert(!sample());assert(!actions.sprint_controller_down&&!actions.sprint_pressed);
 first.axis(SDL_GAMEPAD_AXIS_LEFTX,32767);first.button(true);assert(sample());assert(actions.sprint_controller_down&&actions.sprint_pressed);
 assert(sample());assert(actions.sprint_controller_down&&!actions.sprint_pressed);first.button(false);assert(sample());
 first.axis(SDL_GAMEPAD_AXIS_LEFTX,0);for(int i=0;i<7;++i)assert(sample());assert(!sample());
 // Keyboard completed taps are backend-delivered once; no SDL/OS keyboard injection.
 assert(bluewake_sprint_configure_modes(BW_SPRINT_TOGGLE,BW_SPRINT_HOLD));assert(!sample());assert(!sample());
 tap(SDL_SCANCODE_C);assert(sample());assert(actions.sprint_keyboard_pressed&&!actions.sprint_held);
 assert(sample());assert(!actions.sprint_keyboard_pressed);tap(SDL_SCANCODE_C);assert(!sample());
 assert(bluewake_controls_set_keyboard_enabled(false));assert(!sample());tap(SDL_SCANCODE_C);assert(!sample());assert(!actions.sprint_keyboard_pressed);
 assert(bluewake_controls_set_keyboard_enabled(true));assert(!sample());assert(!sample());
 // Mode change while held must suppress the actual SDL button until released.
 first.button(true);assert(sample());assert(bluewake_sprint_configure_modes(BW_SPRINT_HOLD,BW_SPRINT_TOGGLE));
 assert(!sample());assert(!sample());assert(!actions.sprint_controller_down&&!actions.sprint_pressed);
 first.button(false);assert(!sample());first.button(true);assert(sample());first.button(false);assert(sample());
 bluewake_controls_set_input_blocked(true);assert(!sample());PADBlockInput(true);PADStatus blocked[PAD_CHANMAX]{};PADRead(blocked);assert(!blocked[0].button);
 first.button(true);bluewake_controls_set_input_blocked(false);PADBlockInput(false);assert(!sample());assert(!sample());
 first.button(false);assert(!sample());first.button(true);assert(sample());
 const auto epoch=actions.generation;
 assert(bluewake_controls_set_axis(0,{SDL_GAMEPAD_AXIS_RIGHTX,1,-1}));assert(!sample());assert(actions.generation!=epoch);assert(!sample());
 first.button(false);first.axis(SDL_GAMEPAD_AXIS_RIGHTX,32767);assert(!sample());first.button(true);assert(sample());
 first.detach();refresh();assert(!sample());assert(!sample());
 Pad replacement(87);replacement.axis(SDL_GAMEPAD_AXIS_LEFTX,32767);replacement.button(true);refresh();assert(!sample());assert(!sample());
 assert(bluewake_controls_select(replacement.id));assert(!sample());replacement.button(false);assert(!sample());
 replacement.axis(SDL_GAMEPAD_AXIS_RIGHTX,32767);replacement.button(true);assert(sample());
 bluewake_controls_cancel_actions();assert(!sample());assert(!sample());replacement.button(false);assert(!sample());
 replacement.detach();other.detach();SDL_Quit();
 puts("PASS actual SDL virtual/PAD/controls Sprint signals, selection/hotplug/rebind/menu/mode release gates");
}
