// SPDX-License-Identifier: GPL-3.0-or-later
/* Production hooks with isolated guest RAM, real REL alias resolution and
 * actual optimized caller bodies when an own-disc translated tree exists.
 * Animation resources, actor/event execution and controller input remain
 * synthetic; this is not genuine equipped-boots/Wind-Waker qualification. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "enhancement_hooks.h"
#include "game_events.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef BLUEWAKE_ENHANCEMENT_OPTIMIZED_CALL_FIXTURE
#include "dispatch_loop.h"
#endif

enum { PLAYER=0x80010000u, BIRD=0x80018000u, STACK=0x80020000u,
    HEADER=0x80021000u, EVENTS=0x80021040u,
    PLAYER_PROFILE=0x8038FD8Cu, PLAYER_METHODS=0x8038FD68u,
    BIRD_PROFILE=0xC1F10A98u, BIRD_METHODS=0xC1F10A78u,
    GAME=0x803C4C08u, CONTROL=0x803C9DE0u, MANAGER=0x803C9ED4u,
    STAGE=0x803C9D3Cu, ROOM=0x803F6A78u, PAUSE=0x803F7097u,
    NEXT=0x803C9D54u, OVERLAP=0x803F6160u, FRAME=0x803E8140u,
    WIND_FLAGS=0x803E5460u, BOOTS_HIO=0x8035DD14u };
static CPUState cpu;
static uint8_t* ram_before;
static uint8_t rel_data[0x50];
static const char* phase;
static void w32(uint32_t address, uint32_t value) { mem_write32(&cpu,address,value); }
static void w16(uint32_t address, uint16_t value) { mem_write16(&cpu,address,value); }
static void w8(uint32_t address, uint8_t value) { mem_write8(&cpu,address,value); }
static BwEnhancementHooksStats stats(void) {
    BwEnhancementHooksStats result; bluewake_enhancement_hooks_stats(&result); return result;
}
static void profile(uint32_t actor, uint32_t prof, uint32_t methods, bool bird) {
    const uint16_t name=bird?0xC5u:0xA9u;
    w32(actor,0x09130001u); w32(actor+4u,bird?9u:7u);
    w16(actor+8u,name); w16(actor+0xEu,name);
    w32(actor+0x10u,prof); w32(actor+0xA8u,0x803726E8u);
    w32(actor+0xB8u,0x80371FF8u); w32(actor+0xC0u,0x09130003u);
    w32(actor+0xECu,methods); w8(actor+0x1BEu,bird?0:1);
    w32(prof,0xFFFFFFFDu); w16(prof+4u,bird?7:5); w16(prof+6u,0xFFFDu);
    w16(prof+8u,name); w32(prof+0xCu,0x803726E8u);
    w32(prof+0x10u,bird?0x2A8u:0x4C28u);
    w32(prof+0x1Cu,0x80371FF8u); w32(prof+0x24u,methods);
    w32(methods+8u,bird?0xC1F10934u:0x80122D30u);
    if(!bird)w32(actor+0x498u,actor+0x1F8u);
}
static void reset(bool wind, bool boots) {
    ppc_guest_alias_clear(); memset(rel_data,0,sizeof rel_data);
    assert(ppc_guest_alias_add_shared(BIRD_METHODS,sizeof rel_data,rel_data));
    memset(cpu.ram,0,cpu.ram_size); memset(cpu.gpr,0,sizeof cpu.gpr);
    memset(cpu.fpr,0,sizeof cpu.fpr); memset(cpu.ps1,0,sizeof cpu.ps1);
    cpu.pc=BLUEWAKE_ENHANCEMENT_WIND_COMMIT; cpu.exception=0; cpu.msr=PPC_MSR_FP;
    cpu.timebase=77; cpu.downcount=-7; cpu.cycle_budget=100;
    cpu.gpr[1]=STACK;
    w32(0x803F6A18u,0x09130001u); w32(0x803F69D0u,0x09130003u);
    profile(PLAYER,PLAYER_PROFILE,PLAYER_METHODS,false);
    profile(BIRD,BIRD_PROFILE,BIRD_METHODS,true);
    w32(0x803CA74Cu,PLAYER); w32(0x803CA754u,PLAYER);
    memcpy(cpu.ram+(STAGE-0x80000000u),"sea\0\0\0\0\0",8); w8(ROOM,44);
    w32(FRAME,12); w32(STACK,STACK+0x20u);
    w32(STACK+36u,0xC1F10978u);
    w8(BIRD+0x29Cu,2); w16(BIRD+0x29Eu,70); w16(BIRD+0x2A6u,0);
    w16(BIRD+0xF8u,2); w8(CONTROL+0xC2u,2);
    w32(CONTROL+0xC4u,9); w16(CONTROL+0xD8u,0);
    w32(CONTROL+0xC8u,7);
    w32(MANAGER,HEADER); w32(MANAGER+4u,EVENTS);
    w32(HEADER,0x40u); w32(HEADER+4u,1);
    memcpy(cpu.ram+(EVENTS-0x80000000u),"TACT_WINDOW2",sizeof "TACT_WINDOW2");
    w32(EVENTS+0xA4u,2);
    w16(BOOTS_HIO,19); w16(BOOTS_HIO+2u,150);
    w32(BOOTS_HIO+4u,0x3F800000u); w32(BOOTS_HIO+8u,0);
    w32(BOOTS_HIO+0xCu,0x41980000u); w32(BOOTS_HIO+0x10u,0x40A00000u);
    bluewake_enhancement_faster_wind(wind); bluewake_enhancement_faster_boots(boots);
    bluewake_game_events_attach(&cpu); bluewake_enhancement_hooks_attach(&cpu);
}
static void wind_entry(bool mirror) {
    cpu.pc=BLUEWAKE_ENHANCEMENT_WIND_COMMIT|(mirror?0x40000000u:0);
    cpu.lr=BLUEWAKE_ENHANCEMENT_WIND_RETURN|(mirror?0x40000000u:0);
    cpu.gpr[30]=BIRD; cpu.gpr[28]=PLAYER; cpu.gpr[31]=0xC1F10A10u;
}
static void wind_commit(void) {
    /* Native 8008A870 sets exactly these two fields; selection and earlier
     * movement have already completed before this call can be armed. */
    w8(WIND_FLAGS,mem_read8(&cpu,WIND_FLAGS)|1u); w8(WIND_FLAGS+1u,0xFFu);
    cpu.pc=cpu.lr;
}
static void boots_entry(bool mirror) {
    cpu.pc=BLUEWAKE_ENHANCEMENT_BOOTS_ANIMATION|(mirror?0x40000000u:0);
    cpu.lr=BLUEWAKE_ENHANCEMENT_BOOTS_RETURN|(mirror?0x40000000u:0);
    cpu.gpr[3]=PLAYER; cpu.gpr[30]=PLAYER; cpu.gpr[31]=0x29u;
    cpu.gpr[4]=0xAEu; cpu.gpr[5]=19;
    cpu.fpr[1]=cpu.ps1[1]=1.0; cpu.fpr[2]=cpu.ps1[2]=0; cpu.fpr[3]=cpu.ps1[3]=5;
    w32(STACK+20u,0x8010CE28u);
    w32(STACK,STACK+0x10u);
    w32(PLAYER+0x31D8u,0xA1u); w32(PLAYER+0x31E4u,0x801198E0u);
    w32(PLAYER+0x3570u,0x13579BDFu); /* Not set to the item until after this return. */
    w8(CONTROL+0xC2u,0); w8(GAME+0x3Cu+9u,0x29);
    w8(GAME+9u,9); w8(0x803CA7DBu,0x29);
}
static void dispatch(void) { bluewake_enhancement_hooks_dispatch(&cpu,cpu.pc); }
static void snapshot(void) { memcpy(ram_before,cpu.ram,cpu.ram_size); }
static void ram_unchanged(void) {
    if(memcmp(ram_before,cpu.ram,cpu.ram_size)!=0) {
        for(uint32_t i=0;i<cpu.ram_size;++i)if(ram_before[i]!=cpu.ram[i]) {
            fprintf(stderr,"%s: unexpected RAM byte %08X: expected %02X, got %02X\n",
                    phase,i+0x80000000u,ram_before[i],cpu.ram[i]);break;
        }
        assert(false);
    }
}
static void no_change(void) {
    const CPUState before=cpu; snapshot(); dispatch();
    assert(memcmp(&before,&cpu,sizeof cpu)==0); ram_unchanged();
}
static void test_disabled_and_independent_settings(void) {
    reset(false,false); wind_entry(false); no_change(); wind_commit(); no_change();
    boots_entry(false); no_change(); cpu.pc=cpu.lr; no_change();
    assert(!bluewake_enhancement_hooks_observes(BLUEWAKE_ENHANCEMENT_WIND_COMMIT));
    assert(!bluewake_enhancement_hooks_observes(BLUEWAKE_ENHANCEMENT_BOOTS_ANIMATION));
    assert(stats().wind_entries==0&&stats().boots_entries==0);
    bluewake_enhancement_faster_wind(true);
    assert(bluewake_enhancement_faster_wind_enabled()&&!bluewake_enhancement_faster_boots_enabled());
    bluewake_enhancement_faster_boots(true); bluewake_enhancement_faster_wind(false);
    assert(!bluewake_enhancement_faster_wind_enabled()&&bluewake_enhancement_faster_boots_enabled());
    cpu.lr=0x80110000u;
    assert(!bluewake_enhancement_hooks_observes(BLUEWAKE_ENHANCEMENT_BOOTS_ANIMATION));
    /* Irrelevant edges remain no-op even with an invalid RAM pointer. */
    uint8_t* original=cpu.ram; cpu.ram=NULL;
    bluewake_enhancement_hooks_dispatch(&cpu,0x80003100u); assert(cpu.ram==NULL);
    cpu.ram=original;
}
static void test_wind_postcommit_and_replay(void) {
    reset(true,false); wind_entry(true); snapshot();
    const CPUState before=cpu; dispatch(); dispatch();
    assert(memcmp(&before,&cpu,sizeof cpu)==0); ram_unchanged();
    assert(stats().wind_entries==1&&mem_read16(&cpu,BIRD+0x29Eu)==70);
    assert(bluewake_enhancement_hooks_observes(0xC1F10624u));
    wind_commit(); const CPUState committed=cpu; snapshot(); dispatch();
    assert(memcmp(&committed,&cpu,sizeof cpu)==0);
    assert(mem_read16(&cpu,BIRD+0x29Eu)==15&&stats().wind_shortened==1);
    /* Only the audited two timer bytes change. Native wind assignment remains. */
    ram_before[BIRD+0x29Eu-0x80000000u]=0;
    ram_before[BIRD+0x29Fu-0x80000000u]=15; ram_unchanged();
    no_change(); assert(stats().wind_shortened==1);
    assert(!bluewake_enhancement_hooks_observes(0xC1F10624u));
    reset(true,false); wind_entry(false); dispatch(); wind_commit(); dispatch();
    assert(stats().wind_shortened==1);
    reset(true,false); memcpy(cpu.ram+(EVENTS-0x80000000u),"TACT_WINDOW2_SHIP",sizeof "TACT_WINDOW2_SHIP");
    wind_entry(true); dispatch(); wind_commit(); dispatch(); assert(stats().wind_shortened==1);
}
static void test_contextual_admission(void) {
    const uint32_t entries[] = { BLUEWAKE_ENHANCEMENT_WIND_COMMIT,
                                BLUEWAKE_ENHANCEMENT_BOOTS_ANIMATION };
    const uint32_t returns[] = { BLUEWAKE_ENHANCEMENT_WIND_RETURN,
                                BLUEWAKE_ENHANCEMENT_BOOTS_RETURN };
    reset(false, false);
    for (unsigned i=0; i<2; ++i) {
        CPUState probe=cpu;
        probe.lr=returns[i];
        assert(!bluewake_enhancement_hooks_observes_context(&probe, entries[i]));
        assert(!bluewake_enhancement_hooks_observes_context(NULL, entries[i]));
    }
    for (unsigned i=0; i<2; ++i) {
        reset(i==0, i==1);
        cpu.lr=0x80004004u;
        CPUState probe=cpu;
        probe.lr=returns[i]|0x40000000u;
        probe.ram=NULL; probe.ram_size=0; /* Admission must not read RAM. */
        const CPUState saved_cpu=cpu, saved_probe=probe;
        snapshot();
        assert(bluewake_enhancement_hooks_observes_context(&probe, entries[i]));
        assert(bluewake_enhancement_hooks_observes_context(&probe, entries[i]|0x40000000u));
        assert(!bluewake_enhancement_hooks_observes(entries[i]));
        assert(!bluewake_enhancement_hooks_observes_context(NULL, entries[i]));
        assert(!bluewake_enhancement_hooks_observes_context(&probe, 0x80003100u));
        assert(memcmp(&saved_cpu,&cpu,sizeof cpu)==0);
        assert(memcmp(&saved_probe,&probe,sizeof probe)==0); ram_unchanged();
        cpu.lr=returns[i]; probe.lr=0x80004004u;
        assert(bluewake_enhancement_hooks_observes(entries[i]));
        assert(!bluewake_enhancement_hooks_observes_context(&probe, entries[i]));
        if (i==0) wind_entry(true); else boots_entry(false);
        dispatch();
        assert(bluewake_enhancement_hooks_observes_context(&probe, returns[i]));
        /* Live disable must still admit an already armed return for cleanup. */
        bluewake_enhancement_faster_wind(false); bluewake_enhancement_faster_boots(false);
        assert(!bluewake_enhancement_hooks_observes_context(&probe, entries[i]));
        assert(bluewake_enhancement_hooks_observes_context(&probe, returns[i]));
        bluewake_enhancement_hooks_reset(&cpu);
        assert(!bluewake_enhancement_hooks_observes_context(&probe, returns[i]));
        bluewake_enhancement_hooks_reset(NULL);
        probe.lr=returns[i];
        assert(!bluewake_enhancement_hooks_observes_context(&probe, entries[i]));
        assert(!bluewake_enhancement_hooks_observes_context(&probe, returns[i]));
    }
}
static void test_wind_native_tail(void) {
    /* Independent primary-source native actionMove scheduling. The enhancement
     * never changes the interactive selection, 140→70 movement, or cleanup. */
    for(unsigned enabled=0;enabled<2;++enabled) {
        reset(enabled!=0,false); wind_entry(true); dispatch(); wind_commit(); dispatch();
        unsigned moves=0, fades=0, cuts=0, restores=0, sounds=0, ticks=0;
        while(mem_read8(&cpu,BIRD+0x29Cu)==2) {
            ++ticks; assert(ticks<75);
            int16_t timer=(int16_t)mem_read16(&cpu,BIRD+0x29Eu);
            if(timer>0) {
                w16(BIRD+0x29Eu,(uint16_t)--timer);
                if(timer>60)++moves;
                if(timer<15)++fades;
                assert(timer!=70); /* The real commit has already occurred exactly once. */
            } else { ++cuts; ++restores; ++sounds; w8(BIRD+0x29Cu,1); }
            bluewake_enhancement_hooks_retrace(&cpu);
        }
        assert(cuts==1&&restores==1&&sounds==1&&fades==15);
        assert(ticks==(enabled?16u:71u)&&moves==(enabled?0u:9u));
        assert(mem_read8(&cpu,WIND_FLAGS)==1&&mem_read8(&cpu,WIND_FLAGS+1u)==0xFFu);
    }
}
static void test_wind_guards(void) {
    const struct {uint32_t address,value;unsigned width;} mutations[]={
        {BIRD,0,4},{BIRD+4,0,4},{BIRD+8,0xA9,2},{BIRD+0xE,0,2},
        {BIRD+0xB,1,1},{BIRD+0x10,PLAYER_PROFILE,4},{BIRD+0xEC,PLAYER_METHODS,4},
        {BIRD_PROFILE+0x10,0x290,4},{BIRD_METHODS+8,0xC1F10550,4},
        {BIRD+0x29C,3,1},{BIRD+0x29D,1,1},{BIRD+0x29E,71,2},
        {BIRD+0x2A6,0xFFFF,2},{CONTROL+0xC2,0,1},{CONTROL+0xC3,1,1},
        {CONTROL+0xC4,7,4},{CONTROL+0xC8,9,4},{CONTROL+0xD8,1,2},{CONTROL+0xE8,8,2},
        {BIRD+0xF8,0,2},{BIRD+0x1C8,2,4},{STACK,STACK+0x40,4},
        {EVENTS+0xA4,4,4},{HEADER+4,4097,4},
        {HEADER+4,0,4},{HEADER,0xFFFFFFF0,4},{MANAGER+4,EVENTS+4,4},
        {MANAGER,0xFFFFFFFC,4},{PAUSE,1,1},{NEXT,1,1},{OVERLAP,PLAYER,4},
        {0x803CA754,0,4},{PLAYER+0x498,0,4},{STAGE,0,1},{ROOM,255,1}
    };
    for(unsigned i=0;i<sizeof mutations/sizeof mutations[0];++i) {
        reset(true,false); wind_entry(true);
        const uint32_t a=mutations[i].address,v=mutations[i].value;
        if(mutations[i].width==1)w8(a,(uint8_t)v);
        else if(mutations[i].width==2)w16(a,(uint16_t)v); else w32(a,v);
        no_change(); assert(stats().wind_entries==0);
    }
    reset(true,false); wind_entry(true); cpu.gpr[28]=0; no_change(); assert(stats().wind_entries==0);
    reset(true,false); wind_entry(true); cpu.lr=0xC1F10628; no_change(); assert(stats().wind_entries==0);
    reset(true,false); wind_entry(true); w32(STACK+36,0xC1F10624); no_change(); assert(stats().wind_entries==0);
    reset(true,false); wind_entry(true); cpu.exception=1; no_change(); assert(stats().wind_entries==0);
    reset(true,false); wind_entry(true); dispatch(); cpu.pc=cpu.lr; no_change();
    assert(stats().wind_shortened==0); /* A return without the native flag stores is rejected. */
    reset(true,false); wind_entry(true); memcpy(cpu.ram+(EVENTS-0x80000000u),"TACT_WINDOW3",sizeof "TACT_WINDOW3");
    no_change(); assert(stats().wind_entries==0);
    reset(true,false); wind_entry(true); cpu.gpr[30]=0xFFFFFFFC; no_change(); assert(stats().wind_entries==0);
}
static void test_wind_lifetime(void) {
    for(unsigned change=0;change<12;++change) {
        reset(true,false); wind_entry(true); dispatch(); wind_commit();
        switch(change) {
        case 0:w8(STAGE+4,1);break;
        case 1:w8(ROOM,45);break;
        case 2:w32(BIRD+4,10);w32(CONTROL+0xC4,10);break;
        case 3:w32(PLAYER+4,8);break;
        case 4:w32(STACK,STACK+0x80);break;
        case 5:w32(FRAME,13);break;
        case 6:bluewake_game_events_reset(&cpu,BW_GAME_RESET_STATE_LOAD);break;
        case 7:bluewake_enhancement_hooks_reset(&cpu);break;
        case 8:for(unsigned j=0;j<5;++j)bluewake_enhancement_hooks_retrace(&cpu);break;
        case 9:bluewake_enhancement_faster_wind(false);break;
        case 10:
            assert(ppc_guest_alias_remove(BIRD_METHODS,sizeof rel_data));
            assert(ppc_guest_alias_add(BIRD_METHODS,sizeof rel_data,rel_data));break;
        case 11:cpu.gpr[1]=STACK+0x100;break;
        }
        no_change(); assert(stats().wind_shortened==0);
    }
    reset(true,false); wind_entry(true); dispatch(); cpu.gpr[1]+=0x100; dispatch();
    cpu.gpr[1]=STACK; wind_commit(); no_change(); assert(stats().wind_shortened==0);
    reset(true,false); wind_entry(true); dispatch(); wind_commit();
    uint8_t* original=cpu.ram; cpu.ram=calloc(1,cpu.ram_size); assert(cpu.ram);
    no_change(); assert(stats().wind_shortened==0); free(cpu.ram); cpu.ram=original;
}
static void test_boots_animation_replay(void) {
    reset(false,true); boots_entry(true); snapshot(); const CPUState before=cpu;
    dispatch(); CPUState expected=before; expected.fpr[1]=expected.ps1[1]=2;
    assert(memcmp(&expected,&cpu,sizeof cpu)==0); ram_unchanged();
    assert(stats().boots_scaled==1&&stats().boots_entries==1);
    dispatch(); assert(stats().boots_scaled==1&&cpu.fpr[1]==2); ram_unchanged();
    /* Native setSingleMoveAnime uses the rate then returns; it has not yet
     * stored mProcVar6 (801198C0), so that stale field must not gate the hook. */
    cpu.pc=cpu.lr; cpu.gpr[3]=1; cpu.fpr[1]=13; no_change();
    assert(stats().boots_completed==1&&!bluewake_enhancement_hooks_observes(cpu.pc));
    no_change(); assert(stats().boots_completed==1);
    reset(false,true); boots_entry(false); w32(STACK+20,0x8010D19C); dispatch();
    assert(cpu.fpr[1]==2&&stats().boots_scaled==1);
}
static void test_boots_native_frame_actions(void) {
    for(unsigned enabled=0;enabled<2;++enabled) for(unsigned initially_on=0;initially_on<2;++initially_on) {
        reset(false,enabled!=0); boots_entry(false); dispatch();
        const double rate=cpu.fpr[1];
        cpu.pc=cpu.lr;cpu.gpr[3]=1;dispatch();
        assert(stats().boots_completed==(enabled?1u:0u));
        /* J3DFrameCtrl::checkPass EMode_NONE checks [frame,frame+rate).
         * The end19 is native HIO, not an invented animation end. */
        unsigned toggles=0,vibrations=0,ticks=0; bool on=initially_on!=0;
        double frame=0;
        while(frame<18.999) {
            const double next=fmin(frame+rate,18.999);
            if(frame<=11&&11<next) {on=!on;++toggles;}
            if(frame<=19&&frame<=15&&15<next&&on)++vibrations;
            frame=next; ++ticks; assert(ticks<=20);
        }
        assert(toggles==1&&on!=(initially_on!=0));
        assert(vibrations==(initially_on?0u:1u)&&ticks==(enabled?10u:19u));
        /* No native equipment state is pre-toggled by the host hook. */
        assert(mem_read32(&cpu,PLAYER+0x29Cu)==0&&mem_read32(&cpu,PLAYER+0x3570u)==0x13579BDF);
    }
}
static void test_boots_guards_lifecycle(void) {
    for(unsigned change=0;change<24;++change) {
        reset(false,true); boots_entry(false);
        switch(change) {
        case 0:cpu.gpr[3]=BIRD;break;
        case 1:cpu.gpr[4]=0xAD;break;
        case 2:cpu.gpr[31]=0x2B;break;
        case 3:cpu.lr+=4;break;
        case 4:cpu.msr=0;break;
        case 5:w32(PLAYER+0x31D8,4);break;
        case 6:w32(PLAYER+0x31E4,0x80119864);break;
        case 7:w16(PLAYER+0x304,1);break;
        case 8:w32(PLAYER+0x314,1);break;
        case 9:w8(GAME+0x3C+9,0xFF);break;
        case 10:w8(GAME+9,0xFF);break;
        case 11:w8(0x803CA7DB,0xFF);break;
        case 12:cpu.fpr[1]=NAN;break;
        case 13:cpu.fpr[1]=cpu.ps1[1]=-1;break;
        case 14:cpu.ps1[1]=0;break;
        case 15:cpu.fpr[2]=1;break;
        case 16:cpu.gpr[5]=UINT32_MAX;break;
        case 17:cpu.fpr[3]=INFINITY;break;
        case 18:w32(BOOTS_HIO+4,0x40000000);cpu.fpr[1]=cpu.ps1[1]=2;break;
        case 19:w32(STACK+20,0x801198BC);break;
        case 20:w32(PLAYER_PROFILE+0x10,0x361C);break;
        case 21:w8(CONTROL+0xC2,2);break;
        case 22:w8(PAUSE,1);break;
        case 23:w16(BOOTS_HIO,15);cpu.gpr[5]=15;break;
        }
        no_change(); assert(stats().boots_scaled==0);
    }
    reset(false,true); boots_entry(false); dispatch();
    cpu.gpr[1]+=0x100; cpu.fpr[1]=cpu.ps1[1]=1; no_change();
    assert(stats().boots_scaled==1&&!bluewake_enhancement_hooks_observes(BLUEWAKE_ENHANCEMENT_BOOTS_RETURN));
    reset(false,true); boots_entry(false); dispatch(); bluewake_enhancement_faster_boots(false);
    no_change(); cpu.pc=cpu.lr; no_change(); assert(stats().boots_completed==0);
    reset(false,true); boots_entry(false); dispatch();
    bluewake_game_events_reset(&cpu,BW_GAME_RESET_MACHINE_RESET); cpu.pc=cpu.lr; no_change();
    assert(stats().boots_completed==0);
    reset(false,true); boots_entry(false); dispatch();
    w8(STAGE+8,1); cpu.pc=cpu.lr; no_change(); assert(stats().boots_completed==0);
}

