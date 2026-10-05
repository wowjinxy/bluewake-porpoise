// SPDX-License-Identifier: GPL-3.0-or-later
/* Synthetic game-thread command ownership plus actual localhost sockets/store.
 * No game, native CARD, device or guest progression is used. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "network_game.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif
namespace fs=std::filesystem;
static BwGameEventCallback callback;
extern "C" BwGameEventSubscription bluewake_game_events_subscribe(uint64_t,BwGameEventCallback cb,void*){callback=cb;return 1;}
extern "C" bool bluewake_game_events_unsubscribe(BwGameEventSubscription){callback=nullptr;return true;}
extern "C" bool bluewake_game_events_scene(BwGameScene* out,uint64_t*,uint64_t*){*out={};return false;}
static BwNetworkGameSnapshot snapshot(){BwNetworkGameSnapshot s{};bw_network_game_snapshot(&s);return s;}
static bool until(CPUState& cpu,const std::function<bool()>& fn){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    do{bw_network_game_retrace(&cpu);if(fn())return true;std::this_thread::sleep_for(std::chrono::milliseconds(5));}while(std::chrono::steady_clock::now()<end);return fn();}
int main(int argc,char** argv){
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_CALL_REPORTFAULT);_set_error_mode(_OUT_TO_STDERR);
#endif
    assert(argc==2);const char* bind=argv[1];assert(!std::strcmp(bind,"127.0.0.2")||!std::strcmp(bind,"0.0.0.0"));
    const auto temp=fs::absolute(fs::temp_directory_path()).lexically_normal();
    const auto root=temp/("BlueWake-host-store-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(fs::equivalent(root.parent_path(),temp)&&!fs::exists(root));
    auto probe=bw_network_server_start(bind,0);assert(probe);const auto port=bw_network_server_port(probe);bw_network_server_stop(probe);
    BwNetworkPreferences p{};p.room_mode=true;auto& c=p.config;std::strcpy(c.server,"127.0.0.1");c.port=port;
    std::strcpy(c.room,"host");std::strcpy(c.player_name,"Host");std::memset(c.player_id,'a',32);
    std::strcpy(c.compatibility.game_id,"GZLE01");c.compatibility.progression_schema=BW_PROGRESSION_SCHEMA;
    for(char* d:{c.compatibility.build_id,c.compatibility.module_digest,c.compatibility.options_digest})std::memset(d,'1',64);
    const auto unicode=root/fs::u8path(u8"data-\u2603");const auto path=unicode.u8string();
    assert(!bw_network_game_prepare_with_store(&p,nullptr));assert(bw_network_game_prepare_with_store(&p,path.c_str()));
    assert(!fs::exists(root));CPUState cpu{};bw_network_game_attach(&cpu);assert(!fs::exists(root)); // remote Join opens no store
    assert(bw_network_game_request_host(&c,bind));assert(until(cpu,[]{return snapshot().session.status==BW_NETWORK_CONNECTED;}));
    assert(snapshot().local_host&&snapshot().persistent_host&&snapshot().persistent_host_configured&&!snapshot().native_load_authorized);
    const auto identity=std::string(snapshot().session.room_identity);assert(identity.size()==32);
    auto remote=c;std::strcpy(remote.server,!std::strcmp(bind,"0.0.0.0")?"127.0.0.1":bind);std::memset(remote.player_id,'b',32);
    auto peer=bw_network_create();assert(peer&&bw_network_join(peer,&remote));
    assert(until(cpu,[&]{BwNetworkSnapshot s{};bw_network_status(peer,&s);return s.status==BW_NETWORK_CONNECTED;}));
    assert(bw_network_submit(peer,{2,0x22}));assert(until(cpu,[]{return snapshot().session.server_revision==1;}));
    assert(bw_network_game_request_join(&c));assert(until(cpu,[]{return snapshot().session.status==BW_NETWORK_CONNECTED;}));
    assert(snapshot().local_host&&snapshot().persistent_host&&snapshot().session.server_revision==1&&identity==snapshot().session.room_identity);
    bw_network_game_request_leave();bw_network_game_retrace(&cpu);assert(snapshot().local_host&&!snapshot().exporting);
    assert(bw_network_game_request_join(&c));assert(until(cpu,[]{return snapshot().session.status==BW_NETWORK_CONNECTED;}));
    assert(identity==snapshot().session.room_identity&&snapshot().session.server_revision==1);
    assert(bw_network_game_request_host(&c,bind));assert(until(cpu,[]{return snapshot().session.status==BW_NETWORK_CONNECTED;}));
    assert(identity==snapshot().session.room_identity&&snapshot().session.server_revision==1); // durable owned restart, no reseed
    bw_network_game_detach();bw_network_destroy(peer);assert(!callback&&!snapshot().local_host);
    char error[160]{};const auto store=(unicode/"Network"/"Server").u8string();
    auto reopened=bw_network_server_start_persistent(bind,port,store.c_str(),error,sizeof error);assert(reopened);bw_network_server_stop(reopened);
    assert(fs::equivalent(fs::canonical(root).parent_path(),temp));fs::remove_all(root);
    std::puts("Durable owned host endpoint/join/leave/rejoin/restart fixture passed; no guest RAM or native saves");return 0;
}
