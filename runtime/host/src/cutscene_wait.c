// SPDX-License-Identifier: GPL-3.0-or-later
#include "cutscene_wait.h"
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>

/* Exact GZLE01/f282 source boundaries, not native proof.
 * No borrowed pointer crosses a hook. UI sees only atomic metadata. */
#define MEM1 UINT32_C(0x80000000)
#define MEM1_END UINT32_C(0x81800000)
#define ENTRY UINT32_C(0x8021CC4C)
#define IMPORT UINT32_C(0xC021CC4C)
#define RETURN UINT32_C(0xC10A3DE4)
#define OUTER_RETURN UINT32_C(0xC10A4920)
#define TC_TEXT UINT32_C(0xC10A00F4)
#define TC_TEXT_SIZE UINT32_C(0x67BC)
#define LOADER_SCRATCH UINT32_C(0x81820000)
#define LOADER_SCRATCH_END UINT32_C(0x81F80000)
#define TC_PROFILE UINT32_C(0xC10A7008)
#define TC_METHODS UINT32_C(0xC10A6FE8)
#define EVENT_MANAGER UINT32_C(0x803C9ED4)
#define EVENT_CONTROL UINT32_C(0x803C9DE0)
enum { TC_SIZE = 0x81C, CUT_MEMBER = 0x2C4, TIMER = 0x2DC, EXPIRY_VI = 4 };

typedef struct Rule { uint32_t cut, timer, flag, start, next; } Rule;
static const Rule rules[6] = {
    {675,10,1774,UINT32_MAX,676}, {683,27,1793,UINT32_MAX,684},
    {686,10,1801,1841,687}, {694,24,1824,UINT32_MAX,695},
    {696,15,1827,UINT32_MAX,697}, {698,20,1830,UINT32_MAX,UINT32_MAX}
};
typedef struct CutView {
    uint32_t header, arrays[7], counts[7], event, staff, cut, data, integer;
    uint32_t index, expected_timer;
    uint8_t advance, rule;
} CutView;
typedef struct Pending {
    bool active;
    BwTcWaitLease lease;
    BwTcWaitOwner owner;
    CutView cut;
    uint64_t config;
    uint64_t invocation;
    uint32_t stack, backchain, saved_lr;
} Pending;
typedef struct Runtime {
    CPUState* cpu;
    BwTcWaitHostBinding binding;
    bool attached, busy;
    uint64_t applied;
    Pending pending;
    uint64_t completed_registration, completed_epoch;
    uint32_t completed_actor, completed_pid;
    uint8_t completed_mask;
} Runtime;
static Runtime runtime;
static uint64_t invocation_serial; /* game-thread-only; never reset/reused */
/* Low bit is desired enabled, upper bits monotonic config generation.
 * Saturation permanently disables rather than wrapping a retained generation. */
static atomic_uint_fast64_t desired = ATOMIC_VAR_INIT(0);
static atomic_uint availability = ATOMIC_VAR_INIT(BW_TC_WAIT_UNAVAILABLE_BUILD);
static atomic_uint pending_status = ATOMIC_VAR_INIT(0), last_cut = ATOMIC_VAR_INIT(0);
static atomic_uint_fast64_t entries = ATOMIC_VAR_INIT(0), shortened = ATOMIC_VAR_INIT(0);
static atomic_uint_fast64_t rejected = ATOMIC_VAR_INIT(0), cancelled = ATOMIC_VAR_INIT(0);

