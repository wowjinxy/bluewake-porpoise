/* SPDX-License-Identifier: GPL-3.0-or-later
 * Exact extracted host predicates, actual module handshake/loop and an
 * independent frozen host predicate. Observer answers are authored fixtures;
 * this does not claim gameplay or complete observer-family qualification. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "dispatch_loop.h"
#include "edge_intercept_table.h"
#include "finite_observer_filter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define BW_SEARCH_JUDGE_FILTER 0x80001000u
#define BW_NATIVE_REWARD_SESSION 1
#ifndef BLUEWAKE_ENABLE_DEVELOPER_TRACING
#define BLUEWAKE_ENABLE_DEVELOPER_TRACING 0
#endif
#ifndef BLUEWAKE_EDGE_CENSUS
#define BLUEWAKE_EDGE_CENSUS 0
#endif
static bool g_turn_census_enabled,g_boundary_census_enabled;
static bool g_chassis_service_each_block,g_interrupt_sources_dirty;
static bool g_overlap_observation,g_new_game_intro_reported,g_player_route_waiting;
static bool g_guest_decrementer_pending,g_deadline_census_enabled;
static bool g_delivery_safety_census_enabled,g_guest_state_trace_enabled;
static bool bluewake_jump_button_armed,g_actor_search_native,g_direct_call_trace;
static u64 g_direct_call_queries,g_direct_call_allowed;
static u32 g_name_scene_object,g_ppc_guest_alias_generation,g_overlap_cached_alias_state;
static u32 g_overlap_cached_object,g_overlap_last_phase,g_module1_raw_base;
static const u8 *g_overlap_slot_ptr,*g_overlap_fields_ptr;
static struct {bool triggered,configured;} g_file_start_pulse;
static struct {u32 pi_cause,pi_mask;} g_interrupts;
static void* g_reward_host;
static u32 observers,observer_address;
static unsigned trace[32],trace_size,edge_probes,checks,facts_calls,legacy_calls;
static bool observe(unsigned bit,u32 address) {
    assert(trace_size<32);trace[trace_size++]=bit;
    return address==observer_address && (observers&(1u<<bit))!=0;
}
static bool bluewake_feature_observes(u32 a){return observe(0,a);}
static bool bluewake_game_events_observes(u32 a){return observe(1,a);}
static bool bw_hud_host_observes(const CPUState* c,u32 a){(void)c;return observe(2,a);}
static bool bw_health_host_observes(const CPUState* c,u32 a){(void)c;return observe(3,a);}
static bool bw_randomizer_reward_host_observes(void* h,const CPUState* c,u32 a){(void)c;(void)h;return observe(4,a);}
static bool bluewake_quick_items_observes(u32 a){return observe(5,a);}
static bool bluewake_dialogue_speed_observes(u32 a){return observe(6,a);}
static bool bluewake_enhancement_hooks_observes_context(const CPUState* c,u32 a){(void)c;return observe(7,a);}
static bool bluewake_autosave_observes(u32 a){return observe(8,a);}
static u32 host_canonical_linked_pc(u32 pc){return pc&~0x40000000u;}
static bool edge_probe(u32 a){++edge_probes;return bluewake_edge_maybe_intercept(a);}
#define bluewake_edge_maybe_intercept edge_probe
#include "reference.inc"
#include "actual_predicates.inc"
#undef bluewake_edge_maybe_intercept

BwChunkFn bw_find_chunk(u32 a){(void)a;return NULL;}
static bool full_callback(void* u,const CPUState* c,u32 a){++legacy_calls;return host_direct_can_skip(u,c,a);}
static bool facts_callback(void* u,const CPUState* c,u32 a,u32 f){++facts_calls;return host_direct_can_skip_facts(u,c,a,f);}
static CPUState cpu;
static u8 slot[4],fields[32];
static void reset(void){
    memset(&cpu,0,sizeof cpu);cpu.cycle_budget=100;
    g_turn_census_enabled=g_boundary_census_enabled=g_chassis_service_each_block=false;
    g_interrupt_sources_dirty=g_overlap_observation=g_player_route_waiting=false;
    g_guest_decrementer_pending=g_deadline_census_enabled=g_delivery_safety_census_enabled=false;
    g_guest_state_trace_enabled=bluewake_jump_button_armed=g_actor_search_native=false;
    g_new_game_intro_reported=true;g_name_scene_object=g_ppc_guest_alias_generation=0;
    g_overlap_cached_alias_state=g_overlap_cached_object=g_overlap_last_phase=g_module1_raw_base=0;
    g_overlap_slot_ptr=g_overlap_fields_ptr=NULL;g_file_start_pulse.configured=true;
    g_file_start_pulse.triggered=false;g_interrupts.pi_cause=g_interrupts.pi_mask=0;
    g_reward_host=&cpu;observers=observer_address=0;g_direct_call_trace=true;
    g_direct_call_queries=g_direct_call_allowed=0;trace_size=0;bw_direct_depth=0;
}
static void enable(void){
    assert(bluewake_composite_direct_calls_v2(true,&g_interrupt_sources_dirty,&g_guest_decrementer_pending,
        &g_interrupts.pi_cause,&g_interrupts.pi_mask,full_callback,NULL));
    assert(bluewake_composite_edge_filter(true));
}
static unsigned certify(void){return bluewake_composite_observation_facts_v1(
    GXRUNTIME_CPU_ABI_VERSION,sizeof(CPUState),g_edge_keys_all,BLUEWAKE_EDGE_KEY_COUNT,facts_callback,NULL);}
static void compare(u32 a){
    const CPUState saved=cpu;unsigned old[32];
    trace_size=0;const bool expected=reference_can_skip(NULL,&cpu,a);
    const unsigned n=trace_size;memcpy(old,trace,n*sizeof *old);trace_size=0;
    const bool full=host_can_skip_observation(NULL,&cpu,a);assert(full==expected);
    assert(trace_size==n&&!memcmp(old,trace,n*sizeof *old));
    assert(!memcmp(&saved,&cpu,sizeof cpu));++checks;
    /* Only certify facts under the actual same ready guards and watch miss. */
    if(bw_edge_unwatched(a)&&bw_host_quiet(&cpu)){
        trace_size=0;edge_probes=0;
        const bool actual=host_direct_can_skip_facts(NULL,&cpu,a,BW_OBSERVATION_FACTS_ALL);
        assert(actual==expected&&edge_probes==0);
        assert(trace_size==n&&!memcmp(old,trace,n*sizeof *old));
        assert(!memcmp(&saved,&cpu,sizeof cpu));++checks;
    }
    for(u32 f=0;f<8;++f)if(f!=BW_OBSERVATION_FACTS_ALL){
        trace_size=0;const bool fallback=host_direct_can_skip_facts(NULL,&cpu,a,f);
        assert(fallback==expected&&trace_size==n&&!memcmp(old,trace,n*sizeof *old));++checks;
    }
}
static void handshake_tests(void){
    assert(!certify());enable();assert(certify()==BW_OBSERVATION_FACTS_V1);
    u32 saved[BW_EDGE_WATCH_SLOTS];memcpy(saved,bw_edge_watch_table,sizeof saved);
    /* Every host key must be an actual member, even keys outside DOL code. */
    for(unsigned k=0;k<BLUEWAKE_EDGE_KEY_COUNT;++k){
        assert(!bw_edge_unwatched(g_edge_keys_all[k]));
        assert(!bw_edge_unwatched(g_edge_keys_all[k]|0x40000000u));
        for(unsigned i=0;i<BW_EDGE_WATCH_SLOTS;++i)if(bw_edge_watch_table[i]==g_edge_keys_all[k])bw_edge_watch_table[i]=0;
        assert(!certify()&&bw_host_observation_facts==NULL);
        memcpy(bw_edge_watch_table,saved,sizeof saved);assert(certify()==1);++checks;
    }
    u32 keys[56];memcpy(keys,g_edge_keys_all,sizeof keys);
#define BAD(abi,size,ptr,count,cb) do{assert(!bluewake_composite_observation_facts_v1(abi,size,ptr,count,cb,NULL));assert(!bw_host_observation_facts);assert(certify()==1);++checks;}while(0)
    BAD(0,sizeof cpu,keys,56,facts_callback);BAD(GXRUNTIME_CPU_ABI_VERSION,0,keys,56,facts_callback);
    BAD(GXRUNTIME_CPU_ABI_VERSION,sizeof cpu,NULL,56,facts_callback);
    BAD(GXRUNTIME_CPU_ABI_VERSION,sizeof cpu,keys,0,facts_callback);
    BAD(GXRUNTIME_CPU_ABI_VERSION,sizeof cpu,keys,55,facts_callback);
    BAD(GXRUNTIME_CPU_ABI_VERSION,sizeof cpu,keys,57,facts_callback);
    BAD(GXRUNTIME_CPU_ABI_VERSION,sizeof cpu,keys,56,NULL);
    keys[0]|=0x40000000u;BAD(GXRUNTIME_CPU_ABI_VERSION,sizeof cpu,keys,56,facts_callback);
    keys[0]=keys[1];BAD(GXRUNTIME_CPU_ABI_VERSION,sizeof cpu,keys,56,facts_callback);
    memcpy(keys,g_edge_keys_all,sizeof keys);keys[55]=0x81FFFFFFu;
    BAD(GXRUNTIME_CPU_ABI_VERSION,sizeof cpu,keys,56,facts_callback);
#undef BAD
    assert(bluewake_composite_edge_filter(false)==0&&!bw_host_observation_facts);
    assert(!certify());enable();assert(certify()==1);
    enable();assert(!bw_host_observation_facts);assert(certify()==1);
    assert(!bluewake_composite_direct_calls(false,NULL,NULL,NULL,NULL));assert(!bw_host_observation_facts);
    enable();assert(certify()==1);
    assert(!bluewake_composite_direct_calls_v2(false,NULL,NULL,NULL,NULL,NULL,NULL));assert(!bw_host_observation_facts);
    enable();assert(certify()==1);
    /* Four audited boundaries retain full queries, including REL mirrors. */
    const u32 watched[]={0x8008A870u,0x81F10624u,0x8012821Cu,0x801198BCu};
    for(unsigned i=0;i<4;++i){const unsigned before=facts_calls,old=legacy_calls;
        assert(!bw_edge_unwatched(watched[i]));trace_size=0;(void)bw_direct_call_ready(&cpu,watched[i]);
        assert(facts_calls==before&&legacy_calls==old+1);++checks;}
}
static void dynamic_tests(void){
    const u32 pcs[]={0x80004000u,0xC0004000u,0xC0400000u,0xC1E01B88u,0x1000u,
        0x80018554u,0x8001199Cu,0x80122D30u,0x80328F84u,0xC0328F84u,BW_SEARCH_JUDGE_FILTER,
        0x8008A870u,0x81F10624u,0xC1F10624u,0x8012821Cu,0x801198BCu};
    bool* flags[]={&g_turn_census_enabled,&g_boundary_census_enabled,&g_chassis_service_each_block,
        &g_interrupt_sources_dirty,&g_deadline_census_enabled,&g_delivery_safety_census_enabled,
        &g_guest_state_trace_enabled,&bluewake_jump_button_armed,&g_actor_search_native,&g_player_route_waiting};
    for(unsigned p=0;p<sizeof pcs/sizeof *pcs;++p)for(unsigned state=0;state<20;++state){
        reset();enable();assert(certify()==1);u32 a=pcs[p];
        if(state<10)*flags[state]=true;
        if(state==10)g_new_game_intro_reported=false;
        if(state==11){g_module1_raw_base=a-0xD4u;}
        if(state>=12){g_overlap_observation=true;g_name_scene_object=0x80100000u;
            g_file_start_pulse.configured=false;g_overlap_slot_ptr=slot;g_overlap_fields_ptr=fields;
            g_overlap_cached_object=0x80101000u;g_overlap_last_phase=7;
            write_be32(slot,g_overlap_cached_object);write_be16(fields+4,1);write_be32(fields+28,7);
            if(state==12)g_ppc_guest_alias_generation=1;
            if(state==13)g_overlap_slot_ptr=NULL;
            if(state==14)write_be32(slot,0x80101004u);
            if(state==15)g_overlap_fields_ptr=NULL;
            if(state==16)write_be32(fields+28,8);
            if(state==17)write_be16(fields+4,0);
            if(state==18)write_be32(slot,0);
        }
        compare(a);
    }
    for(unsigned ee=0;ee<2;++ee)for(unsigned pending=0;pending<2;++pending)
    for(unsigned cause=0;cause<4;++cause)for(unsigned mask=0;mask<4;++mask){
        reset();enable();assert(certify()==1);cpu.msr=ee?PPC_MSR_EE:0;
        g_guest_decrementer_pending=pending;g_interrupts.pi_cause=cause;g_interrupts.pi_mask=mask;
        compare(0x80004000u);trace_size=0;
        const unsigned old=facts_calls;(void)bw_chassis_ready_after_unwatched(&cpu,0x80004000u);
        assert(facts_calls==old+(bw_host_quiet(&cpu)?1u:0u));++checks;
    }
    /* All observer answers and their short-circuit order remain authoritative. */
    const u32 observed[]={0x80004000u,0x8008A870u,0x8012821Cu,0x801198BCu,0xC1F10624u};
    for(unsigned p=0;p<5;++p)for(unsigned bits=0;bits<512;++bits){
        reset();enable();assert(certify()==1);observer_address=observed[p];observers=bits;compare(observed[p]);
    }
    reset();enable();assert(certify()==1);trace_size=0;
    assert(!bw_chassis_ready_after_unwatched(NULL,0x80004000u));
    cpu.exception=1;assert(!bw_chassis_ready_after_unwatched(&cpu,0x80004000u));cpu.exception=0;
    cpu.downcount=-100;assert(!bw_chassis_ready_after_unwatched(&cpu,0x80004000u));cpu.downcount=0;
    bw_direct_depth=BW_DIRECT_DEPTH_MAX;assert(!bw_chassis_ready_after_unwatched(&cpu,0x80004000u));bw_direct_depth=0;
    assert(bluewake_composite_edge_filter(false)==0);trace_size=0;
    const unsigned before=legacy_calls;(void)bw_chassis_ready_after_unwatched(&cpu,0x80004000u);
    assert(legacy_calls==before+1);++checks;
}
static unsigned loop_case,loop_dispatches,loop_edges;
static int loop_dispatch(CPUState* c,u32 a){
    ++loop_dispatches;trace_size=0;c->gpr[0]+=a;c->downcount-=2;c->pc=a+4u;
    if(loop_dispatches==2){
        if(loop_case==1)c->pc=0x8008A870u;
        if(loop_case==2)g_interrupt_sources_dirty=true;
        if(loop_case==3){observer_address=c->pc;observers=1u<<1;}
        if(loop_case==4)g_module1_raw_base=c->pc-0xD4u;
        if(loop_case==5)c->pc=0x1000u;
    }
    return 1;
}
static bool loop_edge(void* u,CPUState* c,u32 a){
    (void)u;(void)c;(void)a;++loop_edges;
    /* Model service consuming a dynamic observation, identically in both runs. */
    observers=0;g_module1_raw_base=0;return false;
}
typedef struct LoopResult{CPUState cpu;unsigned dispatches,edges;u64 queries,allowed;}LoopResult;
static LoopResult run_loop(bool facts,unsigned scenario){
    reset();enable();if(facts)assert(certify()==1);
    loop_case=scenario;loop_dispatches=loop_edges=0;cpu.cycle_budget=12;
    assert(bluewake_chassis_dispatch_loop(&cpu,0x80004000u,loop_dispatch,loop_edge,NULL)==1);
    LoopResult out;memset(&out,0,sizeof out);out.cpu=cpu;out.dispatches=loop_dispatches;
    out.edges=loop_edges;out.queries=g_direct_call_queries;out.allowed=g_direct_call_allowed;return out;
}
static void loop_tests(void){
    for(unsigned scenario=0;scenario<6;++scenario){
        const LoopResult full=run_loop(false,scenario),facts=run_loop(true,scenario);
        assert(!memcmp(&full,&facts,sizeof full));assert(full.dispatches==6);
#if !(BLUEWAKE_ENABLE_DEVELOPER_TRACING || BLUEWAKE_EDGE_CENSUS)
        assert(full.edges==(scenario==0?0u:scenario==2?4u:scenario==5?4u:1u));
#else
        assert(full.edges==5);
#endif
        ++checks;
    }
}
int main(void){reset();handshake_tests();dynamic_tests();loop_tests();
    printf("observation facts PASS checks=%u legacy=%u facts=%u tracing=%u census=%u\n",
        checks,legacy_calls,facts_calls,BLUEWAKE_ENABLE_DEVELOPER_TRACING,BLUEWAKE_EDGE_CENSUS);
    return 0;
}
