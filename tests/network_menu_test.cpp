// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "network_menu.h"
#include "network_game.h"
#include "health_host.h"
#include "network_digest.h"
#include <windows.h>
#include <imgui.h>
#include <imgui_internal.h>
#ifdef rename
#undef rename
#endif
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
namespace {
struct Item { ImRect bounds; std::string label; ImGuiItemFlags flags = 0; ImGuiItemStatusFlags status = 0; };
std::map<ImGuiID, Item> items;
std::string rendered, bind_address;
BwNetworkPreferences mounted{};
BwNetworkGameSnapshot snapshot{};
BwNetworkConfig joined{}, hosted{};
unsigned prepares, joins, hosts, leaves, detaches;

void check(bool yes, const std::string& message) { if (!yes) throw std::runtime_error(message.empty()?"Fixture check failed without a native error message":message); }
std::string utf8(const fs::path& path) {
    const auto value = path.u8string(); return {reinterpret_cast<const char*>(value.data()), value.size()};
}
std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary); check(input.good(), "Cannot read fixture: " + utf8(path));
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void write(const fs::path& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size())); check(output.good(), "Cannot write fixture");
}
BwNetworkPreferences load(const fs::path& directory) {
    BwNetworkPreferences value{}; char error[256]{};
    check(bw_network_preferences_load(utf8(directory).c_str(), &value, error, sizeof error), error); return value;
}
void frame() {
    items.clear(); ImGui::NewFrame(); ImGui::LogToBuffer(0);
    ImGui::SetNextWindowPos(ImVec2(5,5), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1400,1250), ImGuiCond_Always);
    ImGui::Begin("Network menu fixture", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    bw_network_menu_draw(); ImGui::End();
    rendered.assign(GImGui->LogBuffer.begin(), GImGui->LogBuffer.end()); ImGui::LogFinish(); ImGui::Render();
    check(ImGui::GetDrawData() && ImGui::GetDrawData()->TotalVtxCount > 0, "Actual network menu emitted no geometry");
}
const Item& item(const std::string& label) {
    for (const auto& [id, value] : items) if (value.label == label) return value;
    throw std::runtime_error("Actual network widget was not exposed: " + label);
}
bool disabled(const std::string& label) { return (item(label).flags & ImGuiItemFlags_Disabled) != 0; }
void click(const std::string& label) {
    const auto center = item(label).bounds.GetCenter(); auto& io = ImGui::GetIO();
    // All events belong solely to this in-memory ImGui context. No backend,
    // native window, SDL initialization, OS cursor or desktop input exists.
    io.AddMousePosEvent(center.x, center.y); frame();
    io.AddMouseButtonEvent(0,true); frame(); io.AddMouseButtonEvent(0,false); frame(); frame();
}
void text(const std::string& label, const char* value) {
    click(label); auto& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl,true); frame(); io.AddKeyEvent(ImGuiKey_A,true); frame();
    io.AddKeyEvent(ImGuiKey_A,false); frame(); io.AddKeyEvent(ImGuiMod_Ctrl,false); frame();
    io.AddInputCharactersUTF8(value); frame(); frame();
    io.AddKeyEvent(ImGuiKey_Enter,true); frame(); io.AddKeyEvent(ImGuiKey_Enter,false); frame();
}
void connection_buttons(bool join, bool leave) {
    check(disabled("Join / rejoin room") != join && disabled("Host room") != join && disabled("Leave room") != leave,
        "Network command eligibility disagreed with mounted route or pending command");
}
void preserve(const fs::path& directory, const std::string& controls, const std::string& personal) {
    check(read(directory / "controls.ini") == controls, "Network action changed unrelated controls profile");
    check(read(directory / "GZLE01.card") == personal, "Network action changed the personal CARD");
}
struct HoldPreferences {
    HANDLE handle;
    explicit HoldPreferences(const fs::path& path) : handle(CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {
        check(handle != INVALID_HANDLE_VALUE, "Cannot hold real preferences file against atomic replacement");
    }
    ~HoldPreferences() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
void save_failure(const fs::path& directory, bool room_mode) {
    const std::string before = read(directory / "network.ini");
    text("Player name", "UnsavedName");
    const auto joins_before = joins;
    {
        HoldPreferences hold(directory / "network.ini");
        click(room_mode ? "Join / rejoin room" : "Save network settings");
    }
    check(*bw_network_menu_error() && read(directory / "network.ini") == before && joins == joins_before,
        "Failed real atomic Save replaced saved preferences or queued a Join");
    check(!bw_network_menu_restart_needed(), "Failed preference replacement changed saved route");
    click("Discard network edits");
    check(!*bw_network_menu_error() && rendered.find("Network edits are not saved.") == std::string::npos,
        "Discard did not return to the last successfully saved preferences");
}
void personal(const fs::path& directory) {
    check(!mounted.room_mode && !bw_network_menu_restart_needed(), "Personal startup requested room save or restart");
    frame(); frame(); connection_buttons(false,false);
    const auto calls = joins + hosts + leaves;
    click("Join / rejoin room"); click("Host room"); click("Leave room");
    check(joins + hosts + leaves == calls, "Disabled personal-mode command reached the game bridge");
    click("Use room saves after restart");
    check(!bw_network_menu_restart_needed(), "Unsaved room checkbox changed the mounted route");
    connection_buttons(false,false);
    click("Save network settings");
    check(load(directory).room_mode && bw_network_menu_restart_needed(), "Saving room mode did not require restart");
    check(rendered.find("Restart required") != std::string::npos && rendered.find("personal card") != std::string::npos,
        "Pending room-mode restart or mounted personal route was not displayed");
    click("Use room saves after restart"); click("Discard network edits");
    check((item("Use room saves after restart").status & ImGuiItemStatusFlags_Checked) != 0 && bw_network_menu_restart_needed(),
        "Discard lost saved pending room choice");
    click("Use room saves after restart"); click("Save network settings");
    check(!load(directory).room_mode && !bw_network_menu_restart_needed(), "Restoring personal preference did not cancel restart");
    save_failure(directory,false);
}
void room(const fs::path& directory, const std::string& route) {
    check(mounted.room_mode && !bw_network_menu_restart_needed(), "Mounted room initially required restart");
    snapshot.session.status = BW_NETWORK_CONNECTED; snapshot.session.server_revision = 17;
    snapshot.applied = 9; snapshot.deferred_updates = 2; snapshot.session.peer_count = 2;
    std::strcpy(snapshot.session.message, "Room connection accepted"); std::strcpy(snapshot.message, "Native room save authorized");
    std::strcpy(snapshot.session.peers[0].player_name,"Alpha"); std::strcpy(snapshot.session.peers[0].stage,"sea");
    snapshot.session.peers[0].online=true; snapshot.session.peers[0].room=44;
    std::strcpy(snapshot.session.peers[1].player_name,"Beta"); std::strcpy(snapshot.session.peers[1].stage,"LinkRM");
    snapshot.session.peers[1].online=false; snapshot.session.peers[1].room=0;
    frame(); frame(); connection_buttons(true,true);
    for (const char* label : {"Connected","Alpha","Beta","Online","Offline","sea","LinkRM","Room revision 17","waiting 2"})
        check(rendered.find(label) != std::string::npos, std::string("Missing actual network status/roster: ")+label);
    check(rendered.find(route) != std::string::npos, "Mounted isolated room CARD was not displayed");
    text("Player name", "RoomLink"); click("Join / rejoin room");
    check(joins==1 && std::strcmp(joined.player_name,"RoomLink")==0 &&
        bw_network_same_room(&joined,&mounted.config) && std::strcmp(load(directory).config.player_name,"RoomLink")==0,
        "Same-room Join did not persist and copy edited display name");
    snapshot.pending_command=true; frame(); connection_buttons(false,false);
    click("Join / rejoin room"); click("Host room"); click("Leave room");
    check(joins==1 && hosts==0 && leaves==0, "A second command escaped the pending-command gate");
    snapshot.pending_command=false; frame();
    text("Host bind address","192.168.50.2"); click("Host room");
    check(hosts==1 && hosted.create_room && bind_address=="192.168.50.2" &&
        bw_network_same_room(&hosted,&mounted.config) && load(directory).config.create_room,
        "Host did not copy mounted-room config and explicit numeric bind address");
    check(!bw_network_menu_restart_needed(), "Host startup preference changed isolated CARD identity");
    click("Leave room"); check(leaves==1, "Leave request did not reach bridge");
    text("Room","other_room"); connection_buttons(false,true);
    check(!bw_network_menu_restart_needed(), "Unsaved room edit changed saved restart state");
    click("Join / rejoin room"); click("Host room"); check(joins==1 && hosts==1,"Unsaved room change allowed a connection");
    click("Discard network edits"); connection_buttons(true,true);
    text("Room","other_room"); click("Save network settings");
    check(bw_network_menu_restart_needed() && std::strcmp(load(directory).config.room,"other_room")==0,
        "Saved different room failed to require restart");
    connection_buttons(false,true); click("Join / rejoin room"); check(joins==1,"Pending room restart allowed Join");
    text("Room",mounted.config.room); click("Save network settings");
    check(!bw_network_menu_restart_needed(),"Restoring mounted room did not cancel restart"); connection_buttons(true,true);
    save_failure(directory,true);
    const auto before = read(directory / "network.ini");
    text("Server address","not-a-numeric-server"); click("Save network settings");
    check(*bw_network_menu_error() && read(directory / "network.ini")==before && !bw_network_menu_restart_needed(),
        "Invalid endpoint changed saved room route"); click("Discard network edits"); connection_buttons(true,true);
}
}

// Mock only the UI/game command bridge. Preferences, path isolation, hashing
// and every ImGui widget/action are their real production implementation.
extern "C" bool bw_network_game_prepare(const BwNetworkPreferences* value) {
    check(value && bw_network_config_valid(&value->config,nullptr,0),"UI gave bridge an invalid startup config");
    mounted=*value; snapshot.mounted=value->config; snapshot.mounted_room_mode=value->room_mode; ++prepares; return true;
}
extern "C" void bw_network_game_snapshot(BwNetworkGameSnapshot* value) { if(value)*value=snapshot; }
extern "C" bool bw_network_game_request_join(const BwNetworkConfig* value) {
    check(mounted.room_mode && value && bw_network_same_room(value,&mounted.config),"Join escaped mounted room identity");
    joined=*value; ++joins; return true;
}
extern "C" bool bw_network_game_request_host(const BwNetworkConfig* value,const char* bind) {
    check(mounted.room_mode && value && bw_network_same_room(value,&mounted.config) && bind,"Host escaped mounted room identity");
    hosted=*value; bind_address=bind; ++hosts; return true;
}
extern "C" void bw_network_game_request_leave(void) { check(mounted.room_mode,"Personal route reached Leave"); ++leaves; }
extern "C" void bw_network_game_detach(void) { ++detaches; }

void ImGuiTestEngineHook_ItemAdd(ImGuiContext*,ImGuiID id,const ImRect& bounds,const ImGuiLastItemData* value) {
    if(id){items[id].bounds=bounds;if(value)items[id].flags=value->ItemFlags;}
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*,ImGuiID id,const char* label,ImGuiItemStatusFlags flags) {
    items[id].label=label?label:""; items[id].status=flags;
}
void ImGuiTestEngineHook_Log(ImGuiContext*,const char*,...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*,ImGuiID id) {
    const auto at=items.find(id);return at==items.end()?nullptr:at->second.label.c_str();
}
ImFileHandle ImFileOpen(const char* path,const char* mode) { return reinterpret_cast<ImFileHandle>(std::fopen(path,mode)); }
bool ImFileClose(ImFileHandle file) { return !file || std::fclose(reinterpret_cast<FILE*>(file))==0; }
ImU64 ImFileGetSize(ImFileHandle file) {
    auto* stream=reinterpret_cast<FILE*>(file); if(!stream)return static_cast<ImU64>(-1);
    const auto at=std::ftell(stream); std::fseek(stream,0,SEEK_END);const auto size=std::ftell(stream);std::fseek(stream,at,SEEK_SET);
    return size<0?static_cast<ImU64>(-1):static_cast<ImU64>(size);
}
ImU64 ImFileRead(void* bytes,ImU64 size,ImU64 count,ImFileHandle file) { return std::fread(bytes,size,count,reinterpret_cast<FILE*>(file)); }
ImU64 ImFileWrite(const void* bytes,ImU64 size,ImU64 count,ImFileHandle file) { return std::fwrite(bytes,size,count,reinterpret_cast<FILE*>(file)); }

int main(int argc,char** argv) {
    fs::path folder;
    try {
        check(argc==3 && (std::strcmp(argv[1],"personal")==0 || std::strcmp(argv[1],"room")==0 ||
            std::strcmp(argv[1],"mods-on")==0 || std::strcmp(argv[1],"mods-off")==0),"Expected personal/room/MODS mode and isolated fixture parent");
        const bool room_mode=std::strcmp(argv[1],"room")==0;
        const bool mods_on=std::strcmp(argv[1],"mods-on")==0;
        const bool mods_off=std::strcmp(argv[1],"mods-off")==0;
        const auto parent=fs::absolute(fs::u8path(argv[2])).lexically_normal();
        folder=parent/(std::string("network-menu-")+argv[1]+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directories(folder),"Cannot create fresh network fixture");
        const auto directory=folder/fs::u8path("data-\xE2\x98\x83"); check(fs::create_directory(directory),"Cannot create Unicode preferences folder");
        const auto module=folder/fs::u8path("translation-\xE2\x98\x83.dll"); write(module,"abc");
        const std::string controls="version=3\ncustom-controls-sentinel=preserve\n",personal_card="isolated-personal-card-sentinel";
        write(directory/"controls.ini",controls); write(directory/"GZLE01.card",personal_card);
        // Native MODS uses any-positive-presence, while native OPTIONS is
        // last-wins. Separate processes prove these two MODS lists cannot be
        // fingerprinted as identical despite their shared final negative token.
        _putenv_s("BLUEWAKE_MODS",mods_on?"betterww,-betterww":mods_off?"-betterww":"none");
        _putenv_s("BLUEWAKE_OPTIONS","none,swift_sail,-instant_text");
        std::string seed_id, old_route;
        if(room_mode){
            auto seed=load(directory); seed.room_mode=true; seed_id=seed.config.player_id;char error[256]{},route[4096]{};
            const bool old_route_ok=bw_network_room_card_path(utf8(directory).c_str(),&seed.config,route,sizeof route,error,sizeof error);
            check(old_route_ok,std::string("Seed room path: ")+error);
            old_route=route;write(fs::u8path(old_route),"previous-compatibility-room-card");
            const bool saved_ok=bw_network_preferences_save(utf8(directory).c_str(),&seed,error,sizeof error);
            check(saved_ok,std::string("Seed preferences: ")+error);
        }
        char route[4096]{};
        check(bw_health_host_configure(128,512),"Actual startup health mailbox rejected bounded personal rates");
        if(room_mode) {
            const auto prefs_before=read(directory/"network.ini");
            check(!bw_network_menu_prepare(utf8(directory).c_str(),utf8(module).c_str(),route,sizeof route),
                  "Nonnative health rates escaped room startup policy");
            check(prepares==0 && !*route && read(directory/"network.ini")==prefs_before &&
                  read(fs::u8path(old_route))=="previous-compatibility-room-card",
                  "Rejected health room changed mounted bridge/preferences/old CARD");
            preserve(directory,controls,personal_card);
            check(bw_health_host_configure(256,256),"Failed room attempt incorrectly locked mailbox");
        }

        const bool prepared_ok=bw_network_menu_prepare(utf8(directory).c_str(),utf8(module).c_str(),route,sizeof route);
        check(prepared_ok,std::string("Actual menu prepare: ")+bw_network_menu_error());
        check(prepares==1,"Startup game bridge was not prepared exactly once");
        check(bw_health_host_room_locked()==room_mode,"Actual health room lock disagreed with mounted route");
        if(room_mode) check(!bw_health_host_configure(128,256),"Mounted room accepted live nonnative health rates");
        else check(bw_health_host_configuration().damage_q8==128 && bw_health_host_configuration().healing_q8==512,
                   "Personal routing reset selected health preference");
        const auto loaded=load(directory); const std::string player_id=loaded.config.player_id;
        check(player_id.size()==32 && player_id==mounted.config.player_id && (!room_mode || player_id==seed_id),"Stable generated/loaded player ID was not persisted");
        check(std::strcmp(loaded.config.compatibility.module_digest,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")==0,
            "Actual BCrypt module fingerprint was not standard SHA256(abc)");
        check(bw_net::digest("abc")==loaded.config.compatibility.module_digest,"Real compatibility digest disagreed with known SHA256 vector");
        check(bw_net::digest("BlueWake/GZLE01/Progression1/NativeColdBoot2/TCP1")==loaded.config.compatibility.build_id,
            "UI compatibility did not use the qualified native cold-boot semantics revision");
        const auto expected_options=bw_net::digest(std::string("defaults=off;mods=")+(mods_on?"8:betterww:1;":"")+
            ";options=12:instant_text:0;10:swift_sail:1;");
        check(std::strcmp(loaded.config.compatibility.options_digest,
            expected_options.c_str())==0,
            "Gameplay option manifest was not deterministic normalized native configuration");
        if(room_mode){
            char expected[4096]{},error[256]{};
            const bool expected_ok=bw_network_room_card_path(utf8(directory).c_str(),&loaded.config,expected,sizeof expected,error,sizeof error);
            check(expected_ok,std::string("Expected current room path: ")+error);
            check(std::strcmp(route,expected)==0 && route!=old_route && read(fs::u8path(old_route))=="previous-compatibility-room-card",
                "Compatibility change reused/overwrote previous room route");
        }else check(!*route,"Personal startup selected a room card");
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
        io.DisplaySize=ImVec2(1450,1300);io.DeltaTime=1.0f/60.0f;GImGui->TestEngineHookItems=true;
        io.Fonts->AddFontDefault();unsigned char* pixels=nullptr;int width=0,height=0;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);check(pixels && width>0 && height>0,"Offscreen font atlas failed");
        if(room_mode)room(directory,route);else personal(directory);
        preserve(directory,controls,personal_card);check(load(directory).config.player_id==player_id,"UI actions replaced persisted player identity");
        check(read(module)=="abc","UI altered fingerprint source");
        bw_network_menu_shutdown();check(detaches==1,"Menu shutdown did not detach bridge");ImGui::DestroyContext();
        check(folder.parent_path()==parent && utf8(folder.filename()).rfind("network-menu-",0)==0,"Fixture cleanup escaped its fresh parent");
        fs::remove_all(folder);
        std::cout<<"Actual offscreen network menu "<<argv[1]<<" route/preferences/hash/command/save-failure regression passed\n";return 0;
    }catch(const std::exception& error){
        if(ImGui::GetCurrentContext())ImGui::DestroyContext();
        std::cerr<<error.what()<<"\nRetained isolated fixture: "<<utf8(folder)<<'\n';return 1;
    }
}
