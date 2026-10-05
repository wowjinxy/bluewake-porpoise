// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "controls_menu.h"
#include "controls_test_support.h"
#include <chrono>
#include <filesystem>
#include <iostream>

namespace {
bool blocked;
SDL_Event key_event(SDL_Scancode key){
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=key;return event;
}
SDL_Event controller_button(unsigned id,SDL_GamepadButton button){
    SDL_Event event{};event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.which=id;event.gbutton.button=button;return event;
}
}
extern "C" void PADBlockInput(bool value){blocked=value;}

int main(){
    const auto folder=std::filesystem::temp_directory_path()/
        ("bluewake-capture-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(folder));
    const auto utf8=folder.u8string();
    SDL_Gamepad controller{11,0,{},"capture-device"};defaults(controller);pads={&controller};
    for(unsigned i=0;i<12;++i)keys[i]={SDL_SCANCODE_J,pad_buttons[i]};
    for(unsigned i=0;i<10;++i)key_axes[i]={SDL_SCANCODE_D,static_cast<PADAxis>(i),32767};
    assert(bluewake_controls_init(reinterpret_cast<const char*>(utf8.c_str())));
    assert(!bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_KEY_BUTTON,7));
    bluewake_controls_menu_set_open(true);assert(blocked);

    raw_keys[SDL_SCANCODE_K]=true;
    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_KEY_BUTTON,7));
    auto event=key_event(SDL_SCANCODE_U);assert(bluewake_controls_menu_event(&event));
    BluewakeControlsSnapshot snapshot{};bluewake_controls_snapshot(&snapshot);assert(snapshot.key_buttons[7]==SDL_SCANCODE_J);
    raw_keys.fill(false);event=key_event(SDL_SCANCODE_F6);
    assert(bluewake_controls_menu_event(&event));assert(bluewake_controls_menu_capturing());
    event=key_event(SDL_SCANCODE_U);assert(bluewake_controls_menu_event(&event));
    assert(!bluewake_controls_menu_capturing());bluewake_controls_snapshot(&snapshot);assert(snapshot.key_buttons[7]==SDL_SCANCODE_U);

    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_KEY_AXIS,0));
    event=key_event(SDL_SCANCODE_ESCAPE);assert(bluewake_controls_menu_event(&event));
    assert(!bluewake_controls_menu_capturing() && bluewake_controls_menu_is_open());

    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_KEY_BUTTON,8));
    event={};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_X1;
    assert(bluewake_controls_menu_event(&event));bluewake_controls_snapshot(&snapshot);assert(snapshot.key_buttons[8]==PAD_KEY_MOUSE_X1);

    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_CONTROLLER_BUTTON,7));
    event=controller_button(11,SDL_GAMEPAD_BUTTON_BACK);assert(bluewake_controls_menu_event(&event));
    assert(bluewake_controls_menu_capturing() && bluewake_controls_menu_is_open());
    event=controller_button(12,SDL_GAMEPAD_BUTTON_NORTH);assert(bluewake_controls_menu_event(&event));assert(bluewake_controls_menu_capturing());
    event=controller_button(11,SDL_GAMEPAD_BUTTON_NORTH);assert(bluewake_controls_menu_event(&event));
    bluewake_controls_snapshot(&snapshot);assert(snapshot.controller_buttons[7]==SDL_GAMEPAD_BUTTON_NORTH);

    controller.raw_axes[SDL_GAMEPAD_AXIS_LEFTX]=25000;
    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_CONTROLLER_AXIS,4));
    event={};event.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;event.gaxis.which=11;event.gaxis.axis=SDL_GAMEPAD_AXIS_LEFTY;event.gaxis.value=-25000;
    assert(bluewake_controls_menu_event(&event));assert(bluewake_controls_menu_capturing());
    controller.raw_axes.fill(0);assert(bluewake_controls_menu_event(&event));
    assert(!bluewake_controls_menu_capturing());bluewake_controls_snapshot(&snapshot);
    assert(snapshot.controller_axes[4].axis==SDL_GAMEPAD_AXIS_LEFTY && snapshot.controller_axes[4].sign==-1);

    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_KEY_ACTION,BLUEWAKE_ACTION_JUMP * 2));
    event=key_event(SDL_SCANCODE_F1);assert(bluewake_controls_menu_event(&event));assert(bluewake_controls_menu_capturing());
    event=key_event(SDL_SCANCODE_V);assert(bluewake_controls_menu_event(&event));
    bluewake_controls_snapshot(&snapshot);assert(snapshot.action_keys[BLUEWAKE_ACTION_JUMP][0]==SDL_SCANCODE_V);
    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_KEY_ACTION,BLUEWAKE_ACTION_SPRINT * 2 + 1));
    event={};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_X2;
    assert(bluewake_controls_menu_event(&event));bluewake_controls_snapshot(&snapshot);
    assert(snapshot.action_keys[BLUEWAKE_ACTION_SPRINT][1]==PAD_KEY_MOUSE_X2);
    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_CONTROLLER_ACTION,BLUEWAKE_ACTION_FIRST_PERSON));
    event=controller_button(12,SDL_GAMEPAD_BUTTON_EAST);assert(bluewake_controls_menu_event(&event));assert(bluewake_controls_menu_capturing());
    event=controller_button(11,SDL_GAMEPAD_BUTTON_BACK);assert(bluewake_controls_menu_event(&event));assert(bluewake_controls_menu_capturing());
    event=controller_button(11,SDL_GAMEPAD_BUTTON_EAST);assert(bluewake_controls_menu_event(&event));
    bluewake_controls_snapshot(&snapshot);assert(snapshot.action_buttons[BLUEWAKE_ACTION_FIRST_PERSON]==SDL_GAMEPAD_BUTTON_EAST);
    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_KEY_ACTION,BLUEWAKE_ACTION_QUICK_ITEMS * 2));
    event=key_event(SDL_SCANCODE_TAB);assert(bluewake_controls_menu_event(&event));
    bluewake_controls_snapshot(&snapshot);assert(snapshot.action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][0]==SDL_SCANCODE_TAB);
    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_CONTROLLER_ACTION,BLUEWAKE_ACTION_QUICK_ITEMS));
    event=controller_button(11,SDL_GAMEPAD_BUTTON_BACK);assert(bluewake_controls_menu_event(&event));assert(bluewake_controls_menu_capturing());
    event=controller_button(11,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);assert(bluewake_controls_menu_event(&event));
    bluewake_controls_snapshot(&snapshot);assert(snapshot.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS]==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    assert(!bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_KEY_ACTION,BLUEWAKE_CONTROLS_ACTIONS * 2));
    assert(!bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_CONTROLLER_ACTION,BLUEWAKE_CONTROLS_ACTIONS));

    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_CONTROLLER_BUTTON,8));
    controller.connected=false;bluewake_controls_refresh();
    event=key_event(SDL_SCANCODE_U);assert(bluewake_controls_menu_event(&event));assert(!bluewake_controls_menu_capturing());
    controller.connected=true;bluewake_controls_refresh();
    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_KEY_AXIS,1));
    bluewake_controls_menu_set_open(false);assert(!blocked && !bluewake_controls_menu_capturing());
    assert(!bluewake_controls_dirty());assert(std::filesystem::is_regular_file(folder/"controls.ini"));
    assert(!bluewake_controls_menu_event(&event));
    std::filesystem::remove(folder/"controls.ini");std::filesystem::remove(folder);
    std::cout<<"Capture release arming, reserved hotkeys, cancellation, mouse binding, selected device, axis mapping, disconnect and menu-close persistence passed\n";
}
