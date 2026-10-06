// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "controls_menu.h"
#include "controls_test_support.h"
#include <chrono>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
bool blocked;
SDL_Event key_event(SDL_Scancode key){
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=key;return event;
}
SDL_Event controller_button(unsigned id,SDL_GamepadButton button){
    SDL_Event event{};event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.which=id;event.gbutton.button=button;return event;
}
std::string read_file(const std::filesystem::path& file) {
    std::ifstream stream(file,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
void assert_persisted_profile() {
    BluewakeControlsSnapshot value{};bluewake_controls_snapshot(&value);
    assert(value.key_buttons[7]==SDL_SCANCODE_P);
    assert(value.key_buttons[8]==PAD_KEY_MOUSE_X1);
    assert(value.controller_buttons[7]==SDL_GAMEPAD_BUTTON_WEST);
    assert(value.controller_axes[4].axis==SDL_GAMEPAD_AXIS_LEFTY&&value.controller_axes[4].sign==-1);
    assert(value.dead_zones.stick==12345&&value.invert_camera_y);
    assert(value.action_keys[BLUEWAKE_ACTION_JUMP][0]==SDL_SCANCODE_V);
    assert(value.action_keys[BLUEWAKE_ACTION_SPRINT][1]==PAD_KEY_MOUSE_X2);
    assert(value.action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][0]==SDL_SCANCODE_TAB);
    assert(value.action_buttons[BLUEWAKE_ACTION_FIRST_PERSON]==SDL_GAMEPAD_BUTTON_EAST);
    assert(value.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS]==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
}
#ifdef _WIN32
void fresh_process_reload(const std::filesystem::path& folder) {
    wchar_t exe[32768]{};assert(GetModuleFileNameW(nullptr,exe,std::size(exe))>0);
    std::wstring command=L"\""+std::wstring(exe)+L"\" --reload \""+folder.wstring()+L"\"";
    const HANDLE job=CreateJobObjectW(nullptr,nullptr);assert(job);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    assert(SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof limits));
    STARTUPINFOW startup{};startup.cb=sizeof startup;startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION child{};
    const bool created=CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,
        CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&startup,&child)!=FALSE;
    assert(created);
    if(!AssignProcessToJobObject(job,child.hProcess)) {
        TerminateProcess(child.hProcess,94);WaitForSingleObject(child.hProcess,5000);
        CloseHandle(child.hThread);CloseHandle(child.hProcess);CloseHandle(job);assert(false);
    }
    const bool resumed=ResumeThread(child.hThread)!=DWORD(-1);CloseHandle(child.hThread);
    if(!resumed) {TerminateJobObject(job,95);WaitForSingleObject(child.hProcess,5000);}
    const DWORD waited=WaitForSingleObject(child.hProcess,20000);
    if(waited!=WAIT_OBJECT_0) {TerminateJobObject(job,96);WaitForSingleObject(child.hProcess,5000);}
    DWORD code=97;assert(GetExitCodeProcess(child.hProcess,&code));
    CloseHandle(child.hProcess);CloseHandle(job);
    assert(resumed&&waited==WAIT_OBJECT_0&&code==0);
}

