// SPDX-License-Identifier: GPL-3.0-or-later
#include "hud_host.h"
#include "StaticRecompABI.h"
#include "hud_module_contract.h"
#include "game_events.h"
#include <stdatomic.h>
#include <string.h>

enum { NATIVE_MEM1=0x01800000u, ACTUAL_RAM=0x02000000u };
typedef struct SharedHud {
    BwHudConfig desired;
    BwHudHostStatus snapshot;
    uint64_t revision;
    bool has_desired;
} SharedHud;
static SharedHud shared;
static atomic_flag shared_lock=ATOMIC_FLAG_INIT;
typedef struct HostHud {
    BwHudRuntime runtime;
    CPUState* cpu;
    const uint8_t* ram;
    const StaticRecompModuleDesc* module;
    BwHudAliasGeneration alias_generation;
    void* alias_user;
    uint32_t aliases;
    uint64_t epoch,generation,consumed,reads,rejected,capture_sequence;
    BwGameEventSubscription subscription;
    BwHudAvailability availability;
    BwHudConfig desired;
    bool attached,suspended,requested,renderer,aspect,module_ok,bound,saving;
} HostHud;
static HostHud host;
static bool lock_shared(void){return !atomic_flag_test_and_set_explicit(&shared_lock,memory_order_acquire);}
static void unlock_shared(void){atomic_flag_clear_explicit(&shared_lock,memory_order_release);}
static bool equal_config(const BwHudConfig* a,const BwHudConfig* b) {
    for(unsigned i=0;i<BW_HUD_GROUP_COUNT;++i) {
        const BwHudGroupConfig *x=&a->groups[i],*y=&b->groups[i];
        if(x->offset_x!=y->offset_x||x->offset_y!=y->offset_y||x->scale!=y->scale||
           x->opacity!=y->opacity||x->anchor_x!=y->anchor_x||x->anchor_y!=y->anchor_y||
           x->visible!=y->visible||memcmp(x->tint,y->tint,4))return false;
    }return true;
}
bool bw_hud_host_configure(const BwHudConfig* config) {
    if(!bw_hud_config_valid(config)||!lock_shared())return false;
    if(!shared.has_desired||!equal_config(config,&shared.desired)) {
        /* A saturated mailbox fails instead of recycling an old revision. */
        if(shared.revision==UINT64_MAX){unlock_shared();return false;}
        shared.desired=*config;shared.has_desired=true;++shared.revision;
    }unlock_shared();return true;
}
bool bw_hud_host_snapshot(BwHudHostStatus* out) {
    if(!out||!lock_shared())return false;*out=shared.snapshot;unlock_shared();return true;
}
static void publish(void) {
    if(!lock_shared())return;
    BwHudHostStatus s={0};s.availability=host.availability;s.requested=host.requested;
    s.configured=host.runtime.stats.enabled;s.bound=host.bound;s.requested_revision=shared.revision;
    s.consumed_revision=host.consumed;s.epoch=host.epoch;s.generation=host.generation;
    s.resolver_reads=host.reads;s.rejected_reads=host.rejected;bw_hud_stats(&host.runtime,&s.native);
    shared.snapshot=s;unlock_shared();
}
const char* bw_hud_host_availability_name(BwHudAvailability a) {
    static const char* names[]={"Host is not attached","Available (native pane prototype)",
        "This module's HUD layout has not been audited","This aspect's HUD layout has not been audited",
        "GXCore HUD renderer is unavailable","Machine replacement is suspended",
        "Waiting for an active native scene","Native lifecycle subscriber limit reached"};
    return (unsigned)a<sizeof names/sizeof names[0]?names[a]:"Unavailable";
}
static uint64_t hash_value(uint64_t h,uint64_t v,unsigned bytes) {
    for(unsigned i=0;i<bytes;++i){h^=(v>>(8u*i))&255u;h*=UINT64_C(1099511628211);}return h;
}
uint64_t bw_hud_module_fingerprint(const StaticRecompModuleDesc* m) {
    if(!m||!m->chunk_ranges||!m->chunk_hashes||!m->num_chunk_ranges||m->num_chunk_ranges>4096)return 0;
    uint64_t h=hash_value(UINT64_C(14695981039346656037),m->num_chunk_ranges,4);
    uint32_t previous=0;
    for(uint32_t i=0;i<m->num_chunk_ranges;++i) {
        const StaticRecompRange* r=&m->chunk_ranges[i];
        if(r->start>=r->end||(r->start&3u)||(r->end&3u)||(i&&r->start<previous))return 0;
        previous=r->end;h=hash_value(h,r->start,4);h=hash_value(h,r->end,4);h=hash_value(h,m->chunk_hashes[i],8);
    }return h;
}
static bool module_supported(const StaticRecompModuleDesc* m) {
    return m&&(m->abi_version==3||m->abi_version==4||m->abi_version==5)&&
        m->cpu_abi_version==GXRUNTIME_CPU_ABI_VERSION&&m->cpu_state_size==sizeof(CPUState)&&
        !memcmp(m->game_id,"GZLE01",sizeof("GZLE01"))&&m->dispatch&&
        m->num_chunk_ranges==BW_HUD_MODULE_CHUNKS&&bw_hud_module_fingerprint(m)==BW_HUD_MODULE_FINGERPRINT;
}
static bool cpu_identity(const CPUState* cpu) {
    return host.attached&&!host.suspended&&host.module_ok&&cpu&&cpu==host.cpu&&cpu->ram&&
        cpu->ram==host.ram&&cpu->ram_size==ACTUAL_RAM&&host.alias_generation;
}
static bool current_lifetime(const CPUState* cpu) {
    if(!cpu_identity(cpu)||!host.bound)return false;
    const uint32_t aliases=host.alias_generation(host.alias_user);
    if(!cpu_identity(cpu)||aliases!=host.aliases)return false;
    BwGameScene scene;uint64_t epoch=0,generation=0;
    return bluewake_game_events_scene(&scene,&epoch,&generation)&&cpu_identity(cpu)&&
        scene.active&&!scene.transitioning&&epoch==host.epoch&&generation==host.generation;
}
static const uint8_t* resolve(void* unused,uint32_t address,uint32_t size) {
    (void)unused;
    if(!size||address<0x80000000u||size>NATIVE_MEM1||address-0x80000000u>NATIVE_MEM1-size||
       !current_lifetime(host.cpu)){++host.rejected;return NULL;}
    const uint8_t* p=get_ram_ptr(host.cpu,address,size,NULL);
    if(!p||!current_lifetime(host.cpu)){++host.rejected;return NULL;}
    /* Even a four-byte BE field can straddle a smaller native alias. Match
     * every indexed native byte, not just the containing flat-RAM span. */
    for(uint32_t i=0;i<size;++i) {
        if(!current_lifetime(host.cpu)){++host.rejected;return NULL;}
        const uint8_t* b=get_ram_ptr(host.cpu,address+i,1,NULL);
        if(!current_lifetime(host.cpu)||b!=p+i){++host.rejected;return NULL;}
    }++host.reads;return p;
}
static void unbind(void){bw_hud_detach(&host.runtime);host.bound=false;host.epoch=host.generation=0;}
static void lifecycle(const BwGameEvent* e,void* unused) {
    (void)unused;if(!e)return;
    if(e->kind==BW_GAME_EVENT_RESET||e->kind==BW_GAME_EVENT_SCENE_LEAVING||
       e->kind==BW_GAME_EVENT_TRANSITION_STARTED||e->kind==BW_GAME_EVENT_SCENE_ENTERED) {
        /* No borrowed guest storage is read during cancellation. */
        unbind();host.availability=host.suspended?BW_HUD_SUSPENDED:BW_HUD_WAITING_SCENE;publish();
    }
}
static BwHudAvailability capability(void) {
    if(!host.attached)return BW_HUD_UNATTACHED;
    if(host.suspended)return BW_HUD_SUSPENDED;
    if(!host.module_ok)return BW_HUD_BAD_MODULE;
    if(!host.aspect)return BW_HUD_UNSUPPORTED_ASPECT;
    if(!host.renderer)return BW_HUD_UNSUPPORTED_RENDERER;
    return BW_HUD_AVAILABLE;
}
bool bw_hud_host_attach(CPUState* cpu,const StaticRecompModuleDesc* mod,BwHudAliasGeneration gen,
                         void* alias_user,BwHudEmitBp emit,void* emit_user,bool renderer,bool aspect) {
    bw_hud_host_detach();memset(&host,0,sizeof host);bw_hud_init(&host.runtime,emit,emit_user);
    bw_hud_config_identity(&host.desired);
    host.cpu=cpu;host.ram=cpu?cpu->ram:NULL;host.module=mod;host.alias_generation=gen;host.alias_user=alias_user;
    host.renderer=renderer&&emit;host.aspect=aspect;host.module_ok=module_supported(mod);
    host.attached=cpu&&cpu->ram&&cpu->ram_size==ACTUAL_RAM&&gen;
    host.availability=capability();publish();return host.availability==BW_HUD_AVAILABLE;
}
void bw_hud_host_retrace(CPUState* cpu,uint64_t frame,bool saving) {
    (void)frame; /* VI may preempt a native leaf; it is not a draw boundary. */
    BwHudConfig desired=host.desired;uint64_t revision=host.consumed;
    if(lock_shared()){
        if(shared.has_desired)desired=shared.desired;
        revision=shared.revision;unlock_shared();host.desired=desired;
    }
    host.requested=!bw_hud_config_is_identity(&desired);host.saving=saving;
    const BwHudAvailability cap=capability();
    if(cap!=BW_HUD_AVAILABLE||!host.requested) {
        if(host.subscription){bluewake_game_events_unsubscribe(host.subscription);host.subscription=0;}
        if(host.runtime.stats.enabled||host.bound){unbind();BwHudConfig native;bw_hud_config_identity(&native);bw_hud_set_config(&host.runtime,&native);}
        host.availability=cap;host.consumed=revision;publish();return;
    }
    if(!host.subscription) {
        const uint64_t mask=BW_GAME_EVENT_MASK(BW_GAME_EVENT_RESET)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_SCENE_LEAVING)|
            BW_GAME_EVENT_MASK(BW_GAME_EVENT_SCENE_ENTERED)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_TRANSITION_STARTED);
        host.subscription=bluewake_game_events_subscribe(mask,lifecycle,NULL);
        if(!host.subscription){host.availability=BW_HUD_SUBSCRIBER_FULL;publish();return;}
    }
    if(!equal_config(&desired,&host.runtime.config)){bw_hud_set_config(&host.runtime,&desired);host.bound=false;}
    host.consumed=revision;
    if(saving||!cpu_identity(cpu)) {unbind();host.availability=BW_HUD_WAITING_SCENE;publish();return;}
    BwGameScene scene;uint64_t epoch=0,generation=0;
    if(!bluewake_game_events_scene(&scene,&epoch,&generation)||!scene.active||scene.transitioning||!epoch||!generation) {
        if(host.bound)unbind();host.availability=BW_HUD_WAITING_SCENE;publish();return;
    }
    const uint32_t aliases=host.alias_generation(host.alias_user);
    if(!cpu_identity(cpu)){unbind();publish();return;}
    if(!host.bound||epoch!=host.epoch||generation!=host.generation||aliases!=host.aliases) {
        unbind();host.epoch=epoch;host.generation=generation;host.aliases=aliases;host.bound=true;
        const BwHudMemory memory={host.ram,NATIVE_MEM1,(uintptr_t)cpu,BW_HUD_ABI_GZLE01,
            epoch,generation,resolve,NULL};
        if(!bw_hud_attach(&host.runtime,&memory)||!current_lifetime(cpu)){unbind();publish();return;}
    }
    host.availability=BW_HUD_AVAILABLE;publish();
}
bool bw_hud_host_observes(const CPUState* cpu,uint32_t address) {
    (void)cpu;
    /* Keep an armed boundary observable even after its read identity was
     * revoked. dispatch closes the old FIFO scope without touching RAM;
     * allowing an optimized skip here could otherwise defer cancellation.
     * Identity/unsupported defaults return before events or guest lookups. */
    return bw_hud_observes(&host.runtime,address);
}
void bw_hud_host_dispatch(CPUState* cpu,uint32_t address) {
    if(!bw_hud_observes(&host.runtime,address))return;
    if(host.saving||!current_lifetime(cpu)){unbind();publish();return;}
    if(address==BW_HUD_METER_CAPTURE) {
        /* Actual dMeter_Draw entry owns the next native capture. An ordinary
         * VI/budget yield retains its armed descriptor until native return. */
        if(host.capture_sequence==UINT64_MAX){bw_hud_host_suspend();return;}
        bw_hud_frame(&host.runtime,++host.capture_sequence);
    }
    const BwHudEdge edge={(uintptr_t)cpu,(uintptr_t)cpu->ram,address,cpu->gpr[1],cpu->gpr[3],cpu->lr,
        host.epoch,host.generation};
    bw_hud_dispatch(&host.runtime,&edge);
    if(!current_lifetime(cpu))unbind();publish();
}
void bw_hud_host_suspend(void){unbind();host.suspended=true;host.availability=BW_HUD_SUSPENDED;publish();}
bool bw_hud_host_resume(CPUState* cpu,const StaticRecompModuleDesc* mod) {
    unbind();host.cpu=cpu;host.ram=cpu?cpu->ram:NULL;host.module=mod;host.module_ok=module_supported(mod);
    host.attached=cpu&&cpu->ram&&cpu->ram_size==ACTUAL_RAM&&host.alias_generation;
    host.suspended=false;host.availability=capability();publish();return host.availability==BW_HUD_AVAILABLE;
}
void bw_hud_host_detach(void) {
    unbind();if(host.subscription)bluewake_game_events_unsubscribe(host.subscription);
    BwHudConfig native;bw_hud_config_identity(&native);
    bw_hud_set_config(&host.runtime,&native);host.runtime.emit=NULL;host.runtime.emit_user=NULL;
    host.subscription=0;host.attached=false;host.suspended=true;host.cpu=NULL;host.ram=NULL;host.module=NULL;
    host.alias_generation=NULL;host.bound=false;host.availability=BW_HUD_UNATTACHED;publish();
}
