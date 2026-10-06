// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "settings_ui_mocks.h"
#include "hud_settings_ui_mocks.h"
#include "audio_customization.h"
#include "audio_preview.h"
#include "settings_ui_test_api.h"
#include "win_settings.h"
#include "settings_catalog.h"
#include "controls_bindings.h"
#include "controls_menu.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef rename
#undef rename // The production compatibility macro must not rewrite filesystem declarations.
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
extern "C" bool bluewake_settings_menu_event(const void* event);

namespace {
struct Item {
    ImGuiID id = 0;
    ImRect bounds;
    std::string label, window;
    ImGuiItemStatusFlags flags = 0;
    ImGuiItemFlags item_flags = 0;
};
std::map<ImGuiID, Item> items;
std::string rendered_text;
std::ofstream trace;
unsigned frames;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "Missing file: " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void frame() {
    // This is entirely local ImGui IO. No backend, SDL initialization, GPU,
    // native window, Windows messages, cursor, or desktop input is involved.
    bw_settings_ui_mock_frame();
    items.clear();
    ImGui::NewFrame();
    ImGui::LogToBuffer(0);
    bw_settings_ui_test_draw();
    rendered_text.assign(GImGui->LogBuffer.begin(), GImGui->LogBuffer.end());
    ImGui::LogFinish();
    ImGui::Render();
    require(ImGui::GetDrawData() &&
            ((!bw_settings_ui_test_menu_open() && !*bluewake_controls_menu_save_error()) ||
             ImGui::GetDrawData()->TotalVtxCount > 0),
            "Actual settings menu or save notice emitted no ImGui draw geometry");
    if (trace) trace << "\nFRAME " << ++frames << '\n' << rendered_text;
}
const Item& item(const char* label, ImGuiItemStatusFlags required = 0,
                 ImGuiItemStatusFlags excluded = 0) {
    for (const auto& [id, value] : items)
        if (value.label == label && (value.flags & required) == required && !(value.flags & excluded))
            return value;
    // Bundled ImGui's BeginCombo registers ItemAdd but has no ItemInfo hook.
    // Resolve its actual scoped ID and recorded rectangle, without
    // duplicating widget geometry or changing application state.
    if (required == 0 && excluded == 0)
        if (auto* window = ImGui::FindWindowByName("BlueWake settings")) {
            auto found = items.find(window->GetID(label));
            if (found != items.end()) return found->second;
            size_t count = 0;
            const auto* definitions = bw_setting_definitions(&count);
            for (size_t i = 0; i < count; ++i)
                if (std::strcmp(definitions[i].label, label) == 0) {
                    // setting_widget pushes the catalog ID before BeginCombo.
                    const ImGuiID scope = window->GetID(definitions[i].id);
                    found = items.find(ImHashStr(label, 0, scope));
                    if (found != items.end()) return found->second;
                }
        }
    throw std::runtime_error("Actual UI did not expose widget: " + std::string(label));
}
bool has_item(const char* label) {
    return std::any_of(items.begin(), items.end(), [&](const auto& value) { return value.second.label == label; });
}
void click(const char* label, ImGuiItemStatusFlags required = 0,
           ImGuiItemStatusFlags excluded = 0) {
    const ImVec2 center = item(label, required, excluded).bounds.GetCenter();
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(center.x, center.y);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    frame();
}
void search(const char* query) {
    click("##search");
    ImGui::GetIO().AddInputCharactersUTF8(query);
    frame();
    frame();
}
void clear_search() { click("Clear"); }
#ifdef _WIN32
void controls_save_retry_ui_checks(const std::filesystem::path& file) {
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
    frame();
    SDL_Event reopen{};reopen.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    reopen.gbutton.button=SDL_GAMEPAD_BUTTON_BACK;
    require(bluewake_settings_menu_event(&reopen), "Menu handler did not reopen for save failure");
    frame();frame();
    const auto prior=read_file(file);
    require(bluewake_controls_set_key(false,7,SDL_SCANCODE_P), "Cannot edit fixture binding");
    const HANDLE locked=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
                                   OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(locked!=INVALID_HANDLE_VALUE, "Cannot hold fixture file against replacement");
    struct CloseLock { HANDLE handle;~CloseLock(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);} } lock{locked};
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,true);
    frame();frame();
    require(!bw_settings_ui_test_menu_open() && !bw_settings_ui_mock_effects().blocked &&
            bluewake_controls_dirty() && read_file(file)==prior,
            "Failed menu-close save lost live binding or changed the protected old file");
    require(rendered_text.find("Your controls are active, but could not be saved.")!=std::string::npos,
            "Closed-menu save failure notice was not drawn");
    auto* notice=ImGui::FindWindowByName("##bluewake-controls-save");
    const auto flags=ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav;
    require(notice && (notice->Flags&flags)==flags, "Save notice can take input or focus");
    const auto writes_before=bw_settings_ui_mock_effects().controller_writes;
    require(CloseHandle(locked)!=FALSE, "Cannot release fixture lock");lock.handle=INVALID_HANDLE_VALUE;
    for(unsigned i=0;i<50;++i)frame();
    require(bluewake_controls_dirty() && read_file(file)==prior,
            "Save retry did not respect its frame throttle");
    for(unsigned i=0;i<20 && bluewake_controls_dirty();++i)frame();
    frame();
    require(!bluewake_controls_dirty() && !*bluewake_controls_menu_save_error() && read_file(file)!=prior &&
            rendered_text.find("Your controls are active, but could not be saved.")==std::string::npos,
            "Later frames did not save bindings and retire the failure notice");
    require(bw_settings_ui_mock_effects().controller_writes==writes_before,
            "UI retry reapplied input bindings");
    require(bluewake_controls_load(), bluewake_controls_error());
    BluewakeControlsSnapshot snapshot{};bluewake_controls_snapshot(&snapshot);
    require(snapshot.key_buttons[7]==SDL_SCANCODE_P, "UI retry did not persist the edited binding");
}
#endif
BwAudioPreviewStatus preview_status(BwAudioPreview* handle) {
    BwAudioPreviewStatus status{};
    require(bluewake_audio_preview_status(handle, &status), "Actual preview status unavailable");
    return status;
}
void wait_preview(BwAudioPreview* handle, BwAudioPreviewState state) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (preview_status(handle).state != state) {
        require(std::chrono::steady_clock::now() < deadline, "Actual preview worker did not reach expected state");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    frame(); frame();
}
void wav(const std::filesystem::path& file, unsigned rate, bool valid = true) {
    std::ofstream out(file, std::ios::binary);
    auto u16 = [&](unsigned n) { out.put(static_cast<char>(n));out.put(static_cast<char>(n>>8)); };
    auto u32 = [&](unsigned n) { u16(n);u16(n>>16); };
    if (!valid) { out << "Invalid synthetic WAV"; return; }
    constexpr unsigned count = 128000, bytes = count*4;
    out << "RIFF";u32(36+bytes);out << "WAVEfmt ";u32(16);u16(1);u16(2);u32(rate);u32(rate*4);u16(4);u16(16);
    out << "data";u32(bytes);
    for(unsigned i=0;i<count;++i){u16(2000);u16(static_cast<unsigned>(-1000)&65535u);}
    require(out.good(), "Cannot write own synthetic WAV");
}
void choose_wav(const std::string& path) {
    bw_settings_ui_mock_preview_selection(path);
    click("Choose WAV");
}
void preview_ui_checks(BwAudioPreview* handle,const std::filesystem::path& folder) {
    const auto utf8 = (folder / std::filesystem::u8path("preview-\xE2\x98\x83.wav")).u8string();
    const auto good = std::filesystem::u8path(utf8);
    const auto bad = folder / "bad.wav", rate = folder / "48000.wav", unsupported = folder / "44100.wav";
    wav(good,32000);wav(bad,32000,false);wav(rate,48000);wav(unsupported,44100);
    const Settings untouched = bw_settings_ui_test_saved();
    const auto audio_before = bw_settings_ui_mock_effects();
    click("Sound & Saves");
    require(rendered_text.find("Preview mixes with game audio") != std::string::npos &&
            rendered_text.find("32 or 48 kHz") != std::string::npos,
            "Actual preview help did not explain mix/rate contract");
    require(item("Play WAV").item_flags & ImGuiItemFlags_Disabled, "Empty selection enables Play");
    choose_wav(utf8);
    require(rendered_text.find(utf8) != std::string::npos, "UTF8 selected path was not visible");
    require(preview_status(handle).state==BW_PREVIEW_CANCELLED, "Selecting WAV started playback");
    const auto generation = preview_status(handle).generation;
    choose_wav("");
    require(rendered_text.find(utf8)!=std::string::npos && preview_status(handle).generation==generation,
            "Picker cancellation erased selection or changed playback generation");
    click("Play WAV");wait_preview(handle,BW_PREVIEW_READY);
    require(rendered_text.find("Ready; waiting for game audio")!=std::string::npos &&
            rendered_text.find("File: 32000 Hz / output: 0 Hz")!=std::string::npos,
            "Ready/file-rate status was not visible");
    require(preview_status(handle).path_utf8==utf8 && !preview_status(handle).looping,
            "Actual Play did not submit the selected one-pass UTF8 path");
    uint8_t copied[16]{};
    require(bluewake_audio_preview_mix_be16(handle,copied,sizeof copied,32000), "Actual preview did not mix own synthetic PCM");
    frame();frame();
    require(rendered_text.find("Preview: Playing")!=std::string::npos &&
            rendered_text.find("File: 32000 Hz / output: 32000 Hz")!=std::string::npos &&
            rendered_text.find("Time: 0.0 / 4.0 seconds")!=std::string::npos && rendered_text.find("Playback progress")!=std::string::npos &&
            preview_status(handle).cursor_frame==4 && preview_status(handle).total_frames==128000,
            "Copied playback status/rates/progress were not rendered");
    click("Stop preview");wait_preview(handle,BW_PREVIEW_STOPPED);
    const auto stopped=preview_status(handle);
    std::memset(copied,0x23,sizeof copied);const std::vector<uint8_t> original(copied,copied+sizeof copied);
    require(!bluewake_audio_preview_mix_be16(handle,copied,sizeof copied,32000)&&
            std::equal(original.begin(),original.end(),copied),"Stop changed future native copied bytes");
    require(rendered_text.find("Preview: Stopped")!=std::string::npos,"Stop status was not rendered");
    choose_wav(bad.u8string());click("Play WAV");wait_preview(handle,BW_PREVIEW_FAILED);
    require(rendered_text.find("Preview: Unable to play")!=std::string::npos && preview_status(handle).error[0] &&
            rendered_text.find(preview_status(handle).error)!=std::string::npos,"Bad WAV error was not rendered");
    choose_wav(unsupported.u8string());click("Play WAV");wait_preview(handle,BW_PREVIEW_FAILED);
    require(preview_status(handle).error[0] && rendered_text.find(preview_status(handle).error)!=std::string::npos,
            "Unsupported file rate error was not rendered");
    choose_wav(rate.u8string());click("Play WAV");wait_preview(handle,BW_PREVIEW_READY);
    std::memset(copied,0x23,sizeof copied);
    require(!bluewake_audio_preview_mix_be16(handle,copied,sizeof copied,32000)&&
            std::equal(original.begin(),original.end(),copied),"Rate mismatch changed native copied bytes");
    frame();frame();
    require(preview_status(handle).state==BW_PREVIEW_RATE_MISMATCH&&
            rendered_text.find("Sample rate mismatch")!=std::string::npos&&
            rendered_text.find("File: 48000 Hz / output: 32000 Hz")!=std::string::npos,
            "Actual mismatch/rates were not rendered");
    choose_wav(utf8);click("Play WAV");wait_preview(handle,BW_PREVIEW_READY);
    click("Display");require(preview_status(handle).state==BW_PREVIEW_CANCELLED,"Leaving Sound tab did not cancel ready preview");
    click("Sound & Saves");require(preview_status(handle).state==BW_PREVIEW_CANCELLED,"Returning to Sound auto-started preview");
    click("Play WAV");wait_preview(handle,BW_PREVIEW_READY);search("Music volume");
    require(preview_status(handle).state==BW_PREVIEW_CANCELLED,"Entering settings search did not cancel Sound preview");
    clear_search();click("Sound & Saves");
    // Request a real decode and close through the actual ImGui menu handler.
    click("Play WAV");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,true);frame();
    require(!bw_settings_ui_test_menu_open()&&preview_status(handle).state==BW_PREVIEW_CANCELLED,
            "Menu close did not cancel current/pending decode");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(preview_status(handle).state==BW_PREVIEW_CANCELLED,"Cancelled decode published after menu close");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,false);
    SDL_Event reopen{};reopen.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;reopen.gbutton.button=SDL_GAMEPAD_BUTTON_BACK;
    require(bluewake_settings_menu_event(&reopen),"Actual local gamepad menu handler did not reopen");frame();frame();
    click("Sound & Saves");require(preview_status(handle).state==BW_PREVIEW_CANCELLED,"Menu reopen auto-started selected WAV");
    require(rendered_text.find(utf8)!=std::string::npos,"Transient selected path did not survive menu navigation");
    choose_wav(std::string(BW_PREVIEW_PATH_BYTES,'x'));
    require(rendered_text.find("path is too long")!=std::string::npos&&rendered_text.find(utf8)!=std::string::npos,
            "Oversize selection did not keep prior path with an error");
    require(bw_settings_ui_test_saved().audio_music==untouched.audio_music&&bw_settings_ui_test_saved().audio_sfx==untouched.audio_sfx&&
            bw_settings_ui_test_saved().audio_master==untouched.audio_master&&bw_settings_ui_test_saved().audio_muted==untouched.audio_muted&&
            bw_settings_ui_mock_effects().audio_calls==audio_before.audio_calls,
            "Preview commands mutated unrelated audio preferences");
    bw_settings_ui_test_flush();const auto text=read_file(folder/"settings.ini");
    require(text.find("preview-")==std::string::npos&&text.find("preview_path")==std::string::npos,
            "Preview selection/playback leaked into persistent preferences");
    require(stopped.generation<preview_status(handle).generation,"Real commands did not use new generations");
    click("Display");
}
void choose_preset(const char* name) {
    click("Preset");
    click(name);
}
void section(const char* name, bool wanted) {
    const bool checked = (item(name, ImGuiItemStatusFlags_Checkable).flags & ImGuiItemStatusFlags_Checked) != 0;
    if (checked != wanted) click(name, ImGuiItemStatusFlags_Checkable);
    require(((item(name, ImGuiItemStatusFlags_Checkable).flags & ImGuiItemStatusFlags_Checked) != 0) == wanted,
            std::string("Section checkbox did not change: ") + name);
}
void unchanged_profile(const std::filesystem::path& file, const std::string& original) {
    BluewakeControlsSnapshot snapshot{};
    bluewake_controls_snapshot(&snapshot);
    require(snapshot.key_buttons[7] == SDL_SCANCODE_N, "Unrelated settings replaced the keyboard A binding");
    require(snapshot.controller_buttons[7] == SDL_GAMEPAD_BUTTON_NORTH, "A settings action replaced the controller A profile");
    require(snapshot.invert_camera_x && snapshot.dead_zones.camera == 12000,
            "A settings action replaced profile inversion or deadzone");
    require(snapshot.action_keys[BLUEWAKE_ACTION_JUMP][0] == SDL_SCANCODE_V,
            "A settings action replaced the Jump binding");
    require(snapshot.action_keys[BLUEWAKE_ACTION_QUICK_ITEMS][0] == SDL_SCANCODE_B &&
            snapshot.action_buttons[BLUEWAKE_ACTION_QUICK_ITEMS] == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            "A settings action replaced the custom shortcut modifier");
    require(bluewake_controls_selected() == 101 && !bluewake_controls_automatic(), "A settings action lost the selected device");
    require(read_file(file) == original, "A settings action changed saved controls.ini");
}
void file_line(const std::string& text, const std::string& line) {
    // Windows' text-mode production writer emits CRLF. Keep raw profile bytes
    // for preservation checks and accept either newline here.
    require(text.find("\n" + line + "\n") != std::string::npos ||
            text.find("\n" + line + "\r\n") != std::string::npos,
            "Saved settings missing expected line: " + line);
}
void healing_capability_ui_checks(const std::filesystem::path& folder) {
    const auto* healing=bw_setting_find("healing_rate_q8");
    const auto* damage=bw_setting_find("damage_rate_q8");
    require(healing&&damage,"Missing actual health catalog choices");
    bw_settings_ui_health_policy(true,false);bw_settings_ui_healing_policy(false);
    const auto saved=bw_settings_ui_test_saved();const unsigned calls=bw_settings_ui_health_calls();
    search(healing->label);
    require(item(healing->label).item_flags&ImGuiItemFlags_Disabled,
            "Missing certified return capability left Healing enabled");
    require(rendered_text.find("Pickups keep their native healing rate")!=std::string::npos,
            "Unavailable Healing did not explain its native fallback");
    click(healing->label);
    require(bw_settings_ui_test_session().healing_rate_q8==saved.healing_rate_q8&&
            bw_settings_ui_test_saved().healing_rate_q8==saved.healing_rate_q8&&
            bw_settings_ui_health_calls()==calls,"Disabled Healing erased or applied the saved preference");
    clear_search();search(damage->label);
    require(!(item(damage->label).item_flags&ImGuiItemFlags_Disabled),
            "Missing Healing capability also disabled qualified Damage");
    click(damage->label);click("Half");
    require(bw_settings_ui_test_session().damage_rate_q8==128&&bw_settings_ui_test_saved().damage_rate_q8==128&&
            bw_settings_ui_test_saved().healing_rate_q8==saved.healing_rate_q8&&bw_settings_ui_health_calls()>calls,
            "Independent Damage choice did not persist/apply while preserving Healing");
    clear_search();bw_settings_ui_test_flush();
    file_line(read_file(folder/"settings.ini"),"healing_rate_q8="+std::to_string(saved.healing_rate_q8));
    bw_settings_ui_healing_policy(true);search(healing->label);
    require(!(item(healing->label).item_flags&ImGuiItemFlags_Disabled),
            "Certified Healing capability did not re-enable its existing preference");
    require(bw_settings_ui_test_saved().healing_rate_q8==saved.healing_rate_q8,
            "Restoring Healing capability changed the saved rate");
    clear_search();
}
void clear_launch_environment() {
    // Only this fixture process's environment is changed; never the machine or
    // user's shell. Full explicit list makes a developer's startup overrides
    // unable to change this regression's expected defaults.
    const char* names[] = {"BLUEWAKE_CLIMB", "BLUEWAKE_CLIMB_STAMINA", "BLUEWAKE_STICK_CAMERA",
        "BLUEWAKE_STICK_CAMERA_SPEED", "BLUEWAKE_STICK_AIM_SPEED", "BLUEWAKE_STICK_CAMERA_INVERT_X",
        "BLUEWAKE_STICK_CAMERA_INVERT_Y", "BLUEWAKE_JUMP_BUTTON", "BLUEWAKE_SPRINT_SPEED",
        "BLUEWAKE_FAST_FORWARD", "BLUEWAKE_FADE_FRAMES", "BLUEWAKE_QUICK_DOORS", "BLUEWAKE_OVERLAP_OBSERVATION",
        "DOL_AURORA_RENDER_SCALE", "DOL_AURORA_FORCE_ANISO", "DOL_AURORA_FULLSCREEN",
        "BLUEWAKE_MOUSE_CAMERA", "BLUEWAKE_MOUSE_SENSITIVITY", "BLUEWAKE_MOUSE_INVERT_Y",
        "BLUEWAKE_HAPTICS", "BLUEWAKE_HAPTICS_STRENGTH", "BLUEWAKE_HAPTICS_TRIGGERS",
        "BLUEWAKE_ASPECT", "DOL_AURORA_ASPECT_FIT", "BLUEWAKE_MODS", "BLUEWAKE_OPTIONS",
        "DOL_AURORA_TEXTURE_PACK", "BLUEWAKE_DSP_MODE", "DOL_AURORA_WINDOW", "DOL_AURORA_FRAME_INTERP",
        "DOL_AURORA_FRAME_INTERP_STEPS", "DOL_AURORA_SHOW_FPS"};
    for (const auto* name : names) _putenv_s(name, "");
}
}

