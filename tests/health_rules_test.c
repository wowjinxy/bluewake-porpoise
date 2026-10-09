// SPDX-License-Identifier: GPL-3.0-or-later
#include "health_rules.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef BLUEWAKE_HEALTH_OPTIMIZED_CALL_FIXTURE
#include "dispatch_loop.h"
#ifdef BW_HEALTH_TEST_HOST
#include "health_host.h"
#ifdef BLUEWAKE_HEALTH_MAIN_SKIP_FIXTURE
#include "health_host_main_skip_fixture.h"
#endif
static void prepare_host_optimized(void);
#endif
#endif

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"health_rules:%d: %s\n",__LINE__,#x); abort(); } } while (0)
static CPUState cpu;
static BwHealthRulesRuntime runtime;
static BwHealthRulesLifetime life;
static uint8_t* before_ram;
static const uint32_t PLAYER=0x80600000u, STACK=0x817FF000u;
static const uint32_t INFO=0x803C4C08u, COUNT=0x803CA764u, MAXCOUNT=0x803CA77Eu;
static const uint32_t STAGE=0x803C9D3Cu, PROFILE=0x8038FD8Cu, METHODS=0x8038FD68u;
static const uint32_t BASE=0x803F6A18u, ACTOR=0x803F69D0u, TABLE=0x803888C8u;
static uint32_t bits(float f) { uint32_t v; memcpy(&v,&f,4); return v; }
static void w8(uint32_t a,uint8_t v) { cpu.ram[a-0x80000000u]=v; }
static void w16(uint32_t a,uint16_t v) { w8(a,(uint8_t)(v>>8));w8(a+1,(uint8_t)v); }
static void w32(uint32_t a,uint32_t v) { w16(a,(uint16_t)(v>>16));w16(a+2,(uint16_t)v); }
static void wf(uint32_t a,float v) { w32(a,bits(v)); }
static float rf(uint32_t a) { const uint32_t v=mem_read32(&cpu,a);float f;memcpy(&f,&v,4);return f; }
static void configure(unsigned damage,unsigned healing) {
    BwHealthRulesConfig config={(uint16_t)damage,(uint16_t)healing};CHECK(bw_health_rules_configure(&runtime,&config));
}
static void setup(unsigned damage,unsigned healing) {
    uint8_t* ram=cpu.ram;memset(&cpu,0,sizeof cpu);cpu.ram=ram;cpu.ram_size=BW_HEALTH_RULES_HOST_RAM_SIZE;
    memset(cpu.ram,0,cpu.ram_size);bw_health_rules_init(&runtime);life=(BwHealthRulesLifetime){1,1,1};
    configure(damage,healing);CHECK(bw_health_rules_attach(&runtime,&cpu,&life,BW_HEALTH_RULES_ABI_GZLE01));
    w32(BASE,0x09130001u);w32(ACTOR,0x09130005u);
    w32(PLAYER,0x09130001u);w32(PLAYER+4u,42);w16(PLAYER+8u,0xA9);w16(PLAYER+0xEu,0xA9);
    w32(PLAYER+0xC0u,0x09130005u);w32(PLAYER+0x10u,PROFILE);w32(PLAYER+0xECu,METHODS);
    w8(PLAYER+0x1BEu,1);w32(PLAYER+0x498u,PLAYER+0x1F8u);
    w16(PROFILE+8u,0xA9);w32(PROFILE+0x10u,0x4C28);w32(PROFILE+0x24u,METHODS);w32(METHODS+8u,0x80122D30u);
    w32(0x803CA74Cu,PLAYER);w32(0x803CA754u,PLAYER);memcpy(cpu.ram+(STAGE-0x80000000u),"sea",4);
    w8(0x803F6A78u,44);w16(INFO,12);w16(INFO+2u,12);w32(STACK,STACK+0x40u);w32(STACK+4u,0x80001234u);
    w32(TABLE,BW_HEALTH_RULES_HEART);w32(TABLE+0x16u*4u,BW_HEALTH_RULES_FAIRY);
    cpu.gpr[1]=STACK;cpu.gpr[3]=PLAYER;cpu.msr=PPC_MSR_FP;
}
static void enter(uint32_t pc,uint32_t lr,float amount) {
    cpu.pc=pc;cpu.lr=lr;cpu.gpr[3]=PLAYER;cpu.fpr[1]=amount;
    cpu.ctr=pc;cpu.gpr[12]=pc;bw_health_rules_dispatch(&runtime,&cpu,pc,&life);
}
/* Independent primary-source behavioral fixtures; NOT real game execution.
 * setDamagePoint source4984 preserves native result/shield/buff semantics.
 * mItemLifeCount is a native single-precision addition. */