void bluewake_cutscene_wait_configure(bool enabled) {
    uint_fast64_t old = atomic_load_explicit(&desired, memory_order_acquire);
    for (;;) {
        if ((old & 1u) == (enabled ? 1u : 0u) || old >= UINT64_MAX - 1u) return;
        const uint_fast64_t next = old >= UINT64_MAX - 3u ? UINT64_MAX - 1u :
            ((old & ~UINT64_C(1)) + 2u) | (enabled ? 1u : 0u);
        if (atomic_compare_exchange_weak_explicit(&desired, &old, next,
                memory_order_acq_rel, memory_order_acquire)) return;
    }
}
bool bluewake_cutscene_wait_enabled(void) {
    return (atomic_load_explicit(&desired, memory_order_acquire) & 1u) != 0;
}
const char* bluewake_cutscene_wait_availability_name(BwTcWaitAvailability value) {
    switch (value) {
    case BW_TC_WAIT_AVAILABLE: return "Available";
    case BW_TC_WAIT_UNAVAILABLE_ADMISSION: return "Unavailable for this game build";
    default: return "Unavailable in this build";
    }
}
void bluewake_cutscene_wait_snapshot(BwTcWaitStatus* out) {
    if (!out) return;
    const uint64_t config = atomic_load_explicit(&desired, memory_order_acquire);
    out->availability = (BwTcWaitAvailability)atomic_load_explicit(&availability, memory_order_acquire);
    out->desired_enabled = (config & 1u) != 0;
    out->desired_generation = config >> 1;
    out->pending = atomic_load_explicit(&pending_status, memory_order_acquire) != 0;
    out->entries = atomic_load_explicit(&entries, memory_order_relaxed);
    out->shortened = atomic_load_explicit(&shortened, memory_order_relaxed);
    out->rejected = atomic_load_explicit(&rejected, memory_order_relaxed);
    out->cancelled = atomic_load_explicit(&cancelled, memory_order_relaxed);
    out->last_cut = atomic_load_explicit(&last_cut, memory_order_relaxed);
}
static void cancel_pending(void) {
    if (runtime.pending.active) atomic_fetch_add_explicit(&cancelled, 1, memory_order_relaxed);
    memset(&runtime.pending, 0, sizeof runtime.pending);
    atomic_store_explicit(&pending_status, 0, memory_order_release);
}
static bool config_current(void) {
    const uint64_t now = atomic_load_explicit(&desired, memory_order_acquire);
    if (now != runtime.applied) { cancel_pending(); runtime.applied = now; }
    return (now & 1u) != 0;
}
bool bluewake_cutscene_wait_attach(CPUState* cpu, const BwTcWaitHostBinding* binding) {
    if (runtime.busy) return false;
    cancel_pending();
    memset(&runtime, 0, sizeof runtime);
    runtime.applied = atomic_load_explicit(&desired, memory_order_acquire);
    if (!cpu || !binding || !binding->issuer || !binding->validate_before_cpu || !binding->resolve ||
        !binding->resolve_timer_write || !binding->owner ||
        (unsigned)binding->availability > BW_TC_WAIT_AVAILABLE) {
        atomic_store_explicit(&availability, BW_TC_WAIT_UNAVAILABLE_ADMISSION, memory_order_release);
        return false;
    }
    runtime.cpu = cpu; runtime.binding = *binding; runtime.attached = true;
    atomic_store_explicit(&availability, binding->availability, memory_order_release);
    return true;
}
void bluewake_cutscene_wait_suspend(BwTcWaitSuspendReason reason) {
    cancel_pending();
    /* Reset/replacement invalidates the whole issuer before borrowed storage
     * can die. Ordinary save/capture/scene/config must not invent new CARD auth. */
    if (reason == BW_TC_WAIT_CARD || reason == BW_TC_WAIT_STATE || reason == BW_TC_WAIT_MACHINE ||
        reason == BW_TC_WAIT_CPU || reason == BW_TC_WAIT_RAM || reason == BW_TC_WAIT_CODE ||
        reason == BW_TC_WAIT_DETACH) {
        runtime.attached = false;
        runtime.cpu = NULL;
        memset(&runtime.binding, 0, sizeof runtime.binding);
        runtime.completed_mask = 0;
        atomic_store_explicit(&availability, BW_TC_WAIT_UNAVAILABLE_ADMISSION, memory_order_release);
    }
}
void bluewake_cutscene_wait_detach(void) { bluewake_cutscene_wait_suspend(BW_TC_WAIT_DETACH); }
static bool same_lease(const BwTcWaitLease* a, const BwTcWaitLease* b) {
    return a->issuer == b->issuer && a->cpu == b->cpu && a->ram == b->ram && a->code == b->code &&
        a->native_card_epoch == b->native_card_epoch && a->scene == b->scene && a->alias == b->alias &&
        a->rel_table == b->rel_table && a->mem1 == b->mem1 && a->native_size == b->native_size &&
        a->host_ram_size == b->host_ram_size && a->thread == b->thread && a->context == b->context &&
        a->flags == b->flags;
}
static bool lease(CPUState* cpu, BwTcWaitLease* out) {
    memset(out, 0, sizeof *out);
    if (!runtime.attached || cpu != runtime.cpu || runtime.binding.availability != BW_TC_WAIT_AVAILABLE ||
        !runtime.binding.validate_before_cpu(runtime.binding.user, cpu, out)) return false;
    /* Only after the issuer callback proved live CPU identity may fields be read. */
    return out->issuer == runtime.binding.issuer && out->cpu && out->ram && out->code &&
        out->native_card_epoch && out->scene && out->alias && out->rel_table && out->mem1 &&
        out->native_size == BW_TC_WAIT_NATIVE_MEM1 && out->host_ram_size == BW_TC_WAIT_HOST_RAM &&
        out->flags == BW_TC_WAIT_CARD_AUTHORIZED && out->thread && out->context &&
        cpu->ram == out->mem1 && cpu->ram_size == out->host_ram_size &&
        !cpu->exception && !cpu->program_exception;
}
static bool still(CPUState* cpu, const BwTcWaitLease* expected) {
    BwTcWaitLease now;
    return atomic_load_explicit(&desired, memory_order_acquire) == runtime.applied &&
        lease(cpu, &now) && same_lease(&now, expected) && now.retrace >= expected->retrace &&
        now.retrace - expected->retrace <= EXPIRY_VI;
}
static bool native_span(uint32_t address, uint32_t size) {
    return size && address >= MEM1 && address < MEM1_END && size <= MEM1_END - address;
}
static bool span32(uint32_t address, uint32_t size) {
    return size && (uint64_t)address + size <= UINT32_MAX;
}
static bool read_bytes(CPUState* cpu, const BwTcWaitLease* l, uint32_t address,
                       uint32_t size, void* copied) {
    if (!span32(address, size) || !still(cpu, l)) return false;
    const uint8_t* bytes = runtime.binding.resolve(runtime.binding.user, address, size);
    if (!bytes || !still(cpu, l)) return false;
    if (copied) memcpy(copied, bytes, size);
    return still(cpu, l);
}
static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static bool r32(CPUState* cpu, const BwTcWaitLease* l, uint32_t address, uint32_t* out) {
    uint8_t b[4]; if (!read_bytes(cpu,l,address,4,b)) return false; *out=be32(b);return true;
}
static bool exact_name(const uint8_t* bytes, uint32_t capacity, const char* name) {
    const size_t n = strlen(name);
    return n < capacity && !memcmp(bytes, name, n) && bytes[n] == 0;
}
static bool owner(CPUState* cpu, const BwTcWaitLease* l, uint32_t actor, uint32_t pid,
                  BwTcWaitOwner* out) {
    memset(out,0,sizeof *out);
    if (!native_span(actor,TC_SIZE) || (actor&3u) || !still(cpu,l) ||
        !runtime.binding.owner(runtime.binding.user,cpu,actor,pid,out) || !still(cpu,l) ||
        out->actor!=actor || !out->pid || out->pid>=UINT32_MAX-1u || (pid && pid!=out->pid) ||
        !out->registration || !out->materialization || out->profile!=TC_PROFILE || out->methods!=TC_METHODS ||
        out->actor_tag!=actor+0xC4u || !out->module_header || !out->loader_owner ||
        out->linked_text!=TC_TEXT || out->text_size!=TC_TEXT_SIZE ||
        out->raw_text<LOADER_SCRATCH || out->raw_text>=LOADER_SCRATCH_END ||
        out->text_size>LOADER_SCRATCH_END-out->raw_text ||
        !read_bytes(cpu,l,out->raw_text,out->text_size,NULL)) return false;
    uint8_t a[TC_SIZE];
    if (!read_bytes(cpu,l,actor,sizeof a,a)) return false;
    return be32(a+4)==out->pid && a[0xC]==2 && a[0xD]==2 &&
        (((uint32_t)a[8]<<8)|a[9])==0x147u && (((uint32_t)a[0xE]<<8)|a[0xF])==0x147u &&
        be32(a+0x10)==TC_PROFILE && be32(a+0xEC)==TC_METHODS && a[0x1BE]==0 &&
        be32(a+0xD0)==actor && a[0xD4]==1 && a[0x20A]==0 && a[0x6CF]==11;
}
static bool same_owner(const BwTcWaitOwner* a, const BwTcWaitOwner* b) {
    return a->materialization==b->materialization && a->registration==b->registration &&
        a->actor==b->actor && a->pid==b->pid && a->profile==b->profile && a->methods==b->methods &&
        a->actor_tag==b->actor_tag && a->module_header==b->module_header && a->loader_owner==b->loader_owner &&
        a->raw_text==b->raw_text && a->linked_text==b->linked_text && a->text_size==b->text_size;
}
static uint32_t canonical_tc(const BwTcWaitOwner* o, uint32_t pc) {
    if (pc>=o->raw_text && (uint64_t)pc< (uint64_t)o->raw_text+o->text_size)
        return o->linked_text+(pc-o->raw_text);
    return pc;
}
static bool scene(CPUState* cpu, const BwTcWaitLease* l) {
    uint8_t stage[12], b;
    uint32_t overlap;
    if (!read_bytes(cpu,l,0x803C9D3Cu,sizeof stage,stage) || !exact_name(stage,8,"Pnezumi") || stage[10]!=0 ||
        !read_bytes(cpu,l,0x803F6A78u,1,&b) || b || !read_bytes(cpu,l,0x803F7097u,1,&b) || b ||
        !read_bytes(cpu,l,0x803C9D54u,1,&b) || b || !r32(cpu,l,0x803F6160u,&overlap) || overlap ||
        !read_bytes(cpu,l,EVENT_CONTROL+0xC2u,1,&b) || b<1 || b>3) return false;
    return true;
}
static bool overlap(uint32_t a,uint32_t n,uint32_t b,uint32_t m) {
    return n && m && (uint64_t)a+n>b && (uint64_t)b+m>a;
}
static bool event_cut(CPUState* cpu, const BwTcWaitLease* l, CutView* out) {
    memset(out,0,sizeof *out);
    uint8_t list[0x20], header[0x40], event[0xB0], staff[0x50], cut[0x50], data[0x40];
    if (!scene(cpu,l) || !read_bytes(cpu,l,EVENT_MANAGER,sizeof list,list)) return false;
    out->header=be32(list);
    if (!native_span(out->header,sizeof header) || (out->header&3u) ||
        !read_bytes(cpu,l,out->header,sizeof header,header)) return false;
    /* mList contains eight pointers: header+0 and seven arrays+4..+1C.
     * The final +1C is SData, sized in bytes by header+0x34, not an omitted
     * eighth array or a word count. Every nonempty span is mapped below. */
    const uint32_t widths[7]={0xB0,0x50,0x50,0x40,4,4,1};
    const uint32_t expected[4]={78,263,744,790};
    uint32_t sizes[7];
    for (unsigned i=0;i<7;++i) {
        const uint32_t offset=be32(header+8*i), count=be32(header+8*i+4);
        if ((i<4 && count!=expected[i]) || count>65536u || (i<4 && count>4096u)) return false;
        const uint64_t address=(uint64_t)out->header+offset, size=(uint64_t)count*widths[i];
        if (size>UINT32_MAX || address>UINT32_MAX || (count && (offset<0x40u || (offset&3u)))) return false;
        out->arrays[i]=be32(list+4+4*i); out->counts[i]=count; sizes[i]=(uint32_t)size;
        if (count && (out->arrays[i]!=(uint32_t)address || !native_span((uint32_t)address,(uint32_t)size) ||
            !read_bytes(cpu,l,(uint32_t)address,(uint32_t)size,NULL))) return false;
        if (!count && out->arrays[i]!=0) return false;
        for (unsigned j=0;j<i;++j) if(overlap(out->arrays[i],sizes[i],out->arrays[j],sizes[j])) return false;
    }
    out->event=out->arrays[0]+71u*0xB0u; out->staff=out->arrays[1]+244u*0x50u;
    if (!read_bytes(cpu,l,out->event,sizeof event,event) || !exact_name(event,32,"TC_RESCUE") ||
        be32(event+0x7C)!=3 || be32(event+0x2C)!=243 || be32(event+0x30)!=244 || be32(event+0x34)!=245 ||
        be32(event+0x88)!=1830 || be32(event+0x8C)!=UINT32_MAX || be32(event+0x90)!=UINT32_MAX ||
        be32(event+0xA4)!=2 || !read_bytes(cpu,l,out->staff,sizeof staff,staff) ||
        !exact_name(staff,32,"Tc") || be32(staff+0x20)!=0 ||
        be32(staff+0x2C)!=0 || be32(staff+0x30)!=658 || (staff[0x46]!=1 && staff[0x46]!=2)) return false;
    out->index=be32(staff+0x38); out->advance=staff[0x46];
    unsigned which=0; while(which<6 && rules[which].cut!=out->index) ++which;
    if (which==6) return false;
    out->rule=(uint8_t)which; out->expected_timer=rules[which].timer;
    out->cut=out->arrays[2]+out->index*0x50u;
    if (!read_bytes(cpu,l,out->cut,sizeof cut,cut) || !exact_name(cut,32,"WAIT") ||
        be32(cut+0x28)!=rules[which].start ||
        be32(cut+0x2C)!=UINT32_MAX || be32(cut+0x30)!=UINT32_MAX ||
        be32(cut+0x34)!=rules[which].flag || be32(cut+0x3C)!=rules[which].next) return false;
    const uint32_t data_index=be32(cut+0x38);
    if (data_index>=out->counts[3]) return false;
    out->data=out->arrays[3]+data_index*0x40u;
    if (!read_bytes(cpu,l,out->data,sizeof data,data) || !exact_name(data,32,"Timer") ||
        be32(data+0x24)!=3 || be32(data+0x2C)!=1 ||
        be32(data+0x30)!=UINT32_MAX || be32(data+0x28)>=out->counts[5]) return false;
    out->integer=out->arrays[5]+be32(data+0x28)*4u;
    uint32_t timer;
    return r32(cpu,l,out->integer,&timer) && timer==out->expected_timer;
}
static bool same_cut(const CutView* a,const CutView* b) {
    return a->header==b->header && !memcmp(a->arrays,b->arrays,sizeof a->arrays) &&
        !memcmp(a->counts,b->counts,sizeof a->counts) && a->event==b->event && a->staff==b->staff &&
        a->cut==b->cut && a->data==b->data && a->integer==b->integer && a->index==b->index &&
        a->expected_timer==b->expected_timer && a->advance==b->advance && a->rule==b->rule;
}
static bool completed_for(const BwTcWaitOwner* o,const BwTcWaitLease* l,uint8_t rule) {
    return runtime.completed_registration==o->registration && runtime.completed_epoch==l->native_card_epoch &&
        runtime.completed_actor==o->actor && runtime.completed_pid==o->pid && (runtime.completed_mask&(1u<<rule));
}
void bluewake_cutscene_wait_retrace(CPUState* cpu, bool mutators_held) {
    if (runtime.busy || !config_current()) return;
    if (!runtime.pending.active) return;
    if (mutators_held || !still(cpu,&runtime.pending.lease)) cancel_pending();
    /* Ordinary VI yield does not close an armed native call. */
}
bool bluewake_cutscene_wait_observes(const CPUState* supplied,uint32_t address) {
    if (runtime.busy || !config_current()) return false;
    if (!runtime.attached || supplied!=runtime.cpu || runtime.binding.availability!=BW_TC_WAIT_AVAILABLE) return false;
    if (address==ENTRY || address==IMPORT) return true;
    if (!runtime.pending.active) return false;
    return canonical_tc(&runtime.pending.owner,address)==RETURN;
}
void bluewake_cutscene_wait_dispatch(CPUState* cpu,uint32_t address,bool mutators_held) {
    if (runtime.busy || !config_current()) return;
    if (mutators_held) { cancel_pending(); return; }
    runtime.busy=true;
    bool retired_return=false;
    BwTcWaitLease l;
    if (!lease(cpu,&l)) goto reject;
    if (address==ENTRY || address==IMPORT) {
        Pending candidate; memset(&candidate,0,sizeof candidate);
        candidate.lease=l; candidate.config=runtime.applied;
        if ((cpu->pc!=ENTRY && cpu->pc!=IMPORT) || !owner(cpu,&l,cpu->gpr[30],0,&candidate.owner) ||
            canonical_tc(&candidate.owner,cpu->lr)!=RETURN || cpu->gpr[3]!=candidate.owner.actor+CUT_MEMBER ||
            !event_cut(cpu,&l,&candidate.cut) || completed_for(&candidate.owner,&l,candidate.cut.rule)) goto reject;
        candidate.stack=cpu->gpr[1];
        if ((candidate.stack&0xFu) || !native_span(candidate.stack,0x48) ||
            !r32(cpu,&l,candidate.stack,&candidate.backchain) || candidate.backchain!=candidate.stack+0x40u ||
            !r32(cpu,&l,candidate.stack+0x44u,&candidate.saved_lr) ||
            canonical_tc(&candidate.owner,candidate.saved_lr)!=OUTER_RETURN) goto reject;
        if (runtime.pending.active) {
            const Pending* old=&runtime.pending;
            if (!same_lease(&candidate.lease,&old->lease) || !same_owner(&candidate.owner,&old->owner) ||
                !same_cut(&candidate.cut,&old->cut) || candidate.stack!=old->stack ||
                candidate.backchain!=old->backchain || candidate.saved_lr!=old->saved_lr ||
                candidate.config!=old->config) goto reject;
            runtime.busy=false; return; /* first-PC replay keeps one invocation */
        }
        if (invocation_serial==UINT64_MAX) goto reject;
        candidate.invocation=++invocation_serial;
        candidate.active=true; runtime.pending=candidate;
        atomic_store_explicit(&pending_status,1,memory_order_release);
        atomic_store_explicit(&last_cut,candidate.cut.index,memory_order_relaxed);
        atomic_fetch_add_explicit(&entries,1,memory_order_relaxed);
        runtime.busy=false; return;
    }
    if (runtime.pending.active && canonical_tc(&runtime.pending.owner,address)==RETURN) {
        const Pending p=runtime.pending;
        /* Retire first so duplicate/reentrant paths cannot write twice. */
        memset(&runtime.pending,0,sizeof runtime.pending);
        atomic_store_explicit(&pending_status,0,memory_order_release);
        retired_return=true;
        BwTcWaitOwner now_owner; CutView now_cut; uint32_t backchain,saved_lr;
        uint8_t member[0x1C];
        if (!p.invocation || !same_lease(&l,&p.lease) || !still(cpu,&p.lease) ||
            canonical_tc(&p.owner,cpu->pc)!=RETURN || canonical_tc(&p.owner,cpu->lr)!=RETURN ||
            cpu->gpr[1]!=p.stack || cpu->gpr[30]!=p.owner.actor || (cpu->gpr[3]&0xFFu)!=1 ||
            !r32(cpu,&l,p.stack,&backchain) || backchain!=p.backchain ||
            !r32(cpu,&l,p.stack+0x44u,&saved_lr) || saved_lr!=p.saved_lr ||
            !owner(cpu,&l,p.owner.actor,p.owner.pid,&now_owner) || !same_owner(&now_owner,&p.owner) ||
            !event_cut(cpu,&l,&now_cut) || !same_cut(&now_cut,&p.cut) ||
            !read_bytes(cpu,&l,p.owner.actor+CUT_MEMBER,sizeof member,member) ||
            be32(member+4)!=244 || be32(member+8)!=p.owner.actor || be32(member+0xC)!=p.owner.actor ||
            be32(member+0x10)!=0 || be32(member+0x18)!=p.cut.expected_timer-1u ||
            p.cut.expected_timer<=2 || completed_for(&p.owner,&l,p.cut.rule)) goto reject;
        const uint32_t timer_address=p.owner.actor+TIMER;
        if (!still(cpu,&l)) goto reject;
        uint8_t* writable=runtime.binding.resolve_timer_write(runtime.binding.user,timer_address,4);
        if (!writable || writable!=l.mem1+(timer_address-MEM1) || !still(cpu,&l) ||
            be32(writable)!=p.cut.expected_timer-1u) goto reject;
        /* Sole guest mutation, BE32 one. Caller/native return/budgets untouched. */
        writable[0]=0; writable[1]=0; writable[2]=0; writable[3]=1;
        if (runtime.completed_registration!=p.owner.registration || runtime.completed_epoch!=l.native_card_epoch ||
            runtime.completed_actor!=p.owner.actor || runtime.completed_pid!=p.owner.pid) runtime.completed_mask=0;
        runtime.completed_registration=p.owner.registration; runtime.completed_epoch=l.native_card_epoch;
        runtime.completed_actor=p.owner.actor; runtime.completed_pid=p.owner.pid;
        runtime.completed_mask|=(uint8_t)(1u<<p.cut.rule);
        atomic_fetch_add_explicit(&shortened,1,memory_order_relaxed);
        runtime.busy=false; return;
    }
    runtime.busy=false; return;
reject:
    if (retired_return) atomic_fetch_add_explicit(&cancelled,1,memory_order_relaxed);
    atomic_fetch_add_explicit(&rejected,1,memory_order_relaxed);
    cancel_pending();
    runtime.busy=false;
}
