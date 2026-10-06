// SPDX-License-Identifier: GPL-3.0-or-later
#include "randomizer_reward_host.h"
#include "randomizer_actor_owner.h"
#include "randomizer_card_payload.h"
#include "randomizer_session.h"
#include "autosave.h"
// card_runtime is implemented by the C host. Its legacy header has no C++
// linkage guard; keep this local rather than changing shared host declarations.
extern "C" {
#include "card_runtime.h"
}
#include "game_events.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// GZLE01, pinned native tww49f2e348. This owner deliberately supports only
// LinkUG table11/chest5 and the two qualified transaction codecs (06/23).
// No code in this file calls a guest handler or writes guest memory. Only the
// Accepted creation argument is substituted, at its authentic entry boundary.
namespace {
namespace seed = bluewake::randomizer::seed;
namespace tx = bluewake::randomizer::transaction;
namespace ss = bluewake::randomizer::session;
namespace cp = bluewake::randomizer::card_payload;
using Bytes = bluewake::randomizer::storage::Bytes;
constexpr uint32_t NativeBase = 0x80000000u, NativeEnd = 0x81800000u;
constexpr uint32_t RamSize = 0x02000000u, Info = 0x803C4C08u;
constexpr uint32_t Card = 0x803B39A0u, MainThread = 0x803A2960u;
constexpr uint32_t Create = 0x800261E8u, CreateImport = 0xC00261E8u;
constexpr uint32_t CreateReturn = 0xC1DF2780u;
constexpr uint32_t Award = 0x800C2DFCu, AwardImport = 0xC00C2DFCu;
constexpr uint32_t AwardReturn = 0xC0770A08u;
constexpr uint32_t LoadSync = 0x80019288u, LoadSyncReturn = 0x80230CA4u;
constexpr uint32_t Load = 0x8005EA24u, LoadReturn = 0x80231B08u;
constexpr uint32_t Serialize = 0x8005E780u, SerializeReturn = 0x801D8994u;
constexpr uint32_t Store = 0x800191C4u, StoreReturn = 0x801D89F0u;
constexpr uint32_t Sync = 0x8001931Cu, SyncImmediate = 0x801D89FCu, SyncWait = 0x801D8A6Cu;
constexpr uint32_t TagAdd = 0x80245574u, TagCut = 0x8024541Cu, TagCreate = 0x802455C4u;
constexpr uint32_t Unlink = 0x803056BCu;
constexpr size_t MaxSlots = 512, MaxSections = 4096;
constexpr size_t QuestSize = 0x770, PackedChest = 0x374 + 11 * 0x24;
constexpr uint32_t ChestMask = 0x20u;
constexpr uint32_t DataStarts[2] = {0xC1DF3CF0u, 0xC07710B8u};
constexpr uint32_t DataSizes[2] = {0x364u, 0x28Cu};
// Runtime-only process-lifetime issuance. Never reset, persisted, or derived
// from an address/PID. Exhaustion permanently prevents further issuance.
std::atomic<uint64_t> next_token{1};
uint64_t issue() noexcept {
    uint64_t n = next_token.load(std::memory_order_relaxed);
    while (n && n != UINT64_MAX) {
        if (next_token.compare_exchange_weak(n, n + 1, std::memory_order_relaxed)) return n;
    }
    return 0;
}
uint16_t be16(const uint8_t* p) { return uint16_t((unsigned(p[0]) << 8) | p[1]); }
uint32_t be32(const uint8_t* p) { return (uint32_t(be16(p)) << 16) | be16(p + 2); }
uint64_t be64(const uint8_t* p) { return (uint64_t(be32(p)) << 32) | be32(p + 4); }
bool native_span(uint32_t a, uint32_t n) {
    return n && a >= NativeBase && a < NativeEnd && n <= NativeEnd - a;
}
bool pid_ok(uint32_t p) { return p && p < UINT32_MAX - 1u; }
std::string hex(const uint8_t* p) {
    const char* digits = "0123456789abcdef";
    std::string s(64, '0');
    for (size_t i = 0; i < 32; ++i) { s[2*i] = digits[p[i] >> 4]; s[2*i+1] = digits[p[i] & 15]; }
    return s;
}
bool unhex(const std::string& s, uint8_t* p) noexcept {
    if (s.size() != 64) return false;
    for (size_t i = 0; i < 32; ++i) {
        unsigned v = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned c = static_cast<unsigned char>(s[2*i+j]);
            if (c >= '0' && c <= '9') v = (v << 4) | (c - '0');
            else if (c >= 'a' && c <= 'f') v = (v << 4) | (c - 'a' + 10);
            else return false;
        }
        p[i] = uint8_t(v);
    }
    return true;
}
std::string digest(const Bytes& bytes) {
    return seed::sha256(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}
bool read_file(const std::string& path, Bytes& out) {
    // Dedicated working.card only. Caller holds the runtime lock when mounted.
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
    if (!f) return false;
    const auto length = f.tellg();
    if (length <= 0 || length > std::streamoff(cp::MaxCardBytes)) return false;
    Bytes copy(static_cast<size_t>(length)); f.seekg(0);
    if (!f.read(reinterpret_cast<char*>(copy.data()), length) || f.peek() != std::char_traits<char>::eof()) return false;
    out.swap(copy); return true;
}
struct SnapshotLock {
    bool held;
    explicit SnapshotLock(const std::string& p):held(bluewake_card_runtime_begin_snapshot(p.c_str())) {}
    ~SnapshotLock() { if (held) bluewake_card_runtime_end_snapshot(); }
};
struct Boundary {
    bool live = false;
    uint64_t token = 0, alias = 0, epoch = 0, native_load = 0;
    uint32_t sp = 0, thread = 0, lr = 0, raw_alias = 0;
};
struct Tracked {
    BwRandomizerActorOwner owner{};
    uint64_t registration = 0;
};
struct Materialization {
    uint32_t address = 0, capacity = 0, owner = 0;
    uint64_t token = 0;
};
struct LoadRead {
    Boundary boundary;
    uint32_t name = 0, pid = 0, buffer = 0;
    bool complete = false;
};
struct NativeLoad {
    Boundary boundary;
    uint32_t name = 0, pid = 0, buffer = 0;
    bool returned = false, reset = false;
    uint64_t resulting_epoch = 0;
};
struct NativeSave {
    bool active = false, serialized = false, stored = false, store_returned = false;
    bool completed = false, publish = true, unsupported = false;
    Boundary serialize, store, poll;
    uint32_t menu = 0, buffer = 0;
    uint64_t revision = 0;
    std::array<uint8_t, cp::GameBytes> game{};
};
bool native_entry(uint32_t pc) {
    switch (pc) {
    case Create: case CreateImport: case Award: case AwardImport:
    case LoadSync: case Load: case Serialize: case Store: case Sync:
    case TagAdd: case TagCut: case TagCreate: case Unlink: return true;
    default: return false;
    }
}
} // namespace

