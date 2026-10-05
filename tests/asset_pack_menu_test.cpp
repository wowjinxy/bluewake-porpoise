// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "asset_pack_menu.h"
#include "asset_pack_menu_test_api.h"
#include "asset_packs_test_support.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>

using namespace asset_fixture;
namespace {
struct Item {ImRect bounds;std::string label;};
std::map<ImGuiID,Item> items;
std::string rendered;
void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
void frame() {
    items.clear();ImGui::NewFrame();ImGui::LogToBuffer(0);
    ImGui::SetNextWindowPos(ImVec2(0,0),ImGuiCond_Always);ImGui::SetNextWindowSize(ImVec2(1500,2200),ImGuiCond_Always);
    ImGui::Begin("Asset panel fixture",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
    bw_asset_pack_menu_draw();ImGui::End();rendered.assign(GImGui->LogBuffer.begin(),GImGui->LogBuffer.end());ImGui::LogFinish();ImGui::Render();
    check(ImGui::GetDrawData()->TotalVtxCount>0,"Actual asset panel emitted no draw geometry");
}
void click(const std::string& label) {
    bool found=false;ImVec2 center;
    for(const auto& [id,item]:items)if(item.label==label){check(!found,"Ambiguous UI widget: "+label);found=true;center=item.bounds.GetCenter();}
    check(found,"Missing actual UI widget: "+label);auto& io=ImGui::GetIO();
    // ImGui-local events only: no backend/window/cursor/native message/input.
    io.AddMousePosEvent(center.x,center.y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();frame();
}
void idle() {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(bw_asset_pack_menu_test_busy()){frame();check(std::chrono::steady_clock::now()<deadline,"Filesystem worker did not complete");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    frame();
}
BluewakeAssetPackInfo row(const char* id) {
    for(size_t i=0;i<bw_asset_pack_menu_test_rows();++i){BluewakeAssetPackInfo value{};check(bw_asset_pack_menu_test_row(i,&value),"Cache row missing");if(std::string(value.id)==id)return value;}
    throw std::runtime_error("Cached pack missing");
}
}
void ImGuiTestEngineHook_ItemAdd(ImGuiContext*,ImGuiID id,const ImRect& bounds,const ImGuiLastItemData*){if(id)items[id].bounds=bounds;}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*,ImGuiID id,const char* label,ImGuiItemStatusFlags){items[id].label=label?label:"";}
void ImGuiTestEngineHook_Log(ImGuiContext*,const char*,...){}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*,ImGuiID id){const auto found=items.find(id);return found==items.end()?nullptr:found->second.label.c_str();}
ImFileHandle ImFileOpen(const char* file,const char* mode){return reinterpret_cast<ImFileHandle>(std::fopen(file,mode));}
bool ImFileClose(ImFileHandle file){return !file||std::fclose(reinterpret_cast<FILE*>(file))==0;}
ImU64 ImFileGetSize(ImFileHandle file){auto* stream=reinterpret_cast<FILE*>(file);if(!stream)return static_cast<ImU64>(-1);const auto pos=std::ftell(stream);std::fseek(stream,0,SEEK_END);const auto size=std::ftell(stream);std::fseek(stream,pos,SEEK_SET);return size<0?static_cast<ImU64>(-1):static_cast<ImU64>(size);}
ImU64 ImFileRead(void* data,ImU64 size,ImU64 count,ImFileHandle file){return std::fread(data,size,count,reinterpret_cast<FILE*>(file));}
ImU64 ImFileWrite(const void* data,ImU64 size,ImU64 count,ImFileHandle file){return std::fwrite(data,size,count,reinterpret_cast<FILE*>(file));}

int main(int argc,char** argv) {
    try {
        check(argc==2,"Fixture output parent required");const auto root=directory(argv[1]),data=root/path("data-\xE2\x98\x83");
        pack(data,"alpha","alpha","Alpha");pack(data,"beta","beta","Beta");
        write(data/"controls.ini","profile sentinel");write(data/"settings.ini","settings sentinel");
        check(bw_asset_pack_menu_start(utf8(data).c_str(),true),bw_asset_pack_menu_error());
        IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
        io.DisplaySize=ImVec2(1600,2300);io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
        io.Fonts->AddFontDefault();unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        check(pixels&&width>0&&height>0,"Offscreen font build failed");GImGui->TestEngineHookItems=true;frame();frame();
        const auto reads=bw_asset_pack_menu_test_cache_reads();for(int i=0;i<15;++i)frame();
        check(bw_asset_pack_menu_test_cache_reads()==reads,"Idle draw accessed/scanned the catalog");
        check(rendered.find("Gameplay mods")!=std::string::npos&&rendered.find("Loaded this launch: no")!=std::string::npos,"Asset/code distinction or requested/loaded state missing");
        // A real atomic replacement failure rolls back the in-memory request,
        // including the first change when no previous selection file exists.
        check(fs::create_directory(data/"asset_packs.ini"),"Persistence blocker could not be created");
        click("Enable Alpha");idle();
        check(!row("alpha").enabled&&!bw_asset_pack_menu_restart_needed(),"Failed first save left an enabled/pending selection");
        check(std::string(bw_asset_pack_menu_error()).find("rolled back")!=std::string::npos&&fs::is_directory(data/"asset_packs.ini"),"Save failure was not reported or previous target was replaced");
        fs::rename(data/"asset_packs.ini",root/"retained-state-blocker");
        click("Enable Alpha");idle();check(row("alpha").enabled&&bw_asset_pack_menu_restart_needed(),"Actual enable checkbox did not persist/publish restart pending");
        click("Enable Beta");idle();click("Lower priority##beta");idle();
        check(row("beta").order==0&&row("alpha").priority>row("beta").priority,"Actual priority control did not reorder packs");
        const auto exported=root/"shared.bwpackpreset";bw_asset_pack_menu_test_paths(nullptr,utf8(exported).c_str());
        click("Export pack preset");idle();const auto preset=read(exported);
        check(preset.find("[asset-pack-preset]")!=std::string::npos&&preset.find(utf8(data))==std::string::npos,"UI export leaked local paths or wrong preset format");
        click("Enable Alpha");idle();check(!row("alpha").enabled,"Disable did not persist");
        bw_asset_pack_menu_test_paths(utf8(exported).c_str(),nullptr);click("Import pack preset");idle();
        check(row("alpha").enabled&&row("beta").order==0,"Actual import handler did not restore portable selection/order");
        write(root/"invalid.bwpackpreset",preset+"version=1\n");bw_asset_pack_menu_test_paths(utf8(root/"invalid.bwpackpreset").c_str(),nullptr);
        const auto before=read(data/"asset_packs.ini");click("Import pack preset");idle();
        check(row("alpha").enabled&&read(data/"asset_packs.ini")==before&&std::string(bw_asset_pack_menu_error()).find("Invalid")!=std::string::npos,"Malformed import changed saved/UI selection");
        // Hold only the fixture worker scheduler to prove busy draws use the
        // cache while the real filesystem Refresh is queued, then let it run.
        pack(data,"gamma","gamma","Gamma");bw_asset_pack_menu_test_pause_work(true);
        const auto cached=bw_asset_pack_menu_test_cache_reads();click("Refresh pack catalog");
        check(bw_asset_pack_menu_test_busy(),"Refresh did not run asynchronously");
        for(int i=0;i<12;++i)frame();
        check(bw_asset_pack_menu_test_rows()==2&&bw_asset_pack_menu_test_cache_reads()==cached&&rendered.find("cached catalog")!=std::string::npos,"Busy draws accessed live manager or hid cached rows");
        bw_asset_pack_menu_test_pause_work(false);idle();check(bw_asset_pack_menu_test_rows()==3&&!row("gamma").enabled,"Explicit refresh did not publish new disabled pack");
        bw_asset_pack_menu_test_pause_work(true);click("Refresh pack catalog");check(bw_asset_pack_menu_test_busy(),"Shutdown fixture worker missing");
        bw_asset_pack_menu_shutdown();check(!bw_asset_pack_menu_test_busy(),"Shutdown failed to join filesystem worker");
        check(bw_asset_pack_menu_start(utf8(data).c_str(),true),bw_asset_pack_menu_error());frame();frame();
        check(row("alpha").enabled&&row("beta").order==0&&!bw_asset_pack_menu_restart_needed(),"Relaunch did not restore saved pack selection/order");
        check(read(data/"controls.ini")=="profile sentinel"&&read(data/"settings.ini")=="settings sentinel","Pack panel changed control/scalar preferences");
        bw_asset_pack_menu_shutdown();ImGui::DestroyContext();
        std::cout<<"Actual offscreen pack panel cache, async refresh, persistence rollback, order, preset import/export and shutdown passed; no native input/window/renderer: "<<utf8(root)<<'\n';return 0;
    }catch(const std::exception& error){bw_asset_pack_menu_shutdown();if(ImGui::GetCurrentContext())ImGui::DestroyContext();std::cerr<<"FAIL asset pack menu: "<<error.what()<<'\n';return 1;}
}
