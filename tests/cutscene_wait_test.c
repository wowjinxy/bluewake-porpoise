// SPDX-License-Identifier: GPL-3.0-or-later
/* Authored synthetic fixture. No native function, prepared game code,
 * module, event asset or CARD is loaded. Host issuer/owner callbacks below are
 * stand-ins: successful tests would not establish genuine native authority. */
#include "cutscene_wait.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE UINT32_C(0x80000000)
#define ACTOR UINT32_C(0x80500000)
#define STACK UINT32_C(0x80400000)
#define HEADER UINT32_C(0x80600000)
#define ENTRY UINT32_C(0x8021CC4C)
#define RET UINT32_C(0xC10A3DE4)
#define RAW UINT32_C(0x81880000)
typedef struct Fixture {
    CPUState cpu;
    uint8_t* ram;
    BwTcWaitLease lease;
    BwTcWaitOwner owner;
    uint32_t arrays[7], index, timer, deny_at;
    uint64_t validates, reads, writable, owners;
    bool revoked, wrong_write, deny_owner, flip_config;
    uint8_t alternative[4];
} Fixture;
static Fixture f;
static unsigned checks;
#define CHECK(x) do { ++checks; assert(x); } while(0)
static uint8_t* bytes(uint32_t address) { return f.ram+(address-BASE); }
static void put32(uint32_t address,uint32_t value) {
    uint8_t* p=bytes(address);p[0]=(uint8_t)(value>>24);p[1]=(uint8_t)(value>>16);
    p[2]=(uint8_t)(value>>8);p[3]=(uint8_t)value;
}
static uint32_t get32(uint32_t address) {
    const uint8_t* p=bytes(address);
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static bool validate(void* user,const CPUState* cpu,BwTcWaitLease* out) {
    Fixture* x=(Fixture*)user;++x->validates;memset(out,0,sizeof *out);
    /* Identity/revocation check deliberately precedes any CPU dereference. */
    if(x->revoked || cpu!=&x->cpu)return false;
    *out=x->lease;return true;
}
static const uint8_t* resolve(void* user,uint32_t address,uint32_t size) {
    Fixture* x=(Fixture*)user;++x->reads;
    if(x->flip_config) { x->flip_config=false;bluewake_cutscene_wait_configure(false); }
    const bool ordinary=address>=BASE && address<BASE+BW_TC_WAIT_NATIVE_MEM1 &&
        size<=BASE+BW_TC_WAIT_NATIVE_MEM1-address;
    const bool raw285=address>=RAW && (uint64_t)address+size<=(uint64_t)RAW+0x67BC;
    if(x->revoked || !size || (!ordinary && !raw285) ||
       (x->deny_at>=address && (uint64_t)x->deny_at<(uint64_t)address+size))return NULL;
    return x->ram+(address-BASE);
}
static uint8_t* resolve_write(void* user,uint32_t address,uint32_t size) {
    Fixture* x=(Fixture*)user;++x->writable;
    if(x->revoked || address!=ACTOR+0x2DCu || size!=4)return NULL;
    if(x->wrong_write)return x->alternative;
    return (uint8_t*)resolve(user,address,size);
}
static bool owner(void* user,const CPUState* cpu,uint32_t actor,uint32_t pid,BwTcWaitOwner* out) {
    Fixture* x=(Fixture*)user;++x->owners;memset(out,0,sizeof *out);
    if(x->revoked || x->deny_owner || cpu!=&x->cpu || actor!=ACTOR || (pid && pid!=x->owner.pid))return false;
    *out=x->owner;return true;
}
static BwTcWaitHostBinding binding(void) {
    const BwTcWaitHostBinding b={&f,17,BW_TC_WAIT_AVAILABLE,validate,resolve,resolve_write,owner};return b;
}
static BwTcWaitStatus status(void) { BwTcWaitStatus s;bluewake_cutscene_wait_snapshot(&s);return s; }
static void select_cut(unsigned which) {
    static const uint32_t cuts[6]={675,683,686,694,696,698};
    static const uint32_t timers[6]={10,27,10,24,15,20};
    static const uint32_t flags[6]={1774,1793,1801,1824,1827,1830};
    f.index=cuts[which];f.timer=timers[which];
    const uint32_t staff=f.arrays[1]+244*0x50,cut=f.arrays[2]+f.index*0x50,data=f.arrays[3]+which*0x40;
    put32(staff+0x38,f.index);bytes(staff)[0x46]=1;
    /* Native lookup uses bounded table references, not these unused row
     * self-index fields. Deliberately differ from the actual table indices. */
    memcpy(bytes(cut),"WAIT",5);put32(cut+0x24,UINT32_MAX);
    put32(cut+0x28,which==2?1841:UINT32_MAX);put32(cut+0x2C,UINT32_MAX);put32(cut+0x30,UINT32_MAX);
    put32(cut+0x34,flags[which]);put32(cut+0x38,which);put32(cut+0x3C,which==5?UINT32_MAX:f.index+1);
    memcpy(bytes(data),"Timer",6);put32(data+0x20,UINT32_MAX);put32(data+0x24,3);
    put32(data+0x28,which);put32(data+0x2C,1);put32(data+0x30,UINT32_MAX);put32(f.arrays[5]+which*4,f.timer);
    /* Prior embedded action/timer are intentionally stale at entry. */
    put32(ACTOR+0x2C8,UINT32_MAX);put32(ACTOR+0x2D4,UINT32_MAX);put32(ACTOR+0x2DC,99);
    f.cpu.pc=ENTRY;f.cpu.lr=RET;f.cpu.gpr[3]=ACTOR+0x2C4;f.cpu.gpr[30]=ACTOR;f.cpu.gpr[1]=STACK;
}
static void start(unsigned which) {
    bluewake_cutscene_wait_detach();bluewake_cutscene_wait_configure(false);
    memset(f.ram,0,BW_TC_WAIT_HOST_RAM);memset(&f.cpu,0,sizeof f.cpu);
    f.cpu.ram=f.ram;f.cpu.ram_size=BW_TC_WAIT_HOST_RAM;
    f.validates=f.reads=f.writable=f.owners=0;f.deny_at=0;
    f.revoked=f.wrong_write=f.deny_owner=f.flip_config=false;
    f.lease=(BwTcWaitLease){17,1,2,3,4,5,6,7,100,f.ram,BW_TC_WAIT_NATIVE_MEM1,BW_TC_WAIT_HOST_RAM,
                          0x80410000,0x80420000,BW_TC_WAIT_CARD_AUTHORIZED};
    f.owner=(BwTcWaitOwner){11,12,ACTOR,13,0xC10A7008,0xC10A6FE8,ACTOR+0xC4,
                          0x80700000,0x80710000,RAW,0xC10A00F4,0x67BC};
    put32(ACTOR+4,13);bytes(ACTOR)[8]=1;bytes(ACTOR)[9]=0x47;bytes(ACTOR)[0xE]=1;bytes(ACTOR)[0xF]=0x47;
    bytes(ACTOR)[0xC]=2;bytes(ACTOR)[0xD]=2;put32(ACTOR+0x10,0xC10A7008);
    put32(ACTOR+0xEC,0xC10A6FE8);put32(ACTOR+0xD0,ACTOR);bytes(ACTOR)[0xD4]=1;bytes(ACTOR)[0x6CF]=11;
    put32(ACTOR+0x2CC,ACTOR);put32(ACTOR+0x2D0,ACTOR);
    memcpy(bytes(0x803C9D3C),"Pnezumi",8);bytes(0x803C9EA2)[0]=2;
    put32(STACK,STACK+0x40);put32(STACK+0x44,0xC10A4920);
    const uint32_t counts[7]={78,263,744,790,2,6,3},widths[7]={0xB0,0x50,0x50,0x40,4,4,1};
    uint32_t at=0x40;put32(0x803C9ED4,HEADER);
    for(unsigned i=0;i<7;++i) {
        put32(HEADER+8*i,counts[i]?at:0);put32(HEADER+8*i+4,counts[i]);
        f.arrays[i]=counts[i]?HEADER+at:0;put32(0x803C9ED8+4*i,f.arrays[i]);
        at+=counts[i]*widths[i];
    }
    memcpy(bytes(f.arrays[6]),"Tc",3); /* nonempty SData is independently mapped */
    const uint32_t event=f.arrays[0]+71*0xB0,staff=f.arrays[1]+244*0x50;
    memcpy(bytes(event),"TC_RESCUE",10);put32(event+0x7C,3);put32(event+0x2C,243);
    put32(event+0x30,244);put32(event+0x34,245);put32(event+0x88,1830);
    put32(event+0x8C,UINT32_MAX);put32(event+0x90,UINT32_MAX);put32(event+0xA4,2);
    memcpy(bytes(staff),"Tc",3);put32(staff+0x24,UINT32_MAX);put32(staff+0x30,658);
    select_cut(which);
    const BwTcWaitHostBinding b=binding();CHECK(bluewake_cutscene_wait_attach(&f.cpu,&b));
    bluewake_cutscene_wait_configure(true);
}
static void arm(void) {
    CHECK(bluewake_cutscene_wait_observes(&f.cpu,ENTRY));bluewake_cutscene_wait_dispatch(&f.cpu,ENTRY,false);
    CHECK(status().pending);
}
static void completed_standin(void) {
    /* Authored stand-in for a completed real native common-cut call, not a
     * native helper call or evidence that the actual boundary ran. */
    put32(ACTOR+0x2C8,244);put32(ACTOR+0x2D4,0);put32(ACTOR+0x2DC,f.timer-1);
    f.cpu.pc=RET;f.cpu.lr=RET;f.cpu.gpr[3]=1;
}
static void unchanged_return(void) {
    const CPUState before=f.cpu;const uint32_t timer=get32(ACTOR+0x2DC);
    bluewake_cutscene_wait_dispatch(&f.cpu,RET,false);
    CHECK(!memcmp(&before,&f.cpu,sizeof before));CHECK(get32(ACTOR+0x2DC)==timer);CHECK(!status().pending);
}
static void valid_six(void) {
    for(unsigned i=0;i<6;++i) {
        start(i);const BwTcWaitStatus before=status();arm();
        const uint64_t entries_before=status().entries;
        bluewake_cutscene_wait_dispatch(&f.cpu,ENTRY,false);CHECK(status().entries==entries_before);
        ++f.lease.retrace;bluewake_cutscene_wait_retrace(&f.cpu,false);CHECK(status().pending);
        completed_standin();const CPUState native_return=f.cpu;
        uint8_t* prior=(uint8_t*)malloc(BW_TC_WAIT_HOST_RAM);CHECK(prior!=NULL);memcpy(prior,f.ram,BW_TC_WAIT_HOST_RAM);
        bluewake_cutscene_wait_dispatch(&f.cpu,RET,false);
        CHECK(!memcmp(&native_return,&f.cpu,sizeof native_return));CHECK(get32(ACTOR+0x2DC)==1);
        bool unrelated_same=true;
        for(uint32_t n=0;n<BW_TC_WAIT_HOST_RAM;++n)
            if(n<ACTOR-BASE+0x2DC || n>=ACTOR-BASE+0x2E0) if(prior[n]!=f.ram[n])unrelated_same=false;
        CHECK(unrelated_same);free(prior);CHECK(status().shortened==before.shortened+1);
        unchanged_return();
        /* Another authored initialization of this same cut/lifetime cannot
         * grant a new accepted shortening, including first-PC replay. */
        select_cut(i);bluewake_cutscene_wait_dispatch(&f.cpu,ENTRY,false);CHECK(!status().pending);
    }
}
static void default_off_and_stale(void) {
    start(0);bluewake_cutscene_wait_configure(false);
    const uint64_t callbacks=f.validates+f.reads+f.writable+f.owners;
    CPUState* inaccessible=(CPUState*)(uintptr_t)1;
    const BwTcWaitHostBinding b=binding();CHECK(bluewake_cutscene_wait_attach(inaccessible,&b));
    CHECK(!bluewake_cutscene_wait_observes(inaccessible,ENTRY));
    bluewake_cutscene_wait_dispatch(inaccessible,ENTRY,false);bluewake_cutscene_wait_retrace(inaccessible,false);
    CHECK(f.validates+f.reads+f.writable+f.owners==callbacks);
    BwTcWaitStatus snapshot;bluewake_cutscene_wait_snapshot(&snapshot);CHECK(!snapshot.desired_enabled);
    CHECK(f.validates+f.reads+f.writable+f.owners==callbacks);
    bluewake_cutscene_wait_configure(true);f.revoked=true;
    bluewake_cutscene_wait_dispatch(inaccessible,ENTRY,false);CHECK(!status().pending);
    bluewake_cutscene_wait_detach();const uint64_t after=f.validates;
    bluewake_cutscene_wait_dispatch(inaccessible,RET,false);CHECK(f.validates==after);
}
static void negative_returns(void) {
    for(unsigned negative=0;negative<31;++negative) {
        start(0);arm();completed_standin();
        switch(negative) {
        case 0:f.cpu.gpr[3]=0;break;
        case 1:f.cpu.gpr[1]+=16;break;
        case 2:put32(STACK,STACK+0x50);break;
        case 3:put32(STACK+0x44,0xC10A4924);break;
        case 4:f.lease.thread+=4;break;
        case 5:f.lease.context+=4;break;
        case 6:f.lease.alias++;break;
        case 7:f.lease.rel_table++;break;
        case 8:f.owner.materialization++;break;
        case 9:f.owner.registration++;break;
        case 10:put32(ACTOR+0x2DC,f.timer-2);break;
        case 11:put32(f.arrays[1]+244*0x50+0x38,676);break;
        case 12:bytes(f.arrays[1]+244*0x50)[0x46]=0;break;
        case 13:f.wrong_write=true;break;
        case 14:f.deny_at=ACTOR+0x2DF;break;
        case 15:f.lease.retrace+=5;break;
        case 16:f.lease.flags|=BW_TC_WAIT_SAVE_ACTIVE;break;
        case 17:f.flip_config=true;break;
        case 18:f.cpu.gpr[3]=2;break; /* intentional exact native bool1 */
        case 19:put32(ACTOR+0x2DC,0);break;
        case 20:put32(ACTOR+0x2DC,UINT32_MAX);break;
        case 21:f.lease.issuer++;break;
        case 22:f.lease.cpu++;break;
        case 23:f.lease.ram++;break;
        case 24:f.lease.code++;break;
        case 25:f.lease.native_card_epoch++;break;
        case 26:f.lease.scene++;break;
        case 27:f.lease.flags|=BW_TC_WAIT_CAPTURE_ACTIVE;break;
        case 28:f.lease.host_ram_size--;break;
        case 29:f.cpu.exception=1;break;
        case 30:f.cpu.program_exception=1;break;
        }
        unchanged_return();
    }
}
static void negative_entries(void) {
    for(unsigned negative=0;negative<33;++negative) {
        start(0);const uint32_t data=f.arrays[3];
        switch(negative) {
        case 0:bytes(ACTOR)[0xC]=1;break;
        case 1:bytes(ACTOR)[0xC]=3;break;
        case 2:bytes(ACTOR)[0xD]=1;break;
        case 3:put32(ACTOR+4,UINT32_MAX-1);f.owner.pid=UINT32_MAX-1;break;
        case 4:f.owner.pid=0;put32(ACTOR+4,0);break;
        case 5:f.cpu.gpr[30]+=4;break;
        case 6:f.cpu.lr=RET+4;break;
        case 7:f.cpu.gpr[3]+=4;break;
        case 8:f.cpu.gpr[1]+=4;break;
        case 9:bytes(ACTOR)[0x6CF]=1;break;
        case 10:bytes(0x803C9D3C)[0]='X';break;
        case 11:bytes(0x803C9D54)[0]=1;break;
        case 12:put32(0x803F6160,1);break;
        case 13:f.lease.flags=0;break;
        case 14:put32(data+0x2C,2);break;
        case 15:put32(data+0x30,1);break;
        case 16:put32(HEADER+0x18,get32(HEADER+8));put32(0x803C9ED4+0x10,f.arrays[1]);break;
        case 17:put32(HEADER+0x2C,UINT32_MAX);break;
        case 18:f.deny_owner=true;break;
        case 19:put32(0x803C9ED4+0x1C,f.arrays[6]+1);break;
        case 20:f.deny_at=f.arrays[6]+2;break;
        case 21:put32(HEADER+0x34,65537);break;
        case 22:f.owner.raw_text=0x80780000;break;
        case 23:f.deny_at=RAW+0x67BB;break;
        case 24:put32(f.arrays[5],11);break;
        case 25:put32(f.arrays[1]+244*0x50+0x20,UINT32_MAX);break;
        case 26:put32(f.arrays[1]+244*0x50+0x2C,1);break;
        case 27:bytes(0x803C9EA2)[0]=0;break;
        case 28:put32(f.arrays[0]+71*0xB0+0x88,1827);break;
        case 29:put32(f.arrays[2]+675*0x50+0x3C,676+1);break;
        case 30:put32(f.arrays[1]+244*0x50+0x30,667);break; /* later WAIT is not first cut */
        case 31:memcpy(bytes(f.arrays[2]+675*0x50),"PRESENT",8);break;
        case 32:memcpy(bytes(data),"Other",6);break;
        }
        const CPUState before=f.cpu;const uint32_t timer=get32(ACTOR+0x2DC);
        bluewake_cutscene_wait_dispatch(&f.cpu,ENTRY,false);
        CHECK(!status().pending);CHECK(get32(ACTOR+0x2DC)==timer);CHECK(!memcmp(&before,&f.cpu,sizeof before));
    }
}
static void lifecycle_and_other_cuts(void) {
    start(0);BwTcWaitHostBinding unsupported=binding();unsupported.availability=BW_TC_WAIT_UNAVAILABLE_ADMISSION;
    CHECK(bluewake_cutscene_wait_attach(&f.cpu,&unsupported));
    const uint64_t no_cpu=f.validates+f.reads+f.writable+f.owners;
    CHECK(!bluewake_cutscene_wait_observes(&f.cpu,ENTRY));bluewake_cutscene_wait_dispatch(&f.cpu,ENTRY,false);
    CHECK(f.validates+f.reads+f.writable+f.owners==no_cpu);
    CHECK(status().availability==BW_TC_WAIT_UNAVAILABLE_ADMISSION);
    start(0);arm();const uint64_t before=status().entries;
    f.cpu.gpr[1]+=16;bluewake_cutscene_wait_dispatch(&f.cpu,ENTRY,false);
    CHECK(!status().pending && status().entries==before);
    start(0);arm();bluewake_cutscene_wait_suspend(BW_TC_WAIT_CAPTURE);completed_standin();unchanged_return();
    start(0);arm();bluewake_cutscene_wait_suspend(BW_TC_WAIT_STATE);f.revoked=true;
    const uint64_t callbacks=f.validates;bluewake_cutscene_wait_dispatch((CPUState*)(uintptr_t)1,RET,false);
    CHECK(f.validates==callbacks);
    start(0);arm();bluewake_cutscene_wait_configure(false);completed_standin();unchanged_return();
    start(0);arm();bluewake_cutscene_wait_configure(true);CHECK(status().pending);
    bluewake_cutscene_wait_retrace(&f.cpu,false);CHECK(status().pending); /* idempotent desired value */
    bluewake_cutscene_wait_configure(false);bluewake_cutscene_wait_configure(true);
    completed_standin();unchanged_return(); /* changed config generation, final bool equal */
    start(0);arm();bluewake_cutscene_wait_retrace(&f.cpu,true);CHECK(!status().pending);
    start(0);bytes(f.arrays[1]+244*0x50)[0x46]=2;arm();completed_standin();
    bluewake_cutscene_wait_dispatch(&f.cpu,RET,false);CHECK(get32(ACTOR+0x2DC)==1);
    start(0);f.cpu.lr=RAW+(RET-0xC10A00F4);arm();completed_standin();
    f.cpu.pc=f.cpu.lr=RAW+(RET-0xC10A00F4);
    bluewake_cutscene_wait_dispatch(&f.cpu,f.cpu.pc,false);CHECK(get32(ACTOR+0x2DC)==1);
    start(0);f.cpu.pc=0xC021CC4C;bluewake_cutscene_wait_dispatch(&f.cpu,f.cpu.pc,false);
    CHECK(status().pending);completed_standin();
    bluewake_cutscene_wait_dispatch(&f.cpu,RET,false);CHECK(get32(ACTOR+0x2DC)==1);
    for(unsigned reason=BW_TC_WAIT_CONFIG;reason<=BW_TC_WAIT_DETACH;++reason) {
        start(0);arm();bluewake_cutscene_wait_suspend((BwTcWaitSuspendReason)reason);
        completed_standin();unchanged_return();
        if(reason==BW_TC_WAIT_CARD || reason==BW_TC_WAIT_STATE || reason==BW_TC_WAIT_MACHINE ||
           reason==BW_TC_WAIT_CPU || reason==BW_TC_WAIT_RAM || reason==BW_TC_WAIT_CODE ||
           reason==BW_TC_WAIT_DETACH) {
            f.revoked=true;const uint64_t callbacks=f.validates+f.reads+f.writable+f.owners;
            bluewake_cutscene_wait_dispatch((CPUState*)(uintptr_t)1,RET,false);
            CHECK(f.validates+f.reads+f.writable+f.owners==callbacks);
        }
    }
    for(uint32_t cut=658;cut<=699;++cut) {
        if(cut==675 || cut==683 || cut==686 || cut==694 || cut==696 || cut==698)continue;
        start(0);put32(f.arrays[1]+244*0x50+0x38,cut);
        bluewake_cutscene_wait_dispatch(&f.cpu,ENTRY,false);CHECK(!status().pending);
    }
}
int main(void) {
    memset(&f,0,sizeof f);f.ram=(uint8_t*)malloc(BW_TC_WAIT_HOST_RAM);CHECK(f.ram!=NULL);
    valid_six();default_off_and_stale();negative_returns();negative_entries();lifecycle_and_other_cuts();
    bluewake_cutscene_wait_detach();free(f.ram);
    printf("TC_WAIT_AUTHORED_SYNTHETIC_CHECKS %u\n",checks);
    return 0;
}