static void native_damage(CPUState* context,float amount) {
    const uint32_t p=context->gpr[3];
    const uint32_t flags=mem_read32(context,p+0x2A0u);
    if ((flags&1u)!=0||mem_read16(context,p+0x354Eu)!=0) context->gpr[3]=0;
    else {
        mem_write32(context,COUNT,bits((float)((double)rf(COUNT)+amount)));
        if (amount<0) {
            uint32_t after=flags&~0x8000u;
            if (mem_read8(context,INFO+0xEu)!=0x3Eu) after&=~0x200000u;
            mem_write32(context,p+0x2A0u,after);
        }
        context->gpr[3]=1;
    }
    w32(context->gpr[1]+4u,context->lr);context->pc=context->lr;
}
static void finish(float delta,uint32_t result,bool framed) {
    wf(COUNT,(float)((double)rf(COUNT)+delta));cpu.gpr[3]=result;cpu.pc=cpu.lr;
    if(framed)w32(cpu.gpr[1]+4u,cpu.lr);
    bw_health_rules_dispatch(&runtime,&cpu,cpu.pc,&life);
}
static void no_extra_writes(uint32_t pc) {
    memcpy(before_ram,cpu.ram,cpu.ram_size);CPUState snapshot=cpu;
    bw_health_rules_dispatch(&runtime,&cpu,pc,&life);
    CHECK(memcmp(before_ram,cpu.ram,cpu.ram_size)==0);CHECK(memcmp(&snapshot,&cpu,sizeof cpu)==0);
}
static void pure_rules(void) {
    float out=123;
    CHECK(bw_health_rules_adjust(4,3,512,&out)&&out==2);
    CHECK(bw_health_rules_adjust(-3,1,128,&out)&&out==-1); /* Prior pending damage retained. */
    CHECK(bw_health_rules_adjust(4,3,0,&out)&&out==4);
    CHECK(bw_health_rules_adjust(0,-1,64,&out)&&out==-0.25f);
    CHECK(bw_health_rules_adjust(-0.0f,-0.0f,256,&out)&&bits(out)==0x80000000u);
    for(unsigned rate=0;rate<=4096;rate+=17) {
        for(int a=-80;a<=80;++a) {
            CHECK(bw_health_rules_adjust(3,(float)(a+3),(uint16_t)rate,&out));
            CHECK(out==(float)(3.0+(double)a*rate/256.0));
        }
    }
    const float bad[]={NAN,INFINITY,-INFINITY,257,-257};
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];++i) {
        out=123;CHECK(!bw_health_rules_adjust(bad[i],0,256,&out)&&out==123);
        CHECK(!bw_health_rules_adjust(0,bad[i],256,&out)&&out==123);
    }
    CHECK(!bw_health_rules_adjust(0,81,256,&out));CHECK(!bw_health_rules_adjust(0,0,4097,&out));
    CHECK(!bw_health_rules_adjust(0,0,256,NULL));
}
static void disabled_and_admission(void) {
    setup(256,256);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);
    CHECK(runtime.stats.damage_entries==0);CHECK(!bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_DAMAGE));
    no_extra_writes(BW_HEALTH_RULES_HEART);configure(512,256);
    cpu.lr=0x80110984u;CHECK(!bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_DAMAGE)); /* Intrachunk. */
    cpu.lr=0x8012086Cu;CHECK(!bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_DAMAGE)); /* Restart. */
    cpu.lr=0x80128B18u;CHECK(!bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_DAMAGE)); /* Script throw. */
    cpu.lr=0xC0604020u;CHECK(!bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_DAMAGE)); /* Unknown REL. */
    cpu.lr=0x801165F4u;CHECK(bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_DAMAGE));
    CPUState foreign=cpu;foreign.lr=0x80000004u;CHECK(!bw_health_rules_observes(&runtime,&foreign,BW_HEALTH_RULES_DAMAGE));
    const uint32_t original=cpu.lr;cpu.lr=0x80000004u;CHECK(!bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_DAMAGE));cpu.lr=original;
    CHECK(bw_health_rules_observes(&runtime,&cpu,0xC011029Cu));
    CHECK(!bw_health_rules_observes(&runtime,&cpu,0xC111029Cu));
    configure(256,512);CHECK(!bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_DAMAGE));
    cpu.lr=BW_HEALTH_RULES_ITEM_RETURN;CHECK(bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_HEART));
    BwHealthRulesConfig invalid={4097,0};CHECK(!bw_health_rules_configure(&runtime,&invalid));CHECK(runtime.config.healing_q8==512);
}
static void side_effects_clamps_and_replay(void) {
    setup(512,256);wf(COUNT,3);w32(PLAYER+0x2A0u,0x208000u);
    enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-2);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-2);
    CHECK(runtime.stats.damage_entries==1);
    native_damage(&cpu,-2);const CPUState snapshot=cpu;
    memcpy(before_ram,cpu.ram,cpu.ram_size);bw_health_rules_dispatch(&runtime,&cpu,cpu.pc,&life);
    CHECK(rf(COUNT)==-1);CHECK(mem_read32(&cpu,PLAYER+0x2A0u)==0);CHECK(cpu.gpr[3]==1);
    CHECK(memcmp(&snapshot,&cpu,sizeof cpu)==0);wf(COUNT,1); /* Restore only changed counter for comparison. */
    CHECK(memcmp(before_ram,cpu.ram,cpu.ram_size)==0);wf(COUNT,-1);no_extra_writes(cpu.pc);
    CHECK(runtime.stats.adjusted==1&&runtime.stats.completed==1);
    setup(0,256);w32(PLAYER+0x2A0u,0x208000u);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);
    native_damage(&cpu,-1);bw_health_rules_dispatch(&runtime,&cpu,cpu.pc,&life);
    CHECK(rf(COUNT)==0&&mem_read32(&cpu,PLAYER+0x2A0u)==0&&cpu.gpr[3]==1);
    setup(512,256);w32(PLAYER+0x2A0u,0x208001u);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);
    native_damage(&cpu,-1);no_extra_writes(cpu.pc);CHECK(rf(COUNT)==0&&cpu.gpr[3]==0&&runtime.stats.adjusted==0);
    CHECK(mem_read32(&cpu,PLAYER+0x2A0u)==0x208001u);
    setup(512,256);w16(INFO+2u,1);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);finish(-1,1,true);
    CHECK(rf(COUNT)==-2);CHECK(mem_read16(&cpu,INFO+2u)==1); /* Native meter owns actual death. */
    setup(256,4096);enter(BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_ITEM_RETURN,0);finish(4,0,false);
    CHECK(rf(COUNT)==64&&mem_read16(&cpu,INFO+2u)==12&&mem_read16(&cpu,INFO)==12); /* Native upper clamp later. */
    setup(64,128);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);finish(-1,1,true);CHECK(rf(COUNT)==-0.25f);
    enter(BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_ITEM_RETURN,0);finish(4,0,false);CHECK(rf(COUNT)==1.75f);
}
static void aggregate_and_healing(void) {
    setup(512,128);wf(COUNT,4);enter(BW_HEALTH_RULES_COLLISION,BW_HEALTH_RULES_COLLISION_RETURN,0);
    CHECK(runtime.stats.damage_entries==1);
    cpu.gpr[1]=STACK-0x50u;cpu.lr=0x80117984u;cpu.gpr[3]=PLAYER;
    CHECK(!bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_DAMAGE));
    bw_health_rules_dispatch(&runtime,&cpu,BW_HEALTH_RULES_DAMAGE,&life);
    CHECK(runtime.stats.damage_entries==1&&runtime.stats.nested_damage_ignored==1);
    cpu.gpr[1]=STACK;cpu.lr=BW_HEALTH_RULES_COLLISION_RETURN;finish(-3,0,true);
    CHECK(rf(COUNT)==-2&&runtime.stats.completed==1&&runtime.stats.adjusted==1);
    no_extra_writes(BW_HEALTH_RULES_COLLISION_RETURN);
    setup(512,128);enter(BW_HEALTH_RULES_FAIRY,BW_HEALTH_RULES_ITEM_RETURN,0);finish(40,0,false);
    CHECK(rf(COUNT)==20&&runtime.stats.healing_entries==1&&runtime.stats.completed==1);
    enter(BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_ITEM_RETURN,0);finish(4,0,false);CHECK(rf(COUNT)==22);
    setup(512,0);enter(BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_ITEM_RETURN,0);finish(4,0,false);CHECK(rf(COUNT)==0);
    /* Excluded maximum-heart progression function is never observed. */
    cpu.pc=0x800C2F40u;w16(MAXCOUNT,4);wf(COUNT,12);no_extra_writes(cpu.pc);CHECK(mem_read16(&cpu,MAXCOUNT)==4);
    /* Only the native life counter may change; unrelated inventory and magic remain native. */
    setup(512,128);w8(INFO+0x14u,12);w8(INFO+0x3Cu+7u,0x21);enter(BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_ITEM_RETURN,0);
    finish(4,0,false);CHECK(mem_read8(&cpu,INFO+0x14u)==12&&mem_read8(&cpu,INFO+0x3Cu+7u)==0x21);
}
/* Independent adaptation of dMeter_LifeMove's initial native block (primary
 * source1405..1444). Deliberately does NOT call bw_health_rules_adjust: the
 * meter's native truncation/clamps/progression consume the pending result. */
