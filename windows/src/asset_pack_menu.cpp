// SPDX-License-Identifier: GPL-3.0-or-later
#include "asset_pack_menu.h"
#include "asset_packs.h"
#if defined(BLUEWAKE_ASSET_PACK_MENU_TEST)
#include "asset_pack_menu_test_api.h"
#endif
#ifdef rename
#undef rename
#endif
#include <imgui.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#if !defined(BLUEWAKE_ASSET_PACK_MENU_TEST)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace {
namespace fs=std::filesystem;
struct Cache {std::vector<BluewakeAssetPackInfo> rows;std::vector<BluewakeAssetPackConflict> conflicts;};
struct Outcome {bool ok=false;std::string message;Cache cache;};
BluewakeAssetPacks* g_manager;
BluewakeAssetPackRuntime* g_runtime;
Cache g_cache;
std::future<Outcome> g_worker;
std::string g_data,g_status,g_error,g_initial_selection;
char g_preset_name[192]="My texture packs";
std::atomic<bool> g_started=false,g_restart=false;
bool g_hd_enabled;
unsigned g_cache_reads;
#if defined(BLUEWAKE_ASSET_PACK_MENU_TEST)
std::string g_test_import,g_test_export;
std::atomic<bool> g_test_paused=false;
#endif

Cache cache(BluewakeAssetPacks* manager) {
    Cache out;
    for(size_t i=0,n=bluewake_asset_packs_count(manager);i<n;++i){BluewakeAssetPackInfo row{};if(bluewake_asset_packs_get(manager,i,&row))out.rows.push_back(row);}
    for(size_t i=0,n=bluewake_asset_packs_conflicts(manager);i<n;++i){BluewakeAssetPackConflict row{};if(bluewake_asset_packs_conflict(manager,i,&row))out.conflicts.push_back(row);}
    return out;
}
std::string signature(const Cache& value) {
    std::string out;
    for(const auto& row:value.rows)if(row.enabled){out+=row.id;out+=':';out+=row.expected_hash;out+='\n';}
    return out;
}
void pending() {
    bool changed=signature(g_cache)!=g_initial_selection;
    for(const auto& row:g_cache.rows)if(row.loaded&&(!row.enabled||row.status!=BW_ASSET_PACK_READY||std::strcmp(row.hash,row.loaded_hash)!=0))changed=true;
    g_restart.store(changed,std::memory_order_release);
}
bool busy(){return g_worker.valid();}
void complete() {
    if(!g_worker.valid()||g_worker.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
    try {
        auto outcome=g_worker.get();g_cache=std::move(outcome.cache);++g_cache_reads;
        if(outcome.ok){g_status=outcome.message;g_error.clear();}else {g_error=outcome.message;g_status.clear();}
        pending();
    }catch(...){g_error="Pack filesystem task failed; saved selections and active registrations were kept";g_status.clear();}
}
using Action=std::function<bool(BluewakeAssetPacks*,BluewakeAssetPackResult*)>;
void work(Action action,bool save) {
    if(!g_manager||busy())return;
    auto* manager=g_manager;
    try {
        g_worker=std::async(std::launch::async,[manager,action=std::move(action),save]() {
#if defined(BLUEWAKE_ASSET_PACK_MENU_TEST)
            while(g_test_paused.load(std::memory_order_acquire))std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
            Outcome out;BluewakeAssetPackResult result{};
            std::unique_ptr<BluewakeAssetPackSnapshot,decltype(&bluewake_asset_packs_free_snapshot)> snapshot(
                save?bluewake_asset_packs_snapshot(manager):nullptr,bluewake_asset_packs_free_snapshot);
            if(save&&!snapshot){out.message="Pack change could not be staged safely; current selections were kept";out.cache=cache(manager);return out;}
            try {out.ok=action(manager,&result);out.message=result.message;}
            catch(...){out.ok=false;out.message="Pack filesystem operation failed";}
            if(out.ok&&save) {
                out.ok=bluewake_asset_packs_save(manager,&result);
                if(!out.ok) {
                    out.message=result.message;
                } else out.message+="; saved, applies after restart";
            }
            if(!out.ok&&save){BluewakeAssetPackResult rollback{};const bool restored=bluewake_asset_packs_restore(manager,snapshot.get(),&rollback);out.message+=restored?"; unsaved change rolled back":"; rollback failed, saved file remains unchanged";}
            out.cache=cache(manager);return out;
        });
    }catch(...){g_error="Pack filesystem task could not be started; current selections were kept";}
}
const char* status_name(BluewakeAssetPackStatus status) {
    switch(status){case BW_ASSET_PACK_READY:return "Ready";case BW_ASSET_PACK_MISSING:return "Missing";
    case BW_ASSET_PACK_BROKEN:return "Invalid manifest/root";case BW_ASSET_PACK_HASH_CHANGED:return "Revision changed";
    case BW_ASSET_PACK_UNSUPPORTED:return "Compiled gameplay pack";case BW_ASSET_PACK_DUPLICATE:return "Duplicate ID";
    case BW_ASSET_PACK_RUNTIME_BROKEN:return "Texture decode failed";}return "Unavailable";
}
bool picker(bool exporting,std::string& selected) {
#if defined(BLUEWAKE_ASSET_PACK_MENU_TEST)
    selected=exporting?g_test_export:g_test_import;return !selected.empty();
#else
    std::vector<wchar_t> name(32768,0);
    if(exporting)std::wcsncpy(name.data(),L"packs.bwpackpreset",name.size()-1);
    const auto folder=fs::path(reinterpret_cast<const char8_t*>(g_data.c_str())).wstring();
    OPENFILENAMEW request{};request.lStructSize=sizeof request;request.hwndOwner=GetActiveWindow();
    request.lpstrFilter=L"BlueWake asset pack preset (*.bwpackpreset)\0*.bwpackpreset\0\0";
    request.lpstrFile=name.data();request.nMaxFile=static_cast<DWORD>(name.size());request.lpstrInitialDir=folder.c_str();
    request.lpstrDefExt=L"bwpackpreset";request.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_EXPLORER|
        (exporting?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    if(!(exporting?GetSaveFileNameW(&request):GetOpenFileNameW(&request)))return false;
    const auto value=fs::path(name.data()).u8string();selected.assign(reinterpret_cast<const char*>(value.data()),value.size());return true;
#endif
}
}
extern "C" {
bool bw_asset_pack_menu_start(const char* data_dir,bool enabled) {
    bw_asset_pack_menu_shutdown();g_hd_enabled=enabled;g_data=data_dir?data_dir:"";g_cache_reads=0;
    BluewakeAssetPackResult result{};g_manager=bluewake_asset_packs_create(data_dir,&result);
    if(!g_manager){g_error=result.message;g_started.store(true,std::memory_order_release);return false;}
#if !defined(BLUEWAKE_ASSET_PACK_MENU_TEST)
    g_runtime=bluewake_asset_packs_load_runtime(g_manager,enabled,&result);
    if(!g_runtime)g_error=result.message;else g_status=result.message;
#else
    // Fixture uses actual catalog/persistence handlers. Actual Aurora decode
    // and registry ownership are covered by asset_packs_aurora_test instead.
    g_status="Fixture catalog ready; no renderer/window initialized";
#endif
    g_cache=cache(g_manager);++g_cache_reads;g_initial_selection=signature(g_cache);pending();
    g_started.store(true,std::memory_order_release);
#if defined(BLUEWAKE_ASSET_PACK_MENU_TEST)
    return true;
#else
    return g_runtime!=nullptr;
#endif
}
const char* bw_asset_pack_menu_error(void){return g_error.c_str();}
bool bw_asset_pack_menu_restart_needed(void){return g_restart.load(std::memory_order_acquire);}
void bw_asset_pack_menu_draw(void) {
    if(!g_started.load(std::memory_order_acquire))return;
    complete();ImGui::SeparatorText("Named texture packs");
    ImGui::TextWrapped("Texture assets load after restart. Gameplay mods such as Better Wind Waker are compiled options and use their existing controls.");
    if(!g_error.empty())ImGui::TextWrapped("Pack error: %s",g_error.c_str());
    if(!g_status.empty())ImGui::TextWrapped("%s",g_status.c_str());
    if(!g_manager){ImGui::TextWrapped("Managed catalog unavailable; legacy and native textures remain available.");return;}
    const bool scanning=busy();
    if(scanning)ImGui::TextUnformatted("Working on pack files; showing the cached catalog.");
    if(!g_hd_enabled)ImGui::TextWrapped("HD textures were disabled for this launch. Enable HD textures and restart to load selected packs.");
    if(bw_asset_pack_menu_restart_needed())ImGui::TextUnformatted("Pack selection or revision changed: restart pending.");
    ImGui::BeginDisabled(scanning);
    if(ImGui::Button("Refresh pack catalog"))work([](auto* manager,auto* result){return bluewake_asset_packs_refresh(manager,result);},false);
    ImGui::SameLine();ImGui::TextDisabled("Later enabled packs have higher priority.");
    ImGui::InputText("Pack preset name",g_preset_name,sizeof g_preset_name);
    if(ImGui::Button("Import pack preset")){std::string file;if(picker(false,file))work([file](auto* manager,auto* result){return bluewake_asset_packs_import(manager,file.c_str(),result);},true);}
    ImGui::SameLine();
    if(ImGui::Button("Export pack preset")){std::string file;if(picker(true,file)){const std::string name=g_preset_name;work([file,name](auto* manager,auto* result){return bluewake_asset_packs_export(manager,file.c_str(),name.c_str(),result);},false);}}
    ImGui::TextWrapped("Pack presets share IDs, order and content fingerprints only. Install matching assets separately; settings and control bindings stay separate.");
    for(size_t i=0;i<g_cache.rows.size();++i) {
        const auto& row=g_cache.rows[i];ImGui::PushID(static_cast<int>(i));ImGui::Separator();
        bool enabled=row.enabled;const bool usable=row.status==BW_ASSET_PACK_READY||row.status==BW_ASSET_PACK_HASH_CHANGED;
        ImGui::BeginDisabled(!usable&&!enabled);
        const auto label=std::string("Enable ")+row.name;
        if(ImGui::Checkbox(label.c_str(),&enabled)){const std::string id=row.id;work([id,enabled](auto* manager,auto* result){return bluewake_asset_packs_enable(manager,id.c_str(),enabled,result);},true);}
        ImGui::EndDisabled();
        ImGui::Text("ID: %s | %s | priority %u",row.id,status_name(row.status),row.priority);
        ImGui::Text("Requested: %s | Loaded this launch: %s (%llu registrations)",row.enabled?"enabled":"disabled",row.loaded?"yes":"no",static_cast<unsigned long long>(row.registrations));
        if(row.hash[0])ImGui::TextWrapped("Current content: %s",row.hash);
        if(row.expected_hash[0])ImGui::TextWrapped("Requested content: %s",row.expected_hash);
        if(row.loaded_hash[0])ImGui::TextWrapped("Loaded registration revision: %s",row.loaded_hash);
        ImGui::TextWrapped("%s",row.message);
        ImGui::BeginDisabled(i==0);const auto lower=std::string("Lower priority##")+row.id;
        if(ImGui::Button(lower.c_str())){const std::string id=row.id;work([id,i](auto* manager,auto* result){return bluewake_asset_packs_move(manager,id.c_str(),i-1,result);},true);}ImGui::EndDisabled();
        ImGui::SameLine();ImGui::BeginDisabled(i+1==g_cache.rows.size());const auto higher=std::string("Higher priority##")+row.id;
        if(ImGui::Button(higher.c_str())){const std::string id=row.id;work([id,i](auto* manager,auto* result){return bluewake_asset_packs_move(manager,id.c_str(),i+1,result);},true);}ImGui::EndDisabled();
        ImGui::PopID();
    }
    if(g_cache.rows.empty())ImGui::TextWrapped("No named packs installed. Add AssetPacks/<folder>/pack.ini and its textures, then Refresh.");
    ImGui::EndDisabled();
    if(!g_cache.conflicts.empty()) {
        ImGui::SeparatorText("Loaded managed-pack conflicts");
        for(const auto& conflict:g_cache.conflicts)ImGui::TextWrapped("%s overrides %s: %s",conflict.winner,conflict.loser,conflict.key);
    }
    ImGui::TextWrapped("Conflict rows describe this launch's actual exact-key registrations. Wildcard specificity and legacy priority zero keep their native loader behavior.");
}
void bw_asset_pack_menu_shutdown(void) {
    g_started.store(false,std::memory_order_release);
#if defined(BLUEWAKE_ASSET_PACK_MENU_TEST)
    g_test_paused.store(false,std::memory_order_release);
#endif
    if(g_worker.valid()){try{g_worker.get();}catch(...){}}
#if !defined(BLUEWAKE_ASSET_PACK_MENU_TEST)
    bluewake_asset_packs_unload_runtime(g_runtime);
#endif
    g_runtime=nullptr;bluewake_asset_packs_destroy(g_manager);g_manager=nullptr;g_cache={};g_initial_selection.clear();
    g_status.clear();g_error.clear();g_restart.store(false,std::memory_order_release);
}
#if defined(BLUEWAKE_ASSET_PACK_MENU_TEST)
void bw_asset_pack_menu_test_paths(const char* imported,const char* exported){g_test_import=imported?imported:"";g_test_export=exported?exported:"";}
bool bw_asset_pack_menu_test_busy(void){return busy();}
size_t bw_asset_pack_menu_test_rows(void){return g_cache.rows.size();}
bool bw_asset_pack_menu_test_row(size_t index,BluewakeAssetPackInfo* out){if(!out||index>=g_cache.rows.size())return false;*out=g_cache.rows[index];return true;}
unsigned bw_asset_pack_menu_test_cache_reads(void){return g_cache_reads;}
void bw_asset_pack_menu_test_pause_work(bool paused){g_test_paused.store(paused,std::memory_order_release);}
#endif
}
