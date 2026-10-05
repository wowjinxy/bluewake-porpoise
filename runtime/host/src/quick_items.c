// SPDX-License-Identifier: GPL-3.0-or-later
#include "quick_items.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>

/* GZLE01 retail layout, audited against zeldaret/tww. These gestures use real
 * Link procedures and real ship execution. Saved inventory/XYZ/equipment
 * fields are never written. */
enum {
    kGame=0x803C4C08u, kLive=0x803CA7DBu, kPad=0x803A4DF0u,
    kPlayer=0x803CA74Cu, kLink=0x803CA754u, kShip=0x803CA75Cu,
    kStage=0x803C9D3Cu, kRoom=0x803F6A78u, kPause=0x803F7097u,
    kEvent=0x803C9EA2u, kNextStage=0x803C9D54u, kOverlap=0x803F6160u,
    kCurse=0x803F62B8u, kPlayerStatus=0x803CA8D0u, kCounter=0x803E8140u,
    kWait=0x80113044u, kFreeWait=0x801134A0u, kMove=0x80113628u,
    kSteer=0x8014123Cu, kPaddle=0x801413A4u,
    kCannon=0x80142300u, kCrane=0x80142458u,
    kTactInit=0x8014DBECu, kCannonInit=0x80142250u, kCraneInit=0x80142388u,
    kExecute=0x80121870u, kExecuteEnd=0x80122D30u,
    kUp=0x1000u, kLeft=0x8000u, kRight=0x4000u, kDpad=0xF000u,
    kX=0x0040u, kXYZ=0x0860u
};
static const u32 kRefused = 0x00000002u|0x00000008u|0x00000010u|0x00000020u|
    0x00000200u|0x00000800u|0x00010000u|0x00040000u|0x00100000u|
    0x00200000u|0x00400000u|0x00800000u|0x01000000u|0x10000000u|
    0x20000000u|0x80000000u;
typedef enum Shortcut {NONE,TACT,CANNON,CRANE} Shortcut;
typedef struct Context {u32 player,ship,proc,modes; bool boat;} Context;
typedef struct Overlay {
    bool active;
    u8* ram;
    u32 ram_size,stack,ship,player;
    u64 input_frame;
    u8 stage[8],room,selections[3];
    u8 original_item,item;
    u16 original_hold,original_trigger,hold,trigger;
} Overlay;
static atomic_ullong g_config = 0;
static CPUState* g_cpu;
static u8* g_ram;
static u32 g_ram_size,g_scene_player,g_scene_ship;
static u8 g_stage[8],g_room;
static bool g_scene_known,g_modifier,g_blocked,g_have_packet,g_crane_held,g_fire;
static u16 g_modifier_buttons,g_release_guard,g_owned_buttons;
static u64 g_config_seen,g_generation,g_sample,g_deadline,g_input_frame;
static u32 g_packet_written,g_packet_counter;
static Shortcut g_pending;
static Overlay g_overlay;
static BluewakeQuickItemsStats g_stats;

