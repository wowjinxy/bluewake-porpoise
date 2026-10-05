// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "quick_items.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static CPUState cpu;
static const u32 player=0x80500000u,ship=0x80510000u,methods=0x80401000u;
static const u32 game=0x803C4C08u,live=0x803CA7DBu,pad=0x803A4DF0u;
static const u16 up=0x1000,right=0x4000,down=0x2000,left=0x8000,x=0x40,y=0x20,z=0x800;
static u64 generation=1;
static u32 proc_address;
static void packet(u16 hold,u16 trigger) {mem_write16(&cpu,pad+0x30,hold);mem_write16(&cpu,pad+0x32,trigger);}
static void frame(bool modifier,u16 hold,u16 trigger,u16 source) {
    // Model a new native fapGm invocation, whose preceding invocation completed
    // actor management and incremented g_Counter.mCounter0.
    mem_write32(&cpu,0x803E8140,mem_read32(&cpu,0x803E8140)+1u);
    bluewake_quick_items_input(modifier,false,generation,source);packet(hold,trigger);
    bluewake_quick_items_dispatch(&cpu,BLUEWAKE_QUICK_ITEMS_FRAME);
}
static u16 held(void) {return mem_read16(&cpu,pad+0x30);}
static u16 triggered(void) {return mem_read16(&cpu,pad+0x32);}
static void set_proc(u32 id,u32 entry) {
    mem_write32(&cpu,player+0x31D8,id);mem_write32(&cpu,player+0x31E4,entry);proc_address=entry;
}
static void assignments(void) {
    assert(mem_read8(&cpu,game+9)==8&&mem_read8(&cpu,game+10)==12&&mem_read8(&cpu,game+11)==14);
    assert(mem_read8(&cpu,live)==0x23&&mem_read8(&cpu,live+1)==0x27&&mem_read8(&cpu,live+2)==0x50);
}
static void fixture(bool boat,u32 proc) {
    memset(cpu.ram,0,cpu.ram_size);cpu.exception=0;cpu.pc=0;cpu.ctr=0;cpu.lr=0;cpu.gpr[1]=0x817FF000;
    mem_write32(&cpu,0x803CA74C,player);mem_write32(&cpu,0x803CA754,player);mem_write32(&cpu,0x803CA75C,ship);
    mem_write32(&cpu,player+0x498,player+0x1F8);mem_write32(&cpu,player+0x494,0x20);
    mem_write16(&cpu,player+0x301C,0xFFFF);mem_write32(&cpu,player+0x3618,boat?0x2004:4);
    mem_write8(&cpu,game+0x12A0+0x4942,0x2C);mem_write32(&cpu,0x803CA8D0,boat?0x10000:0);
    memcpy(cpu.ram+(0x803C9D3C-0x80000000),"sea",4);
    memset(cpu.ram+(game-0x80000000)+0x3C,0xFF,21);
    mem_write8(&cpu,game+0x3C+2,0x22);mem_write8(&cpu,game+0x3C+3,0x25);mem_write8(&cpu,game+0x3C+13,0x31);
    mem_write8(&cpu,game+9,8);mem_write8(&cpu,game+10,12);mem_write8(&cpu,game+11,14);
    mem_write8(&cpu,live,0x23);mem_write8(&cpu,live+1,0x27);mem_write8(&cpu,live+2,0x50);
    mem_write16(&cpu,ship+8,0xA7);mem_write32(&cpu,ship+0xEC,methods);mem_write32(&cpu,methods+8,0x80A04000);
    const u8 mode=proc==0x8E?9:proc==0x8F?10:2;
    mem_write8(&cpu,ship+0x34C,mode);mem_write8(&cpu,ship+0x34D,mode);
    mem_write8(&cpu,ship+0x351,0xFF); // native execute: no active demo
    set_proc(proc,proc==4?0x80113044:proc==5?0x801134A0:proc==6?0x80113628:
                  proc==0x88?0x8014123C:proc==0x89?0x801413A4:proc==0x8E?0x80142300:0x80142458);
    bluewake_quick_items_configure(true);bluewake_quick_items_attach(&cpu);frame(false,0,0,0);
}
static void enter_proc(void) {
    cpu.gpr[3]=player;cpu.gpr[4]=0x12345678;cpu.ctr=proc_address;cpu.lr=0x80122500;cpu.pc=proc_address;
    bluewake_quick_items_dispatch(&cpu,proc_address);
}
static void begin_ship(void) {
    cpu.gpr[3]=methods;cpu.gpr[4]=ship;cpu.lr=BLUEWAKE_QUICK_ITEMS_SHIP_RETURN;
    bluewake_quick_items_dispatch(&cpu,BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY);
}
static void end_ship(void) {bluewake_quick_items_dispatch(&cpu,BLUEWAKE_QUICK_ITEMS_SHIP_RETURN);}
static void native_priority(void) {
    for(unsigned b=0;b<2;++b) {
        fixture(false,4);const u16 button=b?y:x;
        frame(true,up|z|button,up|z|button,z);enter_proc();
        assert(cpu.pc==proc_address&&held()==(up|z|button)&&triggered()==(up|z|button));assignments();
    }
}
int main(void) {
    assert(cpu_init(&cpu));assert(!bluewake_quick_items_enabled());
    fixture(false,4);
    frame(false,up|z,up|z,0);assert(held()==(up|z)&&triggered()==(up|z));assignments();
    frame(true,down|z,down|z,z);assert(held()==(down|z)&&triggered()==(down|z));assignments();
    frame(false,0,0,0);
    frame(true,up|z,up|z,z);assert(held()==0&&triggered()==0);
    assert(bluewake_quick_items_observes(0xC0113044));
    BluewakeQuickItemsStats stats;bluewake_quick_items_stats(&stats);assert(stats.accepted==1);
    bluewake_quick_items_dispatch(&cpu,BLUEWAKE_QUICK_ITEMS_FRAME);
    bluewake_quick_items_stats(&stats);assert(stats.accepted==1);
    // The same budget-deferred fap entry can resume across VI with its already
    // owned/stripped native packet; this must not cancel the pending handoff.
    bluewake_quick_items_input(true,false,generation,z);
    bluewake_quick_items_dispatch(&cpu,BLUEWAKE_QUICK_ITEMS_FRAME);
    bluewake_quick_items_stats(&stats);assert(stats.accepted==1);
    const s64 count=cpu.downcount;enter_proc();assert(cpu.pc==0x8014DBEC&&cpu.gpr[4]==0xFFFFFFFF);
    assert(cpu.gpr[3]==player&&cpu.lr==0x80122500&&cpu.ctr==proc_address&&cpu.downcount==count);
    assignments();assert(mem_read16(&cpu,player+0x3560)==0); // native initializer owns equipment lifecycle
    for(u32 p=4;p<=6;++p) {fixture(false,p);frame(true,up,up,0);enter_proc();assert(cpu.pc==0x8014DBEC);assignments();}
    native_priority();
    fixture(false,4);frame(true,up|right|z,up|right|z,z);enter_proc();assert(cpu.pc==proc_address);
    fixture(false,4);frame(true,left|z,left|z,z);enter_proc();assert(cpu.pc==proc_address);assignments();
    fixture(false,4);mem_write8(&cpu,game+0x3C+2,0xFF);frame(true,up|z,up|z,z);enter_proc();assert(cpu.pc==proc_address&&held()==0);assignments();
    // Native calls require the real member-function boundary, not a matching PC alone.
    fixture(false,4);frame(true,up,up,0);cpu.gpr[3]=player;cpu.ctr=proc_address;cpu.lr=0x80001000;cpu.pc=proc_address;
    bluewake_quick_items_dispatch(&cpu,proc_address);assert(cpu.pc==proc_address);
    cpu.lr=0x80122500;cpu.gpr[3]=player+4;bluewake_quick_items_dispatch(&cpu,proc_address);assert(cpu.pc==proc_address);
    enter_proc();assert(cpu.pc==0x8014DBEC);
    // First boat Left deploys through the scheduled native Link initializer, no shot.
    fixture(true,0x89);frame(true,left|z,left|z,z);enter_proc();assert(cpu.pc==0x80142250);
    begin_ship();assert(!bluewake_quick_items_busy());assignments();
    fixture(true,0x88);frame(true,up|z,up|z,z);enter_proc();assert(cpu.pc==0x8014DBEC);assignments();
    // Numeric native demo IDs, including DEMO_INIT (zero), cannot accept a
    // shortcut even when the rest of the sampled context looks ordinary.
    const u8 demos[]={0,1,2,3,4,5,6,7,8,9,10,11,0xFE};
    const u16 boat_buttons[]={up,left,right};
    for(unsigned d=0;d<sizeof demos/sizeof demos[0];++d) {
        for(unsigned b=0;b<sizeof boat_buttons/sizeof boat_buttons[0];++b) {
            fixture(true,0x89);mem_write8(&cpu,ship+0x351,demos[d]);
            frame(true,boat_buttons[b]|z,boat_buttons[b]|z,z);enter_proc();
            bluewake_quick_items_stats(&stats);
            assert(cpu.pc==proc_address&&stats.accepted==0&&stats.native_handoffs==0);
            assert(!bluewake_quick_items_busy());assignments();
        }
    }
    // Native demo ownership can change between the input packet and the Link
    // boundary. Recheck it before handing off to the native initializer.
    fixture(true,0x89);frame(true,right|z,right|z,z);
    mem_write8(&cpu,ship+0x351,0);enter_proc();assert(cpu.pc==proc_address);assignments();
    // A fresh Left in cannon mode overlays only the real ship execute invocation.
    // Real linked Ship methods live at an uncached REL address with registered
    // backing; normalized flat RAM can remain zero. Preserve native arguments.
    fixture(true,0x8E);
    const u32 linked_methods=0xC1B1E108u;
    u8 method_backing[0x14]={0};write_be32(method_backing+8,0xC1B1B8D4u);
    assert(ppc_guest_alias_add_shared(linked_methods,sizeof method_backing,method_backing));
    mem_write32(&cpu,ship+0xEC,linked_methods);frame(true,left|z,left|z,z);
    cpu.gpr[3]=linked_methods;cpu.gpr[4]=ship;cpu.lr=BLUEWAKE_QUICK_ITEMS_SHIP_RETURN;
    bluewake_quick_items_dispatch(&cpu,BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY);
    assert(bluewake_quick_items_busy()&&cpu.gpr[3]==linked_methods&&cpu.gpr[4]==ship);
    assert(held()==x&&triggered()==x);end_ship();assignments();ppc_guest_alias_clear();
    // Missing, partial or invalid method backing cannot start an overlay.
    for(unsigned invalid=0;invalid<5;++invalid) {
        fixture(true,0x8E);
        const u32 bad=invalid==0?0u:invalid==1?0xC2000000u:linked_methods;
        if(invalid==3)assert(ppc_guest_alias_add_shared(linked_methods+8u,1u,method_backing+8));
        if(invalid==4){memset(method_backing,0,sizeof method_backing);assert(ppc_guest_alias_add_shared(linked_methods,sizeof method_backing,method_backing));}
        mem_write32(&cpu,ship+0xEC,bad);frame(true,left|z,left|z,z);
        cpu.gpr[3]=bad;cpu.gpr[4]=ship;cpu.lr=BLUEWAKE_QUICK_ITEMS_SHIP_RETURN;
        bluewake_quick_items_dispatch(&cpu,BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY);
        assert(!bluewake_quick_items_busy()&&held()==0&&triggered()==0);assignments();ppc_guest_alias_clear();
    }
    fixture(true,0x8E);frame(true,left|z,left|z,z);
    cpu.gpr[3]=methods;cpu.gpr[4]=ship;cpu.lr=BLUEWAKE_QUICK_ITEMS_SHIP_RETURN+4;
    bluewake_quick_items_dispatch(&cpu,BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY);assert(!bluewake_quick_items_busy());
    begin_ship();assert(bluewake_quick_items_busy()&&mem_read8(&cpu,live)==0x31&&held()==x&&triggered()==x);
    begin_ship();assert(bluewake_quick_items_busy()&&mem_read8(&cpu,live)==0x31&&triggered()==x);
    bluewake_quick_items_stats(&stats);assert(stats.cannon_operations==1); // same entry replay is idempotent
    assert(mem_read8(&cpu,game+9)==8&&mem_read8(&cpu,live+1)==0x27&&mem_read8(&cpu,live+2)==0x50);
    end_ship();assert(!bluewake_quick_items_busy()&&held()==0&&triggered()==0);assignments();
    begin_ship();assert(!bluewake_quick_items_busy()); // one native shot trigger, no autorepeat
    frame(true,left|z,0,z);begin_ship();assert(!bluewake_quick_items_busy());
    frame(true,z,0,z);frame(true,left|z,left,z);begin_ship();assert(bluewake_quick_items_busy());end_ship();assignments();
    // Crane deployment keeps a held gesture through native part deployment.
    fixture(true,0x89);frame(true,right|z,right|z,z);enter_proc();assert(cpu.pc==0x80142388);
    set_proc(0x8F,0x80142458);mem_write8(&cpu,ship+0x34D,10); // model a completed native Link init only
    frame(true,right|z,0,z);begin_ship();assert(bluewake_quick_items_busy()&&held()==x&&triggered()==0);
    end_ship();assignments();mem_write8(&cpu,ship+0x34C,10);
    frame(true,right|z,0,z);begin_ship();assert(bluewake_quick_items_busy());end_ship();assignments();
    mem_write8(&cpu,ship+0x351,2); // native salvage demo takes control
    frame(true,right|z,0,z);begin_ship();assert(!bluewake_quick_items_busy());assignments();
    mem_write8(&cpu,ship+0x351,0xFF);
    frame(true,right|z,0,z);begin_ship();assert(!bluewake_quick_items_busy());assignments();
    frame(true,z,0,z);begin_ship();assert(!bluewake_quick_items_busy()); // release -> native raises
    fixture(true,0x8F);frame(true,right|z,right|z,z);frame(true,right|z|y,y,z);begin_ship();assert(!bluewake_quick_items_busy());assignments();
    // VI/menu/config cancellation preserves an in-flight invocation to its return.
    fixture(true,0x8E);frame(true,left|z,left|z,z);begin_ship();
    bluewake_quick_items_input(false,true,++generation,0);assert(bluewake_quick_items_busy()&&mem_read8(&cpu,live)==0x31);
    bluewake_quick_items_configure(false);assert(bluewake_quick_items_observes(BLUEWAKE_QUICK_ITEMS_SHIP_RETURN));
    assert(bluewake_quick_items_observes(BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY));
    assert(bluewake_quick_items_observes(0x80121870)&&bluewake_quick_items_observes(proc_address));
    bluewake_quick_items_dispatch(&cpu,0x80004000); // unrelated edges preserve the armed invocation
    assert(bluewake_quick_items_busy()&&mem_read8(&cpu,live)==0x31);
    end_ship();assert(!bluewake_quick_items_busy());assignments();
    // A changed scene/save identity is authoritative; don't inject stale old X.
    fixture(true,0x8E);frame(true,left|z,left|z,z);begin_ship();
    mem_write8(&cpu,0x803F6A78,1);mem_write8(&cpu,live,0x27);end_ship();assert(mem_read8(&cpu,live)==0x27);
    fixture(true,0x8E);frame(true,left|z,left|z,z);begin_ship();
    mem_write8(&cpu,live,0x20);mem_write8(&cpu,game+9,0);end_ship();assert(mem_read8(&cpu,live)==0x20&&mem_read8(&cpu,game+9)==0);
    // Explicit reset AFTER state restoration never writes old overlay bytes back.
    fixture(true,0x8E);frame(true,left|z,left|z,z);begin_ship();mem_write8(&cpu,live,0x50);
    bluewake_quick_items_reset(&cpu);assert(!bluewake_quick_items_busy()&&mem_read8(&cpu,live)==0x50);
    // Unexpected Link reentry restores the overlay before Link sees it.
    fixture(true,0x8E);frame(true,left|z,left|z,z);begin_ship();
    assert(bluewake_quick_items_observes(0x80121870));bluewake_quick_items_dispatch(&cpu,0x80121870);
    assert(!bluewake_quick_items_busy());assignments();
    const u32 barriers[]={0x803F7097,0x803C9EA2,0x803C9D54,live-9,live-8,game+0x12A0+0x4A3A};
    for(unsigned i=0;i<sizeof barriers/sizeof barriers[0];++i) {
        fixture(false,4);mem_write8(&cpu,barriers[i],1);frame(true,up|z,up|z,z);enter_proc();assert(cpu.pc==proc_address);
        mem_write8(&cpu,barriers[i],0);frame(true,up|z,up|z,z);enter_proc();assert(cpu.pc==proc_address);
        frame(false,0,0,0);frame(true,up|z,up|z,z);enter_proc();assert(cpu.pc==0x8014DBEC);assignments();
    }
    fixture(false,4);mem_write16(&cpu,0x803F62B8,1);frame(true,up,up,0);enter_proc();assert(cpu.pc==proc_address);
    fixture(false,4);mem_write32(&cpu,player+0x3618,0x20000004);frame(true,up,up,0);enter_proc();assert(cpu.pc==proc_address);
    fixture(true,0x89);mem_write32(&cpu,ship+0x358,1);frame(true,left,left,0);enter_proc();assert(cpu.pc==proc_address);
    fixture(true,0x89);mem_write32(&cpu,ship+0x41C,0x80520000);frame(true,right,right,0);enter_proc();assert(cpu.pc==proc_address);
    fixture(false,4);frame(true,up,up,0);mem_write32(&cpu,0x803CA74C,player+4);enter_proc();assert(cpu.pc==proc_address);
    fixture(false,4);frame(true,up,up,0);mem_write8(&cpu,0x803F6A78,1);enter_proc();assert(cpu.pc==proc_address);
    fixture(false,4);frame(true,up,up,0);for(unsigned i=0;i<7;++i)bluewake_quick_items_input(true,false,generation,0);
    enter_proc();assert(cpu.pc==proc_address);
    fixture(false,4);frame(true,up,up,0);bluewake_quick_items_input(true,true,++generation,0);enter_proc();assert(cpu.pc==proc_address);
    frame(true,up,up,0);enter_proc();assert(cpu.pc==proc_address);
    frame(false,0,0,0);frame(true,up,up,0);enter_proc();assert(cpu.pc==0x8014DBEC);
    bluewake_quick_items_configure(false);bluewake_quick_items_configure(true);frame(true,up,up,0);enter_proc();assert(cpu.pc==proc_address);
    cpu_free(&cpu);puts("Fixed shortcut native handoff, ship-only overlays, assignment preservation, priority and lifecycle contracts passed.");
}