// Item discovery comes from the actual bundled ImGui widget implementations,
// compiled with their documented test hooks. Events act on real widget bounds
// and execute production handlers; no parallel imitation of draw_menu exists.
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* context, ImGuiID id, const ImRect& bounds,
                               const ImGuiLastItemData* last) {
    if (!id) return;
    auto& value = items[id];
    value.id = id;
    value.bounds = bounds;
    if (last) value.item_flags = last->ItemFlags;
    if (context->CurrentWindow) value.window = context->CurrentWindow->Name;
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID id, const char* label, ImGuiItemStatusFlags flags) {
    auto& value = items[id];
    value.id = id;
    value.label = label ? label : "";
    value.flags = flags;
}
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID id) {
    auto at = items.find(id);
    return at == items.end() ? nullptr : at->second.label.c_str();
}

int main(int argc, char** argv) {
    std::filesystem::path folder;
    try {
        const auto parent = argc > 1 ? std::filesystem::absolute(argv[1]) : std::filesystem::temp_directory_path();
        folder = parent / ("settings-ui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(std::filesystem::create_directories(folder), "Unable to create isolated fixture directory");
        trace.open(folder / "imgui-widget-trace.txt");
        const std::string data_dir = folder.string() + "\\";
        bw_settings_ui_mock_initialize();
        require(bluewake_controls_init(data_dir.c_str()), bluewake_controls_error());
        require(bluewake_controls_select(101), "Unable to select mock GUID/serial controller");
        require(bluewake_controls_set_key(false, 7, SDL_SCANCODE_N), "Unable to seed keyboard profile");
        require(bluewake_controls_set_action_key(BLUEWAKE_ACTION_QUICK_ITEMS,0,SDL_SCANCODE_B), "Unable to seed shortcut keyboard modifier");
        require(bluewake_controls_set_action_button(BLUEWAKE_ACTION_QUICK_ITEMS,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER), "Unable to seed shortcut controller modifier");
        require(bluewake_controls_set_button(7, SDL_GAMEPAD_BUTTON_NORTH), "Unable to seed controller profile");
        require(bluewake_controls_set_invert(false, false, true, false), "Unable to seed camera inversion");
        BluewakeControlsSnapshot controls{};
        bluewake_controls_snapshot(&controls);
        controls.dead_zones.camera = 12000;
        require(bluewake_controls_set_dead_zones(controls.dead_zones), "Unable to seed deadzone");
        require(bluewake_controls_set_action_key(BLUEWAKE_ACTION_JUMP, 0, SDL_SCANCODE_V), "Unable to seed Jump binding");
        require(bluewake_controls_save(), bluewake_controls_error());
        const auto controls_file = folder / "controls.ini";
        const auto profile_file_before = read_file(controls_file);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(1400, 1000);
        io.DeltaTime = 1.f / 60;
        io.ConfigInputTrickleEventQueue = false;
        io.Fonts->AddFontDefault();
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        require(pixels && width > 0 && height > 0, "ImGui font atlas did not build in memory");
        GImGui->TestEngineHookItems = true;

        Settings saved;
        saved.window_w = 1100;
        saved.window_h = 800;
        saved.render_scale = 1;
        saved.anisotropy = 4;
        saved.lle_audio = true;
        saved.audio_music = 75;
        saved.audio_sfx = 65;
        saved.dialogue_speed = 2;
        saved.healing_rate_q8 = 512;
        saved.autosave_interval = 180;
        saved.controller_swap_ab = true;
        saved.options["faster_animations"] = true;
        bw_settings_ui_test_reset(data_dir.c_str(), saved);
        clear_launch_environment();
        _putenv_s("DOL_AURORA_RENDER_SCALE", "3");
        _putenv_s("DOL_AURORA_FRAME_INTERP", "1");
        _putenv_s("BLUEWAKE_CLIMB", "1");
        _putenv_s("BLUEWAKE_ASPECT", "16:9");
        _putenv_s("BLUEWAKE_MODS", "betterww");
        _putenv_s("BLUEWAKE_OPTIONS", "none");
        _putenv_s("BLUEWAKE_DSP_MODE", "lle");
        bw_settings_apply_launch();
        const auto before_install = bw_settings_ui_mock_effects();
        bw_settings_install();
        const auto after_install = bw_settings_ui_mock_effects();
        require(after_install.quick_items_calls == before_install.quick_items_calls + 1 &&
                after_install.audio_calls == before_install.audio_calls + 1 &&
                after_install.audio_master == 100 && !after_install.audio_muted &&
                after_install.audio_music == 75 && after_install.audio_sfx == 65 &&
                after_install.faster_wind_calls == before_install.faster_wind_calls + 1 &&
                after_install.faster_boots_calls == before_install.faster_boots_calls + 1 &&
                !after_install.faster_wind_enabled && !after_install.faster_boots_enabled &&
                after_install.dialogue_speed_calls == before_install.dialogue_speed_calls + 1 &&
                after_install.autosave_calls == before_install.autosave_calls + 1 &&
                !after_install.autosave_enabled && after_install.autosave_interval == 180 &&
                after_install.quick_items_enabled == bw_settings_ui_test_session().quick_items &&
                after_install.dialogue_speed == bw_settings_ui_test_session().dialogue_speed,
                "Saved game features were not configured before the first renderer frame");
        bluewake_controls_set_legacy_preferences(true, false, false, false);
        require(bw_settings_ui_test_session().render_scale == 3 && bw_settings_ui_test_session().climb &&
                bw_settings_ui_test_session().smooth_motion && bw_settings_ui_test_session().aspect == "16:9" &&
                bw_settings_ui_test_session().betterww && bw_settings_ui_test_session().option_defaults_off,
                "Actual launch handler did not apply startup overrides to the session");
        bw_settings_ui_test_flush();
        auto saved_text = read_file(folder / "settings.ini");
        file_line(saved_text, "render_scale=1");
        file_line(saved_text, "climb=0");
        file_line(saved_text, "aspect=4:3");
        file_line(saved_text, "betterww=0");
        file_line(saved_text, "option.faster_animations=1");
        require(saved_text.find("option_defaults_off") == std::string::npos && saved_text.find("option.none") == std::string::npos,
                "Session-only option baseline leaked into settings.ini");
        frame(); frame();
        for (const auto* page : {"Display", "Controls", "Enhancements", "Mods", "Network", "Sound & Saves", "Developer", "HUD"})
            require(has_item(page), std::string("Missing actual menu page: ") + page);

        // Optional-handle failure path uses actual widgets without a picker or worker.
        click("Sound & Saves");
        require(rendered_text.find("WAV preview is unavailable")!=std::string::npos&&
                (item("Choose WAV").item_flags & ImGuiItemFlags_Disabled)&&
                (item("Play WAV").item_flags & ImGuiItemFlags_Disabled),
                "Null optional handle did not show disabled preview controls");
        click("Choose WAV");require(bw_settings_ui_mock_preview_picker_calls()==0,"Disabled Choose invoked a picker seam");
        click("Display");
        std::unique_ptr<BwAudioPreview,decltype(&bluewake_audio_preview_destroy)> preview(
            bluewake_audio_preview_create(),bluewake_audio_preview_destroy);
        require(bool(preview),"Cannot create actual optional preview worker");
        bw_settings_ui_mock_preview_bind(preview.get());
        preview_ui_checks(preview.get(),folder);
        unchanged_profile(controls_file,profile_file_before);
        healing_capability_ui_checks(folder);
        unchanged_profile(controls_file,profile_file_before);

        const auto* hud_opacity = bw_setting_find("hud.hearts.opacity");
        const auto* hud_enabled = bw_setting_find("hud.enabled");
        require(hud_opacity && hud_enabled && hud_opacity->page == BW_PAGE_HUD,
                "HUD controls did not use the real shared catalog");
        search("Hearts opacity");
        require(has_item(hud_opacity->label) && (item(hud_opacity->label).item_flags & ImGuiItemFlags_Disabled),
                "HUD search did not render its master-dependent disabled control");
        click(hud_opacity->label);
        require(bw_settings_ui_test_session().hud.groups[BW_HUD_HEARTS].opacity == 1.f,
                "Disabled HUD slider changed native preferences");
        clear_search();
        search(hud_enabled->label);
        click(hud_enabled->label, ImGuiItemStatusFlags_Checkable);
        const auto master_queue = bw_hud_settings_ui_effects();
        require(bw_settings_ui_test_session().hud_enabled && bw_settings_ui_test_saved().hud_enabled &&
                bw_hud_config_is_identity(&master_queue.queued),
                "Master HUD control did not persist or queue its native identity defaults");
        clear_search();
        search("Hearts opacity");
        require(!(item(hud_opacity->label).item_flags & ImGuiItemFlags_Disabled),
                "HUD dependency remained disabled after its master was enabled");
        const auto queued_before = bw_hud_settings_ui_effects().queued_calls;
        click(hud_opacity->label);
        const auto hud_value = bw_settings_ui_test_session().hud.groups[BW_HUD_HEARTS].opacity;
        require(hud_value >= 0 && hud_value < 1 &&
                bw_settings_ui_test_saved().hud.groups[BW_HUD_HEARTS].opacity == hud_value &&
                bw_hud_settings_ui_effects().queued.groups[BW_HUD_HEARTS].opacity == hud_value &&
                bw_hud_settings_ui_effects().queued_calls > queued_before,
                "Real HUD slider did not persist its edited value and queue copied configuration");
        require(bw_settings_ui_test_saved().render_scale == 1 && !bw_settings_ui_test_saved().climb &&
                !bw_settings_ui_test_saved().betterww && bw_settings_ui_test_saved().aspect == "4:3",
                "HUD edit persisted unrelated launch overrides");
        unchanged_profile(controls_file, profile_file_before);
        bw_hud_settings_ui_availability(BW_HUD_UNSUPPORTED_ASPECT);
        frame();
        const auto unavailable_calls = bw_hud_settings_ui_effects().queued_calls;
        require(item(hud_opacity->label).item_flags & ImGuiItemFlags_Disabled,
                "Unsupported native aspect did not disable HUD metadata controls");
        click(hud_opacity->label);
        require(bw_settings_ui_test_session().hud.groups[BW_HUD_HEARTS].opacity == hud_value &&
                bw_hud_settings_ui_effects().queued_calls == unavailable_calls,
                "Unavailable HUD control changed or queued gameplay configuration");
        bw_hud_settings_ui_availability(BW_HUD_AVAILABLE); frame();
        clear_search(); search(hud_enabled->label);
        click(hud_enabled->label, ImGuiItemStatusFlags_Checkable);
        const auto disabled_queue = bw_hud_settings_ui_effects();
        require(!bw_settings_ui_test_session().hud_enabled && !bw_settings_ui_test_saved().hud_enabled &&
                bw_hud_config_is_identity(&disabled_queue.queued) &&
                bw_settings_ui_test_saved().hud.groups[BW_HUD_HEARTS].opacity == hud_value,
                "Disabling HUD failed to queue native identity or erased stored pane preferences");
        clear_search();

        search("climb");
        require(has_item(bw_setting_find("climb")->label) && has_item(bw_setting_find("climb_stamina")->label),
                "Search did not render climbing controls");
        require(!has_item(bw_setting_find("anisotropy")->label) && !has_item("Presets"),
                "Search rendered unrelated Display controls or expanded presets");
        clear_search();
        search("no-such-bluewake-setting");
        require(rendered_text.find("No matching settings.") != std::string::npos,
                "Empty search did not render its real no-results state");
        clear_search();
        search("frame rate");
        click(bw_setting_find("show_fps")->label, ImGuiItemStatusFlags_Checkable);
        require(bw_settings_ui_test_session().show_fps && bw_settings_ui_test_saved().show_fps,
                "Real setting checkbox did not update session and persistent preference");
        require(bw_settings_ui_mock_effects().fps, "Real setting checkbox did not call the live frame-rate setter");
        require(bw_settings_ui_test_saved().render_scale == 1 && !bw_settings_ui_test_saved().climb &&
                !bw_settings_ui_test_saved().betterww && bw_settings_ui_test_saved().aspect == "4:3",
                "Unrelated widget edit persisted startup overrides");
        unchanged_profile(controls_file, profile_file_before);
        clear_search();

        search("cannon");
        require(rendered_text.find("X/Y/Z item assignments stay unchanged.") != std::string::npos,
                "Fixed shortcut help did not explain preserved item assignments");
        click(bw_setting_find("quick_items")->label, ImGuiItemStatusFlags_Checkable);
        require(bw_settings_ui_test_session().quick_items && bw_settings_ui_test_saved().quick_items &&
                bw_settings_ui_mock_effects().quick_items_enabled,
                "Shortcut switch did not persist/apply the real live setting");
        unchanged_profile(controls_file, profile_file_before);
        clear_search();

        search("cutscene");
        require(rendered_text.find("supported ordinary NPC and cutscene messages") != std::string::npos &&
                rendered_text.find("Scripted waits, page stops, choices and unskippable text stay unchanged") != std::string::npos,
                "Dialogue speed help did not explain supported messages and native timing");
        const auto dialogue_calls=bw_settings_ui_mock_effects().dialogue_speed_calls;
        click(bw_setting_find("dialogue_speed")->label);
        require(bw_settings_ui_test_session().dialogue_speed>1 && bw_settings_ui_test_session().dialogue_speed<=10 &&
                bw_settings_ui_test_saved().dialogue_speed==bw_settings_ui_test_session().dialogue_speed &&
                bw_settings_ui_mock_effects().dialogue_speed_calls>dialogue_calls,
                "Actual dialogue speed slider did not persist/apply the live multiplier");
        unchanged_profile(controls_file,profile_file_before);
        clear_search();

        search("wind changes");
        const auto wind_calls = bw_settings_ui_mock_effects().faster_wind_calls;
        const auto boots_calls = bw_settings_ui_mock_effects().faster_boots_calls;
        click(bw_setting_find("faster_wind")->label, ImGuiItemStatusFlags_Checkable);
        require(bw_settings_ui_test_session().faster_wind && bw_settings_ui_test_saved().faster_wind &&
                bw_settings_ui_mock_effects().faster_wind_enabled &&
                bw_settings_ui_mock_effects().faster_wind_calls == wind_calls + 1 &&
                bw_settings_ui_mock_effects().faster_boots_calls == boots_calls &&
                !bw_settings_ui_mock_effects().faster_boots_enabled,
                "Wind checkbox did not independently publish/persist its live preference");
        unchanged_profile(controls_file, profile_file_before);
        clear_search();
        search("Iron Boots");
        click(bw_setting_find("faster_boots")->label, ImGuiItemStatusFlags_Checkable);
        require(bw_settings_ui_test_session().faster_boots && bw_settings_ui_test_saved().faster_boots &&
                bw_settings_ui_mock_effects().faster_boots_enabled &&
                bw_settings_ui_mock_effects().faster_boots_calls == boots_calls + 1 &&
                bw_settings_ui_mock_effects().faster_wind_calls == wind_calls + 1 &&
                bw_settings_ui_mock_effects().faster_wind_enabled,
                "Boots checkbox reset the independently selected wind preference");
        unchanged_profile(controls_file, profile_file_before);
        clear_search();

        search("Master volume");
        const auto audio_calls = bw_settings_ui_mock_effects().audio_calls;
        click(bw_setting_find("audio_master")->label);
        require(bw_settings_ui_test_session().audio_master >= 0 && bw_settings_ui_test_session().audio_master <= 100 &&
                bw_settings_ui_test_saved().audio_master == bw_settings_ui_test_session().audio_master &&
                bw_settings_ui_mock_effects().audio_master == static_cast<unsigned>(bw_settings_ui_test_session().audio_master) &&
                bw_settings_ui_mock_effects().audio_calls > audio_calls &&
                bw_settings_ui_mock_effects().audio_music == 75 && bw_settings_ui_mock_effects().audio_sfx == 65,
                "Master volume widget did not persist its value or reset independent category gains");
        unchanged_profile(controls_file, profile_file_before);
        clear_search();
        search("Mute game audio");
        click(bw_setting_find("audio_muted")->label, ImGuiItemStatusFlags_Checkable);
        require(bw_settings_ui_test_session().audio_muted && bw_settings_ui_test_saved().audio_muted &&
                bw_settings_ui_mock_effects().audio_muted &&
                bw_settings_ui_mock_effects().audio_master == static_cast<unsigned>(bw_settings_ui_test_session().audio_master) &&
                bw_settings_ui_mock_effects().audio_music == 75 && bw_settings_ui_mock_effects().audio_sfx == 65,
                "Master mute changed the selected volume or category gains");
        unchanged_profile(controls_file, profile_file_before);
        clear_search();

        search("Exact audio");
        click(bw_setting_find("lle_audio")->label, ImGuiItemStatusFlags_Checkable);
        require(!bw_settings_ui_test_session().lle_audio, "Could not enable HLE category controls");
        clear_search();
        for (const auto* id : {"audio_music", "audio_sfx"}) {
            const Settings audio_before = bw_settings_ui_test_session();
            const auto calls_before = bw_settings_ui_mock_effects().audio_calls;
            search(bw_setting_find(id)->label);
            click(bw_setting_find(id)->label);
            const auto& selected = bw_settings_ui_test_session();
            const auto& persisted = bw_settings_ui_test_saved();
            const auto effects = bw_settings_ui_mock_effects();
            require(selected.audio_music == persisted.audio_music && selected.audio_sfx == persisted.audio_sfx &&
                    effects.audio_music == static_cast<unsigned>(selected.audio_music) &&
                    effects.audio_sfx == static_cast<unsigned>(selected.audio_sfx) &&
                    effects.audio_calls > calls_before && effects.audio_muted &&
                    selected.audio_master == audio_before.audio_master && selected.audio_muted == audio_before.audio_muted &&
                    (std::string(id) == "audio_music" ?
                        selected.audio_music != audio_before.audio_music && selected.audio_sfx == audio_before.audio_sfx :
                        selected.audio_sfx != audio_before.audio_sfx && selected.audio_music == audio_before.audio_music),
                    "Category volume widget failed to apply/persist independently");
            unchanged_profile(controls_file, profile_file_before);
            clear_search();
        }
        search("Exact audio");
        click(bw_setting_find("lle_audio")->label, ImGuiItemStatusFlags_Checkable);
        require(bw_settings_ui_test_session().lle_audio, "Exact audio preference was not restored");
        clear_search();

        search("autosave");
        const auto autosave_calls = bw_settings_ui_mock_effects().autosave_calls;
        click(bw_setting_find("autosave")->label, ImGuiItemStatusFlags_Checkable);
        require(bw_settings_ui_test_session().autosave && bw_settings_ui_test_saved().autosave &&
                bw_settings_ui_mock_effects().autosave_enabled &&
                bw_settings_ui_mock_effects().autosave_calls == autosave_calls + 1,
                "Actual autosave checkbox did not persist/apply the native-save preference");
        click(bw_setting_find("autosave_interval")->label);
        require(bw_settings_ui_test_session().autosave_interval >= 60 &&
                bw_settings_ui_test_session().autosave_interval <= 3600 &&
                bw_settings_ui_test_saved().autosave_interval == bw_settings_ui_test_session().autosave_interval &&
                bw_settings_ui_mock_effects().autosave_interval == bw_settings_ui_test_session().autosave_interval,
                "Actual autosave interval did not persist/apply bounded seconds");
        unchanged_profile(controls_file, profile_file_before);
        clear_search();

        click("Presets");
        choose_preset("Performance");
        section("Sound & Saves", false);
        section("Display", true);
        const Settings before_preview = bw_settings_ui_test_session();
        const auto effects_before = bw_settings_ui_mock_effects();
        click("Preview changes");
        require(bw_settings_ui_test_preset_open() && rendered_text.find("Performance: 3 setting changes") != std::string::npos,
                "Real preview popup did not show the three selected Display changes");
        require(bw_settings_ui_test_session().anisotropy == 4 && bw_settings_ui_test_session().render_scale == 3 &&
                bw_settings_ui_test_session().smooth_motion && bw_settings_ui_test_session().lle_audio,
                "Preview mutated settings before Apply");
        require(bw_settings_ui_mock_effects().anisotropy_calls == effects_before.anisotropy_calls,
                "Preview applied a live renderer change");
        click("Cancel");
        require(!bw_settings_ui_test_preset_open() && bw_settings_ui_test_menu_open(), "Cancel failed to close only the preview popup");
        require(bw_settings_ui_test_session().render_scale == before_preview.render_scale &&
                bw_settings_ui_test_session().anisotropy == before_preview.anisotropy,
                "Cancel changed the actual settings state");
        click("Preview changes");
        require(bw_settings_key(0x1B, 0) == 1, "Actual Escape hotkey handler did not consume a preset cancellation");
        frame(); frame();
        require(!bw_settings_ui_test_preset_open() && bw_settings_ui_test_menu_open(), "Escape closed the menu instead of cancelling its preview");
        click("Preview changes");
        click("Apply");
        require(!bw_settings_ui_test_preset_open(), "Apply left the preset popup open");
        const auto& after_performance = bw_settings_ui_test_session();
        require(after_performance.render_scale == 1 && after_performance.anisotropy == 1 && !after_performance.smooth_motion,
                "Preset Apply did not commit selected Display changes");
        require(after_performance.lle_audio && after_performance.climb && after_performance.betterww && after_performance.aspect == "16:9",
                "Preset Apply changed an unselected section");
        require(bw_settings_ui_mock_effects().render_scale == 1 && bw_settings_ui_mock_effects().anisotropy == 1 &&
                !bw_settings_ui_mock_effects().smooth, "Actual preset handler did not apply live renderer settings");
        unchanged_profile(controls_file, profile_file_before);

        choose_preset("Original");
        for (const auto* page : {"Display", "Enhancements", "Mods", "Sound & Saves"}) section(page, false);
        section("Controls", true);
        click("Preview changes");
        click("Apply");
        require(!bw_settings_ui_test_session().controller_swap_ab && !bw_settings_ui_test_session().mouse_camera &&
                !bw_settings_ui_test_session().stick_camera, "Controls-only Original preset did not execute actual control preference handlers");
        require(bw_settings_ui_test_session().lle_audio && bw_settings_ui_test_session().climb,
                "Controls-only preset changed another section");
        unchanged_profile(controls_file, profile_file_before);

        // Local ImGui Escape exercises draw_menu's real close handler, which
        // flushes both settings and profiles through production atomic writers.
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
        frame();
        require(!bw_settings_ui_test_menu_open() && !bw_settings_ui_mock_effects().blocked,
                "Actual menu close handler did not unblock input");
        saved_text = read_file(folder / "settings.ini");
        file_line(saved_text, "show_fps=1");
        file_line(saved_text, "render_scale=1");
        file_line(saved_text, "anisotropy=1");
        file_line(saved_text, "smooth_motion=0");
        file_line(saved_text, "climb=0");
        file_line(saved_text, "betterww=0");
        file_line(saved_text, "aspect=4:3");
        file_line(saved_text, "lle_audio=1");
        file_line(saved_text, "controller_swap_ab=0");
        file_line(saved_text, "quick_items=1");
        file_line(saved_text, "faster_wind=1");
        file_line(saved_text, "faster_boots=1");
        file_line(saved_text, "audio_master=" + std::to_string(bw_settings_ui_test_saved().audio_master));
        file_line(saved_text, "audio_music=" + std::to_string(bw_settings_ui_test_saved().audio_music));
        file_line(saved_text, "audio_sfx=" + std::to_string(bw_settings_ui_test_saved().audio_sfx));
        file_line(saved_text, "audio_muted=1");
        file_line(saved_text,"dialogue_speed="+bw_setting_value(bw_settings_ui_test_saved(),*bw_setting_find("dialogue_speed")));
        file_line(saved_text, "autosave=1");
        file_line(saved_text, "autosave_interval=" + std::to_string(bw_settings_ui_test_saved().autosave_interval));
        file_line(saved_text, "option.faster_animations=1");
        file_line(saved_text, "hud.enabled=0");
        file_line(saved_text, "hud.hearts.opacity=" + bw_setting_value(bw_settings_ui_test_saved(), *hud_opacity));
        require(saved_text.find("option_defaults_off") == std::string::npos, "Closing menu persisted the session-only baseline");
        require(bluewake_controls_load(), bluewake_controls_error());
        unchanged_profile(controls_file, profile_file_before);
        require(preview_status(preview.get()).state==BW_PREVIEW_CANCELLED,
                "Final ordinary menu close failed to leave preview cancelled");
#ifdef _WIN32
        controls_save_retry_ui_checks(controls_file);
#endif
        bw_settings_ui_mock_preview_bind(nullptr);
        preview.reset();
        const auto effects = bw_settings_ui_mock_effects();
        require(effects.native_window_operations == 0 && effects.texture_uploads == 0 && effects.quit_requests == 0 &&
                effects.state_requests == 0, "Headless UI test reached a native/GPU/state/quit operation");
        ImGui::DestroyContext();
        std::ofstream(folder / "result.txt") << "PASS: actual draw_menu, search/results, preset preview/cancel/Escape/section apply, "
            "launch override separation, atomic save, GUID/serial profile preservation and reload. No SDL init/video/native input.\n";
        std::cout << "PASS actual headless ImGui settings UI; evidence: " << folder.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Settings UI fixture failed: " << error.what() << "\nEvidence: " << folder.string()
                  << "\nLast actual UI text:\n" << rendered_text << '\n';
        return 1;
    }
}
