// SPDX-License-Identifier: GPL-3.0-or-later
#include "autosave.h"
#include "game_events.h"
#include "fpu_context.h"
#include <stdatomic.h>
#include <string.h>

/* GZLE01 rev0, zeldaret/tww 49f2e3484e5814cbafde68525128669589e86bb2.
 * main01's actual bl 80006458 -> fapGm_Execute gives LR 8000645C.
 * d_menu_save.cpp 921/1012/1025 is the native preparation/store protocol.
 * m_Do_MemCard.cpp 800191C4 save is void and silently loses a busy try-lock;
 * retaining the recursive retail mutex proves command adoption before unlock.
 * SaveSync consumes completion, so this module ONLY polls its own STORE.
 * VIWaitForRetrace sleeps the caller and lets the lower-priority native card
 * worker run. Actors, UI and ARAM picture producers stay suspended until done.
 * Do not use callback_delivery (it disables EE and the retail scheduler).
 * JKRExpHeap do_alloc itself can panic on OOM. Hold the pinned ExpHeap mutex
 * through getMaxAllocatableSize+allocation; never alter its native error flag.
 * No serializer, card format, rewards or resource refills are reimplemented. */
static const u32 INFO=0x803C4C08u, PLAY=0x803C5EA8u, CARD=0x803B39A0u;
static const u32 THREAD=0x803A2960u, CURRENT_THREAD=0x800000E4u;
static const u32 CURRENT_HEAP=0x803F772Cu, EXP_HEAP_VTABLE=0x8039CC60u;
static const u32 TRY_LOCK=0x803060A8u, UNLOCK=0x80305F70u;
static const u32 HEAP_CAPACITY=0x802B0918u, ALLOCATE=0x802B0434u, FREE=0x802B0518u;
static const u32 TEST_CHECKSUM=0x8001A408u, PUT_STAGE=0x8005D988u;
static const u32 START_STAGE=0x800548FCu, SERIALIZE=0x8005E780u;
static const u32 CHECKSUM=0x8001A454u, STORE=0x800191C4u;
static const u32 POLL=0x8001931Cu, WAIT=0x80313A04u;
// Pure fabsf: ordinary native FP exception handling reacquires the main FPU
// after VIWait/card/audio switches. Its result is discarded.
static const u32 FPU_REOWN=0x800F0BF4u;
enum { LOG_BYTES=0x770, CARD_BYTES=3*LOG_BYTES, MAX_WAIT_TICKS=600 };

/* Only architectural caller state is restored. Elapsed timebase/downcount,
 * deadline/cycle fields, device callbacks, TLB/cache/reservation side effects
 * and all guest memory/scheduler changes remain those of normal native calls. */
typedef struct Caller {
    u32 gpr[32]; f64 fpr[32],ps1[32];
    u32 pc,lr,ctr,cr,xer,fpscr,msr,srr0,srr1,dar,dsisr,ear,hid2;
    u32 sr[16],gqr[8],exception,program_exception;
} Caller;
static atomic_uint config=300u<<1, public_phase, public_reason=BW_AUTOSAVE_DISABLED;
static atomic_ullong completed,failed,deferred,last_success;
static CPUState* owner;
static u8* owner_ram;
static u32 owner_ram_size,stack,heap,scratch,player,frame,stage_table,native_entry;
static u64 epoch,generation,tick,due,start_tick;
static bool authorized,host_ready,input_blocked,heap_locked,card_locked;
static bool outcome_success,queued,slow,abort_before_store;
static u8 slot;
static u32 serial[2];
static BluewakeAutosavePhase phase;
static BluewakeAutosaveReason outcome_reason;
static Caller caller;

