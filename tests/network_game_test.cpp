// SPDX-License-Identifier: GPL-3.0-or-later
/* Lifecycle/accessor fixtures, not a native-game acceptance claim. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "network_game.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <thread>
#include <vector>
static BwGameEventCallback callback;static void* callback_user;static BwGameScene scene;
extern "C" BwGameEventSubscription bluewake_game_events_subscribe(uint64_t mask,BwGameEventCallback cb,void* user){assert(mask&&cb&&!callback);callback=cb;callback_user=user;return 1;}
extern "C" bool bluewake_game_events_unsubscribe(BwGameEventSubscription token){assert(token==1);callback=nullptr;return true;}
extern "C" bool bluewake_game_events_scene(BwGameScene* out,uint64_t* epoch,uint64_t* generation){if(out)*out=scene;if(epoch)*epoch=1;if(generation)*generation=1;return scene.active;}
static void emit(BwGameEventKind kind,BwGameResetReason reason=BW_GAME_RESET_ATTACH){assert(callback);BwGameEvent e{};e.kind=kind;e.reset_reason=reason;e.scene=scene;callback(&e,callback_user);}
static BwNetworkConfig config(unsigned port,char player,bool host){BwNetworkConfig c{};std::strcpy(c.server,"127.0.0.1");c.port=uint16_t(port);std::strcpy(c.room,"bridge");
    std::memset(c.player_id,player,32);std::strcpy(c.player_name,"Link");c.create_room=host;std::strcpy(c.compatibility.game_id,"GZLE01");c.compatibility.progression_schema=BW_PROGRESSION_SCHEMA;
    for(char* field:{c.compatibility.build_id,c.compatibility.module_digest,c.compatibility.options_digest})std::memset(field,'1',64);return c;}
static void w32(CPUState& c,uint32_t addr,uint32_t v){uint8_t* p=c.ram+addr-0x80000000u;p[0]=uint8_t(v>>24);p[1]=uint8_t(v>>16);p[2]=uint8_t(v>>8);p[3]=uint8_t(v);}
static BwNetworkGameSnapshot snapshot(){BwNetworkGameSnapshot s{};bw_network_game_snapshot(&s);return s;}
static bool until(CPUState& cpu,const std::function<bool()>& check){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    do{bw_network_game_retrace(&cpu);if(check())return true;std::this_thread::sleep_for(std::chrono::milliseconds(5));}while(std::chrono::steady_clock::now()<end);return check();}
int main(int argc,char** argv){
    const bool personal=argc==2&&!std::strcmp(argv[1],"--personal");assert(argc==1||personal);
    auto server=bw_network_server_start("127.0.0.1",0);assert(server);auto host=config(bw_network_server_port(server),'a',true);
    auto peer=bw_network_create();assert(peer&&bw_network_join(peer,&host));
    CPUState cpu{};std::vector<uint8_t> ram(24*1024*1024);cpu.ram=ram.data();cpu.ram_size=uint32_t(ram.size());
    std::memset(ram.data()+0x3C4C08+0x3C,0xFF,21);w32(cpu,0x803CA74C,0x80010000);w32(cpu,0x80010498,0x800101F8);w32(cpu,0x803C9DA0,0x80018000);
    ram[0x18009]=22;std::memcpy(ram.data()+0x3C9D3C,"LinkUG",7);
    scene={};std::strcpy(scene.stage,"LinkUG");scene.stay_room=11;scene.player=0x80010000;scene.active=scene.player_valid=scene.controls_ready=true;
    BwNetworkPreferences p{};p.room_mode=!personal;p.config=config(bw_network_server_port(server),'b',false);
    assert(bw_network_game_prepare(&p));assert(!bw_network_game_prepare(&p));const auto before=ram;
    bw_network_game_attach(&cpu);
    if(personal){assert(!callback&&!bw_network_game_request_join(&p.config));bw_network_game_retrace(&cpu);assert(ram==before&&snapshot().session.status==BW_NETWORK_OFFLINE);
        bw_network_game_detach();bw_network_destroy(peer);bw_network_server_stop(server);std::puts("personal-card networking isolation fixture passed");return 0;}
    assert(callback);assert(until(cpu,[]{return snapshot().session.status==BW_NETWORK_CONNECTED;}));
    auto mismatch=p.config;std::strcpy(mismatch.room,"different");assert(!bw_network_game_request_join(&mismatch));
    assert(bw_network_submit(peer,{2,0x22}));std::this_thread::sleep_for(std::chrono::milliseconds(50));bw_network_game_retrace(&cpu);
    assert(ram==before&&!snapshot().clean_boot_authorized&&!snapshot().native_load_authorized); // even mocked active scene alone cannot authorize cold export
    emit(BW_GAME_EVENT_PLAYER_UPDATED);assert(until(cpu,[&]{return ram[0x3C4C46]==0x22;}));assert(snapshot().clean_boot_authorized&&snapshot().applied==1);
    /* UI commands copy fields and never touch CPU. Leave keeps mounted route. */
    const auto unchanged=ram;bw_network_game_request_leave();assert(ram==unchanged);bw_network_game_retrace(&cpu);assert(!snapshot().exporting&&snapshot().mounted_room_mode);
    emit(BW_GAME_EVENT_RESET,BW_GAME_RESET_STATE_LOAD);assert(!snapshot().native_load_authorized);ram[0x3C4C47]=0x25; // synthetic saved-state fact cannot export
    assert(bw_network_game_request_join(&p.config));assert(until(cpu,[]{return snapshot().session.status==BW_NETWORK_CONNECTED;}));emit(BW_GAME_EVENT_PLAYER_UPDATED);
    for(unsigned i=0;i<15;++i)bw_network_game_retrace(&cpu);assert(!snapshot().native_load_authorized&&!snapshot().clean_boot_authorized);
    emit(BW_GAME_EVENT_RESET,BW_GAME_RESET_GAME_LOAD);emit(BW_GAME_EVENT_PLAYER_UPDATED);assert(until(cpu,[]{return snapshot().native_load_authorized&&snapshot().captured>0;}));
    assert(until(cpu,[&]{BwNetworkUpdate u;while(bw_network_poll(peer,&u))if(u.delta.key==3&&u.delta.value==0x25)return true;return false;}));
    scene.paused=true;const auto paused=ram;assert(bw_network_submit(peer,{73,1}));std::this_thread::sleep_for(std::chrono::milliseconds(50));bw_network_game_retrace(&cpu);assert(ram==paused);
    scene.paused=false;assert(until(cpu,[&]{return ram[0x3C4CC5]==1;})); // songs B4+9
    /* Canonical permanent progress survives later native lower-tier writes. */
    assert(bw_network_submit(peer,{12,0x36})&&bw_network_submit(peer,{34,99})&&bw_network_submit(peer,{35,99})&&
           bw_network_submit(peer,{32,2})&&bw_network_submit(peer,{33,32})&&bw_network_submit(peer,{8,0x26}));
    assert(until(cpu,[&]{return ram[0x3C4C50]==0x36&&ram[0x3C4C77]==99&&ram[0x3C4C78]==99&&ram[0x3C4C1A]==2&&ram[0x3C4C1B]==32&&ram[0x3C4C4C]==0x26;}));
    /* Match item_func_bow/wallet/fairy/camera assignments; repairs preserve
     * native current arrows/bombs and equipment selector bytes. */
    ram[0x3C4C50]=0x27;ram[0x3C4C77]=30;ram[0x3C4C78]=60;ram[0x3C4C1A]=1;ram[0x3C4C1B]=16;ram[0x3C4C4C]=0x23;
    ram[0x3C4C6F]=30;ram[0x3C4C70]=60;ram[0x3C4C11]=12;
    emit(BW_GAME_EVENT_INVENTORY_CHANGED);bw_network_game_retrace(&cpu);
    assert(ram[0x3C4C50]==0x36&&ram[0x3C4C77]==99&&ram[0x3C4C78]==99&&ram[0x3C4C1A]==2&&ram[0x3C4C1B]==32&&ram[0x3C4C4C]==0x26);
    assert(ram[0x3C4C6F]==30&&ram[0x3C4C70]==60&&ram[0x3C4C11]==12);
    ram[0x3CA8C8]=1;ram[0x3C4C50]=0x27;ram[0x3C4C77]=30;emit(BW_GAME_EVENT_INVENTORY_CHANGED);
    const auto temporary=ram;const auto before_recollection=snapshot();bw_network_game_retrace(&cpu);
    assert(ram==temporary&&snapshot().captured==before_recollection.captured&&snapshot().applied==before_recollection.applied);
    ram[0x3CA8C8]=0;emit(BW_GAME_EVENT_INVENTORY_CHANGED);bw_network_game_retrace(&cpu);assert(ram[0x3C4C50]==0x36&&ram[0x3C4C77]==99);
    emit(BW_GAME_EVENT_RESET,BW_GAME_RESET_STATE_LOAD);ram[0x3C4C50]=0x27;ram[0x3C4C77]=30;emit(BW_GAME_EVENT_PLAYER_UPDATED);
    const auto untrusted=ram;const auto old=snapshot();for(unsigned i=0;i<65;++i)bw_network_game_retrace(&cpu);
    assert(ram==untrusted&&snapshot().captured==old.captured&&snapshot().applied==old.applied&&!snapshot().native_load_authorized);
    emit(BW_GAME_EVENT_RESET,BW_GAME_RESET_GAME_LOAD);emit(BW_GAME_EVENT_PLAYER_UPDATED);bw_network_game_retrace(&cpu);
    assert(ram[0x3C4C50]==0x36&&ram[0x3C4C77]==99);
    bw_network_game_detach();assert(!callback&&!snapshot().native_load_authorized&&!snapshot().clean_boot_authorized);
    bw_network_game_attach(&cpu);emit(BW_GAME_EVENT_PLAYER_UPDATED);bw_network_game_retrace(&cpu);assert(!snapshot().clean_boot_authorized&&!snapshot().native_load_authorized);
    bw_network_game_detach();bw_network_destroy(peer);bw_network_server_stop(server);std::puts("network bridge cold/native-load/reset/UI/isolation fixtures passed");return 0;
}
