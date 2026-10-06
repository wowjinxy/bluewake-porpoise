#ifdef NDEBUG
#undef NDEBUG
#endif
#define BLUEWAKE_SEARCH_NAME_REUSE 1
#include "native_search.c"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
int reference_native_search(CPUState*,u32);
BwChunkFn bw_find_chunk(u32 address) { (void)address;return NULL; }
BwChunkFn* const bw_chunk_fns=NULL;
#define BYTES 0x00400000u
#define NAME 0x80100040u
#define FILTER 0x80100080u
#define PRM 0x80100100u
#define NODES 0x80100400u
#define ACTORS 0x80102000u
#define SP 0x80110000u
static u8 ram[BYTES],reference_ram[BYTES],before_ram[BYTES],alias_bytes[4096];
static bool dirty,pending,observe,physical_stack_alias;
static u32 cause,mask,refused;
static unsigned long long comparisons,declines;
static bool ready(void* user,const CPUState* cpu,u32 address) {
    (void)user;assert(cpu!=NULL);return !observe && address!=refused;
}
static void put(u32 address,u32 value) {write_be32(ram+address-GC_RAM_BASE,value);}
static void journal(u32 a,u32 n,void* u) {(void)a;(void)n;(void)u;abort();}
static CPUState state(void) {
    CPUState cpu;memset(&cpu,0xA5,sizeof cpu);
    cpu.ram=ram;cpu.ram_size=BYTES;cpu.pc=BLUEWAKE_SEARCH_JUDGE_FILTER;cpu.lr=0x80244F88u;
    cpu.gpr[1]=SP;cpu.gpr[4]=FILTER;cpu.gpr[29]=BLUEWAKE_SEARCH_JUDGE_FILTER;
    cpu.gpr[30]=FILTER;cpu.ctr=BLUEWAKE_SEARCH_JUDGE_FILTER;cpu.host_call=NULL;
    cpu.exception=0;cpu.cycle_budget=100000;cpu.cycle_deadline_budget=0;cpu.downcount=0;
    cpu.cycle_observation_suffix=1;return cpu;
}
static void layout(unsigned table_match,unsigned list_match,u32 name) {
    memset(ram,0xCD,sizeof ram);
    for(unsigned i=0;i<SEARCH_ENTRIES;++i) {
        u8* p=ram+SEARCH_TABLE-GC_RAM_BASE+12*i;memset(p,0,12);p[0]='x';p[8]=0x12;p[9]=0x34;p[10]=2;
    }
    if(table_match<SEARCH_ENTRIES)memcpy(ram+SEARCH_TABLE-GC_RAM_BASE+12*table_match,"actor",6);
    memcpy(ram+NAME-GC_RAM_BASE,"actor",6);memcpy(alias_bytes,"actor",6);
    put(FILTER,SEARCH_FIND_OBJECT);put(FILTER+4,PRM);put(PRM,name);put(PRM+4,0);put(PRM+8,0);
    for(unsigned i=0;i<9;++i) {
        put(NODES+32*i+8,i==8?0:NODES+32*(i+1));put(NODES+32*i+12,ACTORS+512*i);
        u8* actor=ram+ACTORS-GC_RAM_BASE+512*i;actor[14]=i==list_match?0x12:0x56;actor[15]=i==list_match?0x34:0x78;
        actor[449]=2;write_be32(actor+176,0x12345678);
    }
}
static int compare(CPUState* cpu) {
    memcpy(reference_ram,ram,sizeof ram);memcpy(before_ram,ram,sizeof ram);
    CPUState original=*cpu,other=*cpu;other.ram=reference_ram;
    int a=bluewake_native_search(cpu,BLUEWAKE_SEARCH_JUDGE_FILTER);
    if(physical_stack_alias) {
        assert(ppc_guest_alias_remove(0xC1F00000u,4096));
        assert(ppc_guest_alias_add_shared(0xC1F00000u,4096,reference_ram+SP-GC_RAM_BASE-64));
    }
    int b=reference_native_search(&other,BLUEWAKE_SEARCH_JUDGE_FILTER);
    if(physical_stack_alias) {
        assert(ppc_guest_alias_remove(0xC1F00000u,4096));
        assert(ppc_guest_alias_add_shared(0xC1F00000u,4096,ram+SP-GC_RAM_BASE-64));
    }
    other.ram=ram;
    if(a!=b || memcmp(cpu,&other,sizeof *cpu)!=0 || memcmp(ram,reference_ram,sizeof ram)!=0) {
        fprintf(stderr,"comparison=%llu candidate=%d reference=%d cache_hits=%llu name=%08X node=%08X\n",
                comparisons,a,b,s_reuse_hits,read_be32(before_ram+PRM-GC_RAM_BASE),original.gpr[3]);
        for(unsigned j=0;j<sizeof *cpu;++j)if(((u8*)cpu)[j]!=((u8*)&other)[j])
            fprintf(stderr,"CPU byte %u got=%02X reference=%02X\n",j,((u8*)cpu)[j],((u8*)&other)[j]);
        for(unsigned j=0;j<BYTES;++j)if(ram[j]!=reference_ram[j]) {
            fprintf(stderr,"RAM %08X got=%02X reference=%02X\n",GC_RAM_BASE+j,ram[j],reference_ram[j]);break;
        }
        exit(1);
    }
    if(!a) {assert(memcmp(cpu,&original,sizeof *cpu)==0 && memcmp(ram,before_ram,sizeof ram)==0);++declines;}
    ++comparisons;return a;
}
static void setup(void) {
    dirty=pending=observe=physical_stack_alias=false;cause=mask=refused=0;
    bw_host_sources_dirty=&dirty;bw_host_decrementer_pending=&pending;bw_host_pi_cause=&cause;bw_host_pi_mask=&mask;
    bw_host_can_skip=ready;bw_host_can_skip_user=NULL;bw_edge_filter_enabled=bw_edge_watch_ready=true;
    memset(bw_edge_watch_table,0,sizeof bw_edge_watch_table);g_mem_write_journal=NULL;g_ppc_guest_aliases_overlap_mem1=false;
    bluewake_composite_native_entries_v1(true,ready,NULL);
}
int main(void) {
    setup();CPUState cpu;
    // Every table match and absent-name position, every actor-list match position
    // and a no-match list. The outer translated walk is deliberately retained.
    for(unsigned entry=0;entry<=SEARCH_ENTRIES;++entry)for(unsigned match=0;match<=9;++match) {
        layout(entry,match,NAME);
        for(unsigned node=0;node<9;++node) {
            cpu=state();cpu.gpr[3]=NODES+32*node;cpu.gpr[31]=node==8?0:NODES+32*(node+1);
            assert(compare(&cpu));
            assert(cpu.gpr[3]==(entry<SEARCH_ENTRIES && node==match?ACTORS+512*node:0));
        }
    }
    unsigned long long steady_hits=s_reuse_hits,steady_lookups=s_reuse_lookups;
    assert(steady_hits>50000);
    // The entry expires after sixteen uses, even with identical verified bytes.
    layout(0,8,NAME);unsigned long long fills=s_reuse_lookups;
    for(unsigned i=0;i<40;++i) {cpu=state();cpu.gpr[3]=NODES;assert(compare(&cpu));}
    assert(s_reuse_lookups-fills>=2);
    // Mutations of every dependency are observed, not covered by a quiet-host hint.
    for(unsigned mutation=0;mutation<12;++mutation) {
        setup();layout(383,8,NAME);cpu=state();cpu.gpr[3]=NODES;assert(compare(&cpu));
        cpu=state();cpu.gpr[3]=NODES;assert(compare(&cpu));
        switch(mutation) {
        case 0:ram[SEARCH_TABLE-GC_RAM_BASE]='a';ram[SEARCH_TABLE-GC_RAM_BASE+1]=0;break;
        case 1:ram[SEARCH_TABLE-GC_RAM_BASE+383*12]='z';break;
        case 2:ram[NAME-GC_RAM_BASE]='z';break;
        case 3:put(PRM,NAME+1);break;
        case 4:put(PRM+4,0xFFFFFFFF);put(PRM+8,0x12345678);break;
        case 5:put(NODES+12,ACTORS+8*512);break;
        case 6:ram[ACTORS-GC_RAM_BASE+14]=0x12;ram[ACTORS-GC_RAM_BASE+15]=0x34;break;
        case 7:ram[SEARCH_TABLE-GC_RAM_BASE+383*12+10]=3;break;
        case 8:put(NODES+8,0);break;
        case 9:put(NODES+12,SP-14);break;
        case 10:put(PRM,SP-60);break;
        default:put(PRM,NAME|0x40000000u);break;
        }
        cpu=state();cpu.gpr[3]=NODES;compare(&cpu);
    }
    // Guest alias replacement and physical stack aliases revoke reuse.
    setup();layout(8,8,0xC1F00000u);assert(ppc_guest_alias_add_shared(0xC1F00000u,4096,alias_bytes));
    for(unsigned i=0;i<5;++i) {cpu=state();cpu.gpr[3]=NODES;assert(compare(&cpu));}
    alias_bytes[0]='z';cpu=state();cpu.gpr[3]=NODES;compare(&cpu);
    assert(ppc_guest_alias_remove(0xC1F00000u,4096));memcpy(alias_bytes,"actor",6);
    assert(ppc_guest_alias_add_shared(0xC1F00000u,4096,alias_bytes));cpu=state();cpu.gpr[3]=NODES;compare(&cpu);
    assert(ppc_guest_alias_remove(0xC1F00000u,4096));
    assert(ppc_guest_alias_add_shared(0xC1F00000u,4096,ram+SP-GC_RAM_BASE-64));
    physical_stack_alias=true;cpu=state();cpu.gpr[3]=NODES;compare(&cpu);
    physical_stack_alias=false;ppc_guest_alias_clear();
    // Short budgets and deadlines, dirty/interrupt state, journal, every skipped
    // watched helper, alias overlap and changed preserved-register keys.
    static const u32 boundaries[]={SEARCH_FIND_OBJECT,SEARCH_FILTER_RETURN,BLUEWAKE_SEARCH_STAGE_NAME,
        SEARCH_FIND_RETURN,BLUEWAKE_SEARCH_STRCMP,SEARCH_RETURN,0x80328F40,0x80328F8C,0x80041558,0x800415A4};
    for(unsigned limit=0;limit<13000;limit+=7) {
        setup();layout(383,8,NAME);cpu=state();cpu.gpr[3]=NODES;assert(compare(&cpu));
        cpu=state();cpu.gpr[3]=NODES;cpu.cycle_budget=limit;compare(&cpu);
        cpu=state();cpu.gpr[3]=NODES;cpu.cycle_deadline_budget=limit;compare(&cpu);
    }
    for(unsigned trouble=0;trouble<20;++trouble) {
        setup();layout(0,8,NAME);cpu=state();cpu.gpr[3]=NODES;assert(compare(&cpu));
        cpu=state();cpu.gpr[3]=NODES;
        if(trouble<10)refused=boundaries[trouble];
        else if(trouble==10)dirty=true;
        else if(trouble==11){pending=true;cpu.msr|=PPC_MSR_EE;}
        else if(trouble==12){cause=mask=1;cpu.msr|=PPC_MSR_EE;}
        else if(trouble==13)g_mem_write_journal=journal;
        else if(trouble==14)g_ppc_guest_aliases_overlap_mem1=true;
        else if(trouble==15)cpu.exception=1;
        else if(trouble==16)observe=true;
        else if(trouble==17)cpu.gpr[6]^=0xFF;
        else if(trouble==18)cpu.xer^=0x80000000;
        else {cpu.ctr^=0xFF;cpu.cr^=0x12345678;}
        compare(&cpu);
    }
    setup();layout(0,8,NAME);bluewake_composite_native_entries_v1(false,NULL,NULL);
    unsigned long long hits=s_reuse_hits;
    for(unsigned i=0;i<4;++i){cpu=state();cpu.gpr[3]=NODES;assert(compare(&cpu));}assert(s_reuse_hits==hits);
    printf("PASS item14 full_cpu_ram=%llu declines=%llu all_table_positions=826 list_positions=10 "
           "steady_avoided_lookups=%llu steady_real_lookups=%llu total_hits=%llu checked_bytes=%llu\n",
           comparisons,declines,steady_hits,steady_lookups,s_reuse_hits,s_reuse_checked_bytes);
}
