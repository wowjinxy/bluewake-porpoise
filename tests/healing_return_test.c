// SPDX-License-Identifier: GPL-3.0-or-later
/* Authored same-chunk topology + production call/edge helpers and health core.
 * All actor/native outcomes are synthetic; no game bytes or device execution. */
#define main bw_health_base_fixture_main
#include "health_rules_test.c"
#undef main
#include "direct_calls.h"
#include "dispatch_loop.h"

static unsigned leaf_calls,epilogues,queries,allowed,observations,entry_edges,completion_edges;
static bool use_guard,passive;
static const bool clean=false;static const u32 zero=0;
static const u32 outer=0x800F64B8u;
static void synthetic_add(CPUState* context,float amount){
    ++leaf_calls;--context->downcount;
    mem_write32(context,COUNT,bits((float)((double)rf(COUNT)+(double)amount)));
}
#define HEALING_CHUNK_NAME original_chunk
#include "healing_return_original.inc"
#undef HEALING_CHUNK_NAME
#define HEALING_CHUNK_NAME guarded_chunk
#include "healing_return_guarded.inc"
#undef HEALING_CHUNK_NAME
BwChunkFn bw_find_chunk(u32 address){
    if(address==BW_HEALTH_RULES_HEART||address==BW_HEALTH_RULES_FAIRY||address==BW_HEALTH_RULES_ITEM_RETURN)
        return use_guard?guarded_chunk:original_chunk;
    return NULL;
}
static bool skip(void* user,const CPUState* context,u32 address){
    (void)user;++queries;const bool yes=!bw_health_rules_observes(&runtime,context,address);allowed+=yes;return yes;
}
static bool observe(void* user,const CPUState* context,u32 address){
    CHECK(user==&runtime);++observations;
    CHECK(address==BW_HEALTH_RULES_ITEM_RETURN&&context->pc==address&&context->lr==address);
    CHECK(context->gpr[1]==STACK&&mem_read32(context,STACK+20u)==outer&&epilogues==0);
    const CPUState before=*context;const bool yes=passive||!bw_health_rules_observes(&runtime,context,address);
    CHECK(!memcmp(&before,context,sizeof before));return yes;
}
static int dispatch(CPUState* context,u32 address){
    if(address!=0x800C2E1Cu&&bw_find_chunk(address)==NULL)return 0;
    (use_guard?guarded_chunk:original_chunk)(context);return 1;
}
static bool service(void* user,CPUState* context,u32 address){
    (void)user;
    if(address==BW_HEALTH_RULES_HEART||address==BW_HEALTH_RULES_FAIRY)++entry_edges;
    if(address==BW_HEALTH_RULES_ITEM_RETURN)++completion_edges;
    bw_health_rules_dispatch(&runtime,context,address,&life);return false;
}
static void route(unsigned rate,bool fairy,bool guarded,bool observer){
    (void)bluewake_composite_healing_return_v1(GXRUNTIME_CPU_ABI_VERSION,sizeof(CPUState),NULL,NULL);
    setup(256,rate);use_guard=guarded;passive=false;
    cpu.pc=0x800C2E1Cu;cpu.ctr=cpu.gpr[12]=fairy?BW_HEALTH_RULES_FAIRY:BW_HEALTH_RULES_HEART;
    cpu.gpr[3]=fairy?0x16u:0u;cpu.downcount=0;cpu.cycle_budget=100;w32(STACK+20u,outer);wf(COUNT,1.5f);
    leaf_calls=epilogues=queries=allowed=observations=entry_edges=completion_edges=0;
    CHECK(bluewake_composite_direct_calls_v2(true,&clean,&clean,&zero,&zero,skip,NULL));
    CHECK(bluewake_composite_edge_filter(true));
    CHECK(bluewake_composite_healing_return_v1(GXRUNTIME_CPU_ABI_VERSION,sizeof(CPUState),observer?observe:NULL,&runtime)==1);
}
static void execute(void){CHECK(bluewake_chassis_dispatch_loop(&cpu,cpu.pc,dispatch,service,NULL));}
static void null_identity(void){
    for(unsigned fairy=0;fairy<2;++fairy){
        route(256,fairy,false,false);execute();const CPUState old=cpu;
        memcpy(before_ram,cpu.ram,cpu.ram_size);const unsigned q=queries,a=allowed;
        CHECK(leaf_calls==1&&epilogues==1&&observations==0&&completion_edges==0);
        route(256,fairy,true,false);execute();
        CHECK(!memcmp(&old,&cpu,sizeof cpu)&&!memcmp(before_ram,cpu.ram,cpu.ram_size));
        CHECK(queries==q&&allowed==a&&observations==0&&leaf_calls==1&&epilogues==1);
        CHECK(cpu.pc==outer&&cpu.lr==outer&&cpu.gpr[1]==STACK+16u&&cpu.downcount==-8);
    }
}
static void old_gap_and_scaled_returns(void){
    route(512,false,false,true);execute();
    CHECK(rf(COUNT)==5.5f&&runtime.stats.healing_entries==1&&runtime.stats.completed==0);
    CHECK(epilogues==1&&observations==0&&completion_edges==0); /* Old topology canary. */
    for(unsigned fairy=0;fairy<2;++fairy)for(unsigned index=0;index<2;++index){
        const unsigned rate=index?512:128;route(rate,fairy,true,true);execute();
        const float expected=1.5f+(fairy?40.0f:4.0f)*(index?2.0f:0.5f);
        CHECK(rf(COUNT)==expected&&leaf_calls==1&&epilogues==1&&entry_edges==1&&completion_edges==1);
        CHECK(observations==1&&runtime.stats.completed==1&&runtime.stats.adjusted==1);
        CHECK(cpu.pc==outer&&cpu.lr==outer&&cpu.gpr[1]==STACK+16u&&cpu.downcount==-8);
        no_extra_writes(BW_HEALTH_RULES_ITEM_RETURN); /* Replay cannot scale twice. */
    }
    route(256,false,true,true);passive=true;execute();
    CHECK(observations==1&&leaf_calls==1&&epilogues==1&&rf(COUNT)==5.5f&&runtime.stats.completed==0);
}
static void budget_resume_and_lifetime(void){
    route(512,false,true,true);cpu.cycle_budget=3;execute();
    CHECK(leaf_calls==1&&epilogues==0&&observations==0&&cpu.pc==BW_HEALTH_RULES_ITEM_RETURN);
    const CPUState saved=cpu;CHECK(cpu.lr==BW_HEALTH_RULES_ITEM_RETURN&&cpu.gpr[1]==STACK&&rf(COUNT)==5.5f);
    /* Actual host first-PC service after a native budget yield; no leaf replay. */
    cpu.cycle_budget=100;cpu.downcount=0;service(NULL,&cpu,cpu.pc);execute();
    CHECK(leaf_calls==1&&epilogues==1&&rf(COUNT)==9.5f&&runtime.stats.completed==1);
    CHECK(saved.pc==BW_HEALTH_RULES_ITEM_RETURN&&saved.gpr[1]==STACK);
    for(unsigned stale=0;stale<3;++stale){
        route(512,false,true,true);cpu.cycle_budget=3;execute();
        if(stale==0)++life.epoch;
        if(stale==1)life.tick+=5;
        if(stale==2)bw_health_rules_detach(&runtime);
        cpu.cycle_budget=100;cpu.downcount=0;service(NULL,&cpu,cpu.pc);execute();
        CHECK(leaf_calls==1&&epilogues==1&&rf(COUNT)==5.5f&&runtime.stats.completed==0);
    }
}
static void registration_contract(void){
    route(512,false,true,true);CHECK(bw_healing_return_can_continue==observe);
    CHECK(bluewake_composite_healing_return_v1(GXRUNTIME_CPU_ABI_VERSION+1,sizeof(CPUState),observe,&runtime)==0);
    CHECK(!bw_healing_return_can_continue&&!bw_healing_return_user);
    CHECK(bluewake_composite_healing_return_v1(GXRUNTIME_CPU_ABI_VERSION,sizeof(CPUState)-1,observe,&runtime)==0);
    CHECK(!bw_healing_return_can_continue);
    CHECK(bluewake_composite_healing_return_v1(GXRUNTIME_CPU_ABI_VERSION,sizeof(CPUState),NULL,&runtime)==1);
    CHECK(!bw_healing_return_can_continue&&!bw_healing_return_user);
}
int main(void){
    cpu.ram=calloc(1,BW_HEALTH_RULES_HOST_RAM_SIZE);before_ram=malloc(BW_HEALTH_RULES_HOST_RAM_SIZE);CHECK(cpu.ram&&before_ram);
    null_identity();old_gap_and_scaled_returns();budget_resume_and_lifetime();registration_contract();
    (void)bluewake_composite_healing_return_v1(GXRUNTIME_CPU_ABI_VERSION,sizeof(CPUState),NULL,NULL);
    bw_health_rules_detach(&runtime);ppc_guest_alias_clear();free(before_ram);free(cpu.ram);
    printf("Healing return authored topology + real module/edge/health helpers: %u checks PASS (synthetic contexts only)\n",checks);
}
