#ifdef NDEBUG
#undef NDEBUG
#endif
#include "settings_ui_mocks.h"
#include "settings_ui_test_api.h"
#include "settings_catalog.h"
#include "settings_presets.h"
#include "controls_bindings.h"
#include "health_host.h"
#include "win_settings.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <aurora/imgui.h>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef rename
#undef rename
#endif
#include <filesystem>
void bw_menu_size_test_load_font();
void bw_menu_size_test_release_font();
bool bw_menu_size_test_restart_needed();
float bw_menu_size_test_auto_scale();
float bw_menu_size_test_raster_scale();
void bw_menu_size_mock_display(float, int);
const std::vector<uint8_t>& bw_menu_size_mock_uploaded_pixels();
unsigned bw_menu_size_mock_uploaded_width();
unsigned bw_menu_size_mock_uploaded_height();
#if MENU_SIZE_PROJECTED
void bw_menu_size_test_font_mode(bool, bool);
#endif

#if defined(BW_MENU_SIZE_PUBLIC_FIXTURE)
#include "menu_size_font_baseline.inc"
#endif

namespace {
struct Item { ImGuiID id{}; ImRect box; std::string label, window; ImGuiItemStatusFlags flags{}; ImGuiItemFlags item_flags{}; float font{}; };
std::map<ImGuiID,Item> items;
std::string rendered;
std::ofstream trace;
unsigned frames;
void need(bool ok, const std::string& why) { if(!ok) throw std::runtime_error(why); }
void frame() {
    bw_settings_ui_mock_frame(); items.clear();
    ImGui::NewFrame(); ImGui::LogToBuffer();
    bw_settings_ui_test_draw();
    rendered.assign(GImGui->LogBuffer.begin(), GImGui->LogBuffer.end());
    ImGui::LogFinish(); ImGui::Render();
    need(ImGui::GetDrawData()->TotalVtxCount > 0, "Menu did not produce actual geometry");
    trace << "FRAME " << ++frames << '\n' << rendered;
}
Item find(const char* label) {
    for(const auto& [id,item]:items) if(item.label==label) return item;
    auto* w=ImGui::FindWindowByName("BlueWake settings");
    if(w) { auto p=items.find(w->GetID(label)); if(p!=items.end()) return p->second; }
    if(w) {
        const char* definition=nullptr;
        if(std::strcmp(label,"Keyboard Sprint mode")==0||std::strcmp(label,"##sprint_keyboard_mode")==0)definition="sprint_keyboard_mode";
        if(std::strcmp(label,"Controller Sprint mode")==0||std::strcmp(label,"##sprint_controller_mode")==0)definition="sprint_controller_mode";
        if(std::strcmp(label,"Ordinary damage (experimental)")==0||std::strcmp(label,"##damage_rate_q8")==0)definition="damage_rate_q8";
        if(std::strcmp(label,"Heart and fairy pickups (experimental)")==0||std::strcmp(label,"##healing_rate_q8")==0)definition="healing_rate_q8";
        if(definition) {
            const auto* bar=GImGui->TabBars.GetByKey(w->GetID("tabs"));
            const auto seed=ImHashStr(definition,0,bar?bar->SelectedTabId:w->IDStack[0]);
            const auto p=items.find(ImHashStr(label,0,seed));
            if(p!=items.end())return p->second; // BeginCombo has ItemAdd without an ItemInfo label.
        }
    }
    throw std::runtime_error(std::string("Missing actual widget ")+label);
}
void expose(const char* label) {
    for(unsigned n=0;n<4;++n) {
        const auto item=find(label); auto* w=ImGui::FindWindowByName(item.window.c_str());
        need(w,"Widget owning window missing");
        const auto c=item.box.GetCenter();
        if(w->InnerClipRect.Contains(c)) return;
        ImGui::SetScrollY(w,std::clamp(w->Scroll.y+c.y-w->InnerClipRect.GetCenter().y,0.f,w->ScrollMax.y));
        frame();frame();
    }
}
void activate(const char* label, bool gamepad=false) {
    expose(label); const auto item=find(label);
    auto* w=ImGui::FindWindowByName(item.window.c_str());
    ImGui::FocusWindow(w); ImGui::SetFocusID(item.id,w);
    ImGui::SetNavCursorVisible(true);
    const auto key=gamepad?ImGuiKey_GamepadFaceDown:ImGuiKey_Enter;
    ImGui::GetIO().AddKeyEvent(key,true); frame();
    ImGui::GetIO().AddKeyEvent(key,false); frame();frame();
}
void click(const char* label) {
    expose(label); const auto c=find(label).box.GetCenter();
    auto& io=ImGui::GetIO(); io.AddMousePosEvent(c.x,c.y); frame();
    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();frame();
}
void edit_size(int value) {
    const auto label = [&]() {try {find("Menu size (%)");return "Menu size (%)";}catch(...) {return "##menu_size";}};
    expose(label()); const auto item=find(label());
    auto* w=ImGui::FindWindowByName(item.window.c_str());
    ImGui::FocusWindow(w);ImGui::SetFocusID(item.id,w);ImGui::SetNavCursorVisible(true);
    auto& io=ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_Enter,true);frame();io.AddKeyEvent(ImGuiKey_Enter,false);frame();
    io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();
    io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
    io.AddInputCharactersUTF8(std::to_string(value).c_str());frame();
    io.AddKeyEvent(ImGuiKey_Enter,true);frame();io.AddKeyEvent(ImGuiKey_Enter,false);frame();frame();
}
std::string read(const std::filesystem::path& p) {std::ifstream f(p,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}};}
uint64_t hash(const std::vector<uint8_t>& bytes) {uint64_t h=1469598103934665603ull;for(auto b:bytes){h^=b;h*=1099511628211ull;}return h;}
void rect(std::ostream& o, const ImRect& r) {o<<'['<<r.Min.x<<','<<r.Min.y<<','<<r.Max.x<<','<<r.Max.y<<']';}
void font_metrics(std::ostream& o) {
    const auto* font=ImGui::GetIO().FontDefault;
    const auto& pixels=bw_menu_size_mock_uploaded_pixels();
    const unsigned width=bw_menu_size_mock_uploaded_width(),height=bw_menu_size_mock_uploaded_height();
    o<<"{\"size\":"<<font->FontSize<<",\"ascent\":"<<font->Ascent<<",\"descent\":"<<font->Descent<<",\"glyphs\":[";
    bool first=true;
    for(const auto& glyph:font->Glyphs) {
        if(!first)o<<',';first=false;
        const int x0=static_cast<int>(std::round(glyph.U0*width)),x1=static_cast<int>(std::round(glyph.U1*width));
        const int y0=static_cast<int>(std::round(glyph.V0*height)),y1=static_cast<int>(std::round(glyph.V1*height));
        need(x0>=0&&y0>=0&&x1<=int(width)&&y1<=int(height),"Glyph UV escaped copied atlas");
        uint64_t h=1469598103934665603ull;
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)for(unsigned c=0;c<4;++c) {h^=pixels[(size_t(y)*width+x)*4+c];h*=1099511628211ull;}
        o<<'['<<glyph.Codepoint<<','<<glyph.AdvanceX<<','<<glyph.X0<<','<<glyph.Y0<<','<<glyph.X1<<','<<glyph.Y1<<','<<glyph.Visible<<','<<glyph.Colored<<','<<h<<']';
    }
    o<<"]}";
}
void config_tests(const std::filesystem::path& dir) {
#if MENU_SIZE_PROJECTED
    Settings modes;
    need(modes.sprint_keyboard_mode==BW_SPRINT_HOLD&&modes.sprint_controller_mode==BW_SPRINT_TOGGLE,"Legacy mode defaults changed");
    for(const char* id:{"sprint_keyboard_mode","sprint_controller_mode"}) {
        const auto* d=bw_setting_find(id);
        need(d&&d->type==BW_SETTING_CHOICE&&d->apply==BW_SETTING_LIVE&&d->page==BW_PAGE_CONTROLS,"Mode catalog missing live typed choice");
        for(const char* invalid:{"0","1","HOLD","Toggle","togglex",""})
            need(!bw_setting_assign(modes,id,invalid),"Invalid mode accepted");
    }
    need(bw_setting_assign(modes,"sprint_keyboard_mode","toggle")&&bw_setting_assign(modes,"sprint_controller_mode","hold"),"Mode adapters failed");
    const auto mp=bw_settings_capture_preset("Modes",modes,1u<<BW_PAGE_CONTROLS);
    need(mp.values.at("sprint_keyboard_mode")=="toggle"&&mp.values.at("sprint_controller_mode")=="hold","Controls preset omitted modes");
    Settings applied;
    need(bw_settings_preset_apply(mp,applied,1u<<BW_PAGE_CONTROLS)&&
         applied.sprint_keyboard_mode==BW_SPRINT_TOGGLE&&applied.sprint_controller_mode==BW_SPRINT_HOLD,"Controls preset did not apply modes");
    const auto kept=applied;
    need(bw_settings_preset_apply(mp,applied,1u<<BW_PAGE_DISPLAY)&&
         applied.sprint_keyboard_mode==kept.sprint_keyboard_mode&&applied.sprint_controller_mode==kept.sprint_controller_mode,"Unselected Controls section changed modes");
    for(const auto& p:bw_settings_builtin_presets())
        need(!p.values.count("sprint_keyboard_mode")&&!p.values.count("sprint_controller_mode"),"Built-in preset resets accessibility modes");
    const auto* def=bw_setting_find("menu_size");
    need(def && def->minimum==75 && def->maximum==200 && def->apply==BW_SETTING_LIVE &&
         std::string(def->default_value)=="100","Catalog default/bounds/live policy incorrect");
    Settings saved; saved.render_scale=1; saved.menu_size=150; saved.audio_music=73;
    std::string error;
    for(const char* bad:{"74","201","nan","-1","100.5","100x"})
        need(!bw_setting_assign(saved,"menu_size",bad,&error)&&saved.menu_size==150,"Invalid size mutated saved preference");
    const auto preset=bw_settings_capture_preset("Large text",saved,1u<<BW_PAGE_DISPLAY);
    need(preset.values.at("menu_size")=="150","Portable preset omitted size");
    const auto pp=dir/"size.bwpreset";
    need(bw_settings_preset_write(pp.string(),preset,&error),error);
    BwSettingsPreset loaded;need(bw_settings_preset_read(pp.string(),loaded,&error),error);
    need(loaded.values.at("menu_size")=="150","Preset persistence changed size");
    const auto builtins=bw_settings_builtin_presets();
    for(const auto& p:builtins)need(p.values.find("menu_size")==p.values.end(),"Built-in preset reset accessibility size");
    Settings before=saved,session=saved;session.render_scale=4;session.audio_music=15;
    bw_settings_keep_edits(saved,before,session);
    need(saved.menu_size==150,"Unrelated overrides mutated size");
    before=session;session.menu_size=200;bw_settings_keep_edits(saved,before,session);
    need(saved.menu_size==200,"User size change not copied to saved preference");
    before=session;session.render_scale=2;bw_settings_keep_edits(saved,before,session);
    need(saved.menu_size==200,"Unrelated edit reset size");
#endif
}
}
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* c,ImGuiID id,const ImRect& r,const ImGuiLastItemData* info) {
    if(!id)return;auto& v=items[id];v.id=id;v.box=r;v.font=c->FontSize;if(info)v.item_flags=info->ItemFlags;if(c->CurrentWindow)v.window=c->CurrentWindow->Name;
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*,ImGuiID id,const char* label,ImGuiItemStatusFlags flags) {auto& v=items[id];v.id=id;v.label=label?label:"";v.flags=flags;}
void ImGuiTestEngineHook_Log(ImGuiContext*,const char*,...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*,ImGuiID id) {auto p=items.find(id);return p==items.end()?nullptr:p->second.label.c_str();}