static void native_meter(bool force) {
    const int increment=(int16_t)mem_read16(&cpu,MAXCOUNT);
    const float pending=rf(COUNT);
    if(increment!=0||(int16_t)pending!=0||force) {
        int max_hp=(int)mem_read16(&cpu,INFO)+increment;
        int hp=increment>0?(max_hp/4)*4:(int)mem_read16(&cpu,INFO+2u)+(int)pending;
        if(increment!=0) {
            if(max_hp>80)max_hp=80;else if(max_hp<0)max_hp=0;
            w16(INFO,(uint16_t)max_hp);w16(MAXCOUNT,0);
        }
        if(hp>(max_hp/4)*4)hp=(max_hp/4)*4;else if(hp<0)hp=0;
        w16(INFO+2u,(uint8_t)hp);wf(COUNT,0);
    }
}
static unsigned native_death_route(bool has_fairy) {
    /* changeDeadProc primary5540: zero life is not confirmed death when the
     * native fairy route is available. Rules never intercept either path. */
    if(mem_read16(&cpu,INFO+2u)!=0)return 0;
    return has_fairy?1u:2u;
}
static void native_meter_and_death(void) {
    setup(512,128);w16(INFO+2u,1);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);finish(-1,1,true);
    native_meter(false);CHECK(mem_read16(&cpu,INFO+2u)==0&&rf(COUNT)==0);
    CHECK(native_death_route(false)==2&&native_death_route(true)==1);
    /* Actual zero-life fairy revival and its full native heal remain outside
     * pickup scaling, even when configured ordinary healing is disabled. */
    configure(512,0);enter(BW_HEALTH_RULES_FAIRY,BW_HEALTH_RULES_ITEM_RETURN,0);finish(40,0,false);
    CHECK(rf(COUNT)==40);native_meter(false);CHECK(mem_read16(&cpu,INFO+2u)==12);
    setup(512,4096);w16(INFO+2u,8);enter(BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_ITEM_RETURN,0);finish(4,0,false);
    native_meter(false);CHECK(mem_read16(&cpu,INFO+2u)==12&&mem_read16(&cpu,INFO)==12&&rf(COUNT)==0);
    setup(64,128);
    for(unsigned i=0;i<4;++i) {
        enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);finish(-1,1,true);native_meter(false);
        CHECK(mem_read16(&cpu,INFO+2u)==(i==3?11:12));
        CHECK(rf(COUNT)==(i==3?0:-0.25f*(float)(i+1)));
    }
    /* Heart-container and heart-piece updates remain full native progression
     * and their native refill, regardless of both configured rates. */
    for(unsigned increment=1;increment<=4;increment+=3) {
        setup(0,0);w16(INFO+2u,1);w16(MAXCOUNT,(uint16_t)increment);wf(COUNT,12);
        no_extra_writes(0x800C2F40u);native_meter(false);
        CHECK(mem_read16(&cpu,INFO)==12+increment&&mem_read16(&cpu,INFO+2u)==(12+increment)/4*4);
    }
    setup(0,0);w16(INFO,80);w16(INFO+2u,1);w16(MAXCOUNT,4);wf(COUNT,80);
    native_meter(false);CHECK(mem_read16(&cpu,INFO)==80&&mem_read16(&cpu,INFO+2u)==80);
    /* Hard resets/recollection/card-load direct setLife stores have no hook;
     * neither healing nor damage rate rewrites them. */
    setup(512,0);w16(INFO+2u,12);no_extra_writes(0x8018E800u);CHECK(mem_read16(&cpu,INFO+2u)==12);
    setup(512,256);w8(INFO+0xEu,0x3E);w32(PLAYER+0x2A0u,0x208000u);
    enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);native_damage(&cpu,-1);
    bw_health_rules_dispatch(&runtime,&cpu,cpu.pc,&life);
    CHECK(mem_read32(&cpu,PLAYER+0x2A0u)==0x200000u); /* Final native sword retains its separate flag. */
}
static void guard_cases(void) {
    const struct {uint32_t address;unsigned bytes;uint32_t value;} mutations[]={
        {0x803F7097u,1,1},{0x803C9D54u,1,1},{0x803F6160u,4,1},{0x803C9EA2u,1,2},
        {0x803CA8C8u,1,1},{0x803CA801u,1,1},{0x803CA8E2u,1,1},{MAXCOUNT,2,1},
        {PLAYER+0x304u,2,1},{PLAYER+0x314u,4,1},{PLAYER+4u,4,0},{PLAYER+8u,2,0},
        {PLAYER+0xECu,4,0},{PLAYER+0x10u,4,0},{PLAYER+0x1C8u,4,2},{PLAYER+0x498u,4,0},
        {INFO+2u,2,0},{INFO,2,81},{INFO+2u,2,13},{0x803CA754u,4,0},
        {METHODS+8u,4,0},{PROFILE+0x10u,4,1},{0x803F6A78u,1,0xFF}
    };
    for(unsigned at_return=0;at_return<2;++at_return) for(unsigned i=0;i<sizeof mutations/sizeof mutations[0];++i) {
        setup(512,512);if(at_return)enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);
        if(mutations[i].bytes==1)w8(mutations[i].address,(uint8_t)mutations[i].value);
        else if(mutations[i].bytes==2)w16(mutations[i].address,(uint16_t)mutations[i].value);
        else w32(mutations[i].address,mutations[i].value);
        if(!at_return)enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);
        finish(-1,1,true);CHECK(rf(COUNT)==-1&&runtime.stats.adjusted==0);
    }
    setup(512,512);memcpy(cpu.ram+(STAGE-0x80000000u),"Xboss0",7);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);
    finish(-1,1,true);CHECK(rf(COUNT)==-1);
    setup(512,512);w8(STAGE,'?');enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);finish(-1,1,true);CHECK(rf(COUNT)==-1);
    setup(512,512);cpu.fpscr=1;enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);finish(-1,1,true);CHECK(rf(COUNT)==-1);
    setup(512,512);cpu.gpr[1]=STACK+4;enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);finish(-1,1,true);CHECK(rf(COUNT)==-1);
    setup(512,512);cpu.exception=PPC_EXC_PROGRAM;enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);finish(-1,1,true);CHECK(rf(COUNT)==-1);
    setup(512,512);w32(STACK,0);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);finish(-1,1,true);CHECK(rf(COUNT)==-1);
    setup(512,512);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,1);finish(1,1,true);CHECK(rf(COUNT)==1);
    setup(512,512);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,NAN);finish(-1,1,true);CHECK(rf(COUNT)==-1);
    setup(512,512);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);finish(-2,1,true);CHECK(rf(COUNT)==-2);
    setup(512,512);enter(BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_ITEM_RETURN,0);finish(5,0,false);CHECK(rf(COUNT)==5);
    setup(512,512);w32(TABLE,0x800C2F40u);enter(BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_ITEM_RETURN,0);finish(4,0,false);CHECK(rf(COUNT)==4);
    setup(512,512);enter(BW_HEALTH_RULES_COLLISION,BW_HEALTH_RULES_COLLISION_RETURN,0);finish(4,1,true);CHECK(rf(COUNT)==4);
}
static void lifecycle_and_stack(void) {
    for(unsigned change=0;change<7;++change) {
        setup(512,512);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);
        if(change==0)++life.epoch; if(change==1)++life.scene_generation;
        if(change==2)life.tick+=5; if(change==3)bw_health_rules_reset(&runtime,&life);
        if(change==4)configure(1024,512);if(change==5)w32(PLAYER+4u,43);
        if(change==6)w8(STAGE+8u,1);
        finish(-1,1,true);CHECK(rf(COUNT)==-1&&runtime.stats.adjusted==0);
    }
    setup(512,512);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);cpu.gpr[1]-=0x10u;finish(-1,1,true);
    CHECK(rf(COUNT)==-1&&runtime.stats.completed==0);cpu.gpr[1]=STACK;life.tick+=5;
    bw_health_rules_retrace(&runtime,&cpu,&life);CHECK(!bw_health_rules_observes(&runtime,&cpu,0x801165F4u));
    setup(512,512);enter(BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_ITEM_RETURN,0);w32(STACK+4u,0x8000FA00u);
    finish(4,0,false);CHECK(rf(COUNT)==4);
    setup(512,512);enter(BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_ITEM_RETURN,0);uint8_t* original=cpu.ram;
    cpu.ram=before_ram;memset(cpu.ram,0,cpu.ram_size);bw_health_rules_dispatch(&runtime,&cpu,BW_HEALTH_RULES_ITEM_RETURN,&life);
    CHECK(runtime.cpu==NULL);cpu.ram=original;
    setup(512,512);for(unsigned i=0;i<9;++i) {
        cpu.gpr[1]=STACK-i*0x40u;w32(cpu.gpr[1],cpu.gpr[1]+0x40u);w32(cpu.gpr[1]+4u,0x8000AB00u);
        enter(BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_ITEM_RETURN,0);
    }
    CHECK(runtime.stats.healing_entries==8&&runtime.stats.pending_overflow==1);
    bw_health_rules_detach(&runtime);CHECK(!bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_HEART));
    setup(512,512);uint8_t aliased[4]={0};
    CHECK(ppc_guest_alias_add_shared(COUNT,4u,aliased));
    enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);finish(-1,1,true);
    CHECK(runtime.stats.adjusted==0&&aliased[0]==0&&aliased[1]==0&&aliased[2]==0&&aliased[3]==0);
    ppc_guest_alias_clear();
    setup(512,512);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);
    CHECK(ppc_guest_alias_add_shared(0x81590000u,4u,aliased));finish(-1,1,true);
    CHECK(runtime.stats.adjusted==0);ppc_guest_alias_clear();
}