void save_retry_contract(const std::filesystem::path& folder) {
    const auto file=folder/"controls.ini";const auto prior=read_file(file);assert(!prior.empty());
    bluewake_controls_menu_set_open(true);assert(blocked);
    assert(bluewake_controls_set_key(false,7,SDL_SCANCODE_R));
    raw_keys[SDL_SCANCODE_V]=true; // Held Jump must remain suppressed through disk retries.
    const HANDLE locked=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(locked!=INVALID_HANDLE_VALUE);
    bluewake_controls_menu_set_open(false);
    assert(!blocked&&!bluewake_controls_menu_is_open()&&bluewake_controls_dirty());
    const std::string failure=bluewake_controls_menu_save_error();assert(!failure.empty());
    assert(read_file(file)==prior);
    const auto applied=writes;
    bluewake_controls_menu_tick(5000);bluewake_controls_menu_tick(5999);
    assert(bluewake_controls_dirty()&&read_file(file)==prior&&writes==applied);
    bluewake_controls_menu_tick(6000); // Still locked: failure stays visible, profile remains dirty.
    assert(bluewake_controls_dirty()&&!std::string(bluewake_controls_menu_save_error()).empty());
    assert(read_file(file)==prior&&writes==applied);

    bluewake_controls_menu_set_open(true);assert(blocked);
    assert(bluewake_controls_menu_begin_capture(BLUEWAKE_CAPTURE_KEY_AXIS,0));
    bluewake_controls_menu_tick(100000); // Neither open-menu nor capture frames retry disk I/O.
    assert(bluewake_controls_dirty()&&read_file(file)==prior);
    assert(std::string(bluewake_controls_menu_save_error())==failure);
    bluewake_controls_menu_cancel_capture();
    assert(std::string(bluewake_controls_menu_save_error())==failure); // Capture message cannot hide it.

    // Edits made after the failed close must win over any queued save request.
    assert(bluewake_controls_set_key(false,7,SDL_SCANCODE_P));
    assert(bluewake_controls_set_button(7,SDL_GAMEPAD_BUTTON_WEST));
    BluewakeControlsSnapshot latest{};bluewake_controls_snapshot(&latest);
    latest.dead_zones.stick=12345;assert(bluewake_controls_set_dead_zones(latest.dead_zones));
    assert(bluewake_controls_set_invert(false,false,false,true));
    assert(std::string(bluewake_controls_menu_save_error())==failure); // Core edit cleared its own error only.
    bluewake_controls_menu_set_open(false);assert(!blocked&&bluewake_controls_dirty());
    assert(CloseHandle(locked));
    const auto latest_applied=writes;
    bluewake_controls_menu_tick(6000); // First later frame arms the interval after this failed close.
    bluewake_controls_menu_tick(5999); // Clock rollback arms a new interval; it cannot spin.
    bluewake_controls_menu_tick(6998);
    assert(bluewake_controls_dirty()&&read_file(file)==prior&&writes==latest_applied);
    bluewake_controls_menu_tick(6999);
    assert(!bluewake_controls_dirty()&&std::string(bluewake_controls_menu_save_error()).empty());
    assert(read_file(file)!=prior&&writes==latest_applied);assert_persisted_profile();
    BluewakeControlsActions actions{};bluewake_controls_retrace();assert(bluewake_controls_read_actions(&actions));
    assert(!actions.blocked&&!actions.jump_pressed&&!actions.sprint_held&&!actions.first_person_down&&!actions.quick_items_down);
    raw_keys.fill(false);bluewake_controls_retrace();
    assert(bluewake_controls_read_actions(&actions)&&!actions.jump_pressed);
    fresh_process_reload(folder);
    const auto persisted=read_file(file);bluewake_controls_menu_tick(8000);bluewake_controls_menu_tick(UINT64_MAX);
    assert(read_file(file)==persisted&&writes==latest_applied); // Success retires retry completely.
    for(const auto& entry:std::filesystem::directory_iterator(folder))
        assert(entry.path().filename().u8string().find("controls.ini.tmp-")!=0);
}
#endif
}
extern "C" void PADBlockInput(bool value){blocked=value;}

#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    if(argc==3&&std::wcscmp(argv[1],L"--reload")==0) {
        SDL_Gamepad controller{77,0,{},"capture-device"};defaults(controller);pads={&controller};
        const auto utf8=std::filesystem::path(argv[2]).u8string();
        assert(bluewake_controls_init(reinterpret_cast<const char*>(utf8.c_str())));
        assert(bluewake_controls_selected()==77&&!bluewake_controls_dirty());assert_persisted_profile();return 0;
    }
#else
int main(){
#endif
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
#ifdef _WIN32
    save_retry_contract(folder);
#endif
    std::filesystem::remove(folder/"controls.ini");std::filesystem::remove(folder);
    std::cout<<"Capture/input safety and menu persistence passed; Windows lock failure/retry kept latest complete profiles and fresh-process reload\n";
}
