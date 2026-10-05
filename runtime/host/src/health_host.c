// SPDX-License-Identifier: GPL-3.0-or-later
#include "health_host.h"
#include "health_module_contract.h"
#include "StaticRecompABI.h"
#include "game_events.h"
#include <stdatomic.h>
#include <string.h>
enum { RATE_MASK=8191u, ROOM_LOCK=0x80000000u, NATIVE_PAIR=256u|(256u<<13) };
static atomic_uint desired=NATIVE_PAIR;
static atomic_bool attached_flag=false,available=false;
typedef struct HostHealth {
    BwHealthRulesRuntime runtime;
    CPUState* cpu;uint8_t* ram;
    BwGameEventSubscription subscription;
    uint32_t applied,aliases;
    bool attached,suspended,module_ok,saving,bound;
} HostHealth;
static HostHealth host;
static BwHealthRulesConfig unpack(unsigned p){BwHealthRulesConfig c={(uint16_t)(p&RATE_MASK),(uint16_t)((p>>13)&RATE_MASK)};return c;}
bool bw_health_host_configure(unsigned damage,unsigned healing) {
    if(damage>BW_HEALTH_RULES_MAX_RATE||healing>BW_HEALTH_RULES_MAX_RATE)return false;
    const unsigned rates=damage|(healing<<13);
    unsigned old=atomic_load_explicit(&desired,memory_order_acquire);
    for(;;){if((old&ROOM_LOCK)&&rates!=NATIVE_PAIR)return false;
        const unsigned next=(old&ROOM_LOCK)|rates;
        if(atomic_compare_exchange_weak_explicit(&desired,&old,next,memory_order_acq_rel,memory_order_acquire))return true;}
}
BwHealthRulesConfig bw_health_host_configuration(void){return unpack(atomic_load_explicit(&desired,memory_order_acquire));}
bool bw_health_host_prepare_room(bool room) {
    if(atomic_load_explicit(&attached_flag,memory_order_acquire))return false;
    unsigned old=atomic_load_explicit(&desired,memory_order_acquire);
    for(;;){if(room&&(old&~ROOM_LOCK)!=NATIVE_PAIR)return false;
        const unsigned next=(old&~ROOM_LOCK)|(room?ROOM_LOCK:0u);
        if(atomic_compare_exchange_weak_explicit(&desired,&old,next,memory_order_acq_rel,memory_order_acquire))return true;}
}
bool bw_health_host_available(void){return atomic_load_explicit(&available,memory_order_acquire);}
bool bw_health_host_room_locked(void){return (atomic_load_explicit(&desired,memory_order_acquire)&ROOM_LOCK)!=0;}
static uint64_t hash_value(uint64_t h,uint64_t v,unsigned bytes){for(unsigned i=0;i<bytes;++i){h^=(v>>(8*i))&255u;h*=UINT64_C(1099511628211);}return h;}
static bool module_supported(const StaticRecompModuleDesc* m) {
    if(!m||(m->abi_version!=3&&m->abi_version!=4&&m->abi_version!=5)||
       m->cpu_abi_version!=GXRUNTIME_CPU_ABI_VERSION||m->cpu_state_size!=sizeof(CPUState)||
       memcmp(m->game_id,"GZLE01",sizeof("GZLE01"))||!m->dispatch||
       m->num_chunk_ranges!=BW_HEALTH_MODULE_CHUNKS||!m->chunk_ranges||!m->chunk_hashes)return false;
    uint64_t h=hash_value(UINT64_C(14695981039346656037),m->num_chunk_ranges,4);uint32_t previous=0;
    for(uint32_t i=0;i<m->num_chunk_ranges;++i){const StaticRecompRange* r=&m->chunk_ranges[i];
        if(r->start>=r->end||(r->start&3u)||(r->end&3u)||(i&&r->start<previous))return false;
        previous=r->end;h=hash_value(h,r->start,4);h=hash_value(h,r->end,4);h=hash_value(h,m->chunk_hashes[i],8);}
    return h==BW_HEALTH_MODULE_FINGERPRINT;
}
static bool requested(void){return (host.applied&~ROOM_LOCK)!=NATIVE_PAIR;}
static bool identity(const CPUState* cpu){return host.attached&&!host.suspended&&host.module_ok&&
    cpu&&cpu==host.cpu&&cpu->ram&&cpu->ram==host.ram&&cpu->ram_size==BW_HEALTH_RULES_HOST_RAM_SIZE;}
