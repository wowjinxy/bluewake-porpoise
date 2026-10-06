// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "controls_bindings.h"
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <dolphin/pad.h>
#include <array>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "controls_test_support.h"
namespace {
std::string read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void write(const std::filesystem::path& path,const std::string& data){std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<data;assert(out.good());}
bool save(){
    const bool saved=bluewake_controls_save();
    const int saved_errno=errno;
#ifdef _WIN32
    const DWORD windows_error=GetLastError();
#endif
    if(!saved) {
        std::cerr<<"Controls save failed: "<<bluewake_controls_error()<<"; errno="<<saved_errno;
#ifdef _WIN32
        std::cerr<<"; last Windows error="<<windows_error;
#endif
        std::cerr<<'\n';
    }
    return saved;
}
void expect_rejected(const std::filesystem::path& path,const std::string& data){
    BluewakeControlsSnapshot before{},after{};bluewake_controls_snapshot(&before);
    const auto id=bluewake_controls_selected();const auto was_dirty=bluewake_controls_dirty();const auto applied=writes;
    write(path,data);assert(!bluewake_controls_load());bluewake_controls_snapshot(&after);
    assert(before.key_buttons[7]==after.key_buttons[7]);
    assert(before.controller_buttons[7]==after.controller_buttons[7]);
    assert(before.controller_axes[0].axis==after.controller_axes[0].axis);
    assert(id==bluewake_controls_selected());assert(was_dirty==bluewake_controls_dirty());assert(applied==writes);
    assert(std::strlen(bluewake_controls_error())!=0);
}
void trigger_modifier_contract(SDL_Gamepad& pad,SDL_Gamepad& other,const std::filesystem::path& file) {
    BluewakeControlsSnapshot original{},other_original{},state{};
    bluewake_controls_snapshot(&original);
    assert(bluewake_controls_select(other.id));bluewake_controls_snapshot(&other_original);
    assert(bluewake_controls_select(pad.id));
    assert(!bluewake_controls_set_quick_items_trigger(SDL_GAMEPAD_AXIS_LEFTX));
    assert(!bluewake_controls_set_quick_items_trigger(SDL_GAMEPAD_AXIS_COUNT));
    assert(!bluewake_controls_set_quick_items_trigger(-2));
    assert(bluewake_controls_set_quick_items_trigger(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    bluewake_controls_snapshot(&state);
    assert(state.quick_items_trigger==SDL_GAMEPAD_AXIS_LEFT_TRIGGER&&state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS]==-1);
    assert(std::memcmp(original.controller_buttons,state.controller_buttons,sizeof state.controller_buttons)==0);
    assert(std::memcmp(original.controller_axes,state.controller_axes,sizeof state.controller_axes)==0);
    assert(bluewake_controls_set_button(5,-1)&&bluewake_controls_set_button(6,-1));
    assert(bluewake_controls_set_axis(8,{SDL_GAMEPAD_AXIS_LEFT_TRIGGER,1,-1}));
    assert(bluewake_controls_set_axis(9,{SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,1,-1}));
    auto zones=original.dead_zones;zones.emulate_triggers=true;zones.trigger_left=zones.trigger_right=20000;
    assert(bluewake_controls_set_dead_zones(zones));
    raw_keys.fill(false);pad.raw_buttons.fill(false);pad.raw_axes.fill(0);other.raw_axes.fill(0);
    unsigned step=0;
    auto check=[&](bool down,uint16_t mask=0) {
        ++step;
        BluewakeControlsActions value{};bluewake_controls_retrace();assert(bluewake_controls_read_actions(&value));
        if(value.quick_items_down!=down||value.quick_items_native_buttons!=mask)
            std::cerr<<"Trigger step "<<step<<": expected "<<down<<'/'<<mask<<", observed "
                     <<value.quick_items_down<<'/'<<value.quick_items_native_buttons<<'\n';
        assert(value.quick_items_down==down&&value.quick_items_native_buttons==mask);
        assert((value.quick_items_native_buttons&0x0060)==0); // LT/RT must never exempt unrelated X/Y.
    };
    check(false);
    other.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=32767;check(false); // Selected device only.
    pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=-32768;check(false);
    pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=BLUEWAKE_CONTROLS_TRIGGER_PRESS-1;check(false);
    pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=BLUEWAKE_CONTROLS_TRIGGER_PRESS;check(true);
    pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=25000;check(true,0x0200);
    pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=15000;check(true); // Hysteresis, below native L activation.
    pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=BLUEWAKE_CONTROLS_TRIGGER_RELEASE;check(false);
    pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=25000;check(true,0x0200);
    bluewake_controls_set_input_blocked(true);bluewake_controls_set_input_blocked(false);check(false);
    pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=15000;check(false); // Still held after menu close.
    pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=12000;check(false);
    pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=25000;check(true,0x0200);
    assert(bluewake_controls_set_axis(8,{SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,1,-1}));
    assert(bluewake_controls_set_axis(9,{SDL_GAMEPAD_AXIS_LEFT_TRIGGER,1,-1}));
    check(false);pad.raw_axes.fill(0);check(false);
    pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=25000;check(true,0x0400); // Remapped to native R.
    zones.emulate_triggers=false;assert(bluewake_controls_set_dead_zones(zones));check(false);
    pad.raw_axes.fill(0);check(false);pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=25000;check(true);
    zones.emulate_triggers=true;assert(bluewake_controls_set_dead_zones(zones));
    assert(bluewake_controls_set_button(5,SDL_GAMEPAD_BUTTON_NORTH));
    pad.raw_axes.fill(0);check(false);pad.raw_axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=25000;check(true);
    assert(bluewake_controls_set_button(5,-1));pad.raw_axes.fill(0);check(false);
    assert(bluewake_controls_set_quick_items_trigger(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
    pad.raw_axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=25000;check(false); // Rebind held source requires release.
    pad.raw_axes.fill(0);check(false);pad.raw_axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=25000;check(true,0x0200);
    assert(save());const auto persisted=read(file);
    write(file,"version=4\ninvalid=1\n");assert(!bluewake_controls_load());check(true,0x0200);
    write(file,persisted);assert(bluewake_controls_load());check(false); // Load cannot leak a held modifier.
    bluewake_controls_snapshot(&state);assert(state.quick_items_trigger==SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    assert(bluewake_controls_select(other.id));assert(bluewake_controls_set_quick_items_trigger(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    assert(bluewake_controls_select(pad.id));assert(save());assert(bluewake_controls_load());
    assert(bluewake_controls_select(other.id));bluewake_controls_snapshot(&state);
    assert(state.quick_items_trigger==SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    assert(bluewake_controls_select(pad.id));bluewake_controls_snapshot(&state);
    assert(state.quick_items_trigger==SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    pad.connected=false;bluewake_controls_refresh();check(false);
    pad.connected=true;bluewake_controls_refresh();check(false); // Held reconnect stays suppressed.
    pad.raw_axes.fill(0);check(false);pad.raw_axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=25000;check(true,0x0200);
    assert(bluewake_controls_select(other.id));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_QUICK_ITEMS,other_original.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS]));
    assert(bluewake_controls_select(pad.id));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_QUICK_ITEMS,original.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS]));
    bluewake_controls_snapshot(&state);assert(state.quick_items_trigger==-1);
    for(unsigned i=5;i<=6;++i)assert(bluewake_controls_set_button(i,original.controller_buttons[i]));
    for(unsigned i=8;i<=9;++i)assert(bluewake_controls_set_axis(i,original.controller_axes[i]));
    assert(bluewake_controls_set_dead_zones(original.dead_zones));
    pad.raw_axes.fill(0);other.raw_axes.fill(0);check(false);
}
#ifdef _WIN32
int concurrent_writer(const std::filesystem::path& directory,int scancode,const wchar_t* event_name) {
    SDL_Gamepad first{11,2,{},"serial-first"},second{22,1,{},"serial-second"};
    first.guid.data[0]=1;second.guid.data[0]=2;defaults(first);defaults(second);pads={&first,&second};
    const auto utf8=directory.u8string();
    if(!bluewake_controls_init(reinterpret_cast<const char*>(utf8.c_str())))return 90;
    const HANDLE event=OpenEventW(SYNCHRONIZE,FALSE,event_name);if(!event)return 91;
    const DWORD wait=WaitForSingleObject(event,10000);CloseHandle(event);if(wait!=WAIT_OBJECT_0)return 92;
    for(unsigned i=0;i<24;++i) {
        if(!bluewake_controls_set_key(false,7,scancode)||!save()) {
            write(directory/("writer-"+std::to_string(scancode)+".error"),bluewake_controls_error());return 93;
        }
    }
    return 0;
}
void concurrent_saves(const std::filesystem::path& directory,const std::filesystem::path& file) {
    wchar_t executable[32768]{};
    assert(GetModuleFileNameW(nullptr,executable,static_cast<DWORD>(std::size(executable))));
    const auto event_name=L"Local\\BlueWakeControlsTest-"+std::to_wstring(GetCurrentProcessId())+L"-"+
        std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
    const HANDLE event=CreateEventW(nullptr,TRUE,FALSE,event_name.c_str());assert(event);
    PROCESS_INFORMATION children[2]{};HANDLE handles[2]{};
    const int bindings[]={SDL_SCANCODE_Q,SDL_SCANCODE_W};
    for(unsigned i=0;i<2;++i) {
        auto command=L"\""+std::wstring(executable)+L"\" --writer \""+directory.native()+L"\" "+
            std::to_wstring(bindings[i])+L" \""+event_name+L"\"";
        STARTUPINFOW startup{};startup.cb=sizeof startup;
        assert(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,
                              nullptr,directory.c_str(),&startup,&children[i]));
        handles[i]=children[i].hProcess;CloseHandle(children[i].hThread);
    }
    assert(SetEvent(event));
    unsigned reads=0;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(WaitForMultipleObjects(2,handles,TRUE,0)==WAIT_TIMEOUT) {
        // Parsing during replacement must always see one complete profile.
        const bool loaded=bluewake_controls_load();
        if(!loaded)std::cerr<<"Concurrent controls load failed: "<<bluewake_controls_error()
            <<"; errno="<<errno<<"; Windows error="<<GetLastError()<<"; current bytes="<<read(file)<<'\n';
        assert(loaded);++reads;
        assert(std::chrono::steady_clock::now()<deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(reads!=0);
    for(unsigned i=0;i<2;++i) {
        DWORD exit_code=0;assert(GetExitCodeProcess(handles[i],&exit_code));CloseHandle(handles[i]);
        if(exit_code)std::cerr<<"Concurrent controls writer "<<i<<" failed: "<<exit_code<<'\n'
            <<read(directory/("writer-"+std::to_string(bindings[i])+".error"));
        assert(exit_code==0);
    }
    CloseHandle(event);
    assert(bluewake_controls_load());BluewakeControlsSnapshot state{};bluewake_controls_snapshot(&state);
    assert(state.key_buttons[7]==SDL_SCANCODE_Q||state.key_buttons[7]==SDL_SCANCODE_W);
    assert(!read(file).empty());
}
bool same_bindings(const BluewakeControlsSnapshot& a,const BluewakeControlsSnapshot& b) {
    bool same=true;
    auto field=[&](auto x,auto y,const char* name){
        if(x!=y){std::cerr<<"Snapshot mismatch "<<name<<": "<<x<<" vs "<<y<<'\n';same=false;}
    };
    field(a.keyboard_enabled,b.keyboard_enabled,"keyboard_enabled");
    for(unsigned i=0;i<BLUEWAKE_CONTROLS_BUTTONS;++i) {
        field(a.key_buttons[i],b.key_buttons[i],"key_buttons");
        field(a.controller_buttons[i],b.controller_buttons[i],"controller_buttons");
    }
    for(unsigned i=0;i<BLUEWAKE_CONTROLS_AXES;++i) {
        field(a.key_axes[i],b.key_axes[i],"key_axes");
        field(a.controller_axes[i].axis,b.controller_axes[i].axis,"controller_axis");
        field(a.controller_axes[i].sign,b.controller_axes[i].sign,"controller_sign");
        field(a.controller_axes[i].button,b.controller_axes[i].button,"controller_axis_button");
    }
    field(a.dead_zones.enabled,b.dead_zones.enabled,"zones_enabled");
    field(a.dead_zones.emulate_triggers,b.dead_zones.emulate_triggers,"emulate_triggers");
    field(a.dead_zones.stick,b.dead_zones.stick,"stick_zone");
    field(a.dead_zones.camera,b.dead_zones.camera,"camera_zone");
    field(a.dead_zones.trigger_left,b.dead_zones.trigger_left,"left_zone");
    field(a.dead_zones.trigger_right,b.dead_zones.trigger_right,"right_zone");
    field(a.quick_items_trigger,b.quick_items_trigger,"quick_items_trigger");
    field(a.invert_stick_x,b.invert_stick_x,"invert_stick_x");
    field(a.invert_stick_y,b.invert_stick_y,"invert_stick_y");
    field(a.invert_camera_x,b.invert_camera_x,"invert_camera_x");
    field(a.invert_camera_y,b.invert_camera_y,"invert_camera_y");
    for(unsigned i=0;i<BLUEWAKE_CONTROLS_ACTIONS;++i) {
        field(a.action_buttons[i],b.action_buttons[i],"action_buttons");
        for(unsigned j=0;j<BLUEWAKE_CONTROLS_ACTION_KEYS;++j)field(a.action_keys[i][j],b.action_keys[i][j],"action_keys");
    }
    return same;
}
void missing_target_load_contract(const std::filesystem::path& directory,const std::filesystem::path& file) {
    const auto held=directory/"controls-load-held.ini";
    const auto prior=read(file);assert(!prior.empty());
    // Establish the decoded ordinary-load baseline, including current selection.
    assert(bluewake_controls_load());
    BluewakeControlsSnapshot expected{},after{};bluewake_controls_snapshot(&expected);
    const auto selected_before=bluewake_controls_selected();
    assert(bluewake_controls_set_key(false,7,SDL_SCANCODE_Q)&&bluewake_controls_dirty());
    assert(!std::filesystem::exists(held));
    assert(MoveFileExW(file.c_str(),held.c_str(),MOVEFILE_WRITE_THROUGH));
    assert(!std::filesystem::exists(file)&&read(held)==prior);
    // The actual valid profile returns after the reader has a missing target.
    // Real Win32 operations; no CreateFile mock or fabricated profile bytes.
    const auto begin=std::chrono::steady_clock::now();
    std::thread restore([held,file]{
        std::this_thread::sleep_for(std::chrono::milliseconds(35));
        assert(MoveFileExW(held.c_str(),file.c_str(),MOVEFILE_WRITE_THROUGH));
    });
    const bool recovered=bluewake_controls_load();
    const auto transient_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
    restore.join();assert(recovered&&!bluewake_controls_dirty());
    bluewake_controls_snapshot(&after);
    assert(same_bindings(expected,after));
    if(bluewake_controls_selected()!=selected_before)std::cerr<<"Recovered selection mismatch: "<<selected_before<<" vs "<<bluewake_controls_selected()<<'\n';
    assert(bluewake_controls_selected()==selected_before);
    assert(read(file)==prior&&!std::filesystem::exists(held));

    assert(bluewake_controls_set_key(false,7,SDL_SCANCODE_Q)&&bluewake_controls_dirty());
    BluewakeControlsSnapshot unsaved{};bluewake_controls_snapshot(&unsaved);
    const auto applied=writes;const auto dirty_before=bluewake_controls_dirty();
    const auto unsaved_selected=bluewake_controls_selected();
    assert(MoveFileExW(file.c_str(),held.c_str(),MOVEFILE_WRITE_THROUGH));
    const auto absent_begin=std::chrono::steady_clock::now();
    const bool absent_loaded=bluewake_controls_load();
    const auto absent_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-absent_begin).count();
    assert(!absent_loaded&&std::strlen(bluewake_controls_error())!=0);
    bluewake_controls_snapshot(&after);
    assert(same_bindings(unsaved,after));
    if(bluewake_controls_selected()!=unsaved_selected)std::cerr<<"Failed-load selection mismatch: "<<unsaved_selected<<" vs "<<bluewake_controls_selected()<<'\n';
    assert(bluewake_controls_selected()==unsaved_selected);
    assert(dirty_before==bluewake_controls_dirty()&&applied==writes);
    assert(!std::filesystem::exists(file)&&read(held)==prior);
    assert(MoveFileExW(held.c_str(),file.c_str(),MOVEFILE_WRITE_THROUGH));
    assert(read(file)==prior&&!std::filesystem::exists(held));
    std::cout<<"Missing-target load recovered complete profile after "<<transient_ms
        <<"ms; permanent absence preserved dirty bindings after "<<absent_ms<<"ms\n";
}
void replacement_contract(const std::filesystem::path& directory,const std::filesystem::path& file) {
    const auto foreign=directory/"controls.ini.tmp";write(foreign,"another process's candidate");
    // A writer's delete-access lease must not block a reader that explicitly
    // shares rename/delete. The former CRT reader fails this deterministic case.
    const HANDLE replacing=CreateFileW(file.c_str(),DELETE,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(replacing!=INVALID_HANDLE_VALUE);
    const bool loaded_during_replace=bluewake_controls_load();
    assert(CloseHandle(replacing));
    assert(loaded_during_replace);
    const auto old=read(file);assert(bluewake_controls_set_key(false,7,SDL_SCANCODE_Q));
    HANDLE reader=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                              nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(reader!=INVALID_HANDLE_VALUE);
    assert(!bluewake_controls_save());
    assert(std::strlen(bluewake_controls_error())!=0&&bluewake_controls_dirty());
    assert(read(file)==old&&read(foreign)=="another process's candidate");
    assert(CloseHandle(reader));
    // A real target reader briefly prevents Windows from replacing the file.
    reader=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                       nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(reader!=INVALID_HANDLE_VALUE);
    std::thread release([reader]{std::this_thread::sleep_for(std::chrono::milliseconds(35));assert(CloseHandle(reader));});
    const bool replaced=save();release.join();assert(replaced&&!bluewake_controls_dirty());
    assert(read(file)!=old&&read(foreign)=="another process's candidate");
    const auto changed=read(file);
    assert(SetFileAttributesW(file.c_str(),FILE_ATTRIBUTE_READONLY));
    assert(bluewake_controls_set_key(false,7,SDL_SCANCODE_E));
    assert(!bluewake_controls_save()&&bluewake_controls_dirty()&&read(file)==changed);
    assert(SetFileAttributesW(file.c_str(),FILE_ATTRIBUTE_NORMAL));
    assert(save());std::filesystem::remove(foreign);
    for(const auto& entry:std::filesystem::directory_iterator(directory))
        assert(entry.path().filename().u8string().find("controls.ini.tmp-")!=0);
}
#endif
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
    if(argc==5&&std::wcscmp(argv[1],L"--writer")==0)return concurrent_writer(argv[2],std::stoi(argv[3]),argv[4]);
#else
int main(){
#endif
    assert(!bluewake_controls_set_key(false,0,SDL_SCANCODE_A));
    BluewakeControlsInput input{};assert(!bluewake_controls_read_controller(&input));
    const auto root=std::filesystem::temp_directory_path()/
        std::filesystem::u8path("bluewake-controls-test-\xE2\x98\x83-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(root));const auto file=root/"controls.ini";
    const auto root_utf8=root.u8string();const auto* root_name=reinterpret_cast<const char*>(root_utf8.c_str());
    SDL_Gamepad first{11,2,{},"serial-first"},second{22,1,{},"serial-second"};
    first.guid.data[0]=1;second.guid.data[0]=2;defaults(first);defaults(second);
    first.buttons[7].nativeButton=SDL_GAMEPAD_BUTTON_WEST;first.zones.stickDeadZone=4096;
    for(unsigned i=0;i<12;++i)keys[i]={SDL_SCANCODE_Z,pad_buttons[i]};
    for(unsigned i=0;i<10;++i)key_axes[i]={SDL_SCANCODE_C,static_cast<PADAxis>(i),32767};
    pads={&first,&second};
    assert(bluewake_controls_init(root_name));
    BluewakeControlsSnapshot state{};bluewake_controls_snapshot(&state);
    assert(bluewake_controls_selected()==11);assert(state.key_buttons[7]==SDL_SCANCODE_Z);
    assert(state.controller_buttons[7]==SDL_GAMEPAD_BUTTON_WEST);assert(state.dead_zones.stick==4096);
    assert(!std::filesystem::exists(file));assert(!bluewake_controls_dirty());
    const auto unchanged=writes;for(unsigned i=0;i<300;++i)bluewake_controls_refresh();
    assert(writes==unchanged);assert(restores==0);assert(reconciles>=301);
    assert(bluewake_controls_select(22));assert(second.port==0&&first.port!=0);
    bluewake_controls_retrace();assert(bluewake_controls_read_controller(&input)); // neutralize hotplug release gate.
    assert(bluewake_controls_set_key(false,7,SDL_SCANCODE_SPACE));
    assert(bluewake_controls_set_key(true,0,-1));
    assert(bluewake_controls_set_button(7,SDL_GAMEPAD_BUTTON_NORTH));
    assert(bluewake_controls_set_axis(0,{SDL_GAMEPAD_AXIS_LEFTY,-1,-1}));
    assert(bluewake_controls_set_axis(8,{-1,1,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER}));
    assert(bluewake_controls_set_dead_zones({true,true,1234,2345,25000,26000}));
    assert(bluewake_controls_set_invert(true,false,false,true));
    assert(second.axes[0].nativeAxis.nativeAxis==SDL_GAMEPAD_AXIS_LEFTX);
    assert(second.axes[0].nativeAxis.sign==AXIS_SIGN_NEGATIVE);
    assert(bluewake_controls_set_keyboard_enabled(false));
    bluewake_controls_snapshot(&state);assert(!keyboard_active&&!state.keyboard_enabled);
    assert(state.key_buttons[7]==SDL_SCANCODE_SPACE);assert(state.controller_axes[0].axis==SDL_GAMEPAD_AXIS_LEFTY);
    assert(!bluewake_controls_set_key(false,7,SDL_SCANCODE_F1));
    assert(!bluewake_controls_set_key(false,7,SDL_SCANCODE_F6));
    assert(!bluewake_controls_set_key(false,7,SDL_SCANCODE_COUNT));
    assert(!bluewake_controls_set_button(7,SDL_GAMEPAD_BUTTON_COUNT));
    assert(!bluewake_controls_set_button(7,SDL_GAMEPAD_BUTTON_BACK));
    assert(!bluewake_controls_set_axis(2,{SDL_GAMEPAD_AXIS_LEFTX,0,-1}));
    assert(!bluewake_controls_set_axis(8,{SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-1,-1}));
    assert(!bluewake_controls_set_axis(2,{SDL_GAMEPAD_AXIS_LEFTX,1,SDL_GAMEPAD_BUTTON_SOUTH}));
    assert(!bluewake_controls_set_dead_zones({true,true,65535,1,2,3}));
    assert(save());assert(!bluewake_controls_dirty());const auto good=read(file);
    const auto profile_a=state.controller_buttons[7];
    bluewake_controls_set_legacy_preferences(true,true,true,true);
    assert(second.buttons[7].nativeButton==SDL_GAMEPAD_BUTTON_EAST);
    assert(second.buttons[8].nativeButton==SDL_GAMEPAD_BUTTON_NORTH);
    assert(second.axes[6].nativeAxis.sign==AXIS_SIGN_NEGATIVE); // profile Y and legacy Y cancel.
    bluewake_controls_snapshot(&state);assert(state.controller_buttons[7]==profile_a && state.invert_camera_y);
    assert(save());assert(read(file)==good); // overlays never rewrite profiles.
    const auto overlay_writes=writes;
    bluewake_controls_set_legacy_preferences(true,true,true,true);assert(writes==overlay_writes);
    bluewake_controls_set_legacy_preferences(false,false,false,false);
    assert(bluewake_controls_set_axis(4,{SDL_GAMEPAD_AXIS_LEFTX,1,-1}));
    assert(bluewake_controls_set_axis(5,{SDL_GAMEPAD_AXIS_LEFTX,-1,-1}));
    second.raw_axes[SDL_GAMEPAD_AXIS_LEFTX]=20000;
    second.raw_axes[SDL_GAMEPAD_AXIS_RIGHTY]=-18000;
    bluewake_controls_retrace();assert(bluewake_controls_read_controller(&input));
    assert(input.camera_x>0.6f && input.camera_y < -0.5f); // profile inversion applies.
    first.raw_axes[SDL_GAMEPAD_AXIS_RIGHTX]=32767;
    bluewake_controls_retrace();assert(bluewake_controls_read_controller(&input));assert(input.camera_x<0.7f); // other device ignored.
    bluewake_controls_set_legacy_preferences(false,false,true,false);
    bluewake_controls_retrace();assert(bluewake_controls_read_controller(&input));assert(input.camera_x < -0.6f);
    second.raw_axes[SDL_GAMEPAD_AXIS_LEFTX]=1000;
    bluewake_controls_retrace();assert(bluewake_controls_read_controller(&input));assert(input.camera_x==0); // custom dead zone.
    second.raw_axes[SDL_GAMEPAD_AXIS_LEFTX]=20000;
    second.raw_buttons[SDL_GAMEPAD_BUTTON_RIGHT_STICK]=true;
    second.raw_buttons[SDL_GAMEPAD_BUTTON_DPAD_UP]=true;
    bluewake_controls_set_input_blocked(true);
    bluewake_controls_retrace();assert(bluewake_controls_read_controller(&input));assert(input.camera_x==0 && !input.camera_click && !input.zoom);
    bluewake_controls_set_input_blocked(false);
    bluewake_controls_retrace();assert(bluewake_controls_read_controller(&input));assert(input.camera_x==0 && input.camera_y==0 && !input.camera_click && !input.zoom);
    bluewake_controls_retrace();assert(bluewake_controls_read_controller(&input));assert(input.camera_x==0);
    second.raw_axes.fill(0);second.raw_buttons.fill(false);
    bluewake_controls_retrace();assert(bluewake_controls_read_controller(&input));
    second.raw_axes[SDL_GAMEPAD_AXIS_LEFTX]=20000;second.raw_buttons[SDL_GAMEPAD_BUTTON_DPAD_UP]=true;
    bluewake_controls_retrace();assert(bluewake_controls_read_controller(&input));assert(input.camera_x < -0.6f && input.zoom==1);
    second.raw_axes.fill(0);second.raw_buttons.fill(false);first.raw_axes.fill(0);
    bluewake_controls_set_legacy_preferences(false,false,false,false);
    assert(good.find("serial-second")==std::string::npos); // serialized identity is escaped, not instanceID.
    assert(bluewake_controls_set_key(false,7,SDL_SCANCODE_V));
    assert(bluewake_controls_load());bluewake_controls_snapshot(&state);
    assert(state.key_buttons[7]==SDL_SCANCODE_SPACE);assert(state.controller_buttons[7]==SDL_GAMEPAD_BUTTON_NORTH);
    assert(state.dead_zones.stick==1234&&state.invert_camera_y);assert(bluewake_controls_selected()==22);
    const auto applied=writes;for(unsigned i=0;i<300;++i)bluewake_controls_refresh();assert(writes==applied);
    second.connected=false;bluewake_controls_refresh();assert(bluewake_controls_selected()==11);assert(bluewake_controls_using_fallback());
    second.id=77;second.connected=true;bluewake_controls_refresh();assert(bluewake_controls_selected()==77);assert(!bluewake_controls_using_fallback());
    bluewake_controls_snapshot(&state);assert(state.controller_buttons[7]==SDL_GAMEPAD_BUTTON_NORTH);
    assert(bluewake_controls_init(root_name));assert(bluewake_controls_selected()==77);
    bluewake_controls_snapshot(&state);assert(state.key_buttons[7]==SDL_SCANCODE_SPACE&&state.dead_zones.camera==2345);
    expect_rejected(file,good+"unknown=1\n");
    expect_rejected(file,good.substr(0,good.find("axis5=")));
    auto bad=good;bad.replace(bad.find("axis0="),bad.find('\n',bad.find("axis0="))-bad.find("axis0="),"axis0=999,1,-1");expect_rejected(file,bad);
    bad=good;bad.replace(bad.find("dead_zones="),bad.find('\n',bad.find("dead_zones="))-bad.find("dead_zones="),"dead_zones=1,1,32768,1,1,1");expect_rejected(file,bad);
    expect_rejected(file,good+"invert=0,0,0,0\n");
    bad=good;const auto keys_at=bad.find("key_buttons=");
    bad.replace(keys_at,bad.find('\n',keys_at)-keys_at,"key_buttons="+std::to_string(SDL_SCANCODE_F1)+",29,29,29,29,29,29,44,29,29,29,29");expect_rejected(file,bad); // F1 cannot bypass capture through a file.
    bad=good;const auto button_at=bad.find("buttons=",bad.find("[controller "));
    bad.replace(button_at,bad.find('\n',button_at)-button_at,"buttons="+std::to_string(SDL_GAMEPAD_BUTTON_BACK)+",0,0,0,0,0,0,0,0,0,0,0");expect_rejected(file,bad); // Back remains reserved.
    bad=good;const auto axis_at=bad.find("axis0=");
    bad.replace(axis_at,bad.find('\n',axis_at)-axis_at,"axis0=-1,1,"+std::to_string(SDL_GAMEPAD_BUTTON_BACK));expect_rejected(file,bad);
    expect_rejected(file,good+std::string(270000,'x'));
    write(file,good);assert(bluewake_controls_load());
    assert(bluewake_controls_set_key(false,7,SDL_SCANCODE_Z));assert(save()); // atomically replace existing file.
    assert(bluewake_controls_reset_controller());assert(restores==1);
    bluewake_controls_snapshot(&state);assert(!state.invert_stick_x && !state.invert_camera_y);
    bluewake_controls_reset_keyboard();bluewake_controls_snapshot(&state);
    assert(state.keyboard_enabled&&state.key_buttons[7]==SDL_SCANCODE_J);
    // Version 1 maps survive migration and gain the original host-action defaults.
    auto legacy=good;
    legacy.replace(legacy.find("version=4"),9,"version=1");
    for(const auto* field:{"host_keys=","host_buttons=","quick_items_trigger="})for(size_t at=legacy.find(field);at!=std::string::npos;at=legacy.find(field))
        legacy.erase(at,legacy.find('\n',at)-at+1);
    write(file,legacy);assert(bluewake_controls_load());bluewake_controls_snapshot(&state);
    assert(state.controller_buttons[7]==SDL_GAMEPAD_BUTTON_NORTH&&state.controller_axes[0].axis==SDL_GAMEPAD_AXIS_LEFTY);
    assert(state.action_keys[BLUEWAKE_ACTION_JUMP][0]==SDL_SCANCODE_SPACE);
    assert(state.action_keys[BLUEWAKE_ACTION_SPRINT][0]==SDL_SCANCODE_LSHIFT&&state.action_keys[BLUEWAKE_ACTION_SPRINT][1]==SDL_SCANCODE_RSHIFT);
    assert(state.action_buttons[BLUEWAKE_ACTION_JUMP]==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    assert(state.action_buttons[BLUEWAKE_ACTION_FIRST_PERSON]==SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    assert(state.action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][0]==SDL_SCANCODE_TAB);
    assert(state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS]==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    assert(state.quick_items_trigger==-1);
    assert(save());assert(read(file).find("version=4")!=std::string::npos);
    assert(!bluewake_controls_set_action_key(0,2,SDL_SCANCODE_V));
    assert(!bluewake_controls_set_action_key(BLUEWAKE_CONTROLS_ACTIONS,0,SDL_SCANCODE_V));
    assert(!bluewake_controls_set_action_key(0,0,SDL_SCANCODE_F1));
    assert(!bluewake_controls_set_action_button(0,SDL_GAMEPAD_BUTTON_BACK));
    assert(!bluewake_controls_set_action_button(BLUEWAKE_CONTROLS_ACTIONS,SDL_GAMEPAD_BUTTON_NORTH));
    assert(bluewake_controls_set_keyboard_enabled(true));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_JUMP,0,SDL_SCANCODE_V));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_JUMP,1,PAD_KEY_MOUSE_X1));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_SPRINT,0,SDL_SCANCODE_C));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_FIRST_PERSON,0,SDL_SCANCODE_G));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_JUMP,SDL_GAMEPAD_BUTTON_NORTH));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_SPRINT,SDL_GAMEPAD_BUTTON_WEST));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_FIRST_PERSON,SDL_GAMEPAD_BUTTON_EAST));
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_QUICK_ITEMS,0,SDL_SCANCODE_TAB));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_QUICK_ITEMS,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER));
    raw_keys.fill(false);second.raw_axes.fill(0);second.raw_buttons.fill(false);
    bluewake_controls_retrace();
    BluewakeControlsActions actions{};assert(bluewake_controls_read_actions(&actions));
    raw_keys[SDL_SCANCODE_TAB]=true;
    second.raw_buttons[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER]=true;
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&actions.quick_items_down);
    assert(actions.quick_items_native_buttons==0x0800); // LB's native Z is identifiable, LT mapping stays intact.
    bluewake_controls_set_input_blocked(true);bluewake_controls_set_input_blocked(false);
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&!actions.quick_items_down&&!actions.quick_items_native_buttons);
    raw_keys.fill(false);second.raw_buttons.fill(false);bluewake_controls_retrace();
    raw_keys[SDL_SCANCODE_TAB]=true;bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions)&&actions.quick_items_down&&!actions.quick_items_native_buttons);
    raw_keys.fill(false);bluewake_controls_retrace();
    SDL_Event modifier_tap{};modifier_tap.type=SDL_EVENT_KEY_DOWN;modifier_tap.key.scancode=SDL_SCANCODE_TAB;
    bluewake_controls_action_event(&modifier_tap);modifier_tap.type=SDL_EVENT_KEY_UP;bluewake_controls_action_event(&modifier_tap);
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&!actions.quick_items_down); // Modifier does not latch taps.
    trigger_modifier_contract(second,first,file);
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_JUMP,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER));
    bluewake_controls_retrace();second.raw_buttons[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER]=true;
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&actions.jump_pressed&&actions.jump_modifier_conflict);
    raw_keys[SDL_SCANCODE_V]=true;bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions)&&actions.jump_pressed&&!actions.jump_modifier_conflict); // Independent Jump key remains usable.
    raw_keys.fill(false);second.raw_buttons.fill(false);bluewake_controls_retrace();
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_QUICK_ITEMS,0,SDL_SCANCODE_SPACE));
    bluewake_controls_retrace();raw_keys[SDL_SCANCODE_SPACE]=true;bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions)&&actions.quick_items_down&&actions.quick_items_native_buttons==0x0100);
    raw_keys.fill(false);bluewake_controls_retrace();
    assert(bluewake_controls_set_action_key(BLUEWAKE_ACTION_QUICK_ITEMS,0,SDL_SCANCODE_TAB));
    assert(bluewake_controls_set_action_button(BLUEWAKE_ACTION_JUMP,SDL_GAMEPAD_BUTTON_NORTH));
    bluewake_controls_retrace();
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=SDL_SCANCODE_V;
    bluewake_controls_action_event(&event);event.type=SDL_EVENT_KEY_UP;bluewake_controls_action_event(&event);
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&actions.jump_pressed); // short tap survives until VI.
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&!actions.jump_pressed);
    raw_keys[SDL_SCANCODE_SPACE]=true;raw_keys[SDL_SCANCODE_LSHIFT]=true;
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&!actions.jump_pressed&&!actions.sprint_held);
    raw_keys.fill(false);raw_keys[SDL_SCANCODE_C]=true;raw_keys[SDL_SCANCODE_G]=true;
    second.raw_axes[SDL_GAMEPAD_AXIS_LEFTY]=-25000;second.raw_buttons[SDL_GAMEPAD_BUTTON_EAST]=true;
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&actions.sprint_held&&actions.first_person_down);
    assert(actions.movement_tilt>0.7f);
    assert(bluewake_controls_read_controller(&input)&&input.camera_click);
    bluewake_controls_set_input_blocked(true);const auto before=actions.generation;
    assert(bluewake_controls_read_actions(&actions)&&actions.blocked&&actions.generation!=before&&!actions.jump_pressed);
    bluewake_controls_set_input_blocked(false);bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions)&&!actions.sprint_held&&!actions.first_person_down);
    raw_keys.fill(false);second.raw_buttons.fill(false);bluewake_controls_retrace();
    raw_keys[SDL_SCANCODE_C]=true;raw_keys[SDL_SCANCODE_G]=true;second.raw_buttons[SDL_GAMEPAD_BUTTON_EAST]=true;
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&actions.sprint_held&&actions.first_person_down);
    event={};event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.which=second.id;event.gbutton.button=SDL_GAMEPAD_BUTTON_WEST;
    bluewake_controls_action_event(&event);event.type=SDL_EVENT_GAMEPAD_BUTTON_UP;bluewake_controls_action_event(&event);
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&actions.sprint_pressed);
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&!actions.sprint_pressed);
    event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.which=first.id;bluewake_controls_action_event(&event);
    bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&!actions.sprint_pressed); // selected only.
    raw_keys.fill(false);second.raw_buttons.fill(false);bluewake_controls_retrace();
    raw_mouse=SDL_BUTTON_X1MASK;bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions)&&actions.jump_pressed);
    raw_mouse=0;bluewake_controls_retrace();
    assert(bluewake_controls_set_keyboard_enabled(false));bluewake_controls_retrace();
    raw_keys[SDL_SCANCODE_V]=true;event={};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=SDL_SCANCODE_V;
    bluewake_controls_action_event(&event);bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions)&&!actions.jump_pressed&&!actions.sprint_held);
    raw_keys.fill(false);assert(save());const auto action_good=read(file);
    assert(bluewake_controls_load());bluewake_controls_snapshot(&state);
    assert(state.action_keys[BLUEWAKE_ACTION_JUMP][0]==SDL_SCANCODE_V&&state.action_keys[BLUEWAKE_ACTION_JUMP][1]==PAD_KEY_MOUSE_X1);
    assert(state.action_buttons[BLUEWAKE_ACTION_FIRST_PERSON]==SDL_GAMEPAD_BUTTON_EAST);
    assert(state.action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][0]==SDL_SCANCODE_TAB);
    assert(state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS]==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    // Version 2 retains custom action maps and gains only the new modifier defaults.
    auto v3=action_good;v3.replace(v3.find("version=4"),9,"version=3");
    for(size_t at=v3.find("quick_items_trigger=");at!=std::string::npos;at=v3.find("quick_items_trigger="))
        v3.erase(at,v3.find('\n',at)-at+1);
    write(file,v3);assert(bluewake_controls_load());bluewake_controls_snapshot(&state);
    assert(state.quick_items_trigger==-1&&state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS]==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    assert(save());assert(read(file)==action_good);
    auto bad_trigger=action_good;
    auto trigger_at=bad_trigger.find("quick_items_trigger=");
    bad_trigger.replace(trigger_at,bad_trigger.find('\n',trigger_at)-trigger_at,"quick_items_trigger=0");expect_rejected(file,bad_trigger);
    bad_trigger=action_good;bad_trigger.replace(trigger_at,bad_trigger.find('\n',trigger_at)-trigger_at,"quick_items_trigger=4");
    expect_rejected(file,bad_trigger); // A trigger and digital modifier cannot both be selected.
    expect_rejected(file,action_good+"quick_items_trigger=-1\n"); // Duplicate field.
    auto v2=v3;v2.replace(v2.find("version=3"),9,"version=2");
    for(const auto* field:{"host_keys=","host_buttons="}) {
        size_t at=0;
        while((at=v2.find(field,at))!=std::string::npos) {
            const size_t end=v2.find('\n',at);auto row=v2.substr(at,end-at);
            row.erase(row.rfind(','));
            if(std::string(field)=="host_keys=")row.erase(row.rfind(','));
            v2.replace(at,end-at,row);at+=row.size();
        }
    }
    write(file,v2);assert(bluewake_controls_load());bluewake_controls_snapshot(&state);
    assert(state.action_keys[BLUEWAKE_ACTION_JUMP][0]==SDL_SCANCODE_V&&state.action_buttons[BLUEWAKE_ACTION_FIRST_PERSON]==SDL_GAMEPAD_BUTTON_EAST);
    assert(state.action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][0]==SDL_SCANCODE_TAB&&state.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS]==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    assert(save());assert(read(file)==action_good);