struct BwRandomizerRewardHost {
    const std::thread::id thread = std::this_thread::get_id();
    BwIcLoadedCode* code = nullptr;
    const StaticRecompModuleDesc* descriptor = nullptr;
    uint64_t code_generation = 0, code_token = 0;
    std::unique_ptr<ss::Session> session;
    ss::Lease lease;
    std::string path, module_digest, origin, confirmed_digest;
    Bytes baseline;
    std::array<uint8_t, cp::GameBytes> baseline_game{};
    cp::Match baseline_match{};
    BwRewardHostStatus status{};
    CPUState* cpu = nullptr;
    uint8_t* ram = nullptr;
    uint32_t ram_size = 0, raw_alias = 0;
    uint64_t cpu_token = 0, ram_token = 0, alias_token = 0, scene_token = 0, epoch = 0;
    std::array<BwRewardHostBacking, 2> backings{};
    std::array<Materialization, MaxSlots> materializations{};
    std::array<BwRandomizerRelSlot, MaxSlots> slots{};
    std::array<BwRandomizerRelAlias, MaxSlots> aliases{};
    size_t slot_count = 0, alias_count = 0;
    std::vector<BwRandomizerRelSection> sections;
    bool tables_current = false, backend_open = false, stopped = false, abandoned = false;
    bool backing_live = false, inside = false;
    BwGameEventSubscription subscription = 0;
    Tracked link, chest, item;
    BwRandomizerRelOwner chest_rel{}, item_rel{};
    tx::Owner last_owner;
    Boundary creation, award;
    tx::Selection selected;
    uint32_t item_pid = 0;
    uint8_t reward = 255;
    LoadRead load_read;
    NativeLoad load;
    NativeSave save;
    bool owner_thread() const { return thread == std::this_thread::get_id(); }
    bool drain() const { return !abandoned && (save.active || bluewake_autosave_active()); }
    void reason(const char* s) noexcept {
        std::memset(status.reason, 0, sizeof status.reason);
        if (s) std::strncpy(status.reason, s, sizeof status.reason - 1);
    }
    void fail(const char* s) noexcept {
        status.stop_required = true; status.hold_seed_selections = true;
        status.phase = BW_REWARD_HOST_STOP_REQUIRED;
        save.publish = false; reason(s);
    }
    bool code_live() const {
        return owner_thread() && !stopped && !abandoned &&
            bw_ic_code_is_live(code, code_generation, descriptor);
    }
    bool memory_live() const {
        // Check actual owner/code BEFORE inspecting any borrowed CPU field.
        return code_live() && cpu && cpu_token && ram_token && backing_live &&
            cpu->ram == ram && cpu->ram_size == ram_size && ram_size == RamSize &&
            raw_alias == g_ppc_guest_alias_generation;
    }
    const uint8_t* resolve(uint32_t a, uint32_t n) const {
        if (!memory_live() || !n || uint64_t(a) + n > UINT32_MAX) return nullptr;
        const uint8_t* expected = nullptr;
        if (a >= NativeBase && uint64_t(a) + n <= uint64_t(NativeBase) + ram_size) {
            expected = ram + (a - NativeBase);
        } else {
            for (const auto& b : backings) {
                if (a >= b.linked_start && uint64_t(a) + n <= uint64_t(b.linked_start) + b.size) {
                    uint8_t* actual = nullptr;
                    if (!ppc_guest_alias_get_storage(b.linked_start, b.size, &actual) || actual != b.storage) return nullptr;
                    expected = actual + (a - b.linked_start); break;
                }
            }
        }
        if (!expected || get_ram_ptr(cpu, a, n, nullptr) != expected) return nullptr;
        // Whole-span success cannot authorize bytes shadowed by a smaller alias.
        for (uint32_t i = 0; i < n; ++i) {
            if (!memory_live() || get_ram_ptr(cpu, a + i, 1, nullptr) != expected + i) return nullptr;
        }
        return memory_live() ? expected : nullptr;
    }
    static const uint8_t* resolver(void* p, uint32_t a, uint32_t n) {
        return static_cast<BwRandomizerRewardHost*>(p)->resolve(a, n);
    }
    bool u8(uint32_t a, uint8_t& n) const { const auto* p=resolve(a,1); if(!p)return false; n=*p; return true; }
    bool u32(uint32_t a, uint32_t& n) const { const auto* p=resolve(a,4); if(!p)return false; n=be32(p); return true; }
    bool thread_stack(uint32_t& sp, uint32_t& native_thread) const {
        if (!memory_live() || cpu->exception || cpu->program_exception ||
            !u32(0x800000E4u, native_thread) || native_thread != MainThread) return false;
        const auto* t = resolve(native_thread, 0x310);
        uint32_t context;
        if (!t || be16(t + 0x2C8) != 2 || !u32(0x800000D4u, context) || context != native_thread) return false;
        sp=cpu->gpr[1]; const auto top=be32(t+0x304), bottom=be32(t+0x308);
        return !(sp&15u) && bottom < top && native_span(bottom,top-bottom) &&
            sp>bottom && sp<=top-16u && resolve(sp,16) && memory_live();
    }
    bool boundary(Boundary& out, uint32_t lr) {
        uint32_t sp, native_thread;
        if (!thread_stack(sp,native_thread) || cpu->lr != lr) return false;
        const auto token=issue(); if(!token){fail("Runtime token space exhausted");return false;}
        out={true,token,alias_token,epoch,status.native_card_load,sp,native_thread,lr,raw_alias};return true;
    }
    bool same_boundary(const Boundary& b) const {
        uint32_t sp,t;
        return b.live && b.alias==alias_token && b.raw_alias==raw_alias &&
            b.epoch==epoch && b.native_load==status.native_card_load &&
            thread_stack(sp,t) && sp==b.sp && t==b.thread;
    }
    bool mounted() const {
        const char* active=bluewake_card_runtime_path();
        return backend_open && active && path==active;
    }
    bool locked_card(Bytes& out) const {
        SnapshotLock lock(path);
        return lock.held && mounted() && read_file(path,out);
    }
    bool refresh_confirmed(const std::string* expected_digest=nullptr,uint64_t expected_generation=0) {
        ss::Snapshot s; std::string error;
        if (!session->snapshot(s,error) || s.retired || s.recovery_needed ||
            s.quest!=status.quest || s.module!=module_digest || s.origin_card!=origin ||
            !s.generation || !seed::is_digest(s.card_digest) ||
            (expected_digest&&s.card_digest!=*expected_digest) ||
            (expected_generation&&s.generation!=expected_generation)) {
            fail("Session confirmed pair unavailable; reopen verified pair");return false;
        }
        std::array<uint8_t,32> parsed{};
        if(!unhex(s.card_digest,parsed.data())){fail("Invalid confirmed digest");return false;}
        // The cached tuple changes together, only after a Session snapshot
        // copied the actual Store-issued receipt. Allocation/validation above
        // cannot partially acknowledge a new generation. A loss of this ack
        // after publication stops the adapter with its prior tuple intact.
        confirmed_digest.swap(s.card_digest);
        status.confirmed_generation=s.generation;
        std::copy(parsed.begin(),parsed.end(),status.confirmed_card_sha256);
        return true;
    }
    template<class F> bool ledger(F&& f) {
        std::string error;
        if(!session->with_ledger(lease,std::forward<F>(f),error)){fail(error.c_str());return false;}
        return true;
    }
    bool revise() {
        if(status.ledger_revision==UINT64_MAX){fail("Ledger revision exhausted");return false;}
        ++status.ledger_revision;return true;
    }
    void invalidate(tx::Invalidation why) {
        ledger([&](tx::Ledger& l){l.invalidate(why);});
        creation={};award={};chest={};item={};link={};chest_rel={};item_rel={};last_owner={};item_pid=0;
        scene_token=issue(); if(!scene_token)fail("Scene token exhausted");
        revise();
    }
    void revoke_cpu() noexcept {
        cpu=nullptr;ram=nullptr;ram_size=0;cpu_token=ram_token=alias_token=0;backing_live=false;
        backings={};materializations={};slots={};aliases={};slot_count=alias_count=0;tables_current=false;
        link={};chest={};item={};creation={};award={};load_read={};load={};chest_rel={};item_rel={};item_pid=0;
        status.native_load_authorized=false;status.native_card_load=0;
    }
    bool actor(BwRandomizerActorKind kind,uint32_t address,Tracked& tracked) {
        if (tracked.registration && tracked.owner.address!=address) return false;
        BwRandomizerActorView v{resolver,this}; BwRandomizerActorOwner o{};
        const auto pid=tracked.registration?tracked.owner.pid:0;
        if(!bw_randomizer_actor_owner(&v,kind,address,pid,&o)||!memory_live())return false;
        if(!tracked.registration) {
            tracked.registration=issue();if(!tracked.registration)return false;
            tracked.owner=o;
        } else if(o.address!=tracked.owner.address||o.pid!=tracked.owner.pid||
            o.profile!=tracked.owner.profile||o.methods!=tracked.owner.methods||
            o.actor_tag!=tracked.owner.actor_tag||o.process!=tracked.owner.process||
            o.init_state!=tracked.owner.init_state||o.create_result!=tracked.owner.create_result||
            o.room!=tracked.owner.room) return false;
        return true;
    }
    bool rel(BwRandomizerRelKind kind,BwRandomizerRelOwner& out) {
        if(!tables_current || !memory_live())return false;
        BwRandomizerRelView v{resolver,this,aliases.data(),uint32_t(alias_count),
            slots.data(),uint32_t(slot_count),sections.data(),uint32_t(sections.size())};
        return bw_randomizer_rel_owner(&v,kind,&out)&&memory_live();
    }
    tx::Actor copied_actor(const Tracked& t,const BwRandomizerRelOwner& r) const {
        return {t.owner.address,t.owner.pid,t.owner.process,uint16_t(r.module_id),
            r.materialization,t.registration,t.owner.init_state,t.owner.create_result,t.owner.room};
    }
    bool inventory(tx::Inventory& i) const {
        uint8_t slot,obtained;uint32_t queue;
        if(!u8(Info+0x44,slot)||!u8(Info+0x59,obtained)||!u32(0x803CA768u,queue))return false;
        i={slot,obtained,int32_t(queue)};return true;
    }
    bool owner(tx::Owner& out) {
        uint32_t player,stag,overlap;uint8_t quest,recollection,next_stage;
        if(!status.native_load_authorized || !mounted() || !memory_live() ||
            !u32(0x803CA74Cu,player)||!u32(0x803C9DA0u,stag)||!u8(Info+0x1290,quest)||
            quest!=status.quest || !u8(0x803CA8C8u,recollection)||recollection ||
            !u8(0x803C9D54u,next_stage)||next_stage||!u32(0x803F6160u,overlap)||overlap||
            !actor(BW_RANDOMIZER_ACTOR_LINK,player,link))return false;
        const auto* stage=resolve(0x803C9D3Cu,12);const auto* info=resolve(stag,0x20);
        if(!stage||!info)return false;
        size_t n=0;while(n<8&&stage[n]){
            const auto c=stage[n];if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;++n;
        }
        uint8_t stay;
        if(!n||n==8||!u8(0x803F6A78u,stay)||int8_t(stay)<0||int8_t(stage[10])<0||
            stage[10]!=stay||link.owner.room!=int8_t(stay))return false;
        const uint8_t table=(info[9]>>1)&0x7F;if(table>=16)return false;
        std::string name(reinterpret_cast<const char*>(stage),n);
        if(name=="GTower"||name.compare(0,5,"Xboss")==0)return false;
        if(!scene_token){scene_token=issue();if(!scene_token)return false;}
        out={module_digest,cpu_token,ram_token,code_token,alias_token,epoch,scene_token,
            status.native_card_load,player,link.owner.pid,name,int8_t(stay),table,quest};
        if(!last_owner.stage.empty() && (last_owner.stage!=out.stage||last_owner.room!=out.room||
           last_owner.player!=out.player||last_owner.player_pid!=out.player_pid)) {
            invalidate(tx::Invalidation::SceneChange);
            if(status.stop_required)return false;
            if(!actor(BW_RANDOMIZER_ACTOR_LINK,player,link))return false;
            out.scene=scene_token;out.player_pid=link.owner.pid;
        }
        tx::Status bound=tx::Status::Unavailable;
        if(!ledger([&](tx::Ledger& l){bound=l.bind(out);})||bound!=tx::Status::Accepted)return false;
        last_owner=out;return memory_live();
    }
    // The real name scene owns saveMemory+560. Verify its executing line queue
    // rather than accepting a caller-provided object or matching arbitrary RAM.
    bool name_scene(uint32_t a,uint32_t expected_pid,uint32_t& pid) const {
        const auto* p=resolve(a,0x1BBC);uint32_t base_type,tree,lists;
        if(!p||!native_span(a,0x1BBC)||!u32(0x803F6A18u,base_type)||!base_type||
            be32(p)!=base_type||!pid_ok(pid=be32(p+4))||(expected_pid&&pid!=expected_pid)||
            be16(p+8)!=0xC||be16(p+0xE)!=0xC||p[0xC]!=2||p[0xD]!=2||
            be32(p+0x10)!=0x80394590u||be32(p+0x14)||
            !u32(0x803F6180u,tree)||tree!=0x803BCD60u||!u32(0x803F6184u,lists)||lists!=16)return false;
        const uint32_t line=be32(p+0x48);if(line>=16||p[0x44]!=1||be32(p+0x40)!=a)return false;
        const uint32_t q=tree+line*12;const auto* list=resolve(q,12);if(!list)return false;
        uint32_t node=be32(list),tail=be32(list+4),count=be32(list+8),prev=0,matches=0;
        if(!count||count>1024)return false;
        std::array<uint32_t,1024> visited{};uint32_t n=0;
        while(node){
            if(n>=count||!native_span(node,0x14))return false;
            for(uint32_t j=0;j<n;++j)if(visited[j]==node)return false;
            visited[n++]=node;const auto* tag=resolve(node,0x14);
            if(!tag||be32(tag)!=prev||be32(tag+4)!=q||tag[0x10]!=1)return false;
            if(node==a+0x34&&be32(tag+0xC)==a)++matches;
            prev=node;node=be32(tag+8);
        }
        return n==count&&prev==tail&&matches==1&&memory_live();
    }
    bool baseline_compatible(const std::array<uint8_t,cp::GameBytes>& game,bool completing_save=false) {
        const auto* q=game.data()+status.quest*QuestSize;
        ss::Snapshot s;std::string error;
        if(!session->snapshot(s,error)||s.retired||s.recovery_needed)return false;
        const auto found=std::find_if(s.rewards.begin(),s.rewards.end(),[](const auto& r){return r.location_id==seed::linkug_location_id();});
        const bool saved=found!=s.rewards.end()&&(found->state==tx::State::Saved||
            (completing_save&&found->state==tx::State::NativeAwardCompleted));
        const bool open=(be32(q+PackedChest)&ChestMask)!=0;
        if(open!=saved)return false;
        // Never convert an existing or Deluxe camera into an experimental grant.
        // A reopened Picto reward must be represented by its immutable Saved row.
        if(reward==0x23) return saved ? q[0x44]==0x23&&(q[0x59]&3)==1 : q[0x44]==0xFF&&(q[0x59]&3)==0;
        return reward==0x06;
    }
    bool current_loaded_baseline() const {
        const auto* q=baseline_game.data()+status.quest*QuestSize;
        uint8_t slot,obtained,quest;uint32_t saved_chest;
        return u8(Info+0x1290,quest)&&quest==status.quest&&u8(Info+0x44,slot)&&slot==q[0x44]&&
            u8(Info+0x59,obtained)&&obtained==q[0x59]&&u32(0x803C5114u,saved_chest)&&saved_chest==be32(q+PackedChest);
    }
    bool menu(uint32_t a) const {
        const auto* m=resolve(a,0x1BA4);uint8_t quest,recollection;
        return m&&native_span(a,0x1BA4)&&be32(m)==0x803919A4u&&u8(Info+0x1290,quest)&&
            quest==status.quest&&u8(0x803CA8C8u,recollection)&&!recollection;
    }
    bool save_resources_idle() const {
        uint32_t write_pointer,queue;uint8_t flag,status_byte;
        const uint32_t play=Info+0x12A0;
        const auto* picture_count=resolve(play+0x48DE,2);
        if(!u32(Card+0x1654,write_pointer)||write_pointer||!picture_count||be16(picture_count)||
           !u8(play+0x495B,flag)||flag||!u8(play+0x495E,status_byte)||status_byte)return false;
        return reward!=0x06||(u32(0x803CA768u,queue)&&queue==0);
    }
    static void event(const BwGameEvent* e,void* user) noexcept {
        auto* h=static_cast<BwRandomizerRewardHost*>(user);
        if(!h||!e||!h->owner_thread()||h->stopped||h->abandoned)return;
        try {
            if(e->kind==BW_GAME_EVENT_RESET){
                if(e->reset_reason==BW_GAME_RESET_GAME_LOAD&&h->load.returned&&!h->load.reset){
                    h->load.reset=true;h->load.resulting_epoch=e->epoch;return;
                }
                if(e->reset_reason==BW_GAME_RESET_ATTACH&&!h->status.native_load_authorized&&!h->creation.live&&!h->award.live){h->epoch=e->epoch;return;}
                h->invalidate(tx::Invalidation::NativeLoad);h->status.native_load_authorized=false;
                h->fail("Uncorrelated native/machine reset; close and reopen seed pair");
            } else if(e->kind==BW_GAME_EVENT_SCENE_LEAVING||e->kind==BW_GAME_EVENT_TRANSITION_STARTED||e->kind==BW_GAME_EVENT_SCENE_ENTERED){
                // GameEvents emits old-scene leaving before a native Load arm.
                // That signal cannot grant CARD authority or erase load evidence.
                if(!h->status.native_load_authorized)return;
                h->invalidate(tx::Invalidation::SceneChange);
                if(h->save.active){h->save.publish=false;h->fail("Scene changed during manual save; draining native request");}
            }
        } catch(...) {h->fail("Exception while invalidating reward ownership");}
    }
    bool mutate_actor(uint32_t pc) {
        uint32_t tag=0;
        if(pc==TagAdd&&cpu->lr==0x8002400Cu&&cpu->gpr[3]==0x80372028u)tag=cpu->gpr[4];
        else if(pc==TagCut&&cpu->lr==0x8002402Cu)tag=cpu->gpr[3];
        else if(pc==TagCreate&&cpu->lr==0x8002404Cu&&uint64_t(cpu->gpr[4])+0xC4==cpu->gpr[3])tag=cpu->gpr[3];
        if(!tag)return true;
        if((link.registration&&link.owner.actor_tag==tag)||(chest.registration&&chest.owner.actor_tag==tag)||
           (item.registration&&item.owner.actor_tag==tag))invalidate(tx::Invalidation::ActorRemoved);
        return !status.stop_required||drain();
    }
    void revoke_rel(uint32_t index) {
        const auto token=materializations[index].token;
        materializations[index]={};slots[index].materialization=0;tables_current=false;
        if(token&&((chest_rel.materialization==token)||(item_rel.materialization==token)))invalidate(tx::Invalidation::AliasChange);
    }
    bool observe_load(uint32_t pc) {
        if(pc==LoadSync&&cpu->lr==LoadSyncReturn){
            uint32_t pid;const auto name=cpu->gpr[31];
            if(cpu->gpr[3]!=Card||cpu->gpr[4]!=uint64_t(name)+0x560||cpu->gpr[5]!=cp::GameBytes||cpu->gpr[6]||
               !name_scene(name,load_read.complete?load_read.pid:0,pid)||!mounted())return true;
            if(load_read.boundary.live){
                if(!same_boundary(load_read.boundary)||load_read.name!=name)fail("Nested/mismatched native CARD read");
                return !status.stop_required||drain();
            }
            Boundary b;if(!boundary(b,LoadSyncReturn)){fail("Native CARD read context refused");return false;}
            load_read={b,name,pid,cpu->gpr[4],false};return true;
        }
        if(pc==LoadSyncReturn&&load_read.boundary.live){
            uint32_t pid;
            if(!same_boundary(load_read.boundary)||!name_scene(load_read.name,load_read.pid,pid)){fail("Native CARD read return ownership changed");return false;}
            load_read.boundary.live=false;
            if(cpu->gpr[3]==0)return true;
            if(cpu->gpr[3]!=1){fail("Native CARD controller read failed");return false;}
            const auto* bytes=resolve(load_read.buffer,cp::GameBytes);
            const auto* control=resolve(Card,0x1698);
            if(!bytes||!control||std::memcmp(bytes,baseline_game.data(),cp::GameBytes)||
               std::memcmp(control,baseline_game.data(),cp::GameBytes)||be64(control+0x1688)!=baseline_match.card_serial||
               be32(control+0x1690)!=0){fail("Native controller read does not match confirmed pair");return false;}
            load_read.complete=true;return true;
        }
        if(pc==Load&&cpu->lr==LoadReturn){
            const auto name=cpu->gpr[22];uint32_t pid;
            if(!load_read.complete||name!=load_read.name||cpu->gpr[3]!=Info||cpu->gpr[4]!=load_read.buffer||
               cpu->gpr[5]!=status.quest||!name_scene(name,load_read.pid,pid)){fail("Native CARD load has no exact controller/name-scene read");return false;}
            const auto* n=resolve(name,0x1BBC);const auto* select=n?resolve(be32(n+0x428),0x394C):nullptr;
            const auto* bytes=resolve(load_read.buffer,cp::GameBytes);
            if(!select||be32(select+0x3938)!=load_read.buffer||select[0x3922]!=status.quest||
               select[0x392C]!=1||select[0x3914+status.quest]||!bytes||
               std::memcmp(bytes,baseline_game.data(),cp::GameBytes)){fail("Native selected quest/buffer baseline refused");return false;}
            if(load.boundary.live){if(!same_boundary(load.boundary)||load.name!=name)fail("Nested native load");return !status.stop_required;}
            Boundary b;if(!boundary(b,LoadReturn)){fail("Native load context refused");return false;}
            load={b,name,pid,load_read.buffer,false,false,0};return true;
        }
        if(pc==LoadReturn&&load.boundary.live){
            uint32_t pid;
            if(!same_boundary(load.boundary)||!name_scene(load.name,load.pid,pid)||cpu->gpr[3]!=0||!current_loaded_baseline()){
                fail("Native load return/postcondition refused");return false;
            }
            load.boundary.live=false;load.returned=true;return true;
        }
        return true;
    }
    bool observe_reward(uint32_t pc) {
        if(pc==Create||pc==CreateImport){
            if(cpu->lr!=CreateReturn)return true;
            if(status.hold_seed_selections||save.active||bluewake_autosave_active())return true;
            tx::Owner o;tx::Inventory inv;
            if(!owner(o)||!inventory(inv)||!actor(BW_RANDOMIZER_ACTOR_TBOX,cpu->gpr[30],chest)||
               !rel(BW_RANDOMIZER_REL_TBOX,chest_rel)){fail("Chest creation owner unavailable");return false;}
            const auto* c=resolve(chest.owner.address,0x770);
            // OpenInit/demo ordering may set the active bit around creation.
            // Its value is never selection/award authority; startup confirmed
            // packed baseline is closed, and saved chest correspondence is
            // checked only after the actual award/native save.
            if(!c||cpu->gpr[3]!=chest.owner.address+0x1F8||
               cpu->gpr[5]!=UINT32_MAX||cpu->gpr[6]!=UINT32_MAX||cpu->gpr[7]||cpu->gpr[8]){
                fail("Chest placement/native creation arguments refused");return false;
            }
            if(creation.live){
                if(!same_boundary(creation)||selected.chest.address!=chest.owner.address||
                   be32(c+0xB0)!=selected.parameters||be16(c+0x1E0)!=selected.home_angle_z||
                   (cpu->gpr[4]!=selected.original_item&&cpu->gpr[4]!=reward)){
                    invalidate(tx::Invalidation::ActorRemoved);fail("Creation replay differs from retained invocation");return false;
                }
                // Keep original evidence; r4 already carries the accepted reward.
                return true;
            }
            if(cpu->gpr[4]!=0x06){fail("Original native chest item is not06");return false;}
            Boundary b;if(!boundary(b,CreateReturn)){fail("Creation caller context refused");return false;}
            tx::Selection e{o,copied_actor(chest,chest_rel),inv,b.token,pc,cpu->lr,cpu->gpr[3],
                be32(c+0xB0),be16(c+0x1E0),uint8_t(cpu->gpr[4]),b.sp};
            tx::Decision decision;
            if(!ledger([&](tx::Ledger& l){decision=l.select(e);}))return false;
            if(decision.status==tx::Status::Duplicate)return true;
            if(decision.status!=tx::Status::Accepted||!decision.reward){fail("Ledger refused native selection");return false;}
            selected=std::move(e);creation=b;reward=*decision.reward;status.reward_item=reward;
            cpu->gpr[4]=reward; // SOLE guest modification in this adapter.
            ++status.substitutions;revise();status.phase=BW_REWARD_HOST_CREATING;return true;
        }
        if(pc==CreateReturn&&creation.live){
            tx::Owner o;
            if(!same_boundary(creation)||!owner(o)||!actor(BW_RANDOMIZER_ACTOR_TBOX,chest.owner.address,chest)||
               !rel(BW_RANDOMIZER_REL_TBOX,chest_rel)){invalidate(tx::Invalidation::ActorRemoved);fail("Creation return owner changed");return false;}
            const auto* c=resolve(chest.owner.address,0x770);
            if(!c||be32(c+0xB0)!=selected.parameters||be16(c+0x1E0)!=selected.home_angle_z){
                invalidate(tx::Invalidation::ActorRemoved);fail("Authored chest changed during native creation");return false;
            }
            tx::Status result;
            tx::Creation e{o,copied_actor(chest,chest_rel),creation.token,pc,cpu->gpr[3],creation.sp};
            if(!ledger([&](tx::Ledger& l){result=l.created(e);})||result!=tx::Status::Accepted){fail("Creation return/PID refused");return false;}
            item_pid=cpu->gpr[3];creation.live=false;revise();status.phase=BW_REWARD_HOST_AWAITING_DELETE;return true;
        }
        if(pc==Award||pc==AwardImport){
            if(cpu->lr!=AwardReturn||!item_pid)return true;
            tx::Owner o;tx::Inventory inv;
            if(!owner(o)||!inventory(inv)||!actor(BW_RANDOMIZER_ACTOR_DELETING_ITEM,cpu->gpr[31],item)||
               item.owner.pid!=item_pid||!rel(BW_RANDOMIZER_REL_DEMO_ITEM,item_rel)){
                invalidate(tx::Invalidation::ActorRemoved);fail("Deleting item native ownership refused");return false;
            }
            const auto* a=resolve(item.owner.address,0x65C);
            if(!a||(a[0x659]&1)||a[0x63A]!=reward||cpu->gpr[3]!=reward){fail("Deleting item argument/flags refused");return false;}
            if(award.live){if(!same_boundary(award))fail("Award replay caller changed");return !status.stop_required;}
            tx::Status bound;
            if(!ledger([&](tx::Ledger& l){bound=l.bind_item(o,copied_actor(item,item_rel));})||
               (bound!=tx::Status::Accepted&&bound!=tx::Status::Duplicate)){fail("Ledger refused deleting item binding");return false;}
            Boundary b;if(!boundary(b,AwardReturn)){fail("Award caller refused");return false;}
            tx::AwardEntry e{o,copied_actor(item,item_rel),inv,b.token,pc,cpu->lr,uint8_t(cpu->gpr[3]),a[0x63A],b.sp};
            if(!ledger([&](tx::Ledger& l){bound=l.award_enter(e);})||bound!=tx::Status::Accepted){fail("Ledger refused native award entry");return false;}
            award=b;revise();status.phase=BW_REWARD_HOST_AWARDING;return true;
        }
        if(pc==AwardReturn&&award.live){
            tx::Owner o;tx::Inventory inv;
            if(!same_boundary(award)||!owner(o)||!inventory(inv)||!actor(BW_RANDOMIZER_ACTOR_DELETING_ITEM,item.owner.address,item)||
               !rel(BW_RANDOMIZER_REL_DEMO_ITEM,item_rel)){invalidate(tx::Invalidation::ActorRemoved);fail("Award return owner changed");return false;}
            const auto* a=resolve(item.owner.address,0x65C);
            if(!a||(a[0x659]&1)||a[0x63A]!=reward){
                invalidate(tx::Invalidation::ActorRemoved);fail("Deleting item changed during native award");return false;
            }
            tx::Status result;tx::AwardReturn e{o,copied_actor(item,item_rel),inv,award.token,pc,award.sp};
            if(!ledger([&](tx::Ledger& l){result=l.award_return(e);})||result!=tx::Status::Accepted){fail("VOID award postcondition failed");return false;}
            award.live=false;item_pid=0;++status.completed_awards;revise();status.phase=BW_REWARD_HOST_UNSAVED;return true;
        }
        return true;
    }
    bool observe_save(uint32_t pc) {
        // This cohort does not claim the host-owned autosave scratch/native
        // protocol. Preserve its genuine helper pipeline, refuse publication,
        // and stop only AFTER it naturally drains.
        if(bluewake_autosave_active()&&(pc==Serialize||pc==Store||pc==Sync)){
            fail("Owned autosave publication unqualified; draining genuine pipeline");return true;
        }
        if(pc==Serialize&&cpu->lr==SerializeReturn){
            const uint32_t m=cpu->gpr[30];tx::Owner o;
            if(creation.live||award.live||item_pid||!owner(o)||!menu(m)||cpu->gpr[3]!=Info||
               cpu->gpr[4]!=uint64_t(m)+0x554||cpu->gpr[5]!=status.quest||!save_resources_idle()){
                fail("Manual serializer owner/quest/resource proof refused");return false;}
            if(save.active){
                if(!same_boundary(save.serialize)||save.menu!=m)fail("Nested manual serialization");
                return !status.stop_required||drain();
            }
            Boundary b;if(!boundary(b,SerializeReturn)){fail("Serializer caller refused");return false;}
            save={};save.active=true;save.serialize=b;save.menu=m;save.buffer=cpu->gpr[4];save.revision=status.ledger_revision;
            status.hold_seed_selections=true;status.phase=BW_REWARD_HOST_SAVE_PENDING;return true;
        }
        if(pc==SerializeReturn&&save.serialize.live){
            if(!same_boundary(save.serialize)||!menu(save.menu)||cpu->gpr[3]!=0||save.revision!=status.ledger_revision){
                fail("Manual serialization result/context refused; draining native menu");
            } else save.serialized=true;
            save.serialize.live=false;return true;
        }
        if(pc==Store&&cpu->lr==StoreReturn){
            // The pending native menu still proceeds after a seed validation
            // failure. Keep drain proof; never turn its void return into success.
            if(!save.active){fail("STORE has no matching manual serializer");return false;}
            const bool correspondence=menu(save.menu)&&cpu->gpr[30]==save.menu&&cpu->gpr[3]==Card&&cpu->gpr[4]==save.buffer&&
               cpu->gpr[5]==cp::GameBytes&&!cpu->gpr[6]&&save.revision==status.ledger_revision&&save_resources_idle();
            if(!correspondence){
                // Still retain the actual call/return for drain. The original
                // native caller must finish; this cannot authorize publication.
                fail("Manual STORE correspondence failed; draining native menu");
            }
            if(save.store.live){if(!same_boundary(save.store))fail("STORE replay owner changed");return true;}
            Boundary b;if(!boundary(b,StoreReturn)){fail("STORE caller refused; drain unavailable");return true;}
            if(b.sp!=save.serialize.sp||b.thread!=save.serialize.thread){fail("STORE differs from serializer frame; draining native menu");save.publish=false;}
            const auto* data=correspondence?resolve(save.buffer,cp::GameBytes):nullptr;
            if(correspondence&&!data)fail("STORE payload inaccessible; draining native menu");
            if(data)std::copy_n(data,cp::GameBytes,save.game.begin());
            save.store=b;save.stored=true;
            if(!save.serialized)save.publish=false;
            return true;
        }
        if(pc==StoreReturn&&save.store.live){
            if(!same_boundary(save.store)||!menu(save.menu))fail("STORE return owner changed; draining native menu");
            else save.store_returned=true;
            save.store.live=false;return true;
        }
        if(pc==Sync&&(cpu->lr==SyncImmediate||cpu->lr==SyncWait)&&save.active){
            const auto menu_register=cpu->lr==SyncImmediate?cpu->gpr[30]:cpu->gpr[31];
            if(cpu->gpr[3]!=Card||menu_register!=save.menu||!menu(save.menu)){
                fail("Manual SaveSync ownership inaccessible; drain unavailable");return true;}
            if(!save.stored||!save.store_returned)fail("Manual SaveSync lacks qualified STORE; draining native menu");
            if(save.poll.live){if(!same_boundary(save.poll))fail("SaveSync replay owner changed");return true;}
            Boundary b;if(!boundary(b,cpu->lr)){fail("SaveSync caller refused; drain unavailable");return true;}
            save.poll=b;return true;
        }
        if((pc==SyncImmediate||pc==SyncWait)&&save.poll.live&&pc==save.poll.lr){
            if(!same_boundary(save.poll)||!menu(save.menu)){
                fail("SaveSync return ownership changed; drain unavailable");return true;
            }
            if(save.revision!=status.ledger_revision)fail("Ledger changed during SaveSync; publication refused");
            save.poll.live=false;
            if(cpu->gpr[3]==0)return true;
            save.active=false;
            if(cpu->gpr[3]!=1){fail("Native manual save failed");return false;}
            if(status.stop_required||!save.publish)return false;
            const auto* control=resolve(Card,0x1698);
            if(!control||be32(control+0x1660)!=1||be32(control+0x1654)!=0||
               be64(control+0x1688)!=baseline_match.card_serial||be32(control+0x1690)!=0||
               std::memcmp(control,save.game.data(),cp::GameBytes)){
                fail("SaveSync consumed success but payload/photo ownership differs");return false;
            }
            save.completed=true;status.phase=BW_REWARD_HOST_CHECKPOINT_PENDING;return true;
        }
        return true;
    }
};