#ifdef BLUEWAKE_ENHANCEMENT_OPTIMIZED_CALL_FIXTURE
unsigned bw_direct_depth;
bool bw_direct_enabled,bw_edge_watch_ready,bw_edge_filter_enabled;
static const bool clean=false;
static const u32 zero=0;
const bool* bw_host_sources_dirty=&clean;
const bool* bw_host_decrementer_pending=&clean;
const u32* bw_host_pi_cause=&zero;
const u32* bw_host_pi_mask=&zero;
BwHostCanSkipFn bw_host_can_skip;
void* bw_host_can_skip_user;
BwHostObservationFactsFn bw_host_observation_facts;
void* bw_host_observation_facts_user;
u32 bw_edge_watch_table[BW_EDGE_WATCH_SLOTS];
static BwChunkFn chunks[74];
BwChunkFn* const bw_chunk_fns=chunks;
static unsigned native_calls,edges;
static bool can_skip(void* user,const CPUState* context,u32 address) {
    (void)user;return !bluewake_enhancement_hooks_observes_context(context,address);
}
static bool edge(void* user,CPUState* context,u32 address) {
    (void)user;++edges;bluewake_enhancement_hooks_dispatch(context,address);return 0;
}
static void native_wind(CPUState* ctx) {
    ++native_calls; --ctx->downcount; wind_commit();
}
static double animation_rate;
static void native_boots(CPUState* ctx) {
    ++native_calls; --ctx->downcount; animation_rate=ctx->fpr[1];
    ctx->pc=ctx->lr; ctx->gpr[3]=1;
}
static void optimized_wind(CPUState* ctx) {
    --ctx->downcount;
#include "enhancement_wind_call_under_test.inc"
label_C1F10624:
    ctx->pc=0xC1F10628u;
}
static void optimized_boots(CPUState* ctx) {
    --ctx->downcount;
#include "enhancement_boots_call_under_test.inc"
label_801198BC:
    ctx->pc=BLUEWAKE_ENHANCEMENT_BOOTS_RETURN+4u;
}
static int translated(CPUState* ctx,u32 address) {
    if(address==0xC1F10620u){optimized_wind(ctx);return 1;}
    if(address==0x801198B8u){optimized_boots(ctx);return 1;}
    if((address&~0x40000000u)==BLUEWAKE_ENHANCEMENT_WIND_COMMIT){native_wind(ctx);return 1;}
    if((address&~0x40000000u)==BLUEWAKE_ENHANCEMENT_BOOTS_ANIMATION){native_boots(ctx);return 1;}
    if((address&~0x40000000u)==BLUEWAKE_ENHANCEMENT_WIND_RETURN){--ctx->downcount;ctx->pc=0xC1F10628;return 1;}
    if((address&~0x40000000u)==BLUEWAKE_ENHANCEMENT_BOOTS_RETURN){--ctx->downcount;ctx->pc=address+4u;return 1;}
    return 0;
}
static void setup_optimized(bool boots,bool enabled) {
    reset(!boots&&enabled,boots&&enabled);
    if(boots)boots_entry(false);else wind_entry(true);
    cpu.pc=boots?0x801198B8u:0xC1F10620u; cpu.downcount=0;cpu.cycle_budget=100;
    bw_direct_enabled=true;bw_direct_depth=0;bw_host_can_skip=can_skip;
    bw_edge_watch_ready=true;bw_edge_filter_enabled=true;
    memset(bw_edge_watch_table,0,sizeof bw_edge_watch_table);
    chunks[34]=native_wind;chunks[73]=native_boots;native_calls=edges=0;animation_rate=0;
}
static void test_actual_optimized_boundaries(void) {
    for(unsigned boots=0;boots<2;++boots) {
        setup_optimized(boots!=0,false);
        assert(bluewake_chassis_dispatch_loop(&cpu,cpu.pc,translated,edge,NULL));
        assert(native_calls==1&&edges==0); /* Disabled direct calls remain eligible. */
        assert(boots?animation_rate==1:mem_read16(&cpu,BIRD+0x29Eu)==70);
        setup_optimized(boots!=0,true);
        assert(bluewake_chassis_dispatch_loop(&cpu,cpu.pc,translated,edge,NULL));
        assert(native_calls==1&&edges==2);
        assert(boots?animation_rate==2:mem_read16(&cpu,BIRD+0x29Eu)==15);
        setup_optimized(boots!=0,true);cpu.cycle_budget=1;
        assert(bluewake_chassis_dispatch_loop(&cpu,cpu.pc,translated,edge,NULL));
        assert(native_calls==0); /* Budget yielded exactly at the native entry. */
        for(unsigned turn=0;turn<4&&native_calls<1;++turn) {
            cpu.downcount=0;
            dispatch(); dispatch(); /* Main first-PC plus edge replay. */
            assert(bluewake_chassis_dispatch_loop(&cpu,cpu.pc,translated,edge,NULL));
        }
        assert(native_calls==1);
        cpu.downcount=0;dispatch();dispatch();
        assert(boots?stats().boots_completed==1:stats().wind_shortened==1);
        assert(boots?animation_rate==2:mem_read16(&cpu,BIRD+0x29Eu)==15);
    }
    puts("enhancement_hooks_test: real optimized slow/fast callers, dynamic watches and first-PC resumes passed");
}
#endif
int main(void) {
    cpu.ram_size=0x01800000u;cpu.ram=calloc(1,cpu.ram_size);ram_before=malloc(cpu.ram_size);
    assert(cpu.ram&&ram_before);
    phase="disabled";test_disabled_and_independent_settings();
    phase="contextual admission";test_contextual_admission();
    phase="wind replay";test_wind_postcommit_and_replay();
    phase="wind tail";test_wind_native_tail();
    phase="wind guards";test_wind_guards();
    phase="wind lifetime";test_wind_lifetime();
    phase="boots replay";test_boots_animation_replay();
    phase="boots actions";test_boots_native_frame_actions();
    phase="boots guards";test_boots_guards_lifecycle();
#ifdef BLUEWAKE_ENHANCEMENT_OPTIMIZED_CALL_FIXTURE
    phase="optimized callers";test_actual_optimized_boundaries();
#endif
    bluewake_enhancement_hooks_reset(NULL);bluewake_game_events_reset(NULL,BW_GAME_RESET_MACHINE_RESET);
    ppc_guest_alias_clear();free(ram_before);free(cpu.ram);
    puts("enhancement_hooks_test: postcommit wind tail, native boots frame actions, disabled no-op and lifetime guards passed");
    return 0;
}