static bool span(const CPUState* c,u32 a,u32 n) {
    if(!c||!c->ram||a<0x80000000u||a>=0x81800000u||n>0x1800000u)return false;
    const u32 o=a-0x80000000u;
    return o<=0x1800000u-n&&n<=c->ram_size&&o<=c->ram_size-n;
}
static u8 r8(const CPUState* c,u32 a){return c->ram[a-0x80000000u];}
static u16 r16(const CPUState* c,u32 a){return (u16)((r8(c,a)<<8)|r8(c,a+1));}
static u32 r32(const CPUState* c,u32 a){return ((u32)r16(c,a)<<16)|r16(c,a+2);}
static void set_phase(BluewakeAutosavePhase p){phase=p;atomic_store_explicit(&public_phase,p,memory_order_release);}
static void reason(BluewakeAutosaveReason r){atomic_store_explicit(&public_reason,r,memory_order_release);}
static unsigned interval(void){return atomic_load_explicit(&config,memory_order_acquire)>>1;}
void bluewake_autosave_configure(bool on,unsigned seconds){
    if(seconds<60)seconds=60;if(seconds>3600)seconds=3600;
    atomic_store_explicit(&config,(seconds<<1)|(on?1u:0u),memory_order_release);
}
bool bluewake_autosave_desired(void){return (atomic_load_explicit(&config,memory_order_acquire)&1u)!=0;}
bool bluewake_autosave_active(void){return atomic_load_explicit(&public_phase,memory_order_acquire)!=BW_AUTOSAVE_IDLE;}
void bluewake_autosave_status(BluewakeAutosaveStatus* o){
    if(!o)return;const unsigned c=atomic_load_explicit(&config,memory_order_acquire);
    o->phase=(BluewakeAutosavePhase)atomic_load_explicit(&public_phase,memory_order_acquire);
    o->reason=(BluewakeAutosaveReason)atomic_load_explicit(&public_reason,memory_order_acquire);
    o->completed=atomic_load(&completed);o->failed=atomic_load(&failed);o->deferred=atomic_load(&deferred);
    o->last_success_tick=atomic_load(&last_success);o->desired=(c&1)!=0;o->interval_seconds=c>>1;
    o->active=o->phase!=BW_AUTOSAVE_IDLE;
    if(!o->desired&&!o->active)o->reason=BW_AUTOSAVE_DISABLED;
}
const char* bluewake_autosave_reason_text(BluewakeAutosaveReason r){
    static const char* const names[]={"Ready","Disabled","Use the game's Save or load a native card quest first",
        "Waiting for host work","Waiting for safe gameplay","Waiting for the native card controller",
        "Card identity changed","Existing quest slot unavailable","Native scratch allocation unavailable",
        "Native save failed","Native invocation owner changed","Native save is taking longer than expected",
        "Waiting for native floating-point ownership"};
    return (unsigned)r<sizeof names/sizeof names[0]?names[r]:"Unknown";
}
static void save_caller(const CPUState* c){
    memcpy(caller.gpr,c->gpr,sizeof caller.gpr);memcpy(caller.fpr,c->fpr,sizeof caller.fpr);
    memcpy(caller.ps1,c->ps1,sizeof caller.ps1);memcpy(caller.sr,c->sr,sizeof caller.sr);memcpy(caller.gqr,c->gqr,sizeof caller.gqr);
#define SAVE(f) caller.f=c->f
    SAVE(pc);SAVE(lr);SAVE(ctr);SAVE(cr);SAVE(xer);SAVE(fpscr);SAVE(msr);SAVE(srr0);SAVE(srr1);
    SAVE(dar);SAVE(dsisr);SAVE(ear);SAVE(hid2);SAVE(exception);SAVE(program_exception);
#undef SAVE
}
static void restore_caller(CPUState* c){
    memcpy(c->gpr,caller.gpr,sizeof caller.gpr);memcpy(c->fpr,caller.fpr,sizeof caller.fpr);
    memcpy(c->ps1,caller.ps1,sizeof caller.ps1);memcpy(c->sr,caller.sr,sizeof caller.sr);memcpy(c->gqr,caller.gqr,sizeof caller.gqr);
#define RESTORE(f) c->f=caller.f
    RESTORE(pc);RESTORE(lr);RESTORE(ctr);RESTORE(cr);RESTORE(xer);RESTORE(fpscr);RESTORE(msr);
    RESTORE(srr0);RESTORE(srr1);RESTORE(dar);RESTORE(dsisr);RESTORE(ear);RESTORE(hid2);
    RESTORE(exception);RESTORE(program_exception);
#undef RESTORE
}
void bluewake_autosave_reset(CPUState* c,bool native_loaded){
    // No cleanup into an already-replaced CPU/RAM image, including reused RAM.
    owner=c;owner_ram=c?c->ram:NULL;owner_ram_size=c?c->ram_size:0;
    authorized=c&&native_loaded;heap_locked=card_locked=queued=false;scratch=heap=0;
    native_entry=0;memset(&caller,0,sizeof caller);
    host_ready=false;input_blocked=true;due=tick+(u64)interval()*60u;
    set_phase(BW_AUTOSAVE_IDLE);reason(authorized?BW_AUTOSAVE_OK:BW_AUTOSAVE_NEEDS_NATIVE_QUEST);
}
void bluewake_autosave_attach(CPUState* c){tick=0;bluewake_autosave_reset(c,false);}
void bluewake_autosave_detach(void){bluewake_autosave_reset(NULL,false);}
void bluewake_autosave_note_native_save(CPUState* c){
    if(c==owner&&c&&c->ram==owner_ram&&c->ram_size==owner_ram_size&&phase==BW_AUTOSAVE_IDLE){
        authorized=true;due=tick+(u64)interval()*60u;reason(BW_AUTOSAVE_OK);
    }
}
void bluewake_autosave_retrace(CPUState* c,u64 now,bool safe,bool blocked){
    tick=now;host_ready=safe;input_blocked=blocked;
    if(phase!=BW_AUTOSAVE_IDLE&&c==owner&&now-start_tick>MAX_WAIT_TICKS&&!slow){
        // Never release a caller while STORE still reads native photo buffers.
        slow=true;reason(BW_AUTOSAVE_SLOW_TRANSACTION);
    }
}
static bool good_name(const CPUState* c,u32 a){
    if(!span(c,a,8)||r8(c,a)==0)return false;
    for(unsigned i=0;i<8;++i){const u8 b=r8(c,a+i);if(!b)return true;
        if(!((b>='A'&&b<='Z')||(b>='a'&&b<='z')||(b>='0'&&b<='9')||b=='_'))return false;}
    return false;
}
static bool native_context(const CPUState* c,BwGameScene* scene,u64* ep,u64* gen){
    if(!bluewake_game_events_scene(scene,ep,gen)||!scene->active||!scene->player_valid||!scene->controls_ready||
       scene->paused||scene->event_running||scene->transitioning||!span(c,scene->player,0x361C))return false;
    if(!span(c,INFO,0x12A0)||!span(c,PLAY,0x4A40)||!span(c,CARD,0x1698)||
       r32(c,0x803CA74Cu)!=scene->player||r32(c,scene->player+0x498)!=scene->player+0x1F8||
       r16(c,scene->player+0x304)!=0||r32(c,scene->player+0x314)!=0||
       r8(c,0x803F7097u)!=0||r32(c,0x803F7000u)!=0||r8(c,0x803C9EA2u)!=0||
       r8(c,0x803C9D54u)!=0||r32(c,0x803F6160u)!=0||r16(c,INFO+2)==0)return false;
    if(strcmp(scene->stage,"PShip")==0||strcmp(scene->stage,"GTower")==0||
       strncmp(scene->stage,"Xboss",5)==0||r8(c,PLAY+0x4A20)!=0||
       r8(c,PLAY+0x492A)||r8(c,PLAY+0x492B)||r8(c,PLAY+0x4959)||
       r8(c,PLAY+0x495E)||r8(c,PLAY+0x4962)||r8(c,PLAY+0x4A3A))return false;
    if(r32(c,PLAY+0x48BC)||r32(c,PLAY+0x48C0))return false;
    static const u16 counts[]={0x48D4,0x48D6,0x48D8,0x48DA,0x48DC,0x48DE,0x48E0,0x48E4,
        0x48E8,0x48EA,0x48EC,0x48EE,0x48F0,0x48F2,0x48F4,0x48F6};
    for(unsigned i=0;i<sizeof counts/sizeof counts[0];++i)if(r16(c,PLAY+counts[i]))return false;
    // Follow setGameStartStage's branch order. Ordinary interiors legitimately
    // have no SCLS: their native return-to-sea location comes from MAP instead.
    const u32 stag=r32(c,PLAY+0x3EB0+0x48);
    if(!span(c,stag,0x20))return false;
    // The pinned native GetSTType getter masks three bits, even though its enum
    // also names UNKNOWN_8. Do not widen the mask and invent another save path.
    const u32 tbl=(r8(c,stag+9)>>1)&0x7F,type=(r32(c,stag+12)>>16)&7;
    if(tbl>=16)return false;
    u32 required_scls=0;
    if(type==7)required_scls=196; // Sea may select any clamped 14x14 mesh cell.
    else if(type==1||type==3||type==6||type==8||tbl==9)required_scls=1;
    else if(tbl==10)required_scls=196; // Ship fallback uses the same sea mesh.
    else if(tbl>=11&&tbl<=13){
        const u32 map=r32(c,PLAY+0x3EB0+0x14);
        if(!span(c,map,0x38))return false; // Native OceanX/Z read byte +0x36.
        const u8 xz=r8(c,map+0x36);
        const int x=(xz&15)>=8?(xz&15)-16:(xz&15);
        const int z=(xz>>4)>=8?(xz>>4)-16:(xz>>4);
        if(x< -3||x>3||z< -3||z>3)return false;
    }
    if(required_scls){
        const u32 scls=r32(c,PLAY+0x3EB0+0x4C);
        if(!span(c,scls,8))return false;
        const u32 n=r32(c,scls),entries=r32(c,scls+4);
        if(n<required_scls||n>4096||!span(c,entries,required_scls*12))return false;
        for(u32 i=0;i<required_scls;++i)if(!good_name(c,entries+i*12))return false;
    }
    const u8 picture_flags=r8(c,PLAY+0x495B);
    if(picture_flags&~7u)return false;
    for(unsigned i=0;i<3;++i)if(picture_flags&(1u<<i)){
        const u32 p=r32(c,PLAY+0x4800+i*4);
        if(!span(c,p,0x24)||r32(c,p+0x18)<0x2000)return false;
    }
    stage_table=tbl;return true;
}
static BluewakeAutosaveReason card_ready(const CPUState* c){
    if(r8(c,INFO+0x1290)>=3||r8(c,INFO+0x1291)||r8(c,INFO+0x1292))return BW_AUTOSAVE_INVALID_SLOT;
    if(r8(c,CARD+0x1658)!=0||r32(c,CARD+0x1654)!=0||r32(c,CARD+0x1660)!=1||r32(c,CARD+0x165C)!=0)
        return BW_AUTOSAVE_CARD_BUSY;
    if(r32(c,INFO+0x1298)!=r32(c,CARD+0x1688)||r32(c,INFO+0x129C)!=r32(c,CARD+0x168C)||
       (r32(c,CARD+0x1688)==0&&r32(c,CARD+0x168C)==0))return BW_AUTOSAVE_WRONG_CARD;
    return BW_AUTOSAVE_OK;
}
static void call(CPUState* c,BluewakeAutosavePhase p,u32 address,u32 a,u32 b,u32 d,u32 e){
    native_entry=address;set_phase(p);c->gpr[3]=a;c->gpr[4]=b;c->gpr[5]=d;c->gpr[6]=e;
    c->lr=BLUEWAKE_AUTOSAVE_RETURN;c->pc=address;
}
static void finish(CPUState* c){
    if(!bluewake_fpu_registers_materialized(c)||r32(c,0x800000D4u)!=THREAD){
        call(c,BW_AUTOSAVE_FPU_REOWN,FPU_REOWN,0,0,0,0);return;
    }
    if(outcome_success){atomic_fetch_add(&completed,1);atomic_store(&last_success,tick);}
    else atomic_fetch_add(&failed,1);
    due=tick+(u64)interval()*60u;reason(outcome_reason);restore_caller(c);
    scratch=heap=native_entry=0;queued=heap_locked=card_locked=false;set_phase(BW_AUTOSAVE_IDLE);
}
static void cleanup(CPUState* c){
    if(heap_locked)call(c,BW_AUTOSAVE_HEAP_UNLOCK,UNLOCK,heap+0x18,0,0,0);
    else if(card_locked)call(c,BW_AUTOSAVE_CARD_UNLOCK,UNLOCK,CARD+0x1664,0,0,0);
    else if(scratch)call(c,BW_AUTOSAVE_FREE,FREE,scratch,heap,0,0);
    else finish(c);
}
static void fail(CPUState* c,BluewakeAutosaveReason r){outcome_success=false;outcome_reason=r;abort_before_store=true;cleanup(c);}
static bool same_owner(CPUState* c){
    u64 ep=0,gen=0;bluewake_game_events_scene(NULL,&ep,&gen);
    return c==owner&&c->ram==owner_ram&&c->ram_size==owner_ram_size&&
        r32(c,CURRENT_THREAD)==THREAD&&r32(c,0x800000D4u)==THREAD&&
        c->gpr[1]==stack&&ep==epoch&&gen==generation;
}
static bool boundary_owner(CPUState* c){
    return c&&phase!=BW_AUTOSAVE_IDLE&&same_owner(c)&&!c->exception&&
        r32(c,0x800000D4u)==THREAD&&r32(c,0x803E8140u)==frame&&r32(c,0x803CA74Cu)==player&&
        r8(c,INFO+0x1290)==slot&&r32(c,CARD+0x1688)==serial[0]&&r32(c,CARD+0x168C)==serial[1];
}
bool bluewake_autosave_owns_native_call(CPUState* c,u32 a){
    if((phase!=BW_AUTOSAVE_SERIALIZE&&phase!=BW_AUTOSAVE_POLL)||!c||c->pc!=a||
       c->lr!=BLUEWAKE_AUTOSAVE_RETURN||!boundary_owner(c))return false;
    if(a==BLUEWAKE_AUTOSAVE_RETURN)return true;
    if(phase==BW_AUTOSAVE_SERIALIZE)
        return a==SERIALIZE&&c->gpr[3]==INFO&&c->gpr[4]==scratch&&c->gpr[5]==slot&&span(c,scratch,CARD_BYTES);
    return a==POLL&&c->gpr[3]==CARD&&queued;
}
void bluewake_autosave_audit(CPUState* c,BluewakeAutosaveAudit* o){
    if(!o)return;memset(o,0,sizeof *o);o->phase=phase;o->boundary_owner_valid=boundary_owner(c);
    o->heap_locked=heap_locked;o->card_locked=card_locked;o->store_queued=queued;
    o->native_entry=native_entry;o->caller_pc=caller.pc;o->caller_lr=caller.lr;o->caller_stack=stack;
    o->expected_thread=THREAD;o->player=player;o->heap=heap;o->scratch=scratch;o->native_frame=frame;
    o->slot=slot;o->stage_table=stage_table;o->serial_high=serial[0];o->serial_low=serial[1];
    o->epoch=epoch;o->scene_generation=generation;o->start_tick=start_tick;o->current_tick=tick;o->due_tick=due;
    if(c){o->current_pc=c->pc;o->current_stack=c->gpr[1];}
    if(span(c,CARD,0x1698)&&span(c,0x800000D4u,20)){
        o->current_thread=r32(c,CURRENT_THREAD);o->current_context=r32(c,0x800000D4u);o->fpu_context=r32(c,0x800000D8u);
        o->card_command=r32(c,CARD+0x165C);o->card_state=r32(c,CARD+0x1660);
        o->card_mutex_owner=r32(c,CARD+0x166C);o->card_mutex_count=r32(c,CARD+0x1670);
    }
    if(span(c,heap,0x30)){o->heap_mutex_owner=r32(c,heap+0x20);o->heap_mutex_count=r32(c,heap+0x24);}
}
bool bluewake_autosave_observes(u32 a){
    if(a==SERIALIZE||a==POLL){
        const unsigned p=atomic_load_explicit(&public_phase,memory_order_acquire);
        return (a==SERIALIZE&&p==BW_AUTOSAVE_SERIALIZE)||(a==POLL&&p==BW_AUTOSAVE_POLL);
    }
    // Defeat native fast-math so the real FP instruction takes the native
    // unavailable exception when another thread owns D8.
    if(a==FPU_REOWN)return atomic_load_explicit(&public_phase,memory_order_acquire)==BW_AUTOSAVE_FPU_REOWN;
    if(a==BLUEWAKE_AUTOSAVE_RETURN)return bluewake_autosave_active();
    return a==BLUEWAKE_AUTOSAVE_FRAME&&bluewake_autosave_desired();
}
bool bluewake_autosave_dispatch(CPUState* c,u32 a){
    if(!c)return false;
    if(a!=BLUEWAKE_AUTOSAVE_FRAME&&a!=BLUEWAKE_AUTOSAVE_RETURN)return false;
    if(c->pc!=a)return false;
    if(a==BLUEWAKE_AUTOSAVE_FRAME){
        if(phase!=BW_AUTOSAVE_IDLE||!bluewake_autosave_desired())return false;
        if(!authorized){reason(BW_AUTOSAVE_NEEDS_NATIVE_QUEST);return false;}
        if(tick<due)return false;
        if(!host_ready||input_blocked){reason(BW_AUTOSAVE_HOST_BUSY);return false;}
        BwGameScene scene;u64 ep=0,gen=0;
        if(c!=owner||!c||c->ram!=owner_ram||c->ram_size!=owner_ram_size||c->exception||
           !(c->msr&PPC_MSR_EE)||c->lr!=BLUEWAKE_AUTOSAVE_FRAME_CALLER||
           !span(c,c->gpr[1]-128u,160)||c->gpr[1]%16||r32(c,CURRENT_THREAD)!=THREAD||
           r32(c,0x803F7A38u)!=0||!native_context(c,&scene,&ep,&gen)){
            reason(BW_AUTOSAVE_UNSAFE_GAME);atomic_fetch_add(&deferred,1);return false;
        }
        BluewakeAutosaveReason r=card_ready(c);
        if(!bluewake_fpu_registers_materialized(c)||r32(c,0x800000D4u)!=THREAD){
            reason(BW_AUTOSAVE_FPU_UNAVAILABLE);atomic_fetch_add(&deferred,1);return false;
        }
        if(r==BW_AUTOSAVE_OK&&(r32(c,CARD+0x166C)!=0||r32(c,CARD+0x1670)!=0))r=BW_AUTOSAVE_CARD_BUSY;
        heap=r32(c,CURRENT_HEAP);
        if(r==BW_AUTOSAVE_OK&&(!span(c,heap,0x70)||r32(c,heap)!=EXP_HEAP_VTABLE||
           !span(c,r32(c,heap+0x30),r32(c,heap+0x34)-r32(c,heap+0x30))||
           r32(c,heap+0x20)!=0||r32(c,heap+0x24)!=0))r=BW_AUTOSAVE_NO_SCRATCH;
        if(r!=BW_AUTOSAVE_OK){reason(r);atomic_fetch_add(&deferred,1);return false;}
        save_caller(c);stack=c->gpr[1];player=scene.player;epoch=ep;generation=gen;
        frame=r32(c,0x803E8140u);slot=r8(c,INFO+0x1290);serial[0]=r32(c,CARD+0x1688);serial[1]=r32(c,CARD+0x168C);
        start_tick=tick;scratch=0;heap_locked=card_locked=queued=slow=abort_before_store=outcome_success=false;
        outcome_reason=BW_AUTOSAVE_OK;reason(BW_AUTOSAVE_OK);
        call(c,BW_AUTOSAVE_HEAP_LOCK,TRY_LOCK,heap+0x18,0,0,0);return true;
    }
    if(phase==BW_AUTOSAVE_IDLE)return false;
    if(c->exception)return false;
    if(c->lr!=BLUEWAKE_AUTOSAVE_RETURN||!same_owner(c)||r32(c,0x803E8140u)!=frame||r32(c,0x803CA74Cu)!=player){
        // No stale PC or RAM restoration. Host must halt/quarantine or replace
        // the machine; reset AFTER replacement invalidates this ownership.
        reason(BW_AUTOSAVE_STALE_OWNER);set_phase(BW_AUTOSAVE_QUARANTINED);return false;
    }
    const u32 result=c->gpr[3];
    switch(phase){
    case BW_AUTOSAVE_HEAP_LOCK:
        if(!result){fail(c,BW_AUTOSAVE_NO_SCRATCH);break;}heap_locked=true;
        if(r32(c,heap+0x20)!=THREAD||r32(c,heap+0x24)!=1){
            reason(BW_AUTOSAVE_STALE_OWNER);set_phase(BW_AUTOSAVE_QUARANTINED);break;
        }
        call(c,BW_AUTOSAVE_HEAP_CAPACITY,HEAP_CAPACITY,heap,32,0,0);break;
    case BW_AUTOSAVE_HEAP_CAPACITY:
        // Conservative room for alignment and allocator block bookkeeping.
        if(result<CARD_BYTES+64u||result>0x1800000u){fail(c,BW_AUTOSAVE_NO_SCRATCH);break;}
        call(c,BW_AUTOSAVE_ALLOCATE,ALLOCATE,CARD_BYTES,32,heap,0);break;
    case BW_AUTOSAVE_ALLOCATE:
        if(!result){fail(c,BW_AUTOSAVE_NO_SCRATCH);break;}
        if(!span(c,result,CARD_BYTES)||(result&31)||result<r32(c,heap+0x30)||
           result>r32(c,heap+0x34)-CARD_BYTES){
            // Unexpected allocator output cannot be safely dereferenced/freed.
            reason(BW_AUTOSAVE_STALE_OWNER);set_phase(BW_AUTOSAVE_QUARANTINED);break;
        }
        scratch=result;call(c,BW_AUTOSAVE_HEAP_UNLOCK,UNLOCK,heap+0x18,0,0,0);break;
    case BW_AUTOSAVE_HEAP_UNLOCK:
        heap_locked=false;if(abort_before_store){cleanup(c);break;}
        call(c,BW_AUTOSAVE_CARD_LOCK,TRY_LOCK,CARD+0x1664,0,0,0);break;
    case BW_AUTOSAVE_CARD_LOCK:
        if(!result){fail(c,BW_AUTOSAVE_CARD_BUSY);break;}card_locked=true;
        if(r32(c,CARD+0x166C)!=THREAD||r32(c,CARD+0x1670)!=1){
            reason(BW_AUTOSAVE_STALE_OWNER);set_phase(BW_AUTOSAVE_QUARANTINED);break;
        }
        if(card_ready(c)!=BW_AUTOSAVE_OK||r8(c,INFO+0x1290)!=slot||
           r32(c,CARD+0x1688)!=serial[0]||r32(c,CARD+0x168C)!=serial[1]){fail(c,BW_AUTOSAVE_WRONG_CARD);break;}
        for(u32 i=0;i<CARD_BYTES;++i)mem_write8(c,scratch+i,r8(c,CARD+i));
        call(c,BW_AUTOSAVE_OLD_CHECKSUM,TEST_CHECKSUM,scratch+slot*LOG_BYTES,0,0,0);break;
    case BW_AUTOSAVE_OLD_CHECKSUM:
        if(!result){fail(c,BW_AUTOSAVE_INVALID_SLOT);break;}
        call(c,BW_AUTOSAVE_PUT_STAGE,PUT_STAGE,INFO,stage_table,0,0);break;
    case BW_AUTOSAVE_PUT_STAGE:call(c,BW_AUTOSAVE_START_STAGE,START_STAGE,0,0,0,0);break;
    case BW_AUTOSAVE_START_STAGE:call(c,BW_AUTOSAVE_SERIALIZE,SERIALIZE,INFO,scratch,slot,0);break;
    case BW_AUTOSAVE_SERIALIZE:
        if(result!=0){fail(c,BW_AUTOSAVE_NATIVE_ERROR);break;}
        call(c,BW_AUTOSAVE_CHECKSUM,CHECKSUM,scratch,slot,0,0);break;
    case BW_AUTOSAVE_CHECKSUM:call(c,BW_AUTOSAVE_STORE,STORE,CARD,scratch,CARD_BYTES,0);break;
    case BW_AUTOSAVE_STORE:
        // The outer recursive lock prevents native worker execution until we
        // verify the exact accepted buffer, STORE command and unchanged slot.
        if(r32(c,CARD+0x166C)!=THREAD||r32(c,CARD+0x1670)!=1||r32(c,CARD+0x165C)!=2||
           r8(c,INFO+0x1290)!=slot||memcmp(c->ram+CARD-0x80000000u,c->ram+scratch-0x80000000u,CARD_BYTES)!=0){
            // STORE may already be queued: do not free or resume mutable game.
            reason(BW_AUTOSAVE_STALE_OWNER);set_phase(BW_AUTOSAVE_QUARANTINED);break;
        }
        queued=true;call(c,BW_AUTOSAVE_CARD_UNLOCK,UNLOCK,CARD+0x1664,0,0,0);break;
    case BW_AUTOSAVE_CARD_UNLOCK:
        card_locked=false;if(abort_before_store){cleanup(c);break;}
        call(c,BW_AUTOSAVE_POLL,POLL,CARD,0,0,0);break;
    case BW_AUTOSAVE_POLL:
        if(!queued){reason(BW_AUTOSAVE_STALE_OWNER);set_phase(BW_AUTOSAVE_QUARANTINED);break;}
        if(result==0){call(c,BW_AUTOSAVE_WAIT,WAIT,0,0,0,0);break;}
        if(result==1){outcome_success=true;outcome_reason=BW_AUTOSAVE_OK;}
        else {outcome_success=false;outcome_reason=BW_AUTOSAVE_NATIVE_ERROR;}
        // Completion is native SaveSync's acknowledgement, not write counters.
        cleanup(c);break;
    case BW_AUTOSAVE_WAIT:call(c,BW_AUTOSAVE_POLL,POLL,CARD,0,0,0);break;
    case BW_AUTOSAVE_FREE:scratch=0;finish(c);break;
    case BW_AUTOSAVE_FPU_REOWN:
        if(!bluewake_fpu_registers_materialized(c)||r32(c,0x800000D4u)!=THREAD){
            reason(BW_AUTOSAVE_FPU_UNAVAILABLE);set_phase(BW_AUTOSAVE_QUARANTINED);break;
        }
        finish(c);break;
    case BW_AUTOSAVE_QUARANTINED:return false;
    default:return false;
    }
    return true;
}
