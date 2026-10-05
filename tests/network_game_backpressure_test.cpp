// SPDX-License-Identifier: GPL-3.0-or-later
/* Transport admission is deliberately controlled; production bridge/accessors
 * are linked unchanged. This tests retries after external queue refusal. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "network_game.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
struct BwNetworkSession{BwNetworkSnapshot snapshot{};};
struct BwNetworkServer{};
static bool refuse=true;
static std::vector<BwProgressionDelta> submitted;
extern "C" BwNetworkSession* bw_network_create(){return new BwNetworkSession;}
extern "C" void bw_network_destroy(BwNetworkSession* s){delete s;}
extern "C" void bw_network_leave(BwNetworkSession* s){if(s)s->snapshot.status=BW_NETWORK_OFFLINE;}
extern "C" bool bw_network_join(BwNetworkSession* s,const BwNetworkConfig*){s->snapshot.status=BW_NETWORK_CONNECTED;s->snapshot.generation=1;return true;}
extern "C" bool bw_network_submit(BwNetworkSession*,BwProgressionDelta d){if(refuse)return false;submitted.push_back(d);return true;}
extern "C" bool bw_network_presence(BwNetworkSession*,const BwGameScene*){return true;}
extern "C" bool bw_network_poll(BwNetworkSession*,BwNetworkUpdate*){return false;}
extern "C" void bw_network_status(BwNetworkSession* s,BwNetworkSnapshot* out){*out=s?s->snapshot:BwNetworkSnapshot{};}
extern "C" BwNetworkServer* bw_network_server_start(const char*,uint16_t){return nullptr;}
extern "C" BwNetworkServer* bw_network_server_start_persistent(const char*,uint16_t,const char*,char*,unsigned){return nullptr;}
extern "C" void bw_network_server_stop(BwNetworkServer*){}
extern "C" uint16_t bw_network_server_port(BwNetworkServer*){return 0;}
static BwGameEventCallback callback;static void* callback_user;static BwGameScene scene;
extern "C" BwGameEventSubscription bluewake_game_events_subscribe(uint64_t,BwGameEventCallback cb,void* user){callback=cb;callback_user=user;return 1;}
extern "C" bool bluewake_game_events_unsubscribe(BwGameEventSubscription){callback=nullptr;return true;}
extern "C" bool bluewake_game_events_scene(BwGameScene* out,uint64_t*,uint64_t*){*out=scene;return scene.active;}
static void emit(BwGameEventKind kind,BwGameResetReason reason=BW_GAME_RESET_ATTACH){BwGameEvent e{};e.kind=kind;e.scene=scene;e.reset_reason=reason;callback(&e,callback_user);}
static void w32(std::vector<uint8_t>& ram,uint32_t address,uint32_t v){uint8_t* p=ram.data()+address-0x80000000;p[0]=uint8_t(v>>24);p[1]=uint8_t(v>>16);p[2]=uint8_t(v>>8);p[3]=uint8_t(v);}
static BwNetworkGameSnapshot status(){BwNetworkGameSnapshot s;bw_network_game_snapshot(&s);return s;}
int main(){
    BwNetworkPreferences p{};p.room_mode=true;auto& c=p.config;std::strcpy(c.server,"127.0.0.1");c.port=49383;std::strcpy(c.room,"retry");std::strcpy(c.player_name,"Link");
    std::memset(c.player_id,'a',32);std::strcpy(c.compatibility.game_id,"GZLE01");c.compatibility.progression_schema=BW_PROGRESSION_SCHEMA;
    for(char* f:{c.compatibility.build_id,c.compatibility.module_digest,c.compatibility.options_digest})std::memset(f,'1',64);
    CPUState cpu{};std::vector<uint8_t> ram(24*1024*1024);cpu.ram=ram.data();cpu.ram_size=uint32_t(ram.size());std::memset(ram.data()+0x3C4C44,0xFF,21);
    ram[0x3C4C46]=0x22;ram[0x3C4C47]=0x25;
    w32(ram,0x803CA74C,0x80010000);w32(ram,0x80010498,0x800101F8);std::memcpy(ram.data()+0x3C9D3C,"LinkUG",7);
    scene={};std::strcpy(scene.stage,"LinkUG");scene.player=0x80010000;scene.stay_room=11;scene.active=scene.player_valid=scene.controls_ready=true;
    assert(bw_network_game_prepare(&p));bw_network_game_attach(&cpu);emit(BW_GAME_EVENT_PLAYER_UPDATED);
    for(unsigned i=0;i<5;++i)bw_network_game_retrace(&cpu);assert(submitted.empty()&&status().captured==0&&status().queue_failures==5);
    refuse=false;bw_network_game_retrace(&cpu);assert(submitted.size()==2&&status().captured==2);
    assert(submitted[0].key==2&&submitted[0].value==0x22&&submitted[1].key==3&&submitted[1].value==0x25);
    for(unsigned i=0;i<120;++i)bw_network_game_retrace(&cpu);assert(submitted.size()==2); // known advanced only after successful admission
    emit(BW_GAME_EVENT_RESET,BW_GAME_RESET_STATE_LOAD);ram[0x3C4C48]=0x24;emit(BW_GAME_EVENT_PLAYER_UPDATED);
    for(unsigned i=0;i<60;++i)bw_network_game_retrace(&cpu);assert(submitted.size()==2);
    emit(BW_GAME_EVENT_RESET,BW_GAME_RESET_GAME_LOAD);emit(BW_GAME_EVENT_PLAYER_UPDATED);bw_network_game_retrace(&cpu);assert(submitted.size()==5&&submitted[4].key==4);
    bw_network_game_detach();std::puts("network bridge backpressure retry and reset cancellation passed");return 0;
}
