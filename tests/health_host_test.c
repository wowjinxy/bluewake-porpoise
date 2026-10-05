// SPDX-License-Identifier: GPL-3.0-or-later
/* Public behavioral fixtures, artificial native ownership and callee outcomes.
 * No ROM bytes, captured guest state, game execution or native death proof. */
#define main bw_core_fixture_main
#include "health_rules_test.c"
#undef main
#include "health_host.h"
#include "StaticRecompABI.h"
#include "game_events.h"
#ifdef BW_HEALTH_PRIVATE_DESCRIPTOR
#include "module_fixture.inc"
#else
#include "synthetic_module_fixture.inc"
#endif
static BwGameEventCallback notify;
static void* notify_user;
static unsigned scene_queries,stats_queries,subscriptions,unsubscribes;
static bool scene_active=true,scene_transition=false,subscriber_full=false;
static StaticRecompModuleDesc module;
static BwHealingReturnCanContinueFn registered_healing;
static void* registered_user;
static unsigned setter_calls;
static bool setter_fail;
static unsigned fixture_setter(u32 abi,u32 size,BwHealingReturnCanContinueFn callback,void* user){
    ++setter_calls;registered_healing=NULL;registered_user=NULL;
    if(setter_fail||abi!=GXRUNTIME_CPU_ABI_VERSION||size!=sizeof(CPUState))return 0;
    registered_healing=callback;registered_user=callback?user:NULL;return BW_HEALING_RETURN_OBSERVATION_V1;
}
static bool fixture_return(void* user,const CPUState* context,u32 address){
    CHECK(user==&module);return !bw_health_host_observes(context,address);
}
static int module_dispatch(CPUState* c,u32 a){(void)c;(void)a;return 0;}
BwGameEventSubscription bluewake_game_events_subscribe(uint64_t mask,BwGameEventCallback f,void* user) {
    CHECK(mask==(BW_GAME_EVENT_MASK(BW_GAME_EVENT_RESET)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_SCENE_LEAVING)|
        BW_GAME_EVENT_MASK(BW_GAME_EVENT_TRANSITION_STARTED)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_SCENE_ENTERED)));
    if(subscriber_full)return 0;CHECK(!notify);notify=f;notify_user=user;++subscriptions;return subscriptions;
}
bool bluewake_game_events_unsubscribe(BwGameEventSubscription id){CHECK(id&&notify);notify=NULL;notify_user=NULL;++unsubscribes;return true;}
bool bluewake_game_events_scene(BwGameScene* s,uint64_t* e,uint64_t* g){++scene_queries;memset(s,0,sizeof *s);s->active=scene_active;s->transitioning=scene_transition;*e=life.epoch;*g=life.scene_generation;return scene_active;}
void bluewake_game_events_stats(BwGameEventStats* s){++stats_queries;memset(s,0,sizeof *s);s->epoch=life.epoch;s->scene_generation=life.scene_generation;s->ticks=life.tick;}
static BwHealthRulesStats host_stats(void){BwHealthRulesStats s={0};bw_health_host_stats(&s);return s;}
static void host_setup(void) {
    bw_health_host_detach();CHECK(bw_health_host_prepare_room(false));CHECK(bw_health_host_configure(256,256));
    setup(512,512);scene_active=true;scene_transition=false;subscriber_full=false;
    module=(StaticRecompModuleDesc){0};module.abi_version=3;module.cpu_abi_version=GXRUNTIME_CPU_ABI_VERSION;
    module.cpu_state_size=sizeof cpu;memcpy(module.game_id,"GZLE01",7);module.dispatch=module_dispatch;
    module.num_chunk_ranges=sizeof fixture_ranges/sizeof fixture_ranges[0];module.chunk_ranges=fixture_ranges;module.chunk_hashes=fixture_hashes;
    scene_queries=stats_queries=subscriptions=unsubscribes=0;CHECK(bw_health_host_attach(&cpu,&module));
    setter_fail=false;setter_calls=0;
    bw_health_host_bind_healing_return(fixture_setter,fixture_return,&module);
    bw_health_host_retrace(&cpu,false);
}
#ifdef BW_HEALTH_TEST_HOST
static void prepare_host_optimized(void){
    const BwHealthRulesConfig c=runtime.config;bw_health_host_detach();CHECK(bw_health_host_prepare_room(false));
    CHECK(bw_health_host_configure(c.damage_q8,c.healing_q8));
    scene_active=true;scene_transition=false;subscriber_full=false;
    module=(StaticRecompModuleDesc){0};module.abi_version=3;module.cpu_abi_version=GXRUNTIME_CPU_ABI_VERSION;
    module.cpu_state_size=sizeof cpu;memcpy(module.game_id,"GZLE01",7);module.dispatch=module_dispatch;
    module.num_chunk_ranges=sizeof fixture_ranges/sizeof fixture_ranges[0];module.chunk_ranges=fixture_ranges;module.chunk_hashes=fixture_hashes;
    CHECK(bw_health_host_attach(&cpu,&module));
    setter_fail=false;bw_health_host_bind_healing_return(fixture_setter,fixture_return,&module);
    bw_health_host_retrace(&cpu,false);
}
#endif
static void host_entry(void){cpu.gpr[3]=PLAYER;cpu.gpr[1]=STACK;cpu.lr=0x801165F4u;cpu.pc=BW_HEALTH_RULES_DAMAGE;cpu.fpr[1]=-1;bw_health_host_dispatch(&cpu,cpu.pc,false);}
static void host_return(void){native_damage(&cpu,-1);const CPUState before=cpu;bw_health_host_dispatch(&cpu,cpu.pc,false);CHECK(!memcmp(&before,&cpu,sizeof cpu));}
static void host_default(void){
    host_setup();cpu.lr=0x801165F4u;cpu.pc=BW_HEALTH_RULES_DAMAGE;
    for(unsigned i=0;i<10000;++i){CHECK(!bw_health_host_observes(&cpu,cpu.pc));bw_health_host_dispatch(&cpu,cpu.pc,false);bw_health_host_retrace(&cpu,false);}
    CHECK(scene_queries==0&&stats_queries==0&&subscriptions==0&&host_stats().adjusted==0);
    CHECK(!bw_health_host_configure(4097,0));CHECK(bw_health_host_configuration().damage_q8==256);
    CHECK(!bw_health_host_prepare_room(true)); /* Startup-only after detach. */
}
static void host_calls_and_lifetime(void){
    host_setup();CHECK(bw_health_host_configure(512,128));bw_health_host_retrace(&cpu,false);
    CHECK(subscriptions==1);host_entry();CHECK(bw_health_host_observes(&cpu,cpu.lr));
    const CPUState before=cpu;++life.tick;bw_health_host_retrace(&cpu,false);CHECK(!memcmp(&before,&cpu,sizeof cpu));
    host_return();CHECK(rf(COUNT)==-2&&host_stats().adjusted==1&&host_stats().completed==1);
    bw_health_host_dispatch(&cpu,cpu.pc,false);CHECK(host_stats().adjusted==1);
    host_entry();bw_health_host_dispatch(&cpu,cpu.pc,true);host_return();CHECK(rf(COUNT)==-3&&host_stats().adjusted==1);
    bw_health_host_retrace(&cpu,false);host_entry();CHECK(bw_health_host_configure(256,256));
    bw_health_host_retrace(&cpu,false);host_return();CHECK(rf(COUNT)==-4&&host_stats().adjusted==1&&!notify);
    for(unsigned kind=0;kind<4;++kind){
        CHECK(bw_health_host_configure(512,128));bw_health_host_retrace(&cpu,false);host_entry();
        BwGameEvent e={0};e.kind=(BwGameEventKind[]){BW_GAME_EVENT_RESET,BW_GAME_EVENT_SCENE_LEAVING,BW_GAME_EVENT_TRANSITION_STARTED,BW_GAME_EVENT_SCENE_ENTERED}[kind];
        ++life.epoch;++life.scene_generation;CHECK(notify);notify(&e,notify_user);host_return();
        CHECK(host_stats().adjusted==1);bw_health_host_retrace(&cpu,false);
    }
    host_entry();uint8_t alias=0;CHECK(ppc_guest_alias_add_shared(0x81601000u,1,&alias));
    host_return();CHECK(host_stats().adjusted==1);ppc_guest_alias_clear();
    bw_health_host_retrace(&cpu,false);host_entry();bw_health_host_suspend();CHECK(!bw_health_host_available());
    host_return();CHECK(host_stats().adjusted==1&&!bw_health_host_observes(&cpu,cpu.pc));
    CHECK(bw_health_host_attach(&cpu,&module));host_entry();host_return();CHECK(host_stats().adjusted==1);
    CPUState foreign=cpu;foreign.lr=0x801165F4u;CHECK(!bw_health_host_observes(&foreign,BW_HEALTH_RULES_DAMAGE));
    host_entry();bw_health_host_dispatch(&foreign,foreign.lr,false);host_return();CHECK(host_stats().adjusted==1);
    bw_health_host_retrace(&cpu,false);host_entry();cpu.ram_size=0x1800000;
    bw_health_host_dispatch(&cpu,cpu.lr,false);cpu.ram_size=BW_HEALTH_RULES_HOST_RAM_SIZE;host_return();CHECK(host_stats().adjusted==1);
}
static void host_module_room_and_capacity(void){
    host_setup();module.game_id[0]='X';CHECK(!bw_health_host_attach(&cpu,&module));CHECK(!bw_health_host_available());
    module.game_id[0]='G';module.cpu_state_size--;CHECK(!bw_health_host_attach(&cpu,&module));module.cpu_state_size++;
    module.num_chunk_ranges--;CHECK(!bw_health_host_attach(&cpu,&module));module.num_chunk_ranges++;
    uint64_t* hashes=malloc(sizeof fixture_hashes);CHECK(hashes);memcpy(hashes,fixture_hashes,sizeof fixture_hashes);
    module.chunk_hashes=hashes;hashes[0]^=1;CHECK(!bw_health_host_attach(&cpu,&module));free(hashes);module.chunk_hashes=fixture_hashes;
    CHECK(bw_health_host_attach(&cpu,&module));CHECK(bw_health_host_configure(512,128));subscriber_full=true;
    bw_health_host_retrace(&cpu,false);host_entry();host_return();CHECK(host_stats().adjusted==0);
    subscriber_full=false;scene_active=false;bw_health_host_retrace(&cpu,false);host_entry();host_return();CHECK(host_stats().adjusted==0);
    bw_health_host_detach();CHECK(!bw_health_host_prepare_room(true));CHECK(bw_health_host_configure(256,256));CHECK(bw_health_host_prepare_room(true));
    CHECK(bw_health_host_room_locked());CHECK(!bw_health_host_configure(512,256));CHECK(!bw_health_host_configure(256,0));
    CHECK(bw_health_host_configure(256,256));CHECK(bw_health_host_prepare_room(false));CHECK(!bw_health_host_room_locked());
}
static void host_healing_capability(void){
    host_setup();CHECK(bw_health_host_available()&&bw_health_host_healing_available());
    CHECK(!registered_healing);const unsigned native_calls=setter_calls;
    for(unsigned i=0;i<10;++i)bw_health_host_retrace(&cpu,false);
    CHECK(setter_calls==native_calls&&!registered_healing);
    CHECK(bw_health_host_configure(512,128));bw_health_host_retrace(&cpu,false);
    CHECK(registered_healing&&registered_user==&module);
    cpu.pc=BW_HEALTH_RULES_HEART;cpu.lr=BW_HEALTH_RULES_ITEM_RETURN;cpu.ctr=cpu.gpr[12]=cpu.pc;
    bw_health_host_dispatch(&cpu,cpu.pc,false);cpu.pc=cpu.lr;
    CHECK(!registered_healing(registered_user,&cpu,cpu.pc));
    const CPUState before=cpu;CHECK(!registered_healing(registered_user,&cpu,cpu.pc));CHECK(!memcmp(&before,&cpu,sizeof cpu));
    bw_health_host_retrace(&cpu,true);CHECK(!registered_healing);
    bw_health_host_retrace(&cpu,false);CHECK(registered_healing);
    bw_health_host_reset();CHECK(!registered_healing);
    bw_health_host_retrace(&cpu,false);CHECK(registered_healing);
    bw_health_host_suspend();CHECK(!registered_healing&&!bw_health_host_healing_available());
    CHECK(bw_health_host_attach(&cpu,&module));CHECK(registered_healing&&bw_health_host_healing_available());
    bw_health_host_bind_healing_return(NULL,NULL,NULL);bw_health_host_retrace(&cpu,false);
    CHECK(bw_health_host_available()&&!bw_health_host_healing_available()&&!registered_healing);
    CHECK(bw_health_host_configuration().healing_q8==128);
    cpu.pc=BW_HEALTH_RULES_HEART;cpu.lr=BW_HEALTH_RULES_ITEM_RETURN;
    CHECK(!bw_health_host_observes(&cpu,cpu.pc));host_entry();host_return();CHECK(host_stats().adjusted==1);
    /* Valid probe followed by registration refusal: no stale pending healing
     * or false UI capability, while separately qualified damage still works. */
    CHECK(bw_health_host_configure(256,128));setter_fail=false;
    bw_health_host_bind_healing_return(fixture_setter,fixture_return,&module);setter_fail=true;
    bw_health_host_retrace(&cpu,false);
    CHECK(!registered_healing&&!bw_health_host_healing_available()&&bw_health_host_available());
    CHECK(!bw_health_host_observes(&cpu,BW_HEALTH_RULES_HEART));
    bw_health_host_detach();CHECK(!registered_healing&&!bw_health_host_healing_available());setter_fail=false;
}
int main(void){
    cpu.ram=calloc(1,BW_HEALTH_RULES_HOST_RAM_SIZE);before_ram=malloc(BW_HEALTH_RULES_HOST_RAM_SIZE);CHECK(cpu.ram&&before_ram);
    host_default();host_calls_and_lifetime();host_module_room_and_capacity();host_healing_capability();
#ifdef BW_HEALTH_TEST_HOST
    optimized_callers();
#endif
    bw_health_host_detach();ppc_guest_alias_clear();free(before_ram);free(cpu.ram);
    printf("Health host synthetic/default/ownership/lifecycle/room-native-only fixtures: %u checks PASS\n",checks);return 0;
}