#ifdef _WIN32
    replacement_contract(root,file);missing_target_load_contract(root,file);concurrent_saves(root,file);
    write(file,action_good);assert(bluewake_controls_load());
#endif
    bad=action_good;const auto host_at=bad.find("host_keys=");bad.replace(host_at,bad.find('\n',host_at)-host_at,"host_keys=58,-1,225,229,-1,-1,43,-1");expect_rejected(file,bad);
    bad=action_good;const auto host_buttons=bad.find("host_buttons=");bad.replace(host_buttons,bad.find('\n',host_buttons)-host_buttons,"host_buttons=4,7,8,9");expect_rejected(file,bad); // Back reserved.
    // Reaching the profile limit cannot leave an old device's actions active
    // while an unprofiled replacement occupies PAD port 0.
    auto full=action_good;
    const auto template_at=full.find("[controller ");
    const auto template_end=full.find("[controller ",template_at+1);
    const auto section=full.substr(template_at,template_end-template_at);
    for(unsigned i=3;i<=32;++i){
        auto extra=section;char guid[33]{};std::snprintf(guid,sizeof guid,"%032x",i);
        extra.replace(12,32,guid);full+=extra;
    }
    write(file,full);assert(bluewake_controls_load());bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions));const auto old_generation=actions.generation;
    SDL_Gamepad replacement{88,-1,{},"serial-replacement"};replacement.guid.data[0]=99;defaults(replacement);
    pads.push_back(&replacement);first.connected=second.connected=false;bluewake_controls_refresh();
    assert(!bluewake_controls_select(replacement.id)&&std::strlen(bluewake_controls_error())!=0);
    assert(bluewake_controls_selected()==0&&replacement.port!=0);
    assert(bluewake_controls_read_actions(&actions)&&actions.generation!=old_generation&&!actions.jump_pressed);
    second.raw_buttons[SDL_GAMEPAD_BUTTON_EAST]=true;
    event={};event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.which=second.id;event.gbutton.button=SDL_GAMEPAD_BUTTON_NORTH;
    bluewake_controls_action_event(&event);bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions)&&!actions.jump_pressed&&!actions.first_person_down);
    assert(bluewake_controls_read_controller(&input)&&!input.camera_click&&input.camera_x==0&&input.stick_x==0);
    assert(!std::filesystem::exists(root/"controller_ports.dat"));assert(!std::filesystem::exists(root/"controls.ini.tmp"));
    std::filesystem::remove(file);std::filesystem::remove(root);
    std::cout<<"Controls persistence, validation, device profiles, hotplug, mapping overlays, mapped camera, dead zones and menu release contracts passed\n";
}
