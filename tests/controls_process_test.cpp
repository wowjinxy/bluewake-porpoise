// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "controls_bindings.h"
#include <SDL3/SDL.h>
#include <dolphin/pad.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "controls_test_support.h"

namespace {
void install(SDL_Gamepad& alpha, SDL_Gamepad& beta) {
    // Deliberately share a GUID: serial must distinguish the profiles and the
    // manually preferred device after every SDL instance ID changes.
    alpha.guid.data[0] = beta.guid.data[0] = 0x44;
    alpha.guid.data[1] = beta.guid.data[1] = 0x55;
    defaults(alpha); defaults(beta);
    pads = {&alpha,&beta};
    for (unsigned i=0;i<12;++i) keys[i] = {SDL_SCANCODE_Z,pad_buttons[i]};
    for (unsigned i=0;i<10;++i) key_axes[i] = {SDL_SCANCODE_C,static_cast<PADAxis>(i),32767};
}

std::string utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()),text.size()};
}

void write_profile(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    // This exact file belongs to this CTest fixture. No recursive cleanup or
    // external settings directories are involved, including after a failed run.
    std::filesystem::remove(dir/"controls.ini");
    SDL_Gamepad alpha{41,0,{},"serial alpha"}, beta{42,-1,{},"serial beta"};
    install(alpha,beta);
    const auto name = utf8(dir);
    assert(bluewake_controls_init(name.c_str()));
    assert(bluewake_controls_select(alpha.id));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_JUMP,SDL_GAMEPAD_BUTTON_WEST));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_SPRINT,SDL_GAMEPAD_BUTTON_NORTH));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_FIRST_PERSON,SDL_GAMEPAD_BUTTON_LEFT_STICK));
    assert(bluewake_controls_set_quick_items_trigger(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    assert(bluewake_controls_select(beta.id));
    assert(bluewake_controls_set_keyboard_enabled(true));
    assert(bluewake_controls_set_key(false,7,SDL_SCANCODE_B));
    assert(bluewake_controls_set_key(true,0,SDL_SCANCODE_N));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_JUMP,0,SDL_SCANCODE_V));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_JUMP,1,PAD_KEY_MOUSE_X1));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_SPRINT,0,SDL_SCANCODE_C));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_SPRINT,1,PAD_KEY_MOUSE_X2));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_FIRST_PERSON,0,SDL_SCANCODE_G));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_FIRST_PERSON,1,PAD_KEY_MOUSE_MIDDLE));
    assert(bluewake_controls_set_button(7,SDL_GAMEPAD_BUTTON_NORTH));
    assert(bluewake_controls_set_axis(0,{SDL_GAMEPAD_AXIS_RIGHTX,1,-1}));
    assert(bluewake_controls_set_axis(1,{SDL_GAMEPAD_AXIS_RIGHTX,-1,-1}));
    assert(bluewake_controls_set_dead_zones({true,false,3210,4567,23000,24000}));
    assert(bluewake_controls_set_invert(true,false,false,true));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_JUMP,SDL_GAMEPAD_BUTTON_EAST));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_SPRINT,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_FIRST_PERSON,SDL_GAMEPAD_BUTTON_NORTH));
    assert(bluewake_controls_set_quick_items_trigger(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
    assert(bluewake_controls_save());
    assert(!bluewake_controls_dirty() && !std::filesystem::exists(dir/"controls.ini.tmp"));
    std::cout << "Saved custom controls and same-GUID serial-specific preference for a fresh process\n";
}

void read_profile(const std::filesystem::path& dir) {
    assert(std::filesystem::is_regular_file(dir/"controls.ini"));
    SDL_Gamepad alpha{101,0,{},"serial alpha"}, beta{202,-1,{},"serial beta"};
    install(alpha,beta);
    const auto name = utf8(dir);
    assert(bluewake_controls_init(name.c_str()));
    assert(bluewake_controls_selected() == beta.id && beta.port == 0 && alpha.port != 0);
    assert(!bluewake_controls_automatic() && !bluewake_controls_using_fallback());
    BluewakeControlsSnapshot state{};
    bluewake_controls_snapshot(&state);
    assert(state.keyboard_enabled && keyboard_active);
    assert(state.key_buttons[7] == SDL_SCANCODE_B && state.key_axes[0] == SDL_SCANCODE_N);
    assert(state.action_keys[BLUEWAKE_ACTION_JUMP][0] == SDL_SCANCODE_V);
    assert(state.action_keys[BLUEWAKE_ACTION_JUMP][1] == PAD_KEY_MOUSE_X1);
    assert(state.action_keys[BLUEWAKE_ACTION_SPRINT][0] == SDL_SCANCODE_C);
    assert(state.action_keys[BLUEWAKE_ACTION_SPRINT][1] == PAD_KEY_MOUSE_X2);
    assert(state.action_keys[BLUEWAKE_ACTION_FIRST_PERSON][0] == SDL_SCANCODE_G);
    assert(state.action_keys[BLUEWAKE_ACTION_FIRST_PERSON][1] == PAD_KEY_MOUSE_MIDDLE);
    assert(state.action_buttons[BLUEWAKE_ACTION_JUMP] == SDL_GAMEPAD_BUTTON_EAST);
    assert(state.action_buttons[BLUEWAKE_ACTION_SPRINT] == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    assert(state.action_buttons[BLUEWAKE_ACTION_FIRST_PERSON] == SDL_GAMEPAD_BUTTON_NORTH);
    assert(state.quick_items_trigger==SDL_GAMEPAD_AXIS_RIGHT_TRIGGER&&state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS]==-1);
    assert(state.controller_buttons[7] == SDL_GAMEPAD_BUTTON_NORTH);
    assert(state.controller_axes[0].axis == SDL_GAMEPAD_AXIS_RIGHTX && state.controller_axes[0].sign == 1);
    assert(state.controller_axes[1].axis == SDL_GAMEPAD_AXIS_RIGHTX && state.controller_axes[1].sign == -1);
    assert(state.dead_zones.enabled && !state.dead_zones.emulate_triggers);
    assert(state.dead_zones.stick == 3210 && state.dead_zones.camera == 4567);
    assert(state.dead_zones.trigger_left == 23000 && state.dead_zones.trigger_right == 24000);
    assert(state.invert_stick_x && !state.invert_stick_y && !state.invert_camera_x && state.invert_camera_y);
    assert(beta.buttons[7].nativeButton == SDL_GAMEPAD_BUTTON_NORTH);
    assert(beta.axes[0].nativeAxis.nativeAxis == SDL_GAMEPAD_AXIS_RIGHTX && beta.axes[0].nativeAxis.sign == AXIS_SIGN_NEGATIVE);
    assert(beta.zones.useDeadzones && !beta.zones.emulateTriggers && beta.zones.stickDeadZone == 3210);

    BluewakeControlsActions action{};
    bluewake_controls_retrace(); // Prime held-source suppression with neutral inputs.
    alpha.raw_buttons[SDL_GAMEPAD_BUTTON_WEST] = true;
    bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&action) && !action.jump_pressed && !action.sprint_pressed && !action.first_person_down);
    beta.raw_buttons[SDL_GAMEPAD_BUTTON_EAST] = true;
    beta.raw_buttons[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = true;
    beta.raw_buttons[SDL_GAMEPAD_BUTTON_NORTH] = true;
    beta.raw_axes[SDL_GAMEPAD_AXIS_RIGHTX] = -25000;
    bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&action) && action.jump_pressed && action.sprint_pressed && action.first_person_down);
    assert(action.movement_tilt > 0.7f);
    beta.raw_buttons.fill(false); raw_keys[SDL_SCANCODE_V] = raw_keys[SDL_SCANCODE_C] = raw_keys[SDL_SCANCODE_G] = true;
    bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&action) && action.jump_pressed && action.sprint_held && action.first_person_down);

    assert(bluewake_controls_select(alpha.id)); bluewake_controls_snapshot(&state);
    assert(state.action_buttons[BLUEWAKE_ACTION_JUMP] == SDL_GAMEPAD_BUTTON_WEST);
    assert(state.action_buttons[BLUEWAKE_ACTION_SPRINT] == SDL_GAMEPAD_BUTTON_NORTH);
    assert(state.action_buttons[BLUEWAKE_ACTION_FIRST_PERSON] == SDL_GAMEPAD_BUTTON_LEFT_STICK);
    assert(state.quick_items_trigger==SDL_GAMEPAD_AXIS_LEFT_TRIGGER&&state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS]==-1);
    assert(state.action_keys[BLUEWAKE_ACTION_JUMP][0] == SDL_SCANCODE_V);
    assert(!std::filesystem::exists(dir/"controller_ports.dat"));
    std::cout << "Fresh process loaded actions, mappings, GUID/serial preference, dead zones and inversions\n";
}
}

int main(int argc,char** argv) {
    assert(argc == 3);
    const auto dir = std::filesystem::u8path(argv[2]);
    const std::string mode = argv[1];
    if (mode == "write") write_profile(dir);
    else if (mode == "read") read_profile(dir);
    else if (mode == "cleanup") {
        std::filesystem::remove(dir/"controls.ini");
        assert(!std::filesystem::exists(dir/"controls.ini.tmp"));
        assert(std::filesystem::remove(dir)); // Empty fixture directory only.
    } else assert(false);
    return 0;
}