extern "C" BwRandomizerRewardHost* bw_randomizer_reward_host_create(const BwRewardHostStartup* s) noexcept {
    try {
        if(!s||!s->dedicated_seed_directory_utf8||!s->canonical_profile||!s->canonical_profile_size||
           s->canonical_profile_size>seed::MaxProfileBytes||s->quest>=3||!s->admitted_code||!s->code_generation||
           !s->admitted_descriptor||bluewake_card_runtime_path()!=nullptr||
           (s->explicit_initial_card_size&&!s->explicit_initial_card)||s->explicit_initial_card_size>cp::MaxCardBytes)return nullptr;
        uint8_t module[32];
        if(!bw_ic_code_artifact_sha256(s->admitted_code,s->code_generation,s->admitted_descriptor,module))return nullptr;
        const auto* d=s->admitted_descriptor;
        static const char game[8]={'G','Z','L','E','0','1',0,0};
        if(std::memcmp(d->game_id,game,8)||d->cpu_abi_version!=GXRUNTIME_CPU_ABI_VERSION||d->cpu_state_size!=sizeof(CPUState)||
           !d->dispatch||!d->rel_modules||!d->num_rel_modules||d->num_rel_modules>MaxSections)return nullptr;
        seed::ProfileBuilder builder(bluewake::randomizer::imported_catalog());
        auto profile=builder.decode(std::string(reinterpret_cast<const char*>(s->canonical_profile),s->canonical_profile_size));
        if(!profile||!profile.value->audited_linkug_binding()||profile.value->placements().size()!=1)return nullptr;
        const auto placement=profile.value->reward_at(seed::linkug_location_id());
        const auto reward=placement?seed::native_reward(*placement):std::optional<seed::NativeReward>{};
        if(!reward)return nullptr;
        auto h=std::make_unique<BwRandomizerRewardHost>();h->code=s->admitted_code;h->descriptor=d;h->code_generation=s->code_generation;
        h->code_token=issue();if(!h->code_token)return nullptr;
        h->module_digest=hex(module);h->origin=hex(s->origin_card_sha256);h->status.quest=s->quest;
        h->status.phase=BW_REWARD_HOST_AWAITING_CARD;h->reward=reward->item;h->status.reward_item=h->reward;
        bool target_seen[2]={};
        for(uint32_t i=0;i<d->num_rel_modules;++i){
            const auto& m=d->rel_modules[i];
            if(m.module_id!=113&&m.module_id!=131)continue;
            const unsigned target=m.module_id==113?0:1;
            if(target_seen[target])return nullptr;target_seen[target]=true;
            if(m.num_sections>MaxSections-h->sections.size()||(m.num_sections&&!m.sections))return nullptr;
            for(uint32_t j=0;j<m.num_sections;++j){const auto& p=m.sections[j];h->sections.push_back({p.module_id,p.section_index,p.linked_start,p.size});}
        }
        if(!target_seen[0]||!target_seen[1])return nullptr;
        Bytes initial;
        if(s->explicit_initial_card_size){
            initial.assign(s->explicit_initial_card,s->explicit_initial_card+s->explicit_initial_card_size);
            std::array<uint8_t,cp::GameBytes> game{};cp::Match match{};
            if(digest(initial)!=h->origin||cp::inspect(initial.data(),initial.size(),game,match)!=cp::Status::Matched)return nullptr;
            const auto* q=game.data()+s->quest*QuestSize;
            // A NEW experiment starts only from the explicit closed-chest
            // baseline. Inspect before Session can publish an empty baseline;
            // a malformed/incompatible input must never create that pair.
            if((be32(q+PackedChest)&ChestMask)||(h->reward==0x23&&(q[0x44]!=0xFF||(q[0x59]&3))))return nullptr;
        }
        tx::Mount mount{h->origin,h->module_digest,s->quest};std::string error;
        h->session=ss::Session::open(s->dedicated_seed_directory_utf8,*profile.value,mount,
            s->explicit_initial_card_size?&initial:nullptr,error);
        if(!h->session||!h->session->lease(h->lease,error)||!h->session->working_card_path(h->path,error)||!h->refresh_confirmed()||
           !read_file(h->path,h->baseline)||digest(h->baseline)!=h->confirmed_digest||
           cp::inspect(h->baseline.data(),h->baseline.size(),h->baseline_game,h->baseline_match)!=cp::Status::Matched||
           !h->baseline_compatible(h->baseline_game))return nullptr;
        h->reason("Confirmed compatible pair restored; native CARD load required");return h.release();
    } catch(...) {return nullptr;}
}
extern "C" bool bw_randomizer_reward_host_card_path(BwRandomizerRewardHost* h,char* out,size_t cap) noexcept {
    if(out&&cap)out[0]=0;
    if(!h||!h->owner_thread()||!out||cap<=h->path.size()||h->stopped||h->status.stop_required)return false;
    std::memcpy(out,h->path.c_str(),h->path.size()+1);return true;
}
extern "C" bool bw_randomizer_reward_host_backend_opened(BwRandomizerRewardHost* h) noexcept {
    if(!h||!h->owner_thread()||h->backend_open||h->stopped)return false;
    try {
        const auto* p=bluewake_card_runtime_path();if(!p||h->path!=p){h->fail("Backend mounted a different CARD");return false;}
        h->backend_open=true;Bytes card;
        if(!h->locked_card(card)||digest(card)!=h->confirmed_digest||card!=h->baseline){h->fail("Mounted CARD differs from confirmed pair");return false;}
        h->status.phase=BW_REWARD_HOST_AWAITING_NATIVE_LOAD;return true;
    } catch(...){h->fail("Mounted CARD validation exception");return false;}
}
extern "C" bool bw_randomizer_reward_host_shared_backing_committed(BwRandomizerRewardHost* h,CPUState* cpu,
    const BwRewardHostBacking* supplied,size_t count) noexcept {
    if(!h||!h->owner_thread()||h->stopped||h->abandoned||!h->code_live()||!cpu||cpu!=h->cpu||!supplied||count!=2||
       cpu->ram!=h->ram||cpu->ram_size!=h->ram_size||h->status.stop_required)return false;
    try {
        const uint32_t before_generation=g_ppc_guest_alias_generation;
        std::array<BwRewardHostBacking,2> next{};bool found[2]={};
        for(size_t i=0;i<count;++i){const auto& b=supplied[i];unsigned which=2;
            for(unsigned j=0;j<2;++j)if(b.linked_start==DataStarts[j]&&b.size==DataSizes[j])which=j;
            uint8_t* actual=nullptr;
            if(which==2||found[which]||!b.storage||!ppc_guest_alias_get_storage(b.linked_start,b.size,&actual)||actual!=b.storage)return false;
            for(uint32_t j=0;j<b.size;++j)if(get_ram_ptr(cpu,b.linked_start+j,1,nullptr)!=b.storage+j)return false;
            found[which]=true;next[which]=b;
        }
        if(before_generation!=g_ppc_guest_alias_generation||cpu->ram!=h->ram||cpu->ram_size!=h->ram_size||!h->code_live())return false;
        const uint64_t token=issue();if(!token)return false;
        h->backings=next;h->alias_token=token;h->raw_alias=before_generation;h->backing_live=true;return true;
    } catch(...){h->fail("Shared backing commit exception");return false;}
}
extern "C" bool bw_randomizer_reward_host_cpu_ready(BwRandomizerRewardHost* h,CPUState* cpu,
    const BwRewardHostBacking* supplied,size_t count) noexcept {
    if(!h||!h->owner_thread()||h->status.stop_required||h->cpu||!h->code_live()||!h->mounted()||!cpu||!cpu->ram||cpu->ram_size!=RamSize)return false;
    try {
        h->cpu=cpu;h->ram=cpu->ram;h->ram_size=cpu->ram_size;h->cpu_token=issue();h->ram_token=issue();h->scene_token=issue();
        if(!h->cpu_token||!h->ram_token||!h->scene_token||!bw_randomizer_reward_host_shared_backing_committed(h,cpu,supplied,count)){
            h->revoke_cpu();h->fail("CPU/RAM/shared backing attachment refused");return false;}
        BwGameEventStats stats{};bluewake_game_events_stats(&stats);h->epoch=stats.epoch;
        const auto mask=BW_GAME_EVENT_MASK(BW_GAME_EVENT_RESET)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_SCENE_LEAVING)|
            BW_GAME_EVENT_MASK(BW_GAME_EVENT_SCENE_ENTERED)|BW_GAME_EVENT_MASK(BW_GAME_EVENT_TRANSITION_STARTED);
        h->subscription=bluewake_game_events_subscribe(mask,BwRandomizerRewardHost::event,h);
        if(!h->subscription){h->revoke_cpu();h->fail("GameEvents subscription unavailable");return false;}
        return true;
    } catch(...){h->revoke_cpu();h->fail("CPU attachment exception");return false;}
}
extern "C" bool bw_randomizer_reward_host_before_owner_change(BwRandomizerRewardHost* h,BwRewardHostChange why) noexcept {
    if(!h||!h->owner_thread()||h->stopped||h->abandoned)return false;
    if(h->drain()){h->fail("Native save must drain before host mutation/teardown");return false;}
    try {
        tx::Invalidation invalid=tx::Invalidation::MachineLoad;
        switch(why){
        case BW_REWARD_HOST_SHARED_ALIAS_CHANGE:invalid=tx::Invalidation::AliasChange;break;
        case BW_REWARD_HOST_CPU_REPLACED:invalid=tx::Invalidation::CpuReplaced;break;
        case BW_REWARD_HOST_RAM_REPLACED:invalid=tx::Invalidation::RamReplaced;break;
        case BW_REWARD_HOST_MODULE_REPLACED:invalid=tx::Invalidation::ModuleReload;break;
        case BW_REWARD_HOST_SHUTDOWN:invalid=tx::Invalidation::Shutdown;break;
        case BW_REWARD_HOST_STATE_CAPTURE:
            if(!h->creation.live&&!h->award.live&&!h->item_pid){h->reason("STATE capture is unsupported in seed mode");return false;}
            break;
        default:break;
        }
        h->invalidate(invalid);
        if(why==BW_REWARD_HOST_SHARED_ALIAS_CHANGE){h->backing_live=false;h->alias_token=0;h->tables_current=false;return !h->status.stop_required;}
        h->revoke_cpu();h->fail("Native owner revoked; seed recovery requires close/reopen");return true;
    } catch(...){h->fail("Owner revocation exception");h->revoke_cpu();return true;}
}
extern "C" void bw_randomizer_reward_host_rel_will_change(BwRandomizerRewardHost* h,uint32_t index) noexcept {
    if(!h||!h->owner_thread()||h->stopped||index>=MaxSlots)return;
    try{h->revoke_rel(index);}catch(...){h->fail("REL revocation exception");}
}
extern "C" bool bw_randomizer_reward_host_rel_materialized(BwRandomizerRewardHost* h,uint32_t index,uint32_t a,uint32_t capacity) noexcept {
    if(!h||!h->owner_thread()||!h->code_live()||index>=MaxSlots||!h->memory_live()||h->status.stop_required||
       !capacity||a<NativeBase||uint64_t(a)+capacity>uint64_t(NativeBase)+h->ram_size||h->materializations[index].token)return false;
    const auto token=issue();if(!token){h->fail("REL token exhausted");return false;}
    h->materializations[index]={a,capacity,0,token};h->tables_current=false;return true;
}
extern "C" bool bw_randomizer_reward_host_rel_loader_associated(BwRandomizerRewardHost* h,uint32_t index,uint32_t owner) noexcept {
    if(!h||!h->owner_thread()||index>=MaxSlots||!h->memory_live()||h->status.stop_required)return false;
    auto& m=h->materializations[index];uint32_t raw;
    if(!m.token||!native_span(owner,0x14)||!h->u32(owner+0x10,raw)||raw!=m.address||(m.owner&&m.owner!=owner))return false;
    m.owner=owner;h->tables_current=false;return true;
}
extern "C" bool bw_randomizer_reward_host_rel_tables(BwRandomizerRewardHost* h,const BwRandomizerRelAlias* aliases,size_t alias_count,
    const BwRewardHostRelSlot* slots,size_t slot_count) noexcept {
    if(!h||!h->owner_thread()||!h->memory_live()||alias_count>MaxSlots||slot_count>MaxSlots||
       (alias_count&&!aliases)||(slot_count&&!slots))return false;
    h->tables_current=false;
    for(size_t i=0;i<slot_count;++i){const auto& s=slots[i];const auto& m=h->materializations[i];
        const bool same=m.token&&m.owner&&m.owner==s.owner&&m.address==s.address&&m.capacity==s.capacity;
        h->slots[i]={s.owner,s.address,s.capacity,same?m.token:0};
    }
    for(size_t i=slot_count;i<MaxSlots;++i)h->slots[i]={};
    if(alias_count)std::copy_n(aliases,alias_count,h->aliases.begin());
    h->slot_count=slot_count;h->alias_count=alias_count;h->tables_current=true;return true;
}
extern "C" bool bw_randomizer_reward_host_observes(const BwRandomizerRewardHost* h,const CPUState* cpu,uint32_t pc) noexcept {
    if(!h||!h->owner_thread()||h->stopped||h->abandoned||h->cpu!=cpu)return false;
    if(native_entry(pc))return true;
    return (h->creation.live&&pc==CreateReturn)||(h->award.live&&pc==AwardReturn)||
        (h->load_read.boundary.live&&pc==LoadSyncReturn)||(h->load.boundary.live&&pc==LoadReturn)||
        (h->save.serialize.live&&pc==SerializeReturn)||(h->save.store.live&&pc==StoreReturn)||
        (h->save.poll.live&&pc==h->save.poll.lr);
}
extern "C" bool bw_randomizer_reward_host_dispatch(BwRandomizerRewardHost* h,CPUState* cpu,uint32_t pc) noexcept {
    if(!h||!h->owner_thread()||h->stopped||h->abandoned)return false;
    if(!bw_randomizer_reward_host_observes(h,cpu,pc))return !h->status.stop_required||h->drain();
    if(h->inside){h->fail("Recursive native reward dispatch refused");return h->drain();}
    if(!h->memory_live()){h->fail("Native CPU/RAM/code/backing lifetime unavailable");return h->drain();}
    h->inside=true;
    try {
        bool result=true;
        if(pc==TagAdd||pc==TagCut||pc==TagCreate)result=h->mutate_actor(pc);
        else if(pc==Unlink&&cpu->lr==0x80240F8Cu){
            for(size_t i=0;i<MaxSlots;++i)if(h->materializations[i].token&&
                (h->materializations[i].address==cpu->gpr[3]||h->materializations[i].owner==cpu->gpr[30]))h->revoke_rel(uint32_t(i));
        } else if(pc==LoadSync||pc==LoadSyncReturn||pc==Load||pc==LoadReturn)result=h->observe_load(pc);
        else if(pc==Serialize||pc==SerializeReturn||pc==Store||pc==StoreReturn||pc==Sync||pc==SyncImmediate||pc==SyncWait)
            result=h->observe_save(pc);
        else if(!h->status.stop_required)result=h->observe_reward(pc);
        h->inside=false;return result&&(!h->status.stop_required||h->drain());
    } catch(...){h->inside=false;h->fail("Native reward observation exception");return h->drain();}
}
extern "C" bool bw_randomizer_reward_host_maintenance(BwRandomizerRewardHost* h) noexcept {
    if(!h||!h->owner_thread()||h->stopped||h->abandoned||h->inside)return false;
    try {
        if(h->status.stop_required)return h->drain();
        // Main services GameEvents synchronously after the native return and
        // before this maintenance boundary. A missing/cancelled CALL_LOAD must
        // not hold guest continuation waiting for a reset it can no longer
        // produce. No native save was started by this load transaction.
        if(h->load.returned&&!h->load.reset){
            h->fail("Native load returned without correlated GAME_LOAD; event proof unavailable");return false;
        }
        if(h->load.returned&&h->load.reset){
            Bytes card;
            if(!h->memory_live()||!h->mounted()||!h->locked_card(card)||digest(card)!=h->confirmed_digest||card!=h->baseline||
               !h->current_loaded_baseline()){h->fail("Native reload no longer matches locked confirmed generation");return false;}
            const auto token=issue();if(!token){h->fail("Native load token exhausted");return false;}
            h->epoch=h->load.resulting_epoch;h->status.native_card_load=token;h->status.native_load_authorized=true;
            h->load={};h->load_read={};h->status.phase=BW_REWARD_HOST_READY;h->reason("Exact native CARD load completed; awaiting audited chest");
        }
        if(h->save.completed){
            if(!h->memory_live()||!h->mounted()||h->save.revision!=h->status.ledger_revision||bluewake_autosave_active()){
                h->fail("Checkpoint ownership changed before publication");return false;}
            tx::Owner owner;if(!h->owner(owner)){h->fail("Live native owner unavailable for checkpoint");return false;}
            Bytes card;cp::Match match{};
            if(!h->locked_card(card)||cp::validate(card.data(),card.size(),h->save.game.data(),h->save.game.size(),match)!=cp::Status::Matched||
               !h->baseline_compatible(h->save.game,true)||
               !cp::preserves_baseline(h->baseline.data(),h->baseline.size(),card.data(),card.size(),h->status.quest)){
                h->fail("Locked native CARD payload/preservation mismatch");return false;}
            if(h->status.confirmed_generation==UINT64_MAX||h->status.published_saves==UINT64_MAX){h->fail("Checkpoint generation/counter exhausted");return false;}
            // Allocate the expected digest before publication. Confirmation is
            // still read back from Session's Store-issued tuple, not inferred
            // from either the bytes or this anticipated successor number.
            const auto candidate_digest=digest(card);
            const auto candidate_generation=h->status.confirmed_generation+1;
            std::string error;
            if(!h->session->publish_snapshot(card,error)||!h->refresh_confirmed(&candidate_digest,candidate_generation)){
                h->fail("Paired CARD+ledger publication/ack failed; reopen verified pair");return false;
            }
            h->baseline.swap(card);h->baseline_game=h->save.game;h->baseline_match=match;h->save={};
            ++h->status.published_saves;h->status.hold_seed_selections=false;h->status.phase=BW_REWARD_HOST_READY;
            h->reason("Native save and complete CARD+ledger generation confirmed");
        }
        return true;
    } catch(...){h->fail("Native checkpoint exception; confirmed pair preserved");return h->drain();}
}
extern "C" void bw_randomizer_reward_host_status(const BwRandomizerRewardHost* h,BwRewardHostStatus* out) noexcept {
    if(!out)return;*out={};
    if(!h||!h->owner_thread())return;
    *out=h->status;out->drain_native_save=h->drain();
    out->hold_guest_mutators=out->drain_native_save||h->save.completed||h->load.returned;
    out->hold_seed_selections=out->hold_seed_selections||out->hold_guest_mutators||!out->native_load_authorized;
}
extern "C" bool bw_randomizer_reward_host_stop(BwRandomizerRewardHost* h) noexcept {
    if(!h||!h->owner_thread())return false;if(h->stopped)return true;
    if(h->drain()){h->fail("Cannot stop while genuine native save drains");return false;}
    try{h->invalidate(tx::Invalidation::Shutdown);}catch(...){h->fail("Stop invalidation exception");}
    if(h->subscription){bluewake_game_events_unsubscribe(h->subscription);h->subscription=0;}
    h->revoke_cpu();h->stopped=true;h->status.phase=BW_REWARD_HOST_STOPPED;return true;
}
extern "C" void bw_randomizer_reward_host_abandon_unavailable(BwRandomizerRewardHost* h) noexcept {
    if(!h||!h->owner_thread()||h->abandoned)return;
    try{h->invalidate(tx::Invalidation::Shutdown);}catch(...){}
    if(h->subscription){bluewake_game_events_unsubscribe(h->subscription);h->subscription=0;}
    h->save={};h->revoke_cpu();h->abandoned=true;h->stopped=true;h->status.stop_required=true;
    h->status.phase=BW_REWARD_HOST_DRAIN_UNAVAILABLE;h->reason("Native continuation impossible; drain unqualified, confirmed pair retained");
}
extern "C" bool bw_randomizer_reward_host_destroy(BwRandomizerRewardHost* h) noexcept {
    if(!h||!h->owner_thread()||h->drain())return false;
    // A global native backend must be closed, even if its path was replaced.
    if(bluewake_card_runtime_path()!=nullptr)return false;
    if(!h->stopped&&!bw_randomizer_reward_host_stop(h))return false;
    delete h;return true;
}
