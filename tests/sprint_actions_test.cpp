#ifdef NDEBUG
#undef NDEBUG
#endif
#include "controls_bindings.h"
#include "controls_test_support.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <cstdio>
static BluewakeControlsActions sample(){bluewake_controls_retrace();BluewakeControlsActions a{};assert(bluewake_controls_read_actions(&a));return a;}
int main(){
 SDL_Gamepad pad{101,0,{},"sprint-keys"};pad.guid=SDL_StringToGUID("00112233445566778899aabbccddeeff");defaults(pad);pads={&pad};
 const auto dir=std::filesystem::temp_directory_path()/("sprint-actions-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 assert(std::filesystem::create_directory(dir));assert(bluewake_controls_init(dir.string().c_str()));
 assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_SPRINT,0,SDL_SCANCODE_C));
 assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_SPRINT,1,SDL_SCANCODE_V));sample();sample();
 raw_keys[SDL_SCANCODE_C]=true;auto a=sample();assert(a.sprint_held&&a.sprint_keyboard_pressed);
 a=sample();assert(a.sprint_held&&!a.sprint_keyboard_pressed);
 raw_keys[SDL_SCANCODE_V]=true;a=sample();assert(a.sprint_held&&a.sprint_keyboard_pressed);
 raw_keys[SDL_SCANCODE_C]=false;a=sample();assert(a.sprint_held&&!a.sprint_keyboard_pressed);
 raw_keys[SDL_SCANCODE_V]=false;a=sample();assert(!a.sprint_held&&!a.sprint_keyboard_pressed);
 // An epoch suppresses both held keys independently until each source releases.
 raw_keys[SDL_SCANCODE_C]=raw_keys[SDL_SCANCODE_V]=true;sample();bluewake_controls_cancel_actions();
 a=sample();assert(!a.sprint_held&&!a.sprint_keyboard_pressed);
 raw_keys[SDL_SCANCODE_C]=false;sample();raw_keys[SDL_SCANCODE_C]=true;
 a=sample();assert(a.sprint_held&&a.sprint_keyboard_pressed); // V still suppressed.
 bluewake_controls_set_input_blocked(true);a=sample();assert(a.blocked&&!a.sprint_held);
 bluewake_controls_set_input_blocked(false);a=sample();assert(!a.sprint_held&&!a.sprint_keyboard_pressed);
 raw_keys[SDL_SCANCODE_C]=raw_keys[SDL_SCANCODE_V]=false;sample();
 SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=SDL_SCANCODE_C;
 bluewake_controls_action_event(&event);event.type=SDL_EVENT_KEY_UP;bluewake_controls_action_event(&event);
 a=sample();assert(!a.sprint_held&&a.sprint_keyboard_pressed);assert(!sample().sprint_keyboard_pressed);
 event.type=SDL_EVENT_KEY_DOWN;event.key.repeat=true;bluewake_controls_action_event(&event);assert(!sample().sprint_keyboard_pressed);
 assert(bluewake_controls_set_keyboard_enabled(false));event.key.repeat=false;bluewake_controls_action_event(&event);assert(!sample().sprint_keyboard_pressed);
 puts("PASS actual controls publication with synthetic keyboard levels/events, alternate keys and release epochs");
}