static void cancel(void){bw_health_rules_detach(&host.runtime);host.bound=false;}
static void lifecycle(const BwGameEvent* e,void* unused){(void)unused;if(e&&(e->kind==BW_GAME_EVENT_RESET||
    e->kind==BW_GAME_EVENT_SCENE_LEAVING||e->kind==BW_GAME_EVENT_TRANSITION_STARTED||e->kind==BW_GAME_EVENT_SCENE_ENTERED))cancel();}
static bool lifetime(CPUState* cpu,BwHealthRulesLifetime* out) {
    if(!identity(cpu)||host.saving)return false;
    BwGameScene scene;uint64_t epoch=0,generation=0;
    if(!bluewake_game_events_scene(&scene,&epoch,&generation)||!scene.active||scene.transitioning||!epoch||!generation||!identity(cpu))return false;
    BwGameEventStats stats;bluewake_game_events_stats(&stats);
    if(stats.epoch!=epoch||stats.scene_generation!=generation||!identity(cpu))return false;
    *out=(BwHealthRulesLifetime){epoch,generation,stats.ticks};return true;
}
void bw_health_host_detach(void) {
    if(host.subscription)bluewake_game_events_unsubscribe(host.subscription);
    cancel();memset(&host,0,sizeof host);
    atomic_store_explicit(&attached_flag,false,memory_order_release);atomic_store_explicit(&available,false,memory_order_release);
}
bool bw_health_host_attach(CPUState* cpu,const StaticRecompModuleDesc* m) {
    bw_health_host_detach();bw_health_rules_init(&host.runtime);
    host.cpu=cpu;host.ram=cpu?cpu->ram:NULL;host.module_ok=module_supported(m);
    host.attached=cpu&&cpu->ram&&cpu->ram_size==BW_HEALTH_RULES_HOST_RAM_SIZE;
    atomic_store_explicit(&attached_flag,host.attached,memory_order_release);
    atomic_store_explicit(&available,host.attached&&host.module_ok,memory_order_release);
    host.applied=NATIVE_PAIR;bw_health_host_retrace(cpu,false);return bw_health_host_available();
}
void bw_health_host_retrace(CPUState* cpu,bool saving) {
    const unsigned p=atomic_load_explicit(&desired,memory_order_acquire);
    if(p!=host.applied){const BwHealthRulesConfig c=unpack(p);bw_health_rules_configure(&host.runtime,&c);host.applied=p;cancel();}
    /* Native identity: no guest/event query, aliases, subscriber or watches. */
    if(!requested()){if(host.subscription){bluewake_game_events_unsubscribe(host.subscription);host.subscription=0;}if(host.bound)cancel();return;}
    host.saving=saving;
    if(!identity(cpu)||saving){cancel();return;}
    if(!host.subscription){const uint64_t mask=BW_GAME_EVENT_MASK(BW_GAME_EVENT_RESET)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_SCENE_LEAVING)|
        BW_GAME_EVENT_MASK(BW_GAME_EVENT_TRANSITION_STARTED)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_SCENE_ENTERED);
        host.subscription=bluewake_game_events_subscribe(mask,lifecycle,NULL);if(!host.subscription){cancel();return;}}
    BwHealthRulesLifetime l;if(!lifetime(cpu,&l)){cancel();return;}
    if(!host.bound){host.aliases=g_ppc_guest_alias_generation;host.bound=bw_health_rules_attach(&host.runtime,cpu,&l,BW_HEALTH_RULES_ABI_GZLE01);}
    if(host.bound){if(host.aliases!=g_ppc_guest_alias_generation){bw_health_rules_reset(&host.runtime,&l);host.aliases=g_ppc_guest_alias_generation;}
        bw_health_rules_retrace(&host.runtime,cpu,&l);}
}
bool bw_health_host_observes(const CPUState* cpu,uint32_t address) {
    if(!requested()||!host.bound||!identity(cpu)||host.saving)return false;
    return bw_health_rules_observes(&host.runtime,cpu,address);
}
void bw_health_host_dispatch(CPUState* cpu,uint32_t address,bool saving) {
    if(!requested())return;
    if(saving||!identity(cpu)){cancel();return;}
    if(!host.bound||!bw_health_rules_observes(&host.runtime,cpu,address))return;
    BwHealthRulesLifetime l;if(!lifetime(cpu,&l)){cancel();return;}
    bw_health_rules_dispatch(&host.runtime,cpu,address,&l);
}
void bw_health_host_reset(void){cancel();}
void bw_health_host_suspend(void){cancel();host.suspended=true;atomic_store_explicit(&available,false,memory_order_release);}
void bw_health_host_stats(BwHealthRulesStats* out){bw_health_rules_stats(&host.runtime,out);}