static void host_ram_contract(void) {
    setup(512,512);
    const uint32_t invalid_sizes[]={BW_HEALTH_RULES_MEM1_SIZE-4u,BW_HEALTH_RULES_MEM1_SIZE,
        BW_HEALTH_RULES_HOST_RAM_SIZE-4u,BW_HEALTH_RULES_HOST_RAM_SIZE+4u,0x04000000u};
    for(unsigned i=0;i<sizeof invalid_sizes/sizeof invalid_sizes[0];++i) {
        cpu.ram_size=invalid_sizes[i];
        CHECK(!bw_health_rules_attach(&runtime,&cpu,&life,BW_HEALTH_RULES_ABI_GZLE01));
        CHECK(runtime.cpu==NULL&&!bw_health_rules_observes(&runtime,&cpu,BW_HEALTH_RULES_HEART));
    }
    cpu.ram_size=BW_HEALTH_RULES_HOST_RAM_SIZE;
    CHECK(bw_health_rules_attach(&runtime,&cpu,&life,BW_HEALTH_RULES_ABI_GZLE01));
    /* A convincing actor clone in the REL extension cannot authorize health
     * work. The larger allocation does not widen native actor/field bounds. */
    const uint32_t extended=0x81804000u;
    memcpy(cpu.ram+(extended-0x80000000u),cpu.ram+(PLAYER-0x80000000u),0x4C28u);
    w32(extended+0x498u,extended+0x1F8u);
    w32(0x803CA74Cu,extended);w32(0x803CA754u,extended);
    cpu.gpr[3]=extended;cpu.pc=BW_HEALTH_RULES_DAMAGE;cpu.lr=0x801165F4u;cpu.fpr[1]=-1;
    bw_health_rules_dispatch(&runtime,&cpu,cpu.pc,&life);
    CHECK(runtime.stats.damage_entries==0&&runtime.stats.rejected==1);
    CHECK(rf(COUNT)==0);no_extra_writes(cpu.lr);
    setup(512,512);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);
    cpu.ram_size=BW_HEALTH_RULES_MEM1_SIZE;
    bw_health_rules_dispatch(&runtime,&cpu,cpu.lr,&life);
    CHECK(runtime.cpu==NULL&&runtime.stats.adjusted==0);
    cpu.ram_size=BW_HEALTH_RULES_HOST_RAM_SIZE;
}

