// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "controls_bindings.h"
#include <SDL3/SDL.h>
#include <dolphin/pad.h>
#include <array>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
struct SDL_Gamepad {
    uint32_t id;
    int port;
    SDL_GUID guid;
    std::string serial;
    bool connected = true;
    std::array<PADButtonMapping,12> buttons;
    std::array<PADAxisMapping,10> axes;
    PADDeadZones zones{true,true,8000,8000,31150,31150};
    std::array<Sint16,SDL_GAMEPAD_AXIS_COUNT> raw_axes{};
    std::array<bool,SDL_GAMEPAD_BUTTON_COUNT> raw_buttons{};
    uint16_t product = 0;
};
namespace {
constexpr PADButton pad_buttons[]={PAD_BUTTON_LEFT,PAD_BUTTON_RIGHT,PAD_BUTTON_DOWN,
    PAD_BUTTON_UP,PAD_TRIGGER_Z,PAD_TRIGGER_R,PAD_TRIGGER_L,PAD_BUTTON_A,
    PAD_BUTTON_B,PAD_BUTTON_X,PAD_BUTTON_Y,PAD_BUTTON_START};
std::vector<SDL_Gamepad*> pads;
std::array<PADKeyButtonBinding,12> keys;
std::array<PADKeyAxisBinding,10> key_axes;
bool keyboard_active=true;
std::array<bool,SDL_SCANCODE_COUNT> raw_keys{};
SDL_MouseButtonFlags raw_mouse;
unsigned writes, restores, reconciles;
SDL_Gamepad* port(unsigned n){for(auto* pad:pads)if(pad->connected&&pad->port==static_cast<int>(n))return pad;return nullptr;}
void defaults(SDL_Gamepad& pad){
    constexpr unsigned native[] = {SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
        SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
        SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_SOUTH,
        SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_START};
    for(unsigned i=0;i<12;++i)pad.buttons[i]={native[i],pad_buttons[i]};
    for(unsigned i=0;i<10;++i)pad.axes[i]={{static_cast<int>(i<8?i/2:i-4),
        i<8 && i/2%2 ? (i%2?AXIS_SIGN_POSITIVE:AXIS_SIGN_NEGATIVE) : (i%2?AXIS_SIGN_NEGATIVE:AXIS_SIGN_POSITIVE)},-1,static_cast<PADAxis>(i)};
    pad.axes[9].nativeAxis.sign=AXIS_SIGN_POSITIVE;
}
}
extern "C" {
void PADRefreshControllers(){++reconciles;}
u32 PADCount(){return static_cast<u32>(pads.size());}
SDL_Gamepad* PADGetSDLGamepadForIndex(u32 i){return i<pads.size()?pads[i]:nullptr;}
bool SDL_GamepadConnected(SDL_Gamepad* pad){return pad->connected;}
SDL_Gamepad* SDL_GetGamepadFromID(SDL_JoystickID id){for(auto* pad:pads)if(pad->connected&&pad->id==id)return pad;return nullptr;}
bool SDL_GetGamepadButton(SDL_Gamepad* pad,SDL_GamepadButton button){return pad->raw_buttons[button];}
Sint16 SDL_GetGamepadAxis(SDL_Gamepad* pad,SDL_GamepadAxis axis){return pad->raw_axes[axis];}
void SDL_LockJoysticks(){}
void SDL_UnlockJoysticks(){}
const bool* SDL_GetKeyboardState(int* count){if(count)*count=static_cast<int>(raw_keys.size());return raw_keys.data();}
SDL_MouseButtonFlags SDL_GetMouseState(float*,float*){return raw_mouse;}
const char* SDL_GetScancodeName(SDL_Scancode){return "Test key";}
const char* SDL_GetGamepadStringForButton(SDL_GamepadButton){return "Test button";}
SDL_JoystickID SDL_GetGamepadID(SDL_Gamepad* pad){return pad->id;}
const char* SDL_GetGamepadName(SDL_Gamepad*){return "Mock controller";}
const char* SDL_GetGamepadSerial(SDL_Gamepad* pad){return pad->serial.c_str();}
Uint16 SDL_GetGamepadProduct(SDL_Gamepad* pad){return pad->product;}
SDL_GUID SDL_StringToGUID(const char* text){
    SDL_GUID result{};
    const auto digit=[](char c){return c>='a'?c-'a'+10:c-'0';};
    for(unsigned i=0;i<16;++i)result.data[i]=static_cast<Uint8>(digit(text[i*2])*16+digit(text[i*2+1]));
    return result;
}
void SDL_GetJoystickGUIDInfo(SDL_GUID guid,Uint16* vendor,Uint16* product,Uint16* version,Uint16* crc){
    if(vendor)*vendor=0;if(product)*product=0;if(version)*version=0;if(crc)*crc=0;
    for(auto* pad:pads)if(std::memcmp(&pad->guid,&guid,sizeof guid)==0 && product)*product=pad->product;
}
SDL_GUID SDL_GetGamepadGUIDForID(SDL_JoystickID id){for(auto* pad:pads)if(pad->id==id)return pad->guid;return {};}
void SDL_GUIDToString(SDL_GUID guid,char* output,int capacity){
    constexpr char alphabet[]="0123456789abcdef";if(capacity<33)return;
    for(unsigned i=0;i<16;++i){output[i*2]=alphabet[guid.data[i]>>4];output[i*2+1]=alphabet[guid.data[i]&15];}output[32]=0;
}
int SDL_GetGamepadPlayerIndex(SDL_Gamepad* pad){return pad->port;}
bool SDL_SetGamepadPlayerIndex(SDL_Gamepad* pad,int index){pad->port=index;++writes;return true;}
PADKeyButtonBinding* PADGetKeyButtonBindings(u32,u32* count){*count=12;return keyboard_active?keys.data():nullptr;}
PADKeyAxisBinding* PADGetKeyAxisBindings(u32,u32* count){*count=10;return keyboard_active?key_axes.data():nullptr;}
void PADSetKeyboardActive(u32,BOOL active){keyboard_active=active!=FALSE;}
PADButtonMapping* PADGetButtonMappings(u32 p,u32* count){auto* pad=port(p);*count=pad?12:0;return pad?pad->buttons.data():nullptr;}
PADAxisMapping* PADGetAxisMappings(u32 p,u32* count){auto* pad=port(p);*count=pad?10:0;return pad?pad->axes.data():nullptr;}
PADDeadZones* PADGetDeadZones(u32 p){auto* pad=port(p);return pad?&pad->zones:nullptr;}
void PADSetButtonMapping(u32 p,PADButtonMapping mapping){auto* pad=port(p);assert(pad);for(auto& old:pad->buttons)if(old.padButton==mapping.padButton)old=mapping;++writes;}
void PADSetAxisMapping(u32 p,PADAxisMapping mapping){auto* pad=port(p);assert(pad);pad->axes[mapping.padAxis]=mapping;++writes;}
void PADRestoreDefaultMapping(u32 p){assert(port(p));defaults(*port(p));++restores;}
BOOL PADIsGCAdapter(u32){return FALSE;}
}
