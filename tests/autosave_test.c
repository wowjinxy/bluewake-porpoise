// SPDX-License-Identifier: GPL-3.0-or-later
/* Native-call phase simulation against isolated big-endian RAM. Native
 * functions below are explicitly mocked: this does NOT prove a card save,
 * checksum decoder, optimized call reachability or game reload. No card files,
 * disc assets, renderer, SDL, native input or desktop windows are touched. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "autosave.h"
#include "game_events.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Private O3/ASan standalone builds need only CPU header memory helpers. The
// integrated CMake target uses real GXRuntime definitions instead.
#ifdef BLUEWAKE_AUTOSAVE_ISOLATED_MEMORY
bool g_ppc_guest_aliases_overlap_mem1;
PPCMemWriteJournal g_mem_write_journal;
void* g_mem_write_journal_user;
bool ppc_guest_alias_resolve(u32 a,u32 n,u8** p,u32* o){(void)a;(void)n;(void)p;(void)o;return false;}
#endif

static CPUState cpu,initial;
static BwGameScene scene;
static u64 scene_epoch=1,scene_generation=1;
static const u32 INFO=0x803C4C08u,PLAY=0x803C5EA8u,CARD=0x803B39A0u;
static const u32 HEAP=0x80580000u,SCRATCH=0x80600000u,PLAYER=0x80500000u;
static const u32 STAG=0x80402000u,SCLS=0x80403000u,ENTRIES=0x80404000u;
static const u32 MAP=0x80408000u;
enum { BYTES=0x1650, LOG=0x770 };
static u8 old_logs[BYTES];
static unsigned native_calls,stores,polls,waits,frees;
static bool old_checksum=true,alloc_success=true,serialize_success=true,store_success=true;
static bool heap_available=true,card_available=true,worker_complete_on_wait=true;
static bool fpu_reown_success=true;
static u32 capacity=BYTES+128;

bool bluewake_game_events_scene(BwGameScene* out,u64* ep,u64* gen){
    if(out)*out=scene;if(ep)*ep=scene_epoch;if(gen)*gen=scene_generation;return scene.active;
}
static void w8(u32 a,u8 n){cpu.ram[a-0x80000000u]=n;}
static void w16(u32 a,u16 n){w8(a,(u8)(n>>8));w8(a+1,(u8)n);}
static void w32(u32 a,u32 n){w16(a,(u16)(n>>16));w16(a+2,(u16)n);}
static u32 r32(u32 a){return ((u32)cpu.ram[a-0x80000000u]<<24)|((u32)cpu.ram[a-0x80000000u+1]<<16)|
    ((u32)cpu.ram[a-0x80000000u+2]<<8)|cpu.ram[a-0x80000000u+3];}
static void bytes(u32 a,const void* p,size_t n){memcpy(cpu.ram+a-0x80000000u,p,n);}
static BluewakeAutosaveStatus status(void){BluewakeAutosaveStatus s;bluewake_autosave_status(&s);return s;}
static void fixture(void){
    memset(cpu.ram,0,cpu.ram_size);memset(&scene,0,sizeof scene);
    for(unsigned i=0;i<32;++i){cpu.gpr[i]=0x12340000u+i;cpu.fpr[i]=i+0.25;cpu.ps1[i]=i+0.75;}
    for(unsigned i=0;i<16;++i)cpu.sr[i]=0x2000u+i;
    for(unsigned i=0;i<8;++i)cpu.gqr[i]=0x3000u+i;
    cpu.pc=BLUEWAKE_AUTOSAVE_FRAME;cpu.lr=BLUEWAKE_AUTOSAVE_FRAME_CALLER;cpu.gpr[1]=0x817FF000u;
    cpu.msr=PPC_MSR_EE|PPC_MSR_FP;cpu.exception=cpu.program_exception=0;
    cpu.ctr=0x1122;cpu.cr=0x3344;cpu.xer=0x5566;cpu.fpscr=0x7788;
    cpu.srr0=1;cpu.srr1=2;cpu.dar=3;cpu.dsisr=4;cpu.ear=5;cpu.hid2=6;
    cpu.timebase=10000;cpu.downcount=100000;cpu.cycle_budget=100000;
    cpu.cycle_deadline_active=1;cpu.cycle_deadline_budget=999;cpu.reserve_addr=123;cpu.reserve_valid=true;
    w32(0x800000E4u,0x803A2960u);w32(0x800000D4u,0x803A2960u);w32(0x800000D8u,0x803A2960u);
    w32(0x803CA74Cu,PLAYER);w32(PLAYER+0x498,PLAYER+0x1F8);
    w16(INFO,12);w16(INFO+2,3);w16(INFO+4,0); // Zero rupees must not inhibit saves.
    bytes(0x803C9D3Cu,"sea",4);w32(PLAY+0x3EB0+0x48,STAG);w8(STAG+9,0);
    w32(STAG+12,7u<<16);w32(PLAY+0x3EB0+0x4C,SCLS);w32(SCLS,196);w32(SCLS+4,ENTRIES);
    for(unsigned i=0;i<196;++i)bytes(ENTRIES+i*12,"sea",4);
    w8(INFO+0x1290,1);w32(INFO+0x1298,0x11223344);w32(INFO+0x129C,0x55667788);
    w32(CARD+0x1688,0x11223344);w32(CARD+0x168C,0x55667788);w32(CARD+0x1660,1);
    w32(0x803F772Cu,HEAP);w32(HEAP,0x8039CC60u);w32(HEAP+0x30,0x80590000u);w32(HEAP+0x34,0x80700000u);
    w8(HEAP+0x68,1); // Retail panic-on-OOM policy is preserved.
    for(unsigned i=0;i<BYTES;++i)old_logs[i]=(u8)(i*17u+9u);
    bytes(CARD,old_logs,BYTES);w8(INFO+9,2);w8(INFO+10,5);w8(INFO+11,9);
    scene.active=scene.player_valid=scene.controls_ready=true;scene.player=PLAYER;scene.stay_room=0;
    strcpy(scene.stage,"sea");++scene_epoch;++scene_generation;
    native_calls=stores=polls=waits=frees=0;
    old_checksum=alloc_success=serialize_success=store_success=true;
    heap_available=card_available=worker_complete_on_wait=fpu_reown_success=true;capacity=BYTES+128;
    bluewake_autosave_configure(true,60);bluewake_autosave_attach(&cpu);
    bluewake_autosave_reset(&cpu,true);bluewake_autosave_retrace(&cpu,3600,true,false);
    initial=cpu;
}
static bool enter(void){return bluewake_autosave_dispatch(&cpu,BLUEWAKE_AUTOSAVE_FRAME);}
static void lock_mock(u32 mutex){w32(mutex+8,0x803A2960u);w32(mutex+12,r32(mutex+12)+1u);}
static void unlock_mock(u32 mutex){assert(r32(mutex+8)==0x803A2960u);const u32 count=r32(mutex+12);assert(count);
    w32(mutex+12,count-1);if(count==1)w32(mutex+8,0);}
static void native_step(void){
    assert(bluewake_autosave_active());const BluewakeAutosavePhase p=status().phase;
    const u32 a=cpu.gpr[3],b=cpu.gpr[4],d=cpu.gpr[5],e=cpu.gpr[6];u32 result=0;
    assert(cpu.lr==BLUEWAKE_AUTOSAVE_RETURN);const u32 saved_stack=cpu.gpr[1];++native_calls;
    if(p==BW_AUTOSAVE_SERIALIZE||p==BW_AUTOSAVE_POLL){
        assert(bluewake_autosave_observes(cpu.pc));
        BluewakeAutosaveAudit audit;bluewake_autosave_audit(&cpu,&audit);
        assert(audit.boundary_owner_valid&&audit.scratch==SCRATCH&&audit.slot==1&&audit.native_entry==cpu.pc);
        assert(bluewake_autosave_owns_native_call(&cpu,cpu.pc));
        const u32 original_lr=cpu.lr;cpu.lr=0x801D8994;
        assert(!bluewake_autosave_owns_native_call(&cpu,cpu.pc));cpu.lr=original_lr;
        w32(0x800000D4u,0x803A9680u);assert(!bluewake_autosave_owns_native_call(&cpu,cpu.pc));
        w32(0x800000D4u,0x803A2960u);
    }
    switch(p){
    case BW_AUTOSAVE_HEAP_LOCK:
        assert(cpu.pc==0x803060A8&&a==HEAP+0x18);
        result=heap_available;if(result)lock_mock(a);break;
    case BW_AUTOSAVE_HEAP_CAPACITY:
        assert(cpu.pc==0x802B0918&&a==HEAP&&b==32&&r32(HEAP+0x20)==0x803A2960u&&r32(HEAP+0x24)==1);
        result=capacity;break;
    case BW_AUTOSAVE_ALLOCATE:
        assert(cpu.pc==0x802B0434&&a==BYTES&&b==32&&d==HEAP&&r32(HEAP+0x24)==1);
        result=alloc_success?SCRATCH:0;break;
    case BW_AUTOSAVE_HEAP_UNLOCK:
        assert(cpu.pc==0x80305F70&&a==HEAP+0x18);unlock_mock(a);break;
    case BW_AUTOSAVE_CARD_LOCK:
        assert(cpu.pc==0x803060A8&&a==CARD+0x1664&&r32(HEAP+0x24)==0);
        result=card_available;if(result)lock_mock(a);break;
    case BW_AUTOSAVE_OLD_CHECKSUM:
        assert(cpu.pc==0x8001A408&&a==SCRATCH+LOG&&memcmp(cpu.ram+SCRATCH-0x80000000u,old_logs,BYTES)==0);
        assert(r32(CARD+0x1670)==1);result=old_checksum;break;
    case BW_AUTOSAVE_PUT_STAGE:
        assert(cpu.pc==0x8005D988&&a==INFO&&b==((cpu.ram[STAG-0x80000000u+9]>>1)&0x7F));break;
    case BW_AUTOSAVE_START_STAGE:assert(cpu.pc==0x800548FC);break;
    case BW_AUTOSAVE_SERIALIZE:
        assert(cpu.pc==0x8005E780&&a==INFO&&b==SCRATCH&&d==1);
        // Deliberate fixture marker, not a replica native/card serializer.
        if(serialize_success)w32(SCRATCH+LOG+0x100,0xA5501234u);
        result=serialize_success?0:0xFFFFFFFFu;break;
    case BW_AUTOSAVE_CHECKSUM:
        assert(cpu.pc==0x8001A454&&a==SCRATCH&&b==1);
        w32(SCRATCH+LOG+LOG-4,0xC550ABCDu);break;
    case BW_AUTOSAVE_STORE:
        assert(cpu.pc==0x800191C4&&a==CARD&&b==SCRATCH&&d==BYTES&&e==0);
        lock_mock(CARD+0x1664);assert(r32(CARD+0x1670)==2);
        bytes(CARD,cpu.ram+SCRATCH-0x80000000u,BYTES);w32(CARD+0x165C,2);
        unlock_mock(CARD+0x1664);++stores;break;
    case BW_AUTOSAVE_CARD_UNLOCK:
        assert(cpu.pc==0x80305F70&&a==CARD+0x1664);unlock_mock(a);break;
    case BW_AUTOSAVE_POLL:
        assert(cpu.pc==0x8001931C&&a==CARD);++polls;
        if(r32(CARD+0x165C)!=0)result=0;
        else if(r32(CARD+0x1660)==4){result=1;w32(CARD+0x1660,1);}else result=2;
        break;
    case BW_AUTOSAVE_WAIT:
        assert(cpu.pc==0x80313A04);++waits;
        // Model native OSSleepThread: card worker can run on another context.
        w32(0x800000E4u,0x803A9680u);cpu.gpr[1]=0x803A9000u;
        assert(!bluewake_autosave_dispatch(&cpu,0x8001906C));
        if(worker_complete_on_wait){w32(CARD+0x165C,0);w32(CARD+0x1660,store_success?4:12);}
        w32(0x800000E4u,0x803A2960u);cpu.gpr[1]=saved_stack;
        // Model normal main-thread return with another thread owning the FPU.
        w32(0x800000D8u,0x803A9680u);cpu.msr&=~PPC_MSR_FP;
        bluewake_autosave_retrace(&cpu,3600+waits,true,false);break;
    case BW_AUTOSAVE_FREE:
        assert(cpu.pc==0x802B0518&&a==SCRATCH&&b==HEAP&&r32(HEAP+0x24)==0&&r32(CARD+0x1670)==0);
        ++frees;break;
    case BW_AUTOSAVE_FPU_REOWN:
        assert(cpu.pc==0x800F0BF4&&bluewake_autosave_observes(cpu.pc));
        assert(r32(0x800000D8u)==0x803A9680u&&(cpu.msr&PPC_MSR_FP)==0);
        // Explicit mocked native exception/reownership acknowledgement.
        // Actual optimized FPUnavailable/handler/RFI proof is separate.
        if(fpu_reown_success){w32(0x800000D8u,0x803A2960u);cpu.msr|=PPC_MSR_FP;}
        break;
    default:assert(!"Unexpected simulated phase");
    }
    // Native elapsed time and cache/reservation side effects must survive.
    cpu.timebase+=13;cpu.downcount-=17;cpu.cycle_budget-=19;cpu.cycle_deadline_budget-=23;
    cpu.reserve_addr+=1;cpu.reserve_valid=false;cpu.tlb_invalidate_count+=1;
    cpu.gpr[3]=result;cpu.fpr[0]=999;cpu.ps1[0]=-999;cpu.pc=BLUEWAKE_AUTOSAVE_RETURN;
    if(p==BW_AUTOSAVE_SERIALIZE||p==BW_AUTOSAVE_POLL)
        assert(bluewake_autosave_owns_native_call(&cpu,BLUEWAKE_AUTOSAVE_RETURN));
    assert(bluewake_autosave_dispatch(&cpu,BLUEWAKE_AUTOSAVE_RETURN));
}
static void drain(void){for(unsigned n=0;bluewake_autosave_active()&&n<64;++n)native_step();assert(!bluewake_autosave_active());}
static void architecture(void){
    assert(memcmp(cpu.gpr,initial.gpr,sizeof cpu.gpr)==0);
    assert(memcmp(cpu.fpr,initial.fpr,sizeof cpu.fpr)==0&&memcmp(cpu.ps1,initial.ps1,sizeof cpu.ps1)==0);
    assert(memcmp(cpu.sr,initial.sr,sizeof cpu.sr)==0&&memcmp(cpu.gqr,initial.gqr,sizeof cpu.gqr)==0);
#define SAME(f) assert(cpu.f==initial.f)
    SAME(pc);SAME(lr);SAME(ctr);SAME(cr);SAME(xer);SAME(fpscr);SAME(msr);SAME(srr0);SAME(srr1);
    SAME(dar);SAME(dsisr);SAME(ear);SAME(hid2);SAME(exception);SAME(program_exception);
#undef SAME
    assert(cpu.timebase>initial.timebase&&cpu.downcount<initial.downcount&&cpu.cycle_budget<initial.cycle_budget);
    assert(cpu.cycle_deadline_budget<initial.cycle_deadline_budget&&cpu.reserve_addr>initial.reserve_addr&&!cpu.reserve_valid);
    assert(r32(HEAP+0x20)==0&&r32(HEAP+0x24)==0&&r32(CARD+0x166C)==0&&r32(CARD+0x1670)==0);
    assert(r32(0x800000D4u)==0x803A2960u&&r32(0x800000D8u)==0x803A2960u);
    assert(cpu.ram[HEAP-0x80000000u+0x68]==1);
}
static void guard(u32 address,u32 value,unsigned width,BluewakeAutosaveReason expected){
    fixture();if(width==1)w8(address,(u8)value);else if(width==2)w16(address,(u16)value);else w32(address,value);
    u8* before=malloc(cpu.ram_size);assert(before);memcpy(before,cpu.ram,cpu.ram_size);
    assert(!enter()&&!bluewake_autosave_active()&&native_calls==0&&status().reason==expected);
    assert(memcmp(before,cpu.ram,cpu.ram_size)==0);free(before);
}
static void rejected_stage(void){
    u8* before=malloc(cpu.ram_size);assert(before);memcpy(before,cpu.ram,cpu.ram_size);
    assert(!enter()&&!bluewake_autosave_active()&&native_calls==0&&status().reason==BW_AUTOSAVE_UNSAFE_GAME);
    assert(memcmp(before,cpu.ram,cpu.ram_size)==0);free(before);
}
static void stage_guards(void){
    // Pnezumi's native interior path uses MAP, not an absent SCLS. Qualify all
    // three return-to-sea save tables, without relaxing the other save guards.
    for(unsigned tbl=11;tbl<=13;++tbl){
        fixture();strcpy(scene.stage,"Pnezumi");w8(STAG+9,(u8)(tbl<<1));w32(STAG+12,2u<<16);
        w32(PLAY+0x3EB0+0x4C,0);w32(PLAY+0x3EB0+0x14,MAP);w8(MAP+0x36,0xD3);
        assert(enter());drain();architecture();assert(stores==1);
    }
    fixture();w8(STAG+9,22);w32(STAG+12,2u<<16);w32(PLAY+0x3EB0+0x4C,0);
    rejected_stage(); // Required MAP missing.
    fixture();w8(STAG+9,22);w32(STAG+12,2u<<16);w32(PLAY+0x3EB0+0x4C,0);
    w32(PLAY+0x3EB0+0x14,0x817FFFD0u);rejected_stage(); // Full 0x38-byte MAP not readable.
    fixture();w8(STAG+9,22);w32(STAG+12,2u<<16);w32(PLAY+0x3EB0+0x4C,0);
    w32(PLAY+0x3EB0+0x14,MAP);w8(MAP+0x36,0x04);rejected_stage(); // Sea X outside -3..3.
    // Hyrule and dungeon/boss/miniboss branches select entry0, before the
    // ship/MAP branches. They must reject absent SCLS even with a valid MAP.
    fixture();w8(STAG+9,18);w32(STAG+12,2u<<16);w32(PLAY+0x3EB0+0x4C,0);
    w32(PLAY+0x3EB0+0x14,MAP);rejected_stage();
    for(unsigned type=1;type<=6;++type)if(type==1||type==3||type==6){
        fixture();w8(STAG+9,22);w32(STAG+12,type<<16);w32(SCLS,1);
        w32(PLAY+0x3EB0+0x14,0);assert(enter());drain();architecture();assert(stores==1);
        fixture();w8(STAG+9,22);w32(STAG+12,type<<16);w32(PLAY+0x3EB0+0x4C,0);
        w32(PLAY+0x3EB0+0x14,MAP);rejected_stage();
    }
    fixture();w8(STAG+9,18);w32(STAG+12,2u<<16);w32(SCLS,1);
    assert(enter());drain();architecture();assert(stores==1);
    fixture();w8(STAG+9,20);w32(STAG+12,2u<<16);w32(SCLS,195);rejected_stage();
    fixture();w8(STAG+9,20);w32(STAG+12,2u<<16);assert(enter());drain();architecture();assert(stores==1);
    // Encoded bit3 is discarded by the actual three-bit native GetSTType.
    fixture();w8(STAG+9,22);w32(STAG+12,8u<<16);w32(PLAY+0x3EB0+0x4C,0);
    w32(PLAY+0x3EB0+0x14,MAP);assert(enter());drain();architecture();assert(stores==1);
}
int main(void){
    cpu.ram_size=24u*1024*1024;cpu.ram=calloc(1,cpu.ram_size);assert(cpu.ram);
    assert(!bluewake_autosave_desired()&&!bluewake_autosave_active());
    assert(status().reason==BW_AUTOSAVE_DISABLED);
    fixture();bluewake_autosave_configure(false,0);assert(!enter());assert(status().interval_seconds==60);
    bluewake_autosave_configure(false,99999);assert(status().interval_seconds==3600);
    fixture();bluewake_autosave_reset(&cpu,false);bluewake_autosave_retrace(&cpu,7200,true,false);
    assert(!enter()&&status().reason==BW_AUTOSAVE_NEEDS_NATIVE_QUEST);
    bluewake_autosave_note_native_save(&cpu);bluewake_autosave_retrace(&cpu,10800,true,false);assert(enter());drain();architecture();
    fixture();bluewake_autosave_retrace(&cpu,3600,false,false);assert(!enter());
    bluewake_autosave_retrace(&cpu,3600,true,true);assert(!enter());
    guard(INFO+0x1290,3,1,BW_AUTOSAVE_INVALID_SLOT);
    guard(INFO+0x1291,1,1,BW_AUTOSAVE_INVALID_SLOT);guard(INFO+0x1292,1,1,BW_AUTOSAVE_INVALID_SLOT);
    guard(CARD+0x1688,0x99887766,4,BW_AUTOSAVE_WRONG_CARD);
    guard(CARD+0x1654,0x80501000,4,BW_AUTOSAVE_CARD_BUSY);guard(CARD+0x1658,1,1,BW_AUTOSAVE_CARD_BUSY);
    guard(CARD+0x165C,2,4,BW_AUTOSAVE_CARD_BUSY);guard(CARD+0x1660,4,4,BW_AUTOSAVE_CARD_BUSY);
    guard(CARD+0x166C,0x803A9680,4,BW_AUTOSAVE_CARD_BUSY);
    guard(0x803F7000,PLAYER,4,BW_AUTOSAVE_UNSAFE_GAME);guard(0x803F7097,1,1,BW_AUTOSAVE_UNSAFE_GAME);
    guard(PLAY+0x4A20,1,1,BW_AUTOSAVE_UNSAFE_GAME);guard(PLAY+0x4A3A,1,1,BW_AUTOSAVE_UNSAFE_GAME);
    guard(PLAY+0x48D4,1,2,BW_AUTOSAVE_UNSAFE_GAME);guard(PLAY+0x48BC,0x3F800000,4,BW_AUTOSAVE_UNSAFE_GAME);
    guard(INFO+2,0,2,BW_AUTOSAVE_UNSAFE_GAME);guard(0x800000E4,0x803A9680,4,BW_AUTOSAVE_UNSAFE_GAME);
    guard(0x803F7A38,1,4,BW_AUTOSAVE_UNSAFE_GAME);guard(STAG+9,32,1,BW_AUTOSAVE_UNSAFE_GAME);
    guard(SCLS,1,4,BW_AUTOSAVE_UNSAFE_GAME);guard(HEAP,0x8039CCC0,4,BW_AUTOSAVE_NO_SCRATCH);
    guard(PLAY+0x495B,1,1,BW_AUTOSAVE_UNSAFE_GAME); // Missing live ARAM photo block.
    guard(HEAP+0x20,0x803A2960,4,BW_AUTOSAVE_NO_SCRATCH);
    fixture();cpu.lr-=4;assert(!enter());fixture();cpu.msr&=~PPC_MSR_EE;assert(!enter());
    fixture();strcpy(scene.stage,"Xboss0");assert(!enter());fixture();strcpy(scene.stage,"GTower");assert(!enter());
    fixture();scene.controls_ready=false;assert(!enter());
    guard(0x800000D8u,0x803A9680u,4,BW_AUTOSAVE_FPU_UNAVAILABLE);
    stage_guards();
    // Successful phase order, complete ownership and untouched other logs.
    fixture();const u64 done_before=status().completed;assert(enter());
    assert(!bluewake_autosave_dispatch(&cpu,BLUEWAKE_AUTOSAVE_FRAME)); // Budget-deferred owned entry is idempotent.
    assert(bluewake_autosave_observes(BLUEWAKE_AUTOSAVE_RETURN));
    drain();architecture();assert(status().completed==done_before+1&&stores==1&&polls==2&&waits==1&&frees==1);
    assert(memcmp(cpu.ram+CARD-0x80000000u,old_logs,LOG)==0);
    assert(memcmp(cpu.ram+CARD-0x80000000u+2*LOG,old_logs+2*LOG,LOG)==0);
    assert(r32(CARD+LOG+0x100)==0xA5501234&&r32(CARD+LOG+LOG-4)==0xC550ABCD);
    assert(cpu.ram[INFO-0x80000000u+9]==2&&cpu.ram[INFO-0x80000000u+10]==5&&cpu.ram[INFO-0x80000000u+11]==9);
    assert(!enter()); // No repeat STORE for the same frame/interval.
    // Genuine photo buffers may be read while actors stay held; do not clear flags.
    fixture();w8(PLAY+0x495B,7);
    for(unsigned i=0;i<3;++i){const u32 p=0x80540000u+i*0x40;w32(PLAY+0x4800+i*4,p);w32(p+0x18,0x2000);}
    assert(enter());drain();assert(cpu.ram[PLAY-0x80000000u+0x495B]==7);architecture();
    fixture();old_checksum=false;assert(enter());drain();assert(stores==0&&frees==1&&status().reason==BW_AUTOSAVE_INVALID_SLOT);architecture();
    fixture();serialize_success=false;assert(enter());drain();assert(stores==0&&frees==1);architecture();
    fixture();capacity=BYTES-1;assert(enter());drain();assert(stores==0&&frees==0);architecture();
    fixture();capacity=BYTES+63;assert(enter());drain();assert(stores==0&&frees==0);architecture();
    fixture();alloc_success=false;assert(enter());drain();assert(stores==0&&frees==0);architecture();
    fixture();heap_available=false;assert(enter());drain();assert(stores==0&&frees==0);architecture();
    fixture();card_available=false;assert(enter());drain();assert(stores==0&&frees==1);architecture();
    fixture();assert(enter());while(status().phase!=BW_AUTOSAVE_CARD_LOCK)native_step();
    w8(INFO+0x1290,2);native_step();drain();assert(stores==0&&frees==1&&status().reason==BW_AUTOSAVE_WRONG_CARD);
    architecture();
    fixture();store_success=false;const u64 failures=status().failed;assert(enter());drain();
    assert(status().failed==failures+1&&status().reason==BW_AUTOSAVE_NATIVE_ERROR);architecture();
    // Configuration/menu changes drain the owned native call, never restore it mid-function.
    fixture();assert(enter());native_step();const u32 in_flight=cpu.pc;
    bluewake_autosave_configure(false,60);bluewake_autosave_retrace(&cpu,3601,false,true);
    assert(cpu.pc==in_flight&&bluewake_autosave_active()&&bluewake_autosave_observes(BLUEWAKE_AUTOSAVE_RETURN));
    drain();architecture();assert(!bluewake_autosave_desired());
    assert(status().reason==BW_AUTOSAVE_DISABLED);
    // Slow card worker: keep caller suspended until actual native completion.
    fixture();worker_complete_on_wait=false;assert(enter());
    while(status().phase!=BW_AUTOSAVE_WAIT)native_step();
    bluewake_autosave_retrace(&cpu,4301,true,false);
    assert(status().reason==BW_AUTOSAVE_SLOW_TRANSACTION&&bluewake_autosave_active());
    worker_complete_on_wait=true;native_step();drain();architecture();
    // Budget and ordinary interrupt/worker visits cannot advance the wrong return.
    fixture();assert(enter());cpu.exception=PPC_EXC_DSI;const u32 pc=cpu.pc;
    assert(!bluewake_autosave_dispatch(&cpu,BLUEWAKE_AUTOSAVE_RETURN)&&cpu.pc==pc&&bluewake_autosave_active());
    cpu.exception=0;native_step();drain();architecture();
    // An unrelated thread/SP claiming the synthetic token is quarantined.
    fixture();assert(enter());cpu.pc=BLUEWAKE_AUTOSAVE_RETURN;w32(0x800000D4u,0x803A9680u);
    const u32 foreign_context=r32(0x800000D4u);const unsigned calls_before=native_calls;
    assert(!bluewake_autosave_dispatch(&cpu,BLUEWAKE_AUTOSAVE_RETURN));
    assert(status().phase==BW_AUTOSAVE_QUARANTINED&&cpu.pc==BLUEWAKE_AUTOSAVE_RETURN&&
           r32(0x800000D4u)==foreign_context&&native_calls==calls_before);
    fixture();assert(enter());cpu.pc=BLUEWAKE_AUTOSAVE_RETURN;cpu.lr=0x8000645C;
    assert(!bluewake_autosave_dispatch(&cpu,BLUEWAKE_AUTOSAVE_RETURN));
    assert(status().phase==BW_AUTOSAVE_QUARANTINED&&cpu.pc==BLUEWAKE_AUTOSAVE_RETURN);
    fixture();assert(enter());cpu.gpr[1]-=16;cpu.pc=BLUEWAKE_AUTOSAVE_RETURN;
    assert(!bluewake_autosave_dispatch(&cpu,BLUEWAKE_AUTOSAVE_RETURN));
    assert(status().phase==BW_AUTOSAVE_QUARANTINED&&cpu.pc==BLUEWAKE_AUTOSAVE_RETURN);
    // Explicit lifecycle replacement drops stale state without touching the new image.
    cpu.pc=0x80012340;cpu.gpr[3]=0xBEEF;bluewake_autosave_reset(&cpu,false);
    assert(!bluewake_autosave_active()&&cpu.pc==0x80012340&&cpu.gpr[3]==0xBEEF);
    fixture();assert(enter());++scene_generation;cpu.pc=BLUEWAKE_AUTOSAVE_RETURN;
    assert(!bluewake_autosave_dispatch(&cpu,BLUEWAKE_AUTOSAVE_RETURN)&&status().phase==BW_AUTOSAVE_QUARANTINED);
    bluewake_autosave_reset(&cpu,false);assert(!bluewake_autosave_active());
    fixture();assert(enter());while(status().phase!=BW_AUTOSAVE_STORE)native_step();
    w32(CARD+0x165C,1);cpu.pc=BLUEWAKE_AUTOSAVE_RETURN;
    assert(bluewake_autosave_dispatch(&cpu,BLUEWAKE_AUTOSAVE_RETURN));
    assert(status().phase==BW_AUTOSAVE_QUARANTINED&&frees==0&&bluewake_autosave_active());
    bluewake_autosave_detach();assert(!bluewake_autosave_active());
    // Never restore saved FPR/MSR into a context that did not regain native ownership.
    fixture();fpu_reown_success=false;assert(enter());
    while(status().phase!=BW_AUTOSAVE_FPU_REOWN)native_step();native_step();
    assert(status().phase==BW_AUTOSAVE_QUARANTINED&&cpu.pc==BLUEWAKE_AUTOSAVE_RETURN&&
           r32(0x800000D8u)==0x803A9680u&&(cpu.msr&PPC_MSR_FP)==0);
    bluewake_autosave_detach();assert(!bluewake_autosave_active());
    assert(!bluewake_autosave_dispatch(NULL,BLUEWAKE_AUTOSAVE_FRAME));free(cpu.ram);
    puts("autosave native-phase simulation passed (actual card save/reload remains unqualified)");return 0;
}