static uint64_t external_reads;
static u64 external_probe(CPUState* c,u32 a,u8 bytes){(void)c;(void)a;(void)bytes;++external_reads;return 0;}
static void byte_alias_guards(void) {
    const uint32_t fields[]={INFO+1,PLAYER+1,PLAYER+0xC1,PROFILE+0x11,METHODS+9,STACK+5,COUNT+1};
    for(unsigned i=0;i<sizeof fields/sizeof fields[0];++i){
        setup(512,512);cpu.external_read=external_probe;external_reads=0;
        uint8_t byte=cpu.ram[fields[i]-0x80000000u];CHECK(ppc_guest_alias_add_shared(fields[i],1,&byte));
        cpu.pc=BW_HEALTH_RULES_DAMAGE;cpu.lr=0x801165F4u;cpu.fpr[1]=-1;
        no_extra_writes(cpu.pc);CHECK(runtime.stats.damage_entries==0&&runtime.stats.adjusted==0&&external_reads==0);
        ppc_guest_alias_clear();
    }
    setup(512,512);uint8_t pause=1;CHECK(ppc_guest_alias_add_shared(0x803F7097u,1,&pause));
    cpu.pc=BW_HEALTH_RULES_DAMAGE;cpu.lr=0x801165F4u;cpu.fpr[1]=-1;no_extra_writes(cpu.pc);
    CHECK(runtime.stats.damage_entries==0);ppc_guest_alias_clear();
    setup(512,512);enter(BW_HEALTH_RULES_DAMAGE,0x801165F4u,-1);
    const CPUState before=cpu;life.tick++;bw_health_rules_retrace(&runtime,&cpu,&life);
    CHECK(memcmp(&before,&cpu,sizeof cpu)==0&&bw_health_rules_observes(&runtime,&cpu,cpu.lr));
    finish(-1,1,true);CHECK(rf(COUNT)==-2&&runtime.stats.adjusted==1);
}

