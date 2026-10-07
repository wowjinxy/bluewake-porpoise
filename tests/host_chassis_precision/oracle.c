/* Authored CPU/RAM fixture: actual old/new chassis predicates are extracted
 * verbatim by run_oracle.py. No game, device, renderer or input execution. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "core/cpu.h"
#include "edge_intercept_table.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_turn_census_enabled,g_boundary_census_enabled;
static bool g_chassis_service_each_block,g_interrupt_sources_dirty;
static bool g_overlap_observation,g_new_game_intro_reported,g_player_route_waiting;
static bool g_guest_decrementer_pending;
static u32 g_name_scene_object,g_ppc_guest_alias_generation,g_overlap_cached_alias_state;
static u32 g_overlap_cached_object,g_overlap_last_phase,g_module1_raw_base;
static const u8 *g_overlap_slot_ptr,*g_overlap_fields_ptr;
static struct {bool triggered,configured;} g_file_start_pulse;
static struct {u32 pi_cause,pi_mask;} g_interrupts;
static u32 host_canonical_linked_pc(u32 pc){return pc & ~0x40000000u;}
#include "host_chassis_predicates.inc"

static u8 ram[0x02000000],saved_ram[0x02000000];
static CPUState cpu;
static unsigned checks,changed;
static u32 random_state=0xF84A6A04u;
static u32 next(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static void clean(void){
    g_turn_census_enabled=g_boundary_census_enabled=g_chassis_service_each_block=false;
    g_interrupt_sources_dirty=g_overlap_observation=g_player_route_waiting=false;
    g_guest_decrementer_pending=false;g_new_game_intro_reported=true;
    g_name_scene_object=g_ppc_guest_alias_generation=g_overlap_cached_alias_state=0;
    g_overlap_cached_object=g_overlap_last_phase=g_module1_raw_base=0;
    g_overlap_slot_ptr=g_overlap_fields_ptr=NULL;
    g_file_start_pulse.triggered=false;g_file_start_pulse.configured=true;
    g_interrupts.pi_cause=g_interrupts.pi_mask=0;cpu.msr=PPC_MSR_FP;
}
static bool compare(u32 address){
    const CPUState before=cpu;
    const bool old=old_requires_full(&cpu,address),now=new_requires_full(&cpu,address);
    assert(!memcmp(&before,&cpu,sizeof cpu));++checks;
    if(old!=now){
        assert(old&&!now);
        assert(host_canonical_linked_pc(address)==0x80328F84u && cpu.lr!=0x80246A04u);
        ++changed;
    }
    return now;
}
static void block(u32 address){assert(compare(address));}
int main(void){
    for(size_t i=0;i<sizeof ram;++i)ram[i]=(u8)(i*17u+(i>>11));
    memcpy(saved_ram,ram,sizeof ram);memset(&cpu,0,sizeof cpu);
    cpu.ram=ram;cpu.ram_size=sizeof ram;cpu.gpr[1]=0x80108000u;
    clean();cpu.lr=0x802F5714u;
    assert(old_requires_full(NULL,0x80328F84u));
    assert(new_requires_full(NULL,0x80328F84u));checks+=2;
    assert(bluewake_edge_maybe_intercept(0x80328F84u));
    assert(old_requires_full(&cpu,0x80328F84u));
    const u32 lrs[]={0,0x802F5714u,0x80246A04u,0xC0246A04u,0xFFFFFFFFu};
    for(unsigned mirror=0;mirror<2;++mirror)for(unsigned i=0;i<5;++i){
        clean();cpu.lr=lrs[i];
        assert(compare(0x80328F84u|(mirror?0x40000000u:0))==(lrs[i]==0x80246A04u));
    }
    /* Every other perfect-hash key and mirror retains exactly the old result. */
    for(size_t i=0;i<BLUEWAKE_EDGE_KEY_COUNT;++i)for(unsigned mirror=0;mirror<2;++mirror){
        clean();cpu.lr=0x80246A04u;block(g_edge_keys_all[i]|(mirror?0x40000000u:0));
        cpu.lr=0x802F5714u;
        assert(compare(g_edge_keys_all[i]|(mirror?0x40000000u:0))==(g_edge_keys_all[i]!=0x80328F84u));
    }
    for(unsigned flag=0;flag<5;++flag){
        clean();cpu.lr=0x802F5714u;
        bool* flags[]={&g_turn_census_enabled,&g_boundary_census_enabled,&g_chassis_service_each_block,&g_interrupt_sources_dirty,&g_guest_decrementer_pending};
        *flags[flag]=true;if(flag==4)cpu.msr|=PPC_MSR_EE;block(0x80328F84u);
    }
    for(unsigned ee=0;ee<2;++ee)for(unsigned pending=0;pending<2;++pending)
    for(unsigned cause=0;cause<4;++cause)for(unsigned mask=0;mask<4;++mask){
        clean();cpu.lr=0x802F5714u;cpu.msr|=ee?PPC_MSR_EE:0;
        g_guest_decrementer_pending=pending!=0;g_interrupts.pi_cause=cause;g_interrupts.pi_mask=mask;
        assert(compare(0x80328F84u)==(ee&&(pending||(cause&mask))));
    }
    /* Actual overlap predicate: identity/pointer/phase changes still block. */
    u8* slot=ram+16;u8* fields=ram+128;
    for(unsigned scenario=0;scenario<9;++scenario){
        clean();cpu.lr=0x802F5714u;g_overlap_observation=true;
        g_name_scene_object=0x80100000u;g_file_start_pulse.configured=false;
        g_overlap_slot_ptr=slot;g_overlap_fields_ptr=fields;
        g_overlap_cached_object=0x80101000u;g_overlap_last_phase=7;
        write_be32(slot,g_overlap_cached_object);write_be16(fields+4,1);write_be32(fields+28,7);
        if(scenario==0)g_ppc_guest_alias_generation=1;
        if(scenario==1)g_overlap_slot_ptr=NULL;
        if(scenario==2)write_be32(slot,0x80101004u);
        if(scenario==3)g_overlap_fields_ptr=NULL;
        if(scenario==4)write_be32(fields+28,8);
        if(scenario==5)write_be16(fields+4,0);
        if(scenario==6)write_be32(slot,0);
        if(scenario==7)g_file_start_pulse.configured=true;
        assert(compare(0x80328F84u)==(scenario<5));
    }
    memcpy(ram,saved_ram,sizeof ram);
    clean();cpu.lr=0x802F5714u;g_module1_raw_base=0x80328EB0u;block(0x80328F84u);
    clean();g_new_game_intro_reported=false;block(0x80018554u);block(0x8001199Cu);
    clean();g_player_route_waiting=true;block(0x80122D30u);
    const u32 maya[]={0x80328F38u,0x802F551Cu,0x80328F84u,0x802F5714u,
        0x802DA64Cu,0x802F55A4u,0x8030D0FCu,0x802F56D0u,0x802F56E0u,0x8030D0C8u,0x802F56F0u};
    for(size_t i=0;i<sizeof maya/sizeof *maya;++i){clean();cpu.lr=i<2?0x802F551Cu:0x802F5714u;assert(!compare(maya[i]));}
    for(unsigned i=0;i<50000;++i){
        clean();cpu.lr=next();u32 address=next();
        if(i%7==0)address=0x80328F84u|((i&1)?0x40000000u:0);
        (void)compare(address);
    }
    assert(!memcmp(ram,saved_ram,sizeof ram));
    assert(changed>7000);printf("host precision oracle: %u checks, %u context refinements; CPU/RAM unchanged\n",checks,changed);
    return 0;
}