int main(int argc,char** argv) {
    try {
        need(argc==5 || argc==6,"Expected width height percent output-dir [default|fallback|retry]");
        int width=std::stoi(argv[1]),height=std::stoi(argv[2]),percent=std::stoi(argv[3]);
        const auto dir=std::filesystem::absolute(argv[4]);std::filesystem::create_directories(dir);
#if MENU_SIZE_PROJECTED
        if(argc==6 && std::string(argv[5])=="reload") {
            const auto text=read(dir/"settings.ini");
            const std::string data=dir.string()+"\\";
            bw_settings_load(data.c_str());
            need(bw_settings_ui_test_saved().menu_size==percent,"Fresh process did not reload persisted menu size");
            need(bw_settings_ui_test_saved().sprint_keyboard_mode==BW_SPRINT_TOGGLE&&
                 bw_settings_ui_test_saved().sprint_controller_mode==BW_SPRINT_HOLD,"Fresh process lost Sprint modes");
            bw_settings_apply_launch();
            need(bw_health_host_configuration().damage_q8==512&&bw_health_host_configuration().healing_q8==64,
                 "Health startup publication was postponed until the overlay install");
            bw_settings_install();BwSprintMode key,pad;bluewake_sprint_modes(&key,&pad);
            need(key==BW_SPRINT_TOGGLE&&pad==BW_SPRINT_HOLD,"Startup did not publish persisted modes before UI");
            need(read(dir/"settings.ini")==text,"Fresh process load modified saved preference");
            std::puts("PASS fresh process native settings parser menu-size persistence");return 0;
        }
#endif
        if(argc==6&&std::string(argv[5])=="legacy-read") {
            const std::string old="menu_size=100\nrender_scale=2\naudio_music=73\n";
            {std::ofstream file(dir/"settings.ini",std::ios::binary);file<<old;}
            const std::string data=dir.string()+"\\";
            bw_settings_load(data.c_str());bw_settings_apply_launch();bw_settings_install();
            BwSprintMode key,pad;bluewake_sprint_modes(&key,&pad);
            need(key==BW_SPRINT_HOLD&&pad==BW_SPRINT_TOGGLE,"Legacy settings did not preserve prior source defaults");
            need(read(dir/"settings.ini")==old,"Legacy settings load rewrote the old file");
            std::puts("PASS legacy settings missing modes preserve source defaults without rewriting");return 0;
        }
        trace.open(dir/"widget-trace.txt");
        bw_settings_ui_mock_initialize();
        const std::string data=dir.string()+"\\";
        need(bluewake_controls_init(data.c_str()),bluewake_controls_error());
        need(bluewake_controls_select(101),"Selected profile missing");
        need(bluewake_controls_set_key(false,7,SDL_SCANCODE_N),"Keyboard sentinel setup failed");
        need(bluewake_controls_save(),bluewake_controls_error());
        const auto bindings_before=read(dir/"controls.ini");
        config_tests(dir);
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=io.LogFilename=nullptr;
        io.DisplaySize=ImVec2(float(width),float(height));io.DeltaTime=1.f/60;
        io.ConfigInputTrickleEventQueue=false;io.BackendFlags|=ImGuiBackendFlags_HasGamepad;
        io.Fonts->AddFontDefault(); unsigned char* tex=nullptr;int tw=0,th=0;io.Fonts->GetTexDataAsRGBA32(&tex,&tw,&th);
        GImGui->TestEngineHookItems=true;
        Settings saved;
#if MENU_SIZE_PROJECTED
        saved.menu_size=percent;
#else
        need(percent==100,"Baseline only qualifies original100 geometry");
#endif
        saved.audio_music=73;saved.audio_sfx=61;saved.render_scale=1;
        saved.damage_rate_q8=512;saved.healing_rate_q8=64;
        bw_settings_ui_test_reset(data.c_str(),saved);
#if MENU_SIZE_PROJECTED
        const std::string mode=argc==6?argv[5]:"default";
        bw_menu_size_test_font_mode(mode=="fallback",mode=="retry");
#endif
        bw_menu_size_mock_display(1.f,height);
        unsigned baseline_uploads = 0;
        std::string original_font_metrics;
#if defined(BW_MENU_SIZE_PUBLIC_FIXTURE)
        if (percent == 100 && mode == "default") {
            const ImGuiStyle original_style = ImGui::GetStyle();
            single_font_baseline::load_font(nullptr);
            std::ostringstream reference; reference << std::setprecision(9);
            font_metrics(reference); original_font_metrics = reference.str();
            auto* original_atlas = io.FontDefault->ContainerAtlas;
            io.FontDefault = nullptr; delete original_atlas;
            ImGui::GetStyle() = original_style;
            baseline_uploads = bw_settings_ui_mock_effects().texture_uploads;
        }
#endif
        bw_menu_size_test_load_font();
        if (!original_font_metrics.empty()) {
            std::ostringstream actual; actual << std::setprecision(9);
            font_metrics(actual);
            need(actual.str() == original_font_metrics, "Original100 glyph metrics or raster pixels changed");
        }
        const auto default_atlas=io.Fonts;
        const auto uploaded_hash=hash(bw_menu_size_mock_uploaded_pixels());
        const auto uploads=bw_settings_ui_mock_effects().texture_uploads;
        need(uploads==baseline_uploads+1,"Font load did not use exactly one copied upload seam");
        need(io.FontDefault && io.FontDefault->ContainerAtlas != default_atlas,"Font used backend-managed atlas");
        need(bw_menu_size_mock_uploaded_width()>0 && bw_menu_size_mock_uploaded_height()>0,"Atlas copy missing");
        frame();frame();
        auto* menu=ImGui::FindWindowByName("BlueWake settings");need(menu,"Actual menu missing");
        const auto full = [&]() {try{return find("Fullscreen");}catch(...){return find("##fullscreen");}};
        const auto fps = [&]() {try{return find("Show frame rate");}catch(...){return find("##show_fps");}};
        const float font=full().font;
        const float base_auto=bw_menu_size_test_auto_scale();
        const float expected=io.FontDefault->FontSize*base_auto/bw_menu_size_test_raster_scale()*(percent/100.f);
        need(std::abs(font-expected)<0.001f,"Selected text enlargement was capped or double scaled");
        need(menu->Pos.x>=0 && menu->Pos.y>=0 && menu->Pos.x+menu->Size.x<=width+1 && menu->Pos.y+menu->Size.y<=height+1,"Menu left viewport");
        std::ofstream output(dir/"geometry.json");
        output<<std::setprecision(9);
        output<<"{\"width\":"<<width<<",\"height\":"<<height<<",\"percent\":"<<percent<<",\"font\":"<<font<<",\"raster\":"<<bw_menu_size_test_raster_scale()<<",\"window\":";
        rect(output,ImRect(menu->Pos,ImVec2(menu->Pos.x+menu->Size.x,menu->Pos.y+menu->Size.y)));
        output<<",\"fullscreen\":";rect(output,full().box);
        output<<",\"show_fps\":";rect(output,fps().box);
        output<<",\"tabs\":[";
        bool first_tab=true;
        for(const char* tab:{"Display","Controls","Enhancements","Mods","Network","Sound & Saves","HUD","Developer"}) {
            if(!first_tab)output<<',';first_tab=false;rect(output,find(tab).box);
        }
        output<<']';
        output<<",\"base_font_metrics_and_glyph_bitmaps\":";font_metrics(output);
        output<<",\"atlas\":["<<bw_menu_size_mock_uploaded_width()<<','<<bw_menu_size_mock_uploaded_height()<<"]}";
        output.close();
#if MENU_SIZE_PROJECTED
        // Persisted size applies live and is independent of restart settings.
        need(!bw_menu_size_test_restart_needed(),"Live size unexpectedly needs restart");
        edit_size(percent==200?150:200);
        need(bw_settings_ui_test_session().menu_size==(percent==200?150:200)&&bw_settings_ui_test_saved().menu_size==(percent==200?150:200),"Actual slider handler did not apply live size");
        need(!bw_menu_size_test_restart_needed(),"Live size edit queued restart");
        edit_size(percent);
        need(bw_settings_ui_test_session().menu_size==percent&&bw_settings_ui_test_saved().menu_size==percent,"Actual slider restore did not preserve selected size");
        bw_settings_ui_test_flush();
        need(read(dir/"settings.ini").find("menu_size="+std::to_string(percent)+"\r\n")!=std::string::npos,"Settings save omitted size");
        const auto before_resize_font=font;
        io.DisplaySize=ImVec2(float(width)+160,float(height)+90);frame();frame();
        need(bw_settings_ui_test_session().menu_size==percent&&bw_settings_ui_test_saved().menu_size==percent,"Resize mutated stored size");
        io.DisplaySize=ImVec2(float(width),float(height));frame();frame();
        need(std::abs(full().font-before_resize_font)<0.001f,"Resize did not restore the selected font size");
        // Actual tab handlers run via keyboard and gamepad activation after
        // the local fixture focuses an actual item ID (no desktop focus).
        for(const char* tab:{"Controls","Enhancements","Mods","Network","Sound & Saves","HUD","Developer","Display"}) {
            ImGui::SetScrollY(menu,0);frame();frame();
            activate(tab,false);need(GImGui->TabBars.GetByKey(menu->GetID("tabs"))->SelectedTabId==find(tab).id,std::string("Keyboard tab activation failed: ")+tab);
            activate(tab,true);need(GImGui->TabBars.GetByKey(menu->GetID("tabs"))->SelectedTabId==find(tab).id,std::string("Gamepad tab activation failed: ")+tab);
        }
        activate("Enhancements",false);frame();
        const auto health_label=[&](const char* normal,const char* compact) {try{find(normal);return normal;}catch(...){return compact;}};
        const char* damage_label=health_label("Ordinary damage (experimental)","##damage_rate_q8");
        const char* healing_label=health_label("Heart and fairy pickups (experimental)","##healing_rate_q8");
        activate(damage_label);activate("Half");
        activate(healing_label,true);activate("Double",true);
        need(bw_settings_ui_test_saved().damage_rate_q8==128&&bw_settings_ui_test_saved().healing_rate_q8==512&&
             bw_health_host_configuration().damage_q8==128&&bw_health_host_configuration().healing_q8==512,
             "Actual health combos did not publish and persist independent rates");
        bw_settings_ui_health_policy(false,false);frame();frame();
        need((find(damage_label).item_flags&ImGuiItemFlags_Disabled)!=0,"Unavailable module enabled health widget");
        activate(damage_label);need(bw_settings_ui_test_saved().damage_rate_q8==128,"Disabled module changed saved health preference");
        bw_settings_ui_health_policy(true,true);frame();frame();
        need((find(healing_label).item_flags&ImGuiItemFlags_Disabled)!=0,"Mounted room enabled health widget");
        const auto rejected_calls=bw_settings_ui_health_calls();activate(healing_label,true);
        need(bw_settings_ui_health_calls()==rejected_calls&&bw_health_host_configuration().damage_q8==256&&
             bw_health_host_configuration().healing_q8==256,"Disabled room widget called health mutator");
        // Return to personal fixture policy, restore original native-adapter
        // choices via real widgets for fresh-process/preset persistence checks.
        bw_settings_ui_health_policy(true,false);frame();frame();
        activate(damage_label);activate("Double");activate(healing_label);activate("Quarter");
        need(bw_settings_ui_test_saved().damage_rate_q8==512&&bw_settings_ui_test_saved().healing_rate_q8==64,
             "Personal health choices failed after leaving fixture room policy");
        activate("Controls",false);frame();
        const auto choice_label=[&](const char* normal,const char* compact) {try{find(normal);return normal;}catch(...){return compact;}};
        activate(choice_label("Keyboard Sprint mode","##sprint_keyboard_mode"));activate("Toggle");
        activate(choice_label("Controller Sprint mode","##sprint_controller_mode"),true);activate("Hold",true);
        need(bw_settings_ui_test_saved().sprint_keyboard_mode==BW_SPRINT_TOGGLE&&
             bw_settings_ui_test_saved().sprint_controller_mode==BW_SPRINT_HOLD,"Actual mode combo did not save selected choices");
        BwSprintMode key_mode,pad_mode;bluewake_sprint_modes(&key_mode,&pad_mode);
        need(key_mode==BW_SPRINT_TOGGLE&&pad_mode==BW_SPRINT_HOLD&&!bw_menu_size_test_restart_needed(),"Actual combo did not apply live atomically");
        bw_settings_ui_test_flush();
        need(read(dir/"settings.ini").find("sprint_keyboard_mode=toggle")!=std::string::npos&&
             read(dir/"settings.ini").find("sprint_controller_mode=hold")!=std::string::npos,"Settings file omitted modes");
        activate("Keyboard",false);frame();
        expose("Save bindings");
        need(find("Save bindings").box.GetWidth()>0,"Controls save became inaccessible");
        need(menu->InnerClipRect.Contains(find("Save bindings").box.GetCenter()),"Scrolled Controls save is still clipped");
        // Local key Enter invokes the real action and must keep our mappings.
        activate("Save bindings",false);
        need(read(dir/"controls.ini")==bindings_before,"Menu scaling altered bindings");
        need(bw_settings_ui_test_saved().audio_music==73&&bw_settings_ui_test_saved().audio_sfx==61&&bw_settings_ui_test_saved().render_scale==1,"Menu scaling altered unrelated settings");
        need(io.Fonts==default_atlas && bw_settings_ui_mock_effects().texture_uploads==uploads&&hash(bw_menu_size_mock_uploaded_pixels())==uploaded_hash,"Navigation/resize rebuilt atlas or changed copied upload");
        need(!bw_settings_ui_mock_effects().native_window_operations&&!bw_settings_ui_mock_effects().quit_requests,"Fixture performed native window/restart action");
        ImGui::SetScrollY(menu,0);frame();frame();activate("Display");
        activate("Presets");frame();
        expose("Preview changes");activate("Preview changes");
        need(bw_settings_ui_test_preset_open(),"Preset preview not reachable");
        auto* popup=ImGui::FindWindowByName("Apply preset");
        need(popup && popup->Size.x<=width && popup->Size.y<=height,"Preset preview exceeded viewport");
        expose("Cancel");activate("Cancel");
        need(!bw_settings_ui_test_preset_open()&&bw_settings_ui_test_session().menu_size==percent,"Discard changed menu size");
        const auto save_label=[&](){try {find("Save current settings");return "Save current settings";}catch(...){return "Save preset";}};
        expose(save_label());activate(save_label());
        bool captured=false;
        for(const auto& entry:std::filesystem::directory_iterator(dir/"presets")) {
            BwSettingsPreset actual;
            std::string error;
            need(bw_settings_preset_read(entry.path().string(),actual,&error),error);
            if(actual.values.count("menu_size")) {
                need(actual.values.at("sprint_keyboard_mode")=="toggle"&&actual.values.at("sprint_controller_mode")=="hold","Actual named preset omitted Sprint modes");
                need(actual.values.at("menu_size")==std::to_string(percent),"Actual Save preset handler captured wrong size");captured=true;
            }
        }
        need(captured,"Actual Save preset did not persist selected menu size");
        edit_size(percent==200?150:200);
        const auto preset_label=[&](){try {find("Preset");return "Preset";}catch(...){return "##preset-choice";}};
        activate(preset_label());activate("My settings");
        activate("Preview changes");
        need(bw_settings_ui_test_preset_open(),"Saved preset preview did not open");
        expose("Apply");activate("Apply",true);
        need(!bw_settings_ui_test_preset_open() && bw_settings_ui_test_session().menu_size==percent &&
             bw_settings_ui_test_saved().menu_size==percent,"Actual preset Apply did not restore selected size");
        need(!bw_menu_size_test_restart_needed(),"Size-only preset Apply unexpectedly queued restart");
        bw_settings_ui_test_flush();
        need(read(dir/"controls.ini")==bindings_before,"Preset Apply changed independent bindings");
        need(bw_settings_ui_mock_effects().texture_uploads==uploads,"Preset flow rebuilt font atlas");
        expose("Close   (F1 or Esc)");activate("Close   (F1 or Esc)",true);
        need(!bw_settings_ui_test_menu_open(),"Gamepad close button became inaccessible");
#endif
        bw_menu_size_test_release_font();ImGui::DestroyContext();
        std::puts("PASS actual offscreen menu size/font/atlas/navigation/persistence");return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
