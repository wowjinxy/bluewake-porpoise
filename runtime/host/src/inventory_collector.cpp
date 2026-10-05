// SPDX-License-Identifier: GPL-3.0-or-later
// Experimental read-only collector. The only guest operations are bounded reads.
#include "inventory_collector.h"
#include "inventory_completion_internal.h"
#include "network_digest.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <thread>
#include <utility>

namespace bluewake::inventory_collector {
namespace {
constexpr std::uint32_t kHostRamSize = 0x02000000u, kNativeEnd = 0x81800000u;
constexpr std::uint32_t kEntry = 0x8003EF38u, kReturn = 0x80023960u;
constexpr std::uint32_t kPlayerExecute = 0x80122D30u;
constexpr std::uint32_t kPlayer = 0x803CA74Cu, kLink = 0x803CA754u;
constexpr std::uint32_t kProfile = 0x8038FD8Cu, kMethods = 0x8038FD68u;
constexpr std::uint32_t kBaseType = 0x803F6A18u, kActorType = 0x803F69D0u;
constexpr std::uint32_t kStage = 0x803C9D3Cu, kNextFlag = 0x803C9D54u;
constexpr std::uint32_t kRoom = 0x803F6A78u, kOverlap = 0x803F6160u;
constexpr std::uint32_t kEvent = 0x803C9EA2u, kPause = 0x803F7097u;
constexpr std::uint32_t kRecollection = 0x803CA8C8u, kFrame = 0x803E8140u;
constexpr std::uint32_t kCard = 0x803B39A0u;
constexpr std::array<std::uint32_t, 5> kInventory = {
    0x803C4C46u, 0x803C4C5Bu, 0x803C4C47u, 0x803C4C5Cu, 0x803C4CC5u};
constexpr std::array<subset::ApprovedCapability, 5> kCapabilities = {
    subset::ApprovedCapability::OwnWindWaker,
    subset::ApprovedCapability::OwnGrapplingHook,
    subset::ApprovedCapability::LearnedWindsRequiem,
    subset::ApprovedCapability::PlayWindsRequiem,
    subset::ApprovedCapability::DefeatGohma};
std::atomic<std::uint64_t> g_issuer{0}; // host process lifetime, never STATE data
std::atomic<RegistryState*> g_registry{nullptr};
std::atomic<CollectorState*> g_collector{nullptr};

std::uint64_t issue() noexcept {
    auto old = g_issuer.load(std::memory_order_relaxed);
    for (;;) {
        if (old == std::numeric_limits<std::uint64_t>::max()) return 0;
        if (g_issuer.compare_exchange_weak(old, old + 1,
            std::memory_order_relaxed, std::memory_order_relaxed)) return old + 1;
    }
}
void increment(std::uint64_t& n) noexcept {
    if (n != std::numeric_limits<std::uint64_t>::max()) ++n;
}
bool same_lease(const BindingLease& a, const BindingLease& b) noexcept {
    return a.live && b.live && a.cpu_lifetime == b.cpu_lifetime &&
        a.memory_lifetime == b.memory_lifetime && a.module_generation == b.module_generation &&
        a.alias_binding == b.alias_binding && a.registration_revision == b.registration_revision &&
        a.raw_alias_generation == b.raw_alias_generation && a.cpu == b.cpu &&
        a.registered_ram == b.registered_ram && a.registered_ram_size == b.registered_ram_size &&
        a.module == b.module && a.provenance == b.provenance;
}
bool same_guard_flags(const HostGuard& a, const HostGuard& b) noexcept {
    return a.input_generation == b.input_generation &&
        a.initialized_controls == b.initialized_controls &&
        a.explicit_headless_no_ui == b.explicit_headless_no_ui &&
        a.input_blocked == b.input_blocked && a.menu_open == b.menu_open &&
        a.machine_capture_pending == b.machine_capture_pending &&
        a.machine_load_pending == b.machine_load_pending && a.reset_pending == b.reset_pending &&
        a.rel_lifecycle_pending == b.rel_lifecycle_pending &&
        a.module_replace_pending == b.module_replace_pending && a.relaunch_pending == b.relaunch_pending &&
        a.shutdown_pending == b.shutdown_pending && a.native_autosave_active == b.native_autosave_active &&
        a.quick_door_active == b.quick_door_active &&
        a.quick_items_overlay_active == b.quick_items_overlay_active &&
        a.card_callback_active == b.card_callback_active;
}
Availability guard_reason(const HostGuard& g) noexcept {
    if (!g.valid || !g.revision || (!g.initialized_controls && !g.explicit_headless_no_ui))
        return Availability::UnknownGuard;
    if (g.input_blocked || g.menu_open || g.machine_capture_pending || g.machine_load_pending ||
        g.reset_pending || g.rel_lifecycle_pending || g.module_replace_pending ||
        g.relaunch_pending || g.shutdown_pending || g.native_autosave_active ||
        g.quick_door_active || g.quick_items_overlay_active || g.card_callback_active)
        return Availability::HostBusy;
    return Availability::Captured;
}
std::uint64_t hash_le(std::uint64_t h, std::uint64_t value, unsigned count) noexcept {
    for (unsigned i = 0; i < count; ++i) {
        h ^= (value >> (8u * i)) & 255u; h *= UINT64_C(1099511628211);
    }
    return h;
}
void append_be(std::string& s, std::uint64_t v, unsigned count) {
    for (unsigned i = count; i; --i) s.push_back(static_cast<char>(v >> (8u * (i - 1))));
}
} // namespace

struct RegistryState {
    const std::thread::id thread = std::this_thread::get_id();
    std::atomic<bool> revoked{true};
    std::atomic<bool> foreign_revocation{false};
    BindingLease binding;
    HostGuard guard;
    bool claimed = false, exhausted = false;
    void* read_gate_context = nullptr;
    OwnedBindingRegistry::TrustedLivenessGate read_gate = nullptr;
    bool capture_eligible = false;
    BindingLease capture_origin;
};
struct CollectorState {
    const std::thread::id thread = std::this_thread::get_id();
    std::atomic<bool> revoked{true};
    RunId run;
    HostProviders providers;
    subset::CapabilityLogic logic;
    RegistryState* registry = nullptr;
    BwGameEventSubscription subscription = 0;
    bool enabled = false, suspended = false, retired = false, callback_active = false;
    BindingLease admitted;
    std::uint64_t last_sequence = 0, last_entry_call = 0, last_completed_call = 0;
    Diagnostic diagnostic;
    struct {
        bool active = false, bound = false, consumed = false;
        const CPUState* cpu = nullptr;
        const BwGameEvent* event = nullptr; // callback scope only, cleared at emit_end
        BwInventoryCollectorCallProof proof{};
    } emission;
    CollectorState(RunId r, HostProviders p, bluewake::randomizer::Catalog c)
        : run(r), providers(p), logic(std::move(c)) {}
};

namespace {
bool registry_read_gate(const RegistryState* r) noexcept {
    return !r->read_gate || (r->read_gate_context && r->read_gate(r->read_gate_context));
}
bool registry_binding_copy(void* raw, BindingLease* out) noexcept {
    if (out) *out = {};
    auto* r = static_cast<RegistryState*>(raw);
    if (!out || !r || g_registry.load(std::memory_order_acquire) != r ||
        r->thread != std::this_thread::get_id() || r->revoked.load(std::memory_order_acquire) ||
        !r->binding.live || r->exhausted || !registry_read_gate(r)) return false;
    *out = r->binding; // no CPU dereference, even if the borrowed pointer is dead
    return true;
}
bool registry_guard_copy(void* raw, HostGuard* out) noexcept {
    if (out) *out = {};
    auto* r = static_cast<RegistryState*>(raw);
    if (!out || !r || g_registry.load(std::memory_order_acquire) != r ||
        r->thread != std::this_thread::get_id() || r->revoked.load(std::memory_order_acquire) ||
        !registry_read_gate(r)) return false;
    *out = r->guard; return true; // copied flags only; no native or SDL provider poll
}
void close_scope(CollectorState* s) noexcept { if (s) s->emission = {}; }
void invalidate(CollectorState* s, Availability why, bool retire = false) noexcept {
    if (!s || s->thread != std::this_thread::get_id()) return;
    close_scope(s);
    s->diagnostic.availability = why;
    increment(s->diagnostic.lifecycle_invalidations);
    if (s->diagnostic.history) s->diagnostic.history->authorization_expired = true;
    if (retire) { s->retired = true; s->revoked.store(true, std::memory_order_release); }
}
CollectorState* live_collector() noexcept {
    auto* s = g_collector.load(std::memory_order_acquire);
    if (!s || s->thread != std::this_thread::get_id() ||
        s->revoked.load(std::memory_order_acquire) || !s->enabled || s->suspended || s->retired)
        return nullptr;
    return s;
}
// Fetching and validating the issuer-owned record ALWAYS precedes CPU reads.
bool owned_lease(CollectorState* s, const CPUState* cpu, BindingLease& b,
                 HostGuard& guard, Availability& why) noexcept {
    why = Availability::Unbound;
    if (!s || s != live_collector() || !s->registry ||
        s->providers.host_registration != s->registry ||
        s->providers.binding_copy != registry_binding_copy ||
        s->providers.guard_copy != registry_guard_copy ||
        !s->providers.binding_copy(s->providers.host_registration, &b)) return false;
    if (!b.live || !b.cpu_lifetime || !b.memory_lifetime || !b.module_generation ||
        !b.alias_binding || !b.registration_revision || b.module != kQualifiedMetadata ||
        !b.registered_ram || b.registered_ram_size != kHostRamSize || !b.cpu || cpu != b.cpu ||
        !same_lease(b, s->admitted)) {
        why = Availability::Retired; invalidate(s, why, true); return false;
    }
    // raw alias is a change detector, not a lifetime token. Unknown change retires.
    if (g_ppc_guest_alias_generation != b.raw_alias_generation) {
        why = Availability::Retired; invalidate(s, why, true); return false;
    }
    if (!s->providers.guard_copy(s->providers.host_registration, &guard)) return false;
    why = guard_reason(guard);
    if (why != Availability::Captured) return false;
    // FIRST borrowed CPU dereference is below. Host lifetime above is live.
    if (cpu->ram != b.registered_ram || cpu->ram_size != b.registered_ram_size) {
        why = Availability::Retired; invalidate(s, why, true); return false;
    }
    return true;
}
class Reader {
    CollectorState* s_;
    const CPUState* cpu_;
public:
    BindingLease lease;
    HostGuard guard;
    Availability failure = Availability::ReadUnavailable;
    bool ok;
    Reader(CollectorState* s, const CPUState* cpu) noexcept : s_(s), cpu_(cpu),
        ok(owned_lease(s, cpu, lease, guard, failure)) {}
    bool stable() noexcept {
        if (!ok) return false;
        BindingLease now; HostGuard current; Availability why;
        if (!owned_lease(s_, cpu_, now, current, why) || !same_lease(now, lease) ||
            current.revision != guard.revision || !same_guard_flags(current, guard)) {
            ok = false; failure = why == Availability::Captured ? Availability::UnstableCapture : why;
        }
        return ok;
    }
    const std::uint8_t* pointer(std::uint32_t address, std::uint32_t size) noexcept {
        if (!stable()) return nullptr;
        if (!size || address < 0x80000000u || size > 0x01800000u || address > kNativeEnd - size) {
            ok = false; failure = Availability::ReadUnavailable; return nullptr;
        }
        auto* mutable_cpu = const_cast<CPUState*>(cpu_); // resolver is read-only
        auto* p = get_ram_ptr(mutable_cpu, address, size, nullptr);
        const auto* flat = lease.registered_ram + (address - 0x80000000u);
        if (!p) { ok = false; failure = Availability::ReadUnavailable; return nullptr; }
        if (p != flat) { ok = false; failure = Availability::AliasedFixedField; return nullptr; }
        for (std::uint32_t i = 0; i < size; ++i) {
            const auto* byte = get_ram_ptr(mutable_cpu, address + i, 1, nullptr);
            if (byte != p + i || byte != flat + i) {
                ok = false; failure = Availability::AliasedFixedField; return nullptr;
            }
        }
        return stable() ? p : nullptr;
    }
    std::uint8_t u8(std::uint32_t a) noexcept {
        const auto* p = pointer(a, 1); return p ? p[0] : 0;
    }
    std::uint16_t u16(std::uint32_t a) noexcept {
        const auto* p = pointer(a, 2); return p ? (std::uint16_t(p[0]) << 8) | p[1] : 0;
    }
    std::uint32_t u32(std::uint32_t a) noexcept {
        const auto* p = pointer(a, 4); return p ? (std::uint32_t(p[0]) << 24) |
            (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | p[3] : 0;
    }
    float f32(std::uint32_t a) noexcept {
        const auto bits = u32(a); float value; std::memcpy(&value, &bits, sizeof value); return value;
    }
};
struct NativeIdentity {
    std::uint32_t player = 0, actor_id = 0, frame = 0;
    BwGameScene scene{};
};
bool native_identity(Reader& r, const CPUState* cpu, NativeIdentity& n) noexcept {
    if (!r.ok) return false;
    if (cpu->exception || cpu->gpr[1] % 16 || !r.pointer(cpu->gpr[1], 8) ||
        r.u32(0x800000D4u) != 0x803A2960u || r.u32(0x800000E4u) != 0x803A2960u ||
        r.u8(kPause) || r.u8(kEvent) || r.u8(kNextFlag) || r.u32(kOverlap) ||
        r.u8(kRecollection) || r.u32(kCard + 0x165Cu) || r.u32(kCard + 0x1660u) != 1)
        return false;
    auto* stage = r.pointer(kStage, 12);
    if (!stage) return false;
    bool ended = false;
    for (unsigned i = 0; i < 8; ++i) {
        const auto ch = stage[i];
        if (!ch) { ended = true; continue; }
        if (ended || (!(ch >= 'a' && ch <= 'z') && !(ch >= 'A' && ch <= 'Z') &&
            !(ch >= '0' && ch <= '9') && ch != '_')) return false;
        n.scene.stage[i] = static_cast<char>(ch);
    }
    if (!n.scene.stage[0]) return false;
    if (!std::memcmp(n.scene.stage, "Xboss", 5) &&
        n.scene.stage[5] >= '0' && n.scene.stage[5] <= '3' && n.scene.stage[6] == 0) return false;
    n.scene.spawn = (std::uint16_t(stage[8]) << 8) | stage[9];
    n.scene.room = static_cast<std::int8_t>(stage[10]);
    n.scene.layer = static_cast<std::int8_t>(stage[11]);
    n.scene.stay_room = static_cast<std::int8_t>(r.u8(kRoom));
    if (n.scene.stay_room < 0) return false;
    n.player = r.u32(kPlayer);
    if (!n.player || n.player % 4 || n.player != r.u32(kLink) ||
        n.player < 0x80000000u || n.player > kNativeEnd - 0x4C28u) return false;
    const auto p = n.player;
    const auto base = r.u32(kBaseType), type = r.u32(kActorType);
    n.actor_id = r.u32(p + 4);
    if (!base || !type || r.u32(p) != base || r.u32(p + 0xC0) != type ||
        !n.actor_id || n.actor_id >= UINT32_MAX - 1u ||
        r.u16(p + 8) != 0xA9 || r.u8(p + 0xB) || r.u8(p + 0xC) != 2 || r.u8(p + 0xD) != 2 ||
        r.u16(p + 0xE) != 0xA9 || r.u32(p + 0x10) != kProfile ||
        r.u32(p + 0xEC) != kMethods || r.u8(p + 0x1BE) != 1 || (r.u32(p + 0x1C8) & 2u) ||
        r.u16(kProfile + 8) != 0xA9 || r.u32(kProfile + 0x10) != 0x4C28u ||
        r.u32(kProfile + 0x24) != kMethods || r.u32(kMethods + 8) != kPlayerExecute ||
        r.u32(p + 0x498) != p + 0x1F8 || r.u16(p + 0x304) || r.u32(p + 0x314)) return false;
    for (unsigned i = 0; i < 3; ++i) {
        n.scene.position[i] = r.f32(p + 0x1F8 + i * 4);
        if (!std::isfinite(n.scene.position[i]) || std::fabs(n.scene.position[i]) >= 1.0e32f) return false;
    }
    n.frame = r.u32(kFrame);
    n.scene.player = p; n.scene.active = n.scene.player_valid = n.scene.controls_ready = true;
    return r.stable();
}
bool scene_equal(const BwGameScene& s, const BwInventoryCollectorCallProof& p) noexcept {
    return s.active && s.player_valid && s.controls_ready && !s.paused && !s.event_running &&
        !s.transitioning && s.player == p.player && !std::memcmp(s.stage, p.stage, 9) &&
        s.spawn == p.spawn && s.room == p.room && s.layer == p.layer && s.stay_room == p.stay_room;
}
bool proof_equal(const BwInventoryCollectorCallProof& p, Reader& r, const NativeIdentity& n) noexcept {
    return p.valid && p.cpu_lifetime == r.lease.cpu_lifetime && p.memory_lifetime == r.lease.memory_lifetime &&
        p.module_generation == r.lease.module_generation && p.alias_binding == r.lease.alias_binding &&
        p.raw_alias_generation == r.lease.raw_alias_generation && p.guard_revision == r.guard.revision &&
        !std::memcmp(p.module, r.lease.module.data(), 32) && p.player == n.player && p.actor_id == n.actor_id &&
        p.entry_frame == n.frame && scene_equal(n.scene, p);
}
bool producer_context(const BwInventoryCollectorCallProof& p) noexcept {
    BwGameScene scene{}; std::uint64_t epoch = 0, generation = 0;
    return bluewake_game_events_scene(&scene, &epoch, &generation) &&
        epoch == p.epoch && generation == p.scene_generation && scene_equal(scene, p);
}
subset::OwnerStamp stamp(const BwInventoryCollectorCallProof& p) noexcept {
    subset::OwnerStamp s;
    std::memcpy(s.module.data(), p.module, 32);
    s.cpu_lifetime = p.cpu_lifetime; s.memory_lifetime = p.memory_lifetime;
    s.module_generation = p.module_generation; s.native_epoch = p.epoch;
    s.scene_generation = p.scene_generation; s.alias_generation = p.alias_binding;
    s.native_frame = p.entry_frame; s.player = p.player;
    std::memcpy(s.stage.data(), p.stage, 9); return s;
}
bool complete_proof(CollectorState* s, const CPUState* cpu,
                    const BwInventoryCollectorCallProof& p, bool entry) noexcept {
    Reader r(s, cpu); NativeIdentity n;
    if (!r.ok || !native_identity(r, cpu, n) || !proof_equal(p, r, n) ||
        cpu->gpr[1] != p.stack || !producer_context(p)) return false;
    if (entry) return cpu->pc == kEntry && cpu->lr == kReturn &&
        cpu->gpr[4] == p.player && cpu->gpr[3] == kMethods && r.u32(cpu->gpr[3] + 8) == kPlayerExecute && r.stable();
    return cpu->pc == kReturn && cpu->gpr[30] == p.player && r.stable();
}
void unavailable(CollectorState* s, History& h, Availability why) noexcept {
    h.availability = why; h.authorization_expired = true; h.bytes.reset(); h.owner.reset();
    h.immediate = {}; h.projection_status = subset::Status::Incomplete;
    s->diagnostic.history = h; s->diagnostic.availability = why;
    if (why == Availability::UnstableCapture) increment(s->diagnostic.unstable_snapshots);
    else increment(s->diagnostic.unavailable_reads);
}
void capture_completed(CollectorState* s, const BwGameEvent* event) {
    History h; h.run = s->run; h.sequence = event->sequence; h.native_call = event->native_call;
    h.source_address = event->source_address;
    BwInventoryCollectorCallProof p{};
    if (!bw_inventory_collector_copy_emitted_lease(event, &p)) {
        unavailable(s, h, Availability::MissingCompletionProof); return;
    }
    Reader first(s, s->emission.cpu); NativeIdentity before;
    h.provenance = first.lease.provenance; h.actor_id = p.actor_id;
    if (!first.ok || !native_identity(first, s->emission.cpu, before) || !proof_equal(p, first, before) ||
        !producer_context(p) || !scene_equal(event->scene, p)) {
        unavailable(s, h, first.ok ? Availability::UnsafeNativeContext : first.failure); return;
    }
    std::array<std::uint8_t, 5> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = first.u8(kInventory[i]);
    if (!first.stable()) { unavailable(s, h, first.failure); return; }
    Reader second(s, s->emission.cpu); NativeIdentity after;
    std::array<std::uint8_t, 5> again{};
    for (std::size_t i = 0; i < again.size(); ++i) again[i] = second.u8(kInventory[i]);
    if (!second.ok || !native_identity(second, s->emission.cpu, after) ||
        !proof_equal(p, second, after) || !same_lease(first.lease, second.lease) ||
        first.guard.revision != second.guard.revision || bytes != again ||
        !producer_context(p) || !second.stable()) {
        unavailable(s, h, second.ok ? Availability::UnstableCapture : second.failure); return;
    }
    const auto owner = stamp(p);
    subset::CopiedInventoryObservation observation;
    observation.before = owner; observation.after = owner;
    observation.safety = subset::ContextSafety{true, true, true, false, false, false, false, false, 0};
    observation.waker_slot = bytes[0]; observation.waker_obtained = bytes[1];
    observation.hook_slot = bytes[2]; observation.hook_obtained = bytes[3]; observation.songs = bytes[4];
    // Only this stack owns Context. It is destroyed before returning to C emit.
    const auto context = subset::project_inventory(observation, owner, kQualifiedMetadata);
    h.projection_status = context.status();
    for (std::size_t i = 0; i < kCapabilities.size(); ++i)
        h.immediate[i] = s->logic.evaluate(kCapabilities[i], context, owner);
    // No authorization remains after this callback; all copied getters are past facts.
    h.bytes = bytes; h.owner = owner; h.native_frame = p.entry_frame;
    h.availability = Availability::Captured; h.authorization_expired = true;
    s->diagnostic.history = h; s->diagnostic.availability = Availability::Captured;
    increment(s->diagnostic.successful_snapshots);
}
} // namespace

bool validate_owned_descriptor(const StaticRecompModuleDesc& m,
    subset::ModuleFingerprint* out, Availability* why) noexcept {
    if (out) *out = {};
    if (why) *why = Availability::InvalidDescriptor;
    try {
        static constexpr char game[8] = {'G','Z','L','E','0','1',0,0};
        if (!out || m.abi_version != 5 || m.cpu_abi_version != 6 ||
            GXRUNTIME_CPU_ABI_VERSION != 6 || m.cpu_state_size != sizeof(CPUState) || sizeof(CPUState) != 3552 ||
            std::memcmp(m.game_id, game, 8) || !m.dispatch || m.num_chunk_ranges != 748 ||
            !m.chunk_ranges || !m.chunk_hashes || !m.code_ranges || !m.num_code_ranges ||
            m.num_code_ranges > 748) return false;
        constexpr char prefix[] = "BW-NATIVE-INVENTORY-MODULE-v1";
        std::string canonical(prefix, sizeof(prefix) - 1);
        canonical.push_back('\0'); canonical.reserve(12022);
        append_be(canonical, m.abi_version, 4); append_be(canonical, m.cpu_abi_version, 4);
        append_be(canonical, m.cpu_state_size, 4); append_be(canonical, m.num_chunk_ranges, 4);
        canonical.append(m.game_id, 8);
        std::uint64_t fnv = hash_le(UINT64_C(14695981039346656037), m.num_chunk_ranges, 4);
        std::uint32_t previous = 0;
        for (std::uint32_t i = 0; i < m.num_chunk_ranges; ++i) {
            const auto r = m.chunk_ranges[i];
            if (r.start >= r.end || (r.start & 3u) || (r.end & 3u) || (i && r.start < previous)) return false;
            previous = r.end;
            fnv = hash_le(fnv, r.start, 4); fnv = hash_le(fnv, r.end, 4); fnv = hash_le(fnv, m.chunk_hashes[i], 8);
            append_be(canonical, r.start, 4); append_be(canonical, r.end, 4); append_be(canonical, m.chunk_hashes[i], 8);
        }
        // Require exact code-range tiling by those chunks, not a guessed envelope.
        std::uint32_t chunk = 0;
        previous = 0;
        for (std::uint32_t i = 0; i < m.num_code_ranges; ++i) {
            const auto r = m.code_ranges[i];
            if (r.start >= r.end || (r.start & 3u) || (r.end & 3u) || (i && r.start < previous)) return false;
            std::uint32_t cursor = r.start;
            while (chunk < m.num_chunk_ranges && cursor < r.end) {
                const auto c = m.chunk_ranges[chunk++];
                if (c.start != cursor || c.end > r.end) return false;
                cursor = c.end;
            }
            if (cursor != r.end) return false;
            previous = r.end;
        }
        if (chunk != m.num_chunk_ranges || canonical.size() != 12022) return false;
        const auto hex = bw_net::digest(canonical);
        if (hex.size() != 64) return false;
        subset::ModuleFingerprint fingerprint{};
        auto digit = [](char c) -> unsigned { return c >= '0' && c <= '9' ? unsigned(c - '0') :
            c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10) : 16; };
        for (std::size_t i = 0; i < fingerprint.size(); ++i) {
            const auto high = digit(hex[i * 2]), low = digit(hex[i * 2 + 1]);
            if (high > 15 || low > 15) return false;
            fingerprint[i] = static_cast<std::uint8_t>((high << 4) | low);
        }
        if (fingerprint != kQualifiedMetadata || fnv != UINT64_C(0x49de3ad1ae035336)) {
            if (why) *why = Availability::UnqualifiedModule; return false;
        }
        *out = fingerprint; if (why) *why = Availability::Captured; return true;
    } catch (...) { if (why) *why = Availability::EvaluationFailure; return false; }
}
bool owner_available_for_start() noexcept {
    return g_registry.load(std::memory_order_acquire) == nullptr &&
        g_collector.load(std::memory_order_acquire) == nullptr;
}

OwnedBindingRegistry::OwnedBindingRegistry() : state_(std::make_unique<RegistryState>()) {}
OwnedBindingRegistry::~OwnedBindingRegistry() {
    if (!state_) return;
    state_->revoked.store(true, std::memory_order_release);
    if (state_->thread != std::this_thread::get_id()) {
        state_->foreign_revocation.store(true, std::memory_order_release);
        // Unsupported off-thread destruction: fail closed without a dangling callback.
        // Leak the revoked host record, rather than read/free a borrowed game owner.
        (void)state_.release(); return;
    }
    suspend_before_actual_action(SuspendReason::Shutdown);
    auto* expected = state_.get(); g_registry.compare_exchange_strong(expected, nullptr);
}
bool OwnedBindingRegistry::install_read_gate(void* context, TrustedLivenessGate gate) noexcept {
    auto* r = state_.get();
    if (!r || r->thread != std::this_thread::get_id() || r->foreign_revocation.load() ||
        r->claimed || r->binding.registration_revision || r->read_gate || !context || !gate ||
        !gate(context)) return false;
    r->read_gate_context = context; r->read_gate = gate; return true;
}
bool OwnedBindingRegistry::bind_after_actual_action(BindingAction action, const CPUState& cpu,
    const std::uint8_t* ram, std::uint32_t size, const subset::ModuleFingerprint& module,
    Provenance provenance) noexcept {
    auto* r = state_.get();
    if (!r || r->thread != std::this_thread::get_id() || r->exhausted || r->foreign_revocation.load() ||
        action < BindingAction::InitialBind || action > BindingAction::MachineStateCaptureCompleted ||
        !registry_read_gate(r)) return false;
    auto* expected = static_cast<RegistryState*>(nullptr);
    if (!g_registry.compare_exchange_strong(expected, r) && expected != r) return false;
    r->claimed = true;
    if (action == BindingAction::MachineStateCaptureCompleted) {
        const bool eligible = r->capture_eligible && r->capture_origin.registration_revision &&
            r->capture_origin.registration_revision == r->binding.registration_revision &&
            r->capture_origin.cpu == &cpu && r->capture_origin.registered_ram == ram &&
            r->capture_origin.registered_ram_size == size &&
            r->capture_origin.raw_alias_generation == g_ppc_guest_alias_generation;
        r->capture_eligible = false; r->capture_origin = {}; // consume even on refusal
        if (!eligible) return false; // before dereferencing a possibly stale CPU
    } else { r->capture_eligible = false; r->capture_origin = {}; }
    // Caller owns this actual CPU/RAM only after a completed host action.
    if (module != kQualifiedMetadata || !ram || size != kHostRamSize || cpu.ram != ram || cpu.ram_size != size) {
        suspend_before_actual_action(SuspendReason::MemoryReplace); return false;
    }
    if (r->binding.live) return false; // caller must retire before actual action
    BindingLease next;
    next.cpu_lifetime = issue(); next.memory_lifetime = issue(); next.module_generation = issue();
    next.alias_binding = issue(); next.registration_revision = issue();
    if (!next.cpu_lifetime || !next.memory_lifetime || !next.module_generation ||
        !next.alias_binding || !next.registration_revision) { r->exhausted = true; return false; }
    // Conservative: every explicit rebind renews all ownership generations.
    // Content origin is separate from lifetime/module/alias ownership changes.
    switch (action) {
        case BindingAction::InitialBind:
            if (r->binding.registration_revision) return false;
            provenance = Provenance::FreshNativeBoot; break;
        case BindingAction::MachineResetCompleted:
            provenance = Provenance::FreshNativeBoot; break;
        case BindingAction::NativeCardLoadCompleted:
            provenance = Provenance::NativeCardLoad; break;
        case BindingAction::MachineStateLoadCompleted:
            provenance = Provenance::MachineStateLoad; break;
        case BindingAction::CpuReplaced: case BindingAction::RamReplaced:
        case BindingAction::ModuleReloadCompleted: case BindingAction::AliasRebuildCompleted:
        case BindingAction::MachineStateCaptureCompleted:
            if (!r->binding.registration_revision) return false;
            provenance = r->binding.provenance; break;
        default: return false;
    }
    next.live = true; next.cpu = &cpu; next.registered_ram = ram; next.registered_ram_size = size;
    next.module = module; next.provenance = provenance;
    next.raw_alias_generation = g_ppc_guest_alias_generation;
    r->guard = {}; r->binding = next; r->revoked.store(false, std::memory_order_release); return true;
}
void OwnedBindingRegistry::suspend_before_actual_action(SuspendReason reason) noexcept {
    auto* r = state_.get(); if (!r) return;
    if (r->thread != std::this_thread::get_id()) {
        r->foreign_revocation.store(true, std::memory_order_release);
        r->revoked.store(true, std::memory_order_release); return;
    }
    if (g_registry.load(std::memory_order_acquire) != r) return;
    auto* s = g_collector.load(std::memory_order_acquire);
    r->capture_eligible = reason == SuspendReason::MachineStateCapture && r->binding.live &&
        !r->revoked.load() && !r->foreign_revocation.load() && registry_read_gate(r) &&
        r->binding.raw_alias_generation == g_ppc_guest_alias_generation &&
        (!s || s->registry != r || (!s->retired && !s->suspended && !s->revoked.load()));
    r->capture_origin = r->capture_eligible ? r->binding : BindingLease{};
    r->revoked.store(true, std::memory_order_release);
    // NO CPU dereference. Revoke before alias/RAM/CPU/module/capture can change.
    r->binding.live = false; r->guard = {};
    bw_inventory_collector_retire_pending_proofs();
    if (s && s->registry == r) { s->suspended = true; invalidate(s, Availability::Suspended); }
}
bool OwnedBindingRegistry::observe_owned_alias_change(std::uint32_t raw) noexcept {
    auto* r = state_.get();
    if (!r || r->thread != std::this_thread::get_id() || g_registry.load() != r ||
        r->revoked.load() || !r->binding.live || r->exhausted || raw != g_ppc_guest_alias_generation ||
        !registry_read_gate(r)) return false;
    const auto alias = issue(), revision = issue();
    if (!alias || !revision) { r->exhausted = true; suspend_before_actual_action(SuspendReason::AliasRebuild); return false; }
    r->capture_eligible = false; r->capture_origin = {};
    bw_inventory_collector_retire_pending_proofs();
    r->binding.alias_binding = alias; r->binding.registration_revision = revision;
    r->binding.raw_alias_generation = raw; r->guard = {};
    auto* s = g_collector.load();
    if (s && s->registry == r) { s->suspended = true; invalidate(s, Availability::Suspended); }
    return true;
}
bool OwnedBindingRegistry::update_host_guard(const HostGuard& actual) noexcept {
    auto* r = state_.get();
    if (!r || r->thread != std::this_thread::get_id() || g_registry.load() != r ||
        r->revoked.load() || !r->binding.live || r->exhausted || !registry_read_gate(r)) return false;
    if (!actual.valid) {
        r->guard = {}; bw_inventory_collector_retire_pending_proofs();
        auto* s = g_collector.load();
        if (s && s->registry == r) invalidate(s, Availability::UnknownGuard);
        return false;
    }
    HostGuard next = actual;
    if (r->guard.valid && same_guard_flags(r->guard, next)) return true;
    next.revision = issue();
    if (!next.revision) { r->exhausted = true; suspend_before_actual_action(SuspendReason::MachineReset); return false; }
    // Any guard change retires an invocation, even if flags become safe again.
    bw_inventory_collector_retire_pending_proofs(); r->guard = next;
    auto* s = g_collector.load();
    if (s && s->registry == r) invalidate(s, guard_reason(next));
    return true;
}
bool OwnedBindingRegistry::native_card_load_completed() noexcept {
    auto* r = state_.get();
    if (!r || r->thread != std::this_thread::get_id() || g_registry.load() != r || r->revoked.load() ||
        !r->binding.live || !registry_read_gate(r)) return false;
    const auto memory = issue(), revision = issue();
    if (!memory || !revision) { r->exhausted = true; suspend_before_actual_action(SuspendReason::MemoryReplace); return false; }
    r->capture_eligible = false; r->capture_origin = {};
    bw_inventory_collector_retire_pending_proofs(); r->binding.memory_lifetime = memory;
    r->binding.registration_revision = revision; r->binding.provenance = Provenance::NativeCardLoad;
    r->guard = {};
    auto* s = g_collector.load();
    if (s && s->registry == r) { s->admitted = r->binding; invalidate(s, Availability::UnknownGuard); }
    return true;
}
bool OwnedBindingRegistry::copy_registered_binding(BindingLease* out) const noexcept {
    return registry_binding_copy(state_.get(), out);
}
bool OwnedBindingRegistry::copy_host_guard(HostGuard* out) const noexcept {
    return registry_guard_copy(state_.get(), out);
}
HostProviders OwnedBindingRegistry::providers() const noexcept {
    return {state_.get(), registry_binding_copy, registry_guard_copy};
}

Collector::Collector(RunId run, HostProviders providers, bluewake::randomizer::Catalog catalog)
    : state_(std::make_unique<CollectorState>(run, providers, std::move(catalog))) {}
Collector::~Collector() {
    if (!state_) return;
    state_->revoked.store(true, std::memory_order_release);
    if (state_->thread != std::this_thread::get_id()) { (void)state_.release(); return; }
    disable();
}
bool Collector::enable(const StaticRecompModuleDesc& descriptor) noexcept {
    auto* s = state_.get();
    if (!s || s->thread != std::this_thread::get_id()) return false;
    auto* current = g_collector.load(std::memory_order_acquire);
    if (current && current != s) { s->diagnostic.availability = Availability::OwnerAlreadyActive; return false; }
    if (s->enabled) return false;
    bool nonzero = false; for (auto b : s->run) nonzero |= b != 0;
    if (!nonzero || s->logic.catalog_status() != subset::Status::Ready) {
        s->diagnostic.availability = Availability::EvaluationFailure; return false;
    }
    subset::ModuleFingerprint metadata; Availability why;
    if (!validate_owned_descriptor(descriptor, &metadata, &why)) { s->diagnostic.availability = why; return false; }
    auto* registry = g_registry.load(std::memory_order_acquire);
    BindingLease binding;
    if (!registry || s->providers.host_registration != registry ||
        s->providers.binding_copy != registry_binding_copy || s->providers.guard_copy != registry_guard_copy ||
        !registry_binding_copy(registry, &binding) || binding.module != metadata) {
        s->diagnostic.availability = Availability::Unbound; return false;
    }
    auto* expected = static_cast<CollectorState*>(nullptr);
    if (!g_collector.compare_exchange_strong(expected, s)) {
        s->diagnostic.availability = Availability::OwnerAlreadyActive; return false;
    }
    s->registry = registry; s->admitted = binding; s->retired = false; s->suspended = false;
    const auto mask = BW_GAME_EVENT_ALL & ~BW_GAME_EVENT_MASK(BW_GAME_EVENT_GAME_TICK);
    s->subscription = bluewake_game_events_subscribe(mask, registered_event, s);
    if (!s->subscription) {
        expected = s; g_collector.compare_exchange_strong(expected, nullptr);
        s->registry = nullptr; s->diagnostic.availability = Availability::SubscriptionUnavailable; return false;
    }
    s->enabled = true; s->revoked.store(false, std::memory_order_release);
    s->diagnostic.availability = Availability::UnknownGuard; return true;
}
void Collector::suspend(SuspendReason) noexcept {
    auto* s = state_.get(); if (!s) return;
    s->revoked.store(true, std::memory_order_release);
    if (s->thread != std::this_thread::get_id() || g_collector.load() != s) return;
    s->suspended = true; bw_inventory_collector_retire_pending_proofs(); invalidate(s, Availability::Suspended);
}
bool Collector::resume() noexcept {
    auto* s = state_.get(); BindingLease binding;
    if (!s || s->thread != std::this_thread::get_id() || g_collector.load() != s || !s->enabled ||
        !registry_binding_copy(s->registry, &binding) || binding.module != kQualifiedMetadata ||
        binding.raw_alias_generation != g_ppc_guest_alias_generation) return false;
    // Resume is host-only and must follow new issuer binding or reported alias.
    if ((s->suspended || s->retired) && same_lease(binding, s->admitted)) return false;
    s->admitted = binding; s->retired = false; s->suspended = false; close_scope(s);
    s->revoked.store(false, std::memory_order_release);
    s->diagnostic.availability = Availability::UnknownGuard; return true;
}
void Collector::disable() noexcept {
    auto* s = state_.get(); if (!s) return;
    s->revoked.store(true, std::memory_order_release);
    if (s->thread != std::this_thread::get_id()) return;
    if (g_collector.load() == s) bw_inventory_collector_retire_pending_proofs();
    if (s->subscription) bluewake_game_events_unsubscribe(s->subscription);
    s->subscription = 0; s->enabled = false; s->suspended = false; close_scope(s);
    auto* expected = s; g_collector.compare_exchange_strong(expected, nullptr);
    s->diagnostic.availability = Availability::Disabled;
}
Diagnostic Collector::diagnostic_copy() const noexcept {
    const auto* s = state_.get();
    if (!s || s->thread != std::this_thread::get_id()) {
        Diagnostic d; d.availability = Availability::WrongThread; return d;
    }
    auto result = s->diagnostic;
    if (result.history) result.history->authorization_expired = true;
    return result;
}
void Collector::registered_event(const BwGameEvent* event, void* raw) noexcept {
    auto* s = static_cast<CollectorState*>(raw);
    if (!s || s->thread != std::this_thread::get_id() || g_collector.load() != s || !event || !s->enabled) return;
    // No failure or allocation may unwind C emission. Scope closes on all exits.
    struct ScopeClose { CollectorState* s; ~ScopeClose() { s->callback_active = false; close_scope(s); } } scope{s};
    s->callback_active = true;
    try {
        if (!event->sequence || event->sequence <= s->last_sequence || !event->epoch || !event->scene_generation) {
            invalidate(s, Availability::StaleEvent, true); bw_inventory_collector_retire_pending_proofs(); return;
        }
        s->last_sequence = event->sequence;
        if (event->kind == BW_GAME_EVENT_RESET) {
            invalidate(s, Availability::Suspended); bw_inventory_collector_retire_pending_proofs();
            if (event->reset_reason == BW_GAME_RESET_GAME_LOAD && !s->suspended && !s->retired &&
                s->registry && !s->registry->revoked.load()) {
                const auto memory = issue(), revision = issue();
                if (!memory || !revision) { s->registry->exhausted = true; invalidate(s, Availability::IssuerExhausted, true); return; }
                s->registry->binding.memory_lifetime = memory;
                s->registry->binding.registration_revision = revision;
                s->registry->binding.provenance = Provenance::NativeCardLoad;
                s->registry->guard = {}; s->admitted = s->registry->binding;
            } else if (event->reset_reason != BW_GAME_RESET_ATTACH) {
                // Host reset must be explicitly re-bound by the actual source seam.
                s->suspended = true; s->revoked.store(true, std::memory_order_release);
            }
            return;
        }
        if (event->kind != BW_GAME_EVENT_PLAYER_UPDATED) {
            invalidate(s, Availability::MissingCompletionProof);
            bw_inventory_collector_retire_pending_proofs(); return;
        }
        increment(s->diagnostic.completed_attempts);
        if (s != live_collector()) { s->diagnostic.availability = Availability::Suspended; return; }
        capture_completed(s, event);
    } catch (...) {
        History failed; failed.run = s->run; failed.sequence = event->sequence;
        failed.native_call = event->native_call; failed.source_address = event->source_address;
        unavailable(s, failed, Availability::EvaluationFailure);
        bw_inventory_collector_retire_pending_proofs();
    }
}
} // namespace bluewake::inventory_collector