bool bluewake_quick_items_enabled(void) {
    return (atomic_load_explicit(&g_config,memory_order_acquire)&1u)!=0;
}
void bluewake_quick_items_configure(bool enabled) {
    u64 previous=atomic_load_explicit(&g_config,memory_order_acquire),published;
    do {published=((previous+2u)&~1ull)|(enabled?1u:0u);}
    while(!atomic_compare_exchange_weak_explicit(&g_config,&previous,published,
                                               memory_order_acq_rel,memory_order_acquire));
}
static bool span(const CPUState* cpu,u32 address,u32 length) {
    const u32 offset=address-0x80000000u;
    return cpu&&cpu->ram&&offset<=cpu->ram_size&&length<=cpu->ram_size-offset;
}
static bool method_span(CPUState* cpu,u32 address) {
    // Linked REL method tables keep their authentic uncached address and may
    // use registered backing outside flat RAM. Match the native memory helper
    // without changing the fixed MEM1 checks for the player, ship or stack.
    const u32 canonical=address&~0x40000000u;
    if(!cpu||!cpu->ram||(address&3u)||canonical<0x80000000u||canonical>0x90000000u-0x14u)return false;
    const u32 generation=g_ppc_guest_alias_generation;
    const u8* methods=get_ram_ptr(cpu,address,0x14u,NULL);
    if(!methods)return false;
    for(u32 i=0;i<0x14u;++i)
        if(generation!=g_ppc_guest_alias_generation||get_ram_ptr(cpu,address+i,1u,NULL)!=methods+i)return false;
    // An unregistered REL table otherwise falls through to zeroed flat RAM.
    const u32 execute=read_be32(methods+8u);
    return execute!=0&&(execute&3u)==0&&generation==g_ppc_guest_alias_generation;
}
static float read_float(CPUState* cpu,u32 address) {
    const u32 bits=mem_read32(cpu,address);float value;memcpy(&value,&bits,sizeof value);return value;
}
static void restore_overlay(CPUState* cpu) {
    if(!g_overlay.active)return;
    // Native writers take precedence. Never carry old bytes into restored RAM.
    bool same_scene=cpu==g_cpu&&cpu&&cpu->ram==g_overlay.ram&&cpu->ram_size==g_overlay.ram_size;
    if(same_scene) {
        same_scene=mem_read32(cpu,kPlayer)==g_overlay.player&&mem_read32(cpu,kShip)==g_overlay.ship&&
            mem_read8(cpu,kRoom)==g_overlay.room;
        for(unsigned i=0;i<8;++i)same_scene&=mem_read8(cpu,kStage+i)==g_overlay.stage[i];
        for(unsigned i=0;i<3;++i)same_scene&=mem_read8(cpu,kGame+9u+i)==g_overlay.selections[i];
    }
    if(same_scene) {
        if(mem_read8(cpu,kLive)==g_overlay.item)mem_write8(cpu,kLive,g_overlay.original_item);
        if(g_input_frame==g_overlay.input_frame) {
            if(mem_read16(cpu,kPad+0x30u)==g_overlay.hold)mem_write16(cpu,kPad+0x30u,g_overlay.original_hold);
            if(mem_read16(cpu,kPad+0x32u)==g_overlay.trigger)mem_write16(cpu,kPad+0x32u,g_overlay.original_trigger);
        }
        ++g_stats.overlay_restores;
    }
    g_overlay.active=false;
}
static void cancel(void) {
    if(g_pending!=NONE||g_crane_held||g_fire)++g_stats.cancelled;
    g_pending=NONE;g_crane_held=g_fire=false;g_release_guard=kDpad;g_have_packet=false;
}
void bluewake_quick_items_reset(CPUState* cpu) {
    // Called AFTER state/game/module/memory replacement. New bytes win; old
    // overlay bytes must never be restored into the newly restored state.
    g_overlay.active=false;
    g_cpu=cpu;g_ram=cpu?cpu->ram:NULL;g_ram_size=cpu?cpu->ram_size:0;
    g_scene_known=false;g_owned_buttons=0;cancel();
}
void bluewake_quick_items_attach(CPUState* cpu) {
    g_config_seen=atomic_load_explicit(&g_config,memory_order_acquire);
    g_generation=g_sample=g_input_frame=0;g_modifier=false;g_modifier_buttons=0;g_blocked=true;
    memset(&g_stats,0,sizeof g_stats);bluewake_quick_items_reset(cpu);
}
void bluewake_quick_items_input(bool modifier_down,bool blocked,u64 generation,u16 native_modifier_buttons) {
    ++g_sample;
    if(generation!=g_generation||blocked||(g_modifier&&!modifier_down)) {
        cancel(); // preserve an in-flight native ship invocation until its return
    }
    g_generation=generation;g_modifier=modifier_down;g_blocked=blocked;
    g_modifier_buttons=modifier_down?native_modifier_buttons:0;
    if((g_pending!=NONE||g_fire)&&g_sample>g_deadline)cancel();
}
bool bluewake_quick_items_busy(void) {return g_overlay.active;}
void bluewake_quick_items_stats(BluewakeQuickItemsStats* out) {if(out)*out=g_stats;}
static bool proc_address(u32 address) {
    return address==kWait||address==kFreeWait||address==kMove||address==kSteer||
           address==kPaddle||address==kCannon||address==kCrane;
}
bool bluewake_quick_items_observes(u32 address) {
    address&=~0x40000000u;
    // This predicate is in every optimized direct call. Reject ordinary game
    // code before reading configuration or touching input/guest memory.
    if(address!=BLUEWAKE_QUICK_ITEMS_FRAME&&address!=BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY&&
       address!=BLUEWAKE_QUICK_ITEMS_SHIP_RETURN&&address!=kExecute&&!proc_address(address))return false;
    if(g_overlay.active&&(address==BLUEWAKE_QUICK_ITEMS_SHIP_RETURN||
       address==BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY||address==kExecute||proc_address(address)))return true;
    if(address==BLUEWAKE_QUICK_ITEMS_SHIP_RETURN||address==kExecute)return false;
    if(proc_address(address)&&g_pending==NONE)return false;
    if(address==BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY&&!g_crane_held&&!g_fire)return false;
    if(!bluewake_quick_items_enabled())return false;
    return address==BLUEWAKE_QUICK_ITEMS_FRAME||
        ((g_crane_held||g_fire)&&address==BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY)||
        (g_pending!=NONE&&proc_address(address));
}
static bool equipment_busy(CPUState* cpu,u32 player) {
    const u16 upper=mem_read16(cpu,player+0x301Cu);
    // Native checkEquipAnime / checkUpperGuardAnime indices in retail LkAnm.
    return upper==0xD7||upper==0x103||upper==0x104||upper==0x105||upper==0x106||
           upper==0x16||upper==0x1B;
}
static bool context(CPUState* cpu,Context* out) {
    if(!span(cpu,kGame,0x5CDC)||!span(cpu,kPad,0x3C)||!span(cpu,kPause,1)||
       !span(cpu,kRoom,1)||!span(cpu,kOverlap,4)||!span(cpu,kCurse,2)||!span(cpu,kCounter,4))return false;
    out->player=mem_read32(cpu,kPlayer);out->ship=mem_read32(cpu,kShip);
    if(out->player==0||out->player!=mem_read32(cpu,kLink)||(out->player&3u)||
       !span(cpu,out->player,0x361C)||mem_read32(cpu,out->player+0x498u)!=out->player+0x1F8u)return false;
    u8 stage[8];for(unsigned i=0;i<8;++i)stage[i]=mem_read8(cpu,kStage+i);
    const u8 room=mem_read8(cpu,kRoom);
    if(!g_scene_known||out->player!=g_scene_player||out->ship!=g_scene_ship||
       room!=g_room||memcmp(stage,g_stage,sizeof stage)!=0) {
        cancel();g_owned_buttons=0;
        memcpy(g_stage,stage,sizeof stage);g_room=room;g_scene_player=out->player;
        g_scene_ship=out->ship;g_scene_known=true;
    }
    out->proc=mem_read32(cpu,out->player+0x31D8u);
    out->modes=mem_read32(cpu,out->player+0x3618u);
    out->boat=(mem_read32(cpu,kPlayerStatus)&0x10000u)!=0;
    return stage[0]!=0&&room<0x80&&mem_read8(cpu,kPause)==0&&mem_read8(cpu,kEvent)==0&&
        mem_read8(cpu,kNextStage)==0&&mem_read32(cpu,kOverlap)==0&&
        mem_read16(cpu,kCurse)!=1&&mem_read16(cpu,out->player+0x304u)==0&&
        mem_read32(cpu,out->player+0x314u)==0&&mem_read8(cpu,kLive-9u)==0&&
        mem_read8(cpu,kLive-8u)==0&&mem_read8(cpu,kGame+0x12A0u+0x4A3Au)==0&&
        mem_read16(cpu,out->player+0x354Cu)==0&&(out->modes&kRefused)==0;
}
static bool boat_context(CPUState* cpu,const Context* c,bool crane_transition) {
    if(!c->boat||(c->modes&0x2000u)==0||memcmp(g_stage,"sea\0\0\0\0\0",8)!=0||
       !span(cpu,c->ship,0x1074)||(c->ship&3u)||mem_read16(cpu,c->ship+8u)!=0xA7)return false;
    const u8 mode=mem_read8(cpu,c->ship+0x34Cu),next=mem_read8(cpu,c->ship+0x34Du);
    if((mode!=1&&mode!=2&&mode!=9&&mode!=10)||
       (next!=mode&&!(crane_transition&&c->proc==0x8F&&next==10))||
       // daShip_c::execute resets this demo ID to -1 (u8 0xFF) outside
       // events. Zero is DEMO_INIT, not ordinary boat control.
       mem_read8(cpu,c->ship+0x351u)!=0xFF||(mem_read32(cpu,c->ship+0x358u)&1u)||
       mem_read32(cpu,c->ship+0x41Cu)!=0||mem_read32(cpu,c->ship+0x424u)!=0)return false;
    return c->proc==0x88||c->proc==0x89||c->proc==0x8E||c->proc==0x8F;
}
static bool usable(CPUState* cpu,const Context* c,Shortcut action) {
    const u32 slot=action==TACT?2u:action==CANNON?13u:3u;
    const u8 item=action==TACT?0x22:action==CANNON?0x31:0x25;
    if(mem_read8(cpu,kGame+0x3Cu+slot)!=item||equipment_busy(cpu,c->player)||
       mem_read32(cpu,c->player+0x3190u)!=0||mem_read32(cpu,c->player+0x3188u)!=0)return false;
    const float grab=read_float(cpu,c->player+0x2B0u);
    if(!isfinite(grab)||grab<0.0f)return false;
    if(c->boat)return boat_context(cpu,c,false);
    return action==TACT&&(c->modes&0x2000u)==0&&(c->modes&4u)!=0&&
        (c->proc==4||c->proc==5||c->proc==6)&&
        (mem_read32(cpu,c->player+0x494u)&0x20u)!=0&&
        (mem_read8(cpu,kGame+0x12A0u+0x4942u)&0x2Cu)!=0;
}
static bool native_item_priority(CPUState* cpu) {
    return ((mem_read16(cpu,kPad+0x30u)|mem_read16(cpu,kPad+0x32u))&kXYZ&~g_owned_buttons&~g_modifier_buttons)!=0;
}
static void frame(CPUState* cpu,const Context* c) {
    ++g_input_frame;
    if(g_overlay.active) {restore_overlay(cpu);cancel();return;} // lost wrapper return: fail closed
    u16 hold=mem_read16(cpu,kPad+0x30u),trigger=mem_read16(cpu,kPad+0x32u);
    const u32 packet=((u32)hold<<16)|trigger;
    // fapGm_Execute increments this native counter after actor management.
    // A budget-deferred entry can replay after VI without a new native packet;
    // the native frame token keeps that owned packet idempotent across VI.
    const u32 native_frame=mem_read32(cpu,kCounter);
    if(g_have_packet&&g_packet_counter==native_frame&&packet==g_packet_written)return;
    g_release_guard&=hold&kDpad;g_owned_buttons&=hold;
    const u16 pending_direction=g_pending==TACT?kUp:g_pending==CANNON?kLeft:kRight;
    if(g_pending!=NONE&&(!g_modifier||(hold&pending_direction)==0||native_item_priority(cpu)))cancel();
    if(g_fire&&(!g_modifier||(hold&kLeft)==0||native_item_priority(cpu)))g_fire=false;
    if(g_crane_held&&(!g_modifier||(hold&kRight)==0||native_item_priority(cpu)||
       mem_read8(cpu,kGame+0x3Cu+3u)!=0x25||!boat_context(cpu,c,true)))g_crane_held=false;
    const u16 directions=hold&kDpad&~g_modifier_buttons;
    const u16 candidate=trigger&directions&~g_release_guard;
    if(g_modifier&&!native_item_priority(cpu)&&candidate!=0&&
       (candidate&(candidate-1u))==0&&(directions&(directions-1u))==0) {
        const Shortcut action=candidate==kUp?TACT:candidate==kLeft?CANNON:candidate==kRight?CRANE:NONE;
        if(action!=NONE) {
            // Unavailable fixed shortcuts never fall through to the map.
            // Down is intentionally never owned.
            g_owned_buttons=candidate|g_modifier_buttons;g_release_guard|=candidate;
            if(usable(cpu,c,action)) {
                ++g_stats.accepted;
                g_deadline=g_sample+6u;
                if(action==CANNON&&c->proc==0x8E)g_fire=true;
                else if(action!=CRANE||c->proc!=0x8F) {g_pending=action;g_deadline=g_sample+6u;}
                if(action==CRANE)g_crane_held=true;
            }
        }
    }
    hold&=~g_owned_buttons;trigger&=~g_owned_buttons;
    mem_write16(cpu,kPad+0x30u,hold);mem_write16(cpu,kPad+0x32u,trigger);
    g_packet_written=((u32)hold<<16)|trigger;g_packet_counter=native_frame;g_have_packet=true;
}
static bool matching_proc(CPUState* cpu,u32 address,const Context* c) {
    const u32 proc=address==kWait?4u:address==kFreeWait?5u:address==kMove?6u:
                   address==kSteer?0x88u:address==kPaddle?0x89u:address==kCannon?0x8Eu:0x8Fu;
    return cpu->gpr[3]==c->player&&(cpu->ctr&~3u)==address&&cpu->lr>=kExecute&&cpu->lr<kExecuteEnd&&
        c->proc==proc&&mem_read32(cpu,c->player+0x31E4u)==address;
}
static void begin_ship(CPUState* cpu,const Context* c) {
    const bool cannon=g_fire&&c->proc==0x8E,crane=g_crane_held&&c->proc==0x8F;
    if((!cannon&&!crane)||!boat_context(cpu,c,crane)||native_item_priority(cpu)||
       cpu->lr!=BLUEWAKE_QUICK_ITEMS_SHIP_RETURN||cpu->gpr[4]!=c->ship||
       !span(cpu,c->ship+0xECu,4)||cpu->gpr[3]!=mem_read32(cpu,c->ship+0xECu)||
       !method_span(cpu,cpu->gpr[3]))return;
    g_overlay=(Overlay){.active=true,.ram=cpu->ram,.ram_size=cpu->ram_size,
        .stack=cpu->gpr[1],.ship=c->ship,.player=c->player,.room=g_room,.input_frame=g_input_frame,
        .original_item=mem_read8(cpu,kLive),
        .item=cannon?0x31:0x25,.original_hold=mem_read16(cpu,kPad+0x30u),
        .original_trigger=mem_read16(cpu,kPad+0x32u)};
    memcpy(g_overlay.stage,g_stage,sizeof g_stage);
    for(unsigned i=0;i<3;++i)g_overlay.selections[i]=mem_read8(cpu,kGame+9u+i);
    g_overlay.hold=g_overlay.original_hold|kX;
    g_overlay.trigger=g_overlay.original_trigger|(cannon?kX:0u);
    mem_write8(cpu,kLive,g_overlay.item);mem_write16(cpu,kPad+0x30u,g_overlay.hold);
    mem_write16(cpu,kPad+0x32u,g_overlay.trigger);
    if(cannon){g_fire=false;++g_stats.cannon_operations;}else ++g_stats.crane_operations;
}
void bluewake_quick_items_dispatch(CPUState* cpu,u32 address) {
    address&=~0x40000000u;
    if(cpu!=g_cpu||!cpu||cpu->ram!=g_ram||cpu->ram_size!=g_ram_size) {bluewake_quick_items_reset(cpu);return;}
    // The host services many unrelated edges. Only shortcut boundaries need
    // configuration loads or validated guest context scans.
    if(address!=BLUEWAKE_QUICK_ITEMS_FRAME&&address!=BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY&&
       address!=BLUEWAKE_QUICK_ITEMS_SHIP_RETURN&&address!=kExecute&&!proc_address(address))return;
    if(g_overlay.active&&address==BLUEWAKE_QUICK_ITEMS_SHIP_RETURN) {
        const bool expected=cpu->gpr[1]==g_overlay.stack;
        restore_overlay(cpu);if(!expected)cancel();return;
    }
    if(g_overlay.active&&address==BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY&&
       cpu->gpr[1]==g_overlay.stack&&cpu->gpr[4]==g_overlay.ship&&
       cpu->gpr[3]==mem_read32(cpu,g_overlay.ship+0xECu)&&
       cpu->lr==BLUEWAKE_QUICK_ITEMS_SHIP_RETURN)return; // replay of the same budget-deferred entry
    if(g_overlay.active&&(proc_address(address)||address==kExecute||address==BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY)) {
        restore_overlay(cpu);cancel();return; // unexpected reentry/unwind cannot expose overlay to Link
    }
    const u64 configuration=atomic_load_explicit(&g_config,memory_order_acquire);
    if(configuration!=g_config_seen){cancel();g_config_seen=configuration;}
    if((configuration&1u)==0)return;
    Context c;
    if(g_blocked||cpu->exception!=0||!context(cpu,&c)){cancel();return;}
    if(address==BLUEWAKE_QUICK_ITEMS_FRAME){frame(cpu,&c);return;}
    if(address==BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY){begin_ship(cpu,&c);return;}
    if(g_pending==NONE||!proc_address(address)||!matching_proc(cpu,address,&c))return;
    const Shortcut action=g_pending;g_pending=NONE;
    if(g_sample>g_deadline||!g_modifier||native_item_priority(cpu)||!usable(cpu,&c,action)){cancel();return;}
    cpu->pc=action==TACT?kTactInit:action==CANNON?kCannonInit:kCraneInit;
    if(action==TACT)cpu->gpr[4]=0xFFFFFFFFu;
    ++g_stats.native_handoffs;
}
