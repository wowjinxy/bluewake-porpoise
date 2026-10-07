/* SPDX-License-Identifier: GPL-3.0-or-later
 * Authored state/PC oracle; actual six production observers are linked.
 * No native actor/CARD/renderer/input execution or authority is implied. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "core/cpu.h"
#include "../../runtime/host/src/finite_observer_filter.h"
#include "../../runtime/host/src/hud_host.h"
#include "../../runtime/host/src/health_host.h"
#include "../../runtime/host/src/quick_items.h"
#include "../../runtime/host/src/dialogue_speed.h"
#include "../../runtime/host/src/enhancement_hooks.h"
#include "../../runtime/host/src/autosave.h"
#include "../../runtime/host/src/game_events.h"
#include "../../runtime/host/src/fpu_context.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decls.h"
static uint32_t random_state=0x38FBD219u;
static uint64_t checks,misses,executed_finite,positive[6];
static unsigned trace[16],trace_size,external;
static bool g_deadline_census_enabled,g_delivery_safety_census_enabled,g_guest_state_trace_enabled;
static bool bluewake_jump_button_armed,g_actor_search_native;
static void* g_reward_host;
/* This authored independent gate PC is outside the six-family union. */
#define BW_SEARCH_JUDGE_FILTER UINT32_C(0x80001000)
#define BW_NATIVE_REWARD_SESSION 1
#define BLUEWAKE_ENABLE_DEVELOPER_TRACING 0
#define BLUEWAKE_EDGE_CENSUS 0
static uint32_t random_word(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static void record(unsigned id){assert(trace_size<16);trace[trace_size++]=id;}
static bool call_hud(const CPUState* c,uint32_t a){record(1);++executed_finite;return bw_hud_host_observes(c,a);}
static bool call_health(const CPUState* c,uint32_t a){record(2);++executed_finite;return bw_health_host_observes(c,a);}
static bool call_quick(uint32_t a){record(3);++executed_finite;return bluewake_quick_items_observes(a);}
static bool call_dialogue(uint32_t a){record(4);++executed_finite;return bluewake_dialogue_speed_observes(a);}
static bool call_enhancement(const CPUState* c,uint32_t a){record(5);++executed_finite;return bluewake_enhancement_hooks_observes_context(c,a);}
static bool call_autosave(uint32_t a){record(6);++executed_finite;return bluewake_autosave_observes(a);}
static bool feature(uint32_t a){(void)a;record(10);return (external&1)!=0;}
static bool events(uint32_t a){(void)a;record(11);return (external&2)!=0;}
static bool reward(void* h,const CPUState* c,uint32_t a){(void)h;(void)c;(void)a;record(12);return (external&4)!=0;}
static bool chassis(const CPUState* c,uint32_t a){(void)c;(void)a;record(13);return (external&8)!=0;}
#define bluewake_feature_observes feature
#define bluewake_game_events_observes events
#define bw_randomizer_reward_host_observes reward
#define host_chassis_requires_full chassis
#define bw_hud_host_observes call_hud
#define bw_health_host_observes call_health
#define bluewake_quick_items_observes call_quick
#define bluewake_dialogue_speed_observes call_dialogue
#define bluewake_enhancement_hooks_observes_context call_enhancement
#define bluewake_autosave_observes call_autosave
#include "finite_observer_chains.inc"
#undef bw_hud_host_observes
#undef bw_health_host_observes
#undef bluewake_quick_items_observes
#undef bluewake_dialogue_speed_observes
#undef bluewake_enhancement_hooks_observes_context
#undef bluewake_autosave_observes
/* Exported inventories use production symbols, not a frozen numeric union.
 * In particular, changed non-DOL returns are exercised after recompilation. */
static uint32_t pcs[512], health_lrs[64], enhancement_lrs[16];
static size_t pc_count,health_lr_count,enhancement_lr_count;
static void gather_pcs(void) {
    const BwTestPcInventory families[]={test_hud_pcs,test_health_pcs,test_quick_pcs,
        test_dialogue_pcs,test_enhancement_pcs,test_autosave_pcs};
    for(size_t f=0;f<sizeof families/sizeof *families;++f) {
        uint32_t values[128]; const size_t n=families[f](values,128);
        assert(n>0 && n<=128);
        for(size_t i=0;i<n;++i) {
            size_t j=0;while(j<pc_count && pcs[j]!=values[i])++j;
            if(j==pc_count){assert(pc_count<512);pcs[pc_count++]=values[i];}
        }
    }
    assert(pc_count>0);
    health_lr_count=test_health_lrs(health_lrs,64);
    enhancement_lr_count=test_enhancement_lrs(enhancement_lrs,16);
    assert(health_lr_count>=3 && enhancement_lr_count>0);
}
static CPUState cpu,before;
static uint8_t ram[0x02000000u];

static void setup(unsigned state,uint32_t pending,uint32_t lr){
    cpu.lr=lr;test_hud_state(&cpu,state,pending);test_health_state(&cpu,state,pending);
    test_quick_state(&cpu,state,pending);test_dialogue_state(&cpu,state,pending);
    test_enhancement_state(&cpu,state,pending);test_autosave_state(&cpu,state,pending);
    before=cpu;
}
static void check(uint32_t address){
    unsigned old[16],old_count;
    trace_size=0;const bool a=old_chain(NULL,&cpu,address);old_count=trace_size;
    memcpy(old,trace,old_count*sizeof *old);trace_size=0;
    const uint64_t calls=executed_finite;
    const bool b=filtered_chain(NULL,&cpu,address);
    assert(a==b);
    if(bluewake_finite_observer_maybe(address)){
        assert(trace_size==old_count&&!memcmp(old,trace,old_count*sizeof *old));
    }else{
        unsigned n=0;for(unsigned i=0;i<old_count;++i)if(old[i]>6){assert(n<trace_size&&trace[n++]==old[i]);}
        assert(n==trace_size&&executed_finite==calls);++misses;
    }
    ++checks;
}
static void positive_domain(uint32_t a){
    const bool actual[]={bw_hud_host_observes(&cpu,a),bw_health_host_observes(&cpu,a),
        bluewake_quick_items_observes(a),bluewake_dialogue_speed_observes(a),
        bluewake_enhancement_hooks_observes_context(&cpu,a),bluewake_autosave_observes(a)};
    for(unsigned i=0;i<6;++i)if(actual[i]){++positive[i];assert(bluewake_finite_observer_maybe(a));}
    check(a);
}
int main(void){
    gather_pcs();
    memset(&cpu,0xA5,sizeof cpu);cpu.ram=ram;cpu.ram_size=sizeof ram;
    for(unsigned i=0;i<sizeof ram;++i)ram[i]=(uint8_t)(i*17u+3u);
    g_reward_host=&cpu;
    /* Exhaust the native DOL range and both mirrors under broad active state.
     * Health uses attached/bound active but neither suspended nor saving. */
    setup(0x1E7u,health_lrs[2],BLUEWAKE_ENHANCEMENT_WIND_RETURN);
    for(uint32_t a=0x80000000u;a<0x80400000u;a+=4){positive_domain(a);positive_domain(a^0x40000000u);}
    assert(!memcmp(&cpu,&before,sizeof cpu));
    /* Exhaust dynamic state bits, all PCs+neighbors+four bit30/highbit aliases,
     * with arbitrary pending health return and every supported LR choice. */
    for(unsigned state=0;state<512;++state){
        uint32_t lrs[81];size_t lr_count=0;
        memcpy(lrs,health_lrs,health_lr_count*sizeof *lrs);lr_count+=health_lr_count;
        memcpy(lrs+lr_count,enhancement_lrs,enhancement_lr_count*sizeof *lrs);lr_count+=enhancement_lr_count;
        assert(lr_count<81);lrs[lr_count++]=random_word();
        for(unsigned li=0;li<lr_count;++li){
            setup(state,pcs[state%pc_count],lrs[li]);
            for(unsigned i=0;i<pc_count;++i)for(int d=-3;d<=3;++d){
                const uint32_t a=pcs[i]+d;
                positive_domain(a);positive_domain(a^0x40000000u);
                check(a^0x80000000u);check(a^0xC0000000u);
            }
            for(unsigned i=0;i<64;++i)check(random_word());
            assert(!memcmp(&cpu,&before,sizeof cpu));
        }
    }
    /* Independent unrestricted hooks keep their call order, including reward
     * between the first and second finite groups, on hits and misses. */
    for(unsigned state=0;state<32;++state){setup(state,random_word(),random_word());
        for(external=0;external<16;++external)for(unsigned flags=0;flags<32;++flags){
            g_deadline_census_enabled=(flags&1)!=0;g_delivery_safety_census_enabled=(flags&2)!=0;
            g_guest_state_trace_enabled=(flags&4)!=0;bluewake_jump_button_armed=(flags&8)!=0;
            g_actor_search_native=(flags&16)!=0;
            for(unsigned i=0;i<pc_count;++i){check(pcs[i]);check(pcs[i]^0x40000000u);}
            check(BW_SEARCH_JUDGE_FILTER);for(unsigned i=0;i<16;++i)check(random_word());
        }
        assert(!memcmp(&cpu,&before,sizeof cpu));
    }
    for(unsigned i=0;i<6;++i)assert(positive[i]>0);
    for(unsigned i=0;i<sizeof ram;++i)assert(ram[i]==(uint8_t)(i*17u+3u));
    printf("finite observer oracle: %llu checks, %llu misses; six families positive\n",(unsigned long long)checks,(unsigned long long)misses);
    return 0;
}

/* Complete production TUs retain unrelated dispatch/lifecycle references.
 * Their fixture-only definitions abort if reached: observers must never call
 * these helpers. No guest mapping/event/FPU authority is supplied. */
bool g_ppc_guest_aliases_overlap_mem1;
u32 g_ppc_guest_alias_generation;
PPCMemWriteJournal g_mem_write_journal;
void* g_mem_write_journal_user;
bool ppc_guest_alias_resolve(u32 address,u32 size,u8** pointer,u32* offset){
    (void)address;(void)size;(void)pointer;(void)offset;abort();
}
bool bluewake_fpu_registers_materialized(CPUState* c){(void)c;abort();}
BwGameEventSubscription bluewake_game_events_subscribe(uint64_t m,BwGameEventCallback cb,void* u){(void)m;(void)cb;(void)u;abort();}
bool bluewake_game_events_unsubscribe(BwGameEventSubscription s){(void)s;abort();}
bool bluewake_game_events_scene(BwGameScene* s,uint64_t* e,uint64_t* g){(void)s;(void)e;(void)g;abort();}
void bluewake_game_events_stats(BwGameEventStats* s){(void)s;abort();}