#ifdef BLUEWAKE_HEALTH_OPTIMIZED_CALL_FIXTURE
unsigned bw_direct_depth;
bool bw_direct_enabled,bw_edge_watch_ready,bw_edge_filter_enabled;
static const bool clean=false;static const u32 zero=0;
const bool* bw_host_sources_dirty=&clean;const bool* bw_host_decrementer_pending=&clean;
const u32* bw_host_pi_cause=&zero;const u32* bw_host_pi_mask=&zero;
BwHostCanSkipFn bw_host_can_skip;void* bw_host_can_skip_user;
/* Historical optimized-call fixtures use the complete legacy host predicate. */
BwHostObservationFactsFn bw_host_observation_facts;void* bw_host_observation_facts_user;
u32 bw_edge_watch_table[BW_EDGE_WATCH_SLOTS];
static BwChunkFn chunks[68];BwChunkFn* const bw_chunk_fns=chunks;
static unsigned native_calls,entry_edges,return_edges,route;
static bool can_skip(void* user,const CPUState* context,u32 address) {
    (void)user;
#ifdef BW_HEALTH_TEST_HOST
#ifdef BLUEWAKE_HEALTH_MAIN_SKIP_FIXTURE
    return host_can_skip_observation(user,context,address);
#else
    return !bw_health_host_observes(context,address);
#endif
#else
    return !bw_health_rules_observes(&runtime,context,address);
#endif
}
static bool edge(void* user,CPUState* context,u32 address) {
    (void)user;
    if(address==BW_HEALTH_RULES_DAMAGE||address==BW_HEALTH_RULES_COLLISION||address==BW_HEALTH_RULES_HEART)++entry_edges;
    else ++return_edges;
#ifdef BW_HEALTH_TEST_HOST
    bw_health_host_dispatch(context,address,false);bw_health_host_stats(&runtime.stats);
#else
    bw_health_rules_dispatch(&runtime,context,address,&life);
#endif
    return false;
}
static void native_result(CPUState* context) {
    ++native_calls;--context->downcount;
    if(route==0)native_damage(context,-1);
    else if(route==1) {mem_write32(context,COUNT,bits((float)((double)rf(COUNT)-3.0)));w32(context->gpr[1]+4u,context->lr);context->pc=context->lr;context->gpr[3]=0;}
    else {mem_write32(context,COUNT,bits((float)((double)rf(COUNT)+4.0)));context->pc=context->lr;}
}
bool bw_call_translated(CPUState* context,u32 target) {
    CHECK(target==BW_HEALTH_RULES_HEART);native_result(context);return true;
}
static void actual_damage_caller_0(CPUState* ctx) {
    --ctx->downcount;
#include "health_damage_call_0.inc"
label_801165F4: ctx->pc=0x801165F8u;
}
static void actual_damage_caller_1(CPUState* ctx) {
    --ctx->downcount;
#include "health_damage_call_1.inc"
label_80116644: ctx->pc=0x80116648u;
}
static void actual_damage_caller_2(CPUState* ctx) {
    --ctx->downcount;
#include "health_damage_call_2.inc"
label_80117984: ctx->pc=0x80117988u;
}
static void actual_damage_caller_3(CPUState* ctx) {
    --ctx->downcount;
#include "health_damage_call_3.inc"
label_80118BAC: ctx->pc=0x80118BB0u;
}
static void actual_damage_caller_4(CPUState* ctx) {
    --ctx->downcount;
#include "health_damage_call_4.inc"
label_8013F744: ctx->pc=0x8013F748u;
}
static void actual_damage_caller_5(CPUState* ctx) {
    --ctx->downcount;
#include "health_damage_call_5.inc"
label_8013F764: ctx->pc=0x8013F768u;
}
static void actual_damage_caller_6(CPUState* ctx) {
    --ctx->downcount;
#include "health_damage_call_6.inc"
label_8015967C: ctx->pc=0x80159680u;
}
static void actual_damage_caller_7(CPUState* ctx) {
    --ctx->downcount;
#include "health_damage_call_7.inc"
label_801596A0: ctx->pc=0x801596A4u;
}
static void actual_collision_caller(CPUState* ctx) {
    --ctx->downcount;
#include "health_collision_call_under_test.inc"
label_80121F58: ctx->pc=0x80121F5Cu;
}
static void actual_heart_caller(CPUState* ctx) {
    --ctx->downcount;
#include "health_heart_call_under_test.inc"
label_800C2E20: ctx->pc=0x800C2E24u;
}
static int translated(CPUState* context,uint32_t address) {
    if(address==0x801165F0u){actual_damage_caller_0(context);return 1;}
if(address==0x80116640u){actual_damage_caller_1(context);return 1;}
if(address==0x80117980u){actual_damage_caller_2(context);return 1;}
if(address==0x80118BA8u){actual_damage_caller_3(context);return 1;}
if(address==0x8013F740u){actual_damage_caller_4(context);return 1;}
if(address==0x8013F760u){actual_damage_caller_5(context);return 1;}
if(address==0x80159678u){actual_damage_caller_6(context);return 1;}
if(address==0x8015969Cu){actual_damage_caller_7(context);return 1;}
    if(address==0x80121F54u){actual_collision_caller(context);return 1;}
    if(address==0x800C2E1Cu){actual_heart_caller(context);return 1;}
    if(address==BW_HEALTH_RULES_DAMAGE||address==BW_HEALTH_RULES_COLLISION||address==BW_HEALTH_RULES_HEART){native_result(context);return 1;}
    if(address==0x801165F4u||address==0x80116644u||address==0x80117984u||address==0x80118BACu||address==0x8013F744u||address==0x8013F764u||address==0x8015967Cu||address==0x801596A0u||address==BW_HEALTH_RULES_COLLISION_RETURN||address==BW_HEALTH_RULES_ITEM_RETURN){--context->downcount;context->pc=address+4u;return 1;}
    return 0;
}
static void optimized_setup(unsigned which,bool enabled) {
    setup(enabled?512:256,enabled?128:256);route=which<8?0:which==8?1:2;
    static const uint32_t callers[]={0x801165F0u,0x80116640u,0x80117980u,0x80118BA8u,0x8013F740u,0x8013F760u,0x80159678u,0x8015969Cu,0x80121F54u,0x800C2E1Cu};
    cpu.pc=callers[which];
    cpu.fpr[1]=-1;cpu.ctr=BW_HEALTH_RULES_HEART;cpu.gpr[12]=BW_HEALTH_RULES_HEART;
    cpu.downcount=0;cpu.cycle_budget=100;
#ifdef BW_HEALTH_TEST_HOST
    prepare_host_optimized();
#endif
    bw_direct_depth=0;bw_direct_enabled=bw_edge_watch_ready=bw_edge_filter_enabled=true;
    bw_host_can_skip=can_skip;memset(bw_edge_watch_table,0,sizeof bw_edge_watch_table);
    chunks[67]=native_result;native_calls=entry_edges=return_edges=0;
}
static void optimized_callers(void) {
    for(unsigned which=0;which<10;++which) {
        optimized_setup(which,false);CHECK(bluewake_chassis_dispatch_loop(&cpu,cpu.pc,translated,edge,NULL));
        CHECK(native_calls==1&&entry_edges==0&&return_edges==0);CHECK(runtime.stats.adjusted==0);
        optimized_setup(which,true);CHECK(bluewake_chassis_dispatch_loop(&cpu,cpu.pc,translated,edge,NULL));
        CHECK(native_calls==1&&entry_edges==1&&return_edges==1);CHECK(runtime.stats.adjusted==1);
        CHECK(rf(COUNT)==(which<8?-2:which==8?-6:2));
        optimized_setup(which,true);cpu.cycle_budget=1;
        CHECK(bluewake_chassis_dispatch_loop(&cpu,cpu.pc,translated,edge,NULL));CHECK(native_calls==0);
        cpu.cycle_budget=100;cpu.downcount=0;
#ifdef BW_HEALTH_TEST_HOST
        bw_health_host_dispatch(&cpu,cpu.pc,false);bw_health_host_dispatch(&cpu,cpu.pc,false);
        bw_health_host_stats(&runtime.stats);
#else
        bw_health_rules_dispatch(&runtime,&cpu,cpu.pc,&life);bw_health_rules_dispatch(&runtime,&cpu,cpu.pc,&life);
#endif
        CHECK(bluewake_chassis_dispatch_loop(&cpu,cpu.pc,translated,edge,NULL));CHECK(native_calls==1);
        CHECK(runtime.stats.adjusted==1&&runtime.stats.completed==1);
        no_extra_writes(cpu.lr);
    }
    puts("Actual prepared slow/fast caller bodies + production direct-call/edge headers PASS; native callee outcomes are source fixtures, not game proof.");
}
#endif
int main(void) {
    cpu.ram=calloc(1,BW_HEALTH_RULES_HOST_RAM_SIZE);before_ram=malloc(BW_HEALTH_RULES_HOST_RAM_SIZE);CHECK(cpu.ram&&before_ram);
    pure_rules();disabled_and_admission();side_effects_clamps_and_replay();aggregate_and_healing();native_meter_and_death();guard_cases();lifecycle_and_stack();
    host_ram_contract();byte_alias_guards();
#ifdef BLUEWAKE_HEALTH_OPTIMIZED_CALL_FIXTURE
    optimized_callers();
#endif
    bw_health_rules_detach(&runtime);ppc_guest_alias_clear();free(before_ram);free(cpu.ram);
    printf("Native damage/healing source fixtures, contribution rules, guards and lifecycle: %u checks PASS\n",checks);
    return 0;
}