using namespace bluewake::inventory_collector;
extern "C" bool bw_inventory_collector_completion_enabled(void) noexcept {
    return live_collector() != nullptr; // no CPU, alias, provider or SDL queries
}
extern "C" bool bw_inventory_collector_call_begin(const CPUState* cpu, std::uint64_t call,
    std::uint64_t epoch, std::uint64_t generation, std::uint32_t stack, std::uint32_t player,
    BwInventoryCollectorCallProof* out) noexcept {
    if (out) *out = {};
    try {
        auto* s = live_collector();
        if (!s || !out) return false;
        if (!call || !epoch || !generation) {
            invalidate(s, Availability::IssuerExhausted, true);
            bw_inventory_collector_retire_pending_proofs(); return false;
        }
        if (call <= s->last_entry_call) { invalidate(s, Availability::StaleEvent, true); return false; }
        s->last_entry_call = call;
        Reader r(s, cpu); NativeIdentity n;
        if (!r.ok || !native_identity(r, cpu, n) || cpu->pc != kEntry || cpu->lr != kReturn ||
            cpu->gpr[1] != stack || cpu->gpr[4] != player || player != n.player ||
            cpu->gpr[3] != kMethods || r.u32(cpu->gpr[3] + 8) != kPlayerExecute || !r.stable()) {
            s->diagnostic.availability = r.ok ? Availability::UnsafeNativeContext : r.failure; return false;
        }
        BwInventoryCollectorCallProof p{};
        p.valid = true; p.cpu_lifetime = r.lease.cpu_lifetime; p.memory_lifetime = r.lease.memory_lifetime;
        p.module_generation = r.lease.module_generation; p.alias_binding = r.lease.alias_binding;
        p.native_call = call; p.epoch = epoch; p.scene_generation = generation;
        p.guard_revision = r.guard.revision; p.raw_alias_generation = r.lease.raw_alias_generation;
        p.stack = stack; p.player = player; p.actor_id = n.actor_id; p.entry_frame = n.frame;
        std::memcpy(p.module, r.lease.module.data(), 32); std::memcpy(p.stage, n.scene.stage, 9);
        p.spawn = n.scene.spawn; p.room = n.scene.room; p.layer = n.scene.layer; p.stay_room = n.scene.stay_room;
        if (!producer_context(p)) return false;
        *out = p; return true;
    } catch (...) { return false; }
}
extern "C" bool bw_inventory_collector_call_replay(const CPUState* cpu,
    const BwInventoryCollectorCallProof* p) noexcept {
    try { auto* s = live_collector(); return s && p && p->native_call &&
        p->native_call <= s->last_entry_call && complete_proof(s, cpu, *p, true); }
    catch (...) { return false; }
}
extern "C" void bw_inventory_collector_emit_begin(const CPUState* cpu,
    const BwInventoryCollectorCallProof* p) noexcept {
    auto* s = live_collector(); if (!s) return; close_scope(s);
    try {
        if (!p || !p->native_call || p->native_call <= s->last_completed_call ||
            p->native_call > s->last_entry_call || !complete_proof(s, cpu, *p, false)) return;
        s->last_completed_call = p->native_call;
        s->emission.active = true; s->emission.cpu = cpu; s->emission.proof = *p;
    } catch (...) { close_scope(s); }
}
extern "C" void bw_inventory_collector_event_scope_bind(const BwGameEvent* event) noexcept {
    auto* s = live_collector(); if (!s) return;
    try {
        const auto& p = s->emission.proof;
        if (!s->emission.active || !event || event->kind != BW_GAME_EVENT_PLAYER_UPDATED ||
            !event->sequence || event->epoch != p.epoch || event->scene_generation != p.scene_generation ||
            event->native_call != p.native_call || event->source_address != kReturn || !scene_equal(event->scene, p)) {
            close_scope(s); return;
        }
        s->emission.bound = true; s->emission.event = event;
    } catch (...) { close_scope(s); }
}
extern "C" bool bw_inventory_collector_copy_emitted_lease(const BwGameEvent* event,
    BwInventoryCollectorCallProof* out) noexcept {
    if (out) *out = {};
    try {
        auto* s = live_collector();
        if (!s || !s->callback_active || !out || !event || !s->emission.active || !s->emission.bound || s->emission.consumed ||
            event != s->emission.event || event->native_call != s->emission.proof.native_call ||
            event->source_address != kReturn || !complete_proof(s, s->emission.cpu, s->emission.proof, false)) return false;
        s->emission.consumed = true; *out = s->emission.proof; return true;
    } catch (...) { return false; }
}
extern "C" void bw_inventory_collector_emit_end(void) noexcept {
    auto* s = g_collector.load(std::memory_order_acquire);
    if (s && s->thread == std::this_thread::get_id()) close_scope(s);
}
