// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic owned-metadata fixture. Actual runtime resolver + guarded actual
// producer are required at link; these are not native gameplay/award proofs.
#include "inventory_collector.h"
#include "inventory_collector_host.h"
#include "inventory_collector_lease_mock.h"
#include "inventory_completion_internal.h"
#include <array>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <new>
#include <vector>

using namespace bluewake::inventory_collector;
// Fixture-only failure injection targets callback evaluator allocation; it is
// never compiled into an application/module or a guest path.
static bool fail_next_allocation = false;
void* operator new(std::size_t size) {
    if (fail_next_allocation) { fail_next_allocation = false; throw std::bad_alloc(); }
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
namespace {
constexpr std::uint32_t entry = 0x8003EF38u, ret = 0x80023960u;
constexpr std::uint32_t player = 0x80800000u, stack = 0x80020000u;
constexpr char run[] = "0123456789abcdef0123456789abcdef";
std::uint64_t assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while (0)
int dummy_dispatch(CPUState*, u32) { return 0; }
std::uint32_t be32(const std::vector<std::uint8_t>& b, std::size_t at) {
    CHECK(at + 4 <= b.size()); return (std::uint32_t(b[at]) << 24) |
        (std::uint32_t(b[at+1]) << 16) | (std::uint32_t(b[at+2]) << 8) | b[at+3];
}
struct Metadata {
    std::vector<StaticRecompRange> chunks, code;
    std::vector<u64> hashes;
    StaticRecompModuleDesc descriptor{};
    explicit Metadata(const char* path) {
        std::ifstream f(path, std::ios::binary);
        CHECK(f.good());
        const std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>{});
        constexpr char prefix[] = "BW-NATIVE-INVENTORY-MODULE-v1";
        CHECK(bytes.size() == 12022 && !std::memcmp(bytes.data(), prefix, sizeof prefix));
        std::size_t at = sizeof prefix;
        descriptor.abi_version = be32(bytes, at); at += 4;
        descriptor.cpu_abi_version = be32(bytes, at); at += 4;
        descriptor.cpu_state_size = be32(bytes, at); at += 4;
        descriptor.num_chunk_ranges = be32(bytes, at); at += 4;
        CHECK(descriptor.num_chunk_ranges == 748);
        std::memcpy(descriptor.game_id, bytes.data() + at, 8); at += 8;
        for (unsigned i = 0; i < 748; ++i) {
            const auto start = be32(bytes, at), end = be32(bytes, at + 4); at += 8;
            const auto hash = (std::uint64_t(be32(bytes, at)) << 32) | be32(bytes, at + 4); at += 8;
            chunks.push_back({start, end}); hashes.push_back(hash);
            if (code.empty() || code.back().end != start) code.push_back({start, end});
            else code.back().end = end;
        }
        CHECK(at == bytes.size());
        descriptor.dispatch = dummy_dispatch; // metadata validation never invokes this
        descriptor.chunk_ranges = chunks.data(); descriptor.chunk_hashes = hashes.data();
        descriptor.code_ranges = code.data(); descriptor.num_code_ranges = static_cast<u32>(code.size());
    }
};
struct Memory {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x02000000);
    CPUState cpu{};
    Memory() { cpu.ram = bytes.data(); cpu.ram_size = static_cast<u32>(bytes.size()); }
    void b8(std::uint32_t a, std::uint8_t b) { CHECK(a >= 0x80000000u && a < 0x82000000u); bytes[a-0x80000000u] = b; }
    void b16(std::uint32_t a, std::uint16_t b) { b8(a, std::uint8_t(b>>8)); b8(a+1, std::uint8_t(b)); }
    void b32(std::uint32_t a, std::uint32_t b) { b16(a, std::uint16_t(b>>16)); b16(a+2, std::uint16_t(b)); }
    void setup() {
        cpu.pc = entry; cpu.lr = ret; cpu.gpr[1] = stack; cpu.gpr[3] = 0x8038FD68u;
        cpu.gpr[4] = cpu.gpr[30] = player;
        b32(0x800000D4u, 0x803A2960u); b32(0x800000E4u, 0x803A2960u);
        b32(0x803CA74Cu, player); b32(0x803CA754u, player);
        b32(0x803F6A18u, 0x1234); b32(0x803F69D0u, 0x5678);
        b32(player, 0x1234); b32(player+4, 7); b16(player+8, 0xA9);
        b8(player+0xC, 2); b8(player+0xD, 2); b16(player+0xE, 0xA9);
        b32(player+0x10, 0x8038FD8Cu); b32(player+0xC0, 0x5678);
        b32(player+0xEC, 0x8038FD68u); b8(player+0x1BE, 1);
        b32(player+0x498, player+0x1F8);
        b16(0x8038FD8Cu+8, 0xA9); b32(0x8038FD8Cu+0x10, 0x4C28u);
        b32(0x8038FD8Cu+0x24, 0x8038FD68u); b32(0x8038FD68u+8, 0x80122D30u);
        b8(0x803C9D3Cu, 's'); b8(0x803C9D3Du, 'e'); b8(0x803C9D3Eu, 'a');
        b8(0x803C9D46u, 44); b8(0x803F6A78u, 44);
        b32(0x803B39A0u+0x1660u, 1);
        inventory(0xFF, 0, 0xFF, 0, 0);
    }
    void inventory(std::uint8_t w, std::uint8_t wg, std::uint8_t h, std::uint8_t hg, std::uint8_t song) {
        b8(0x803C4C46u,w); b8(0x803C4C5Bu,wg); b8(0x803C4C47u,h); b8(0x803C4C5Cu,hg); b8(0x803C4CC5u,song);
    }
    void execute(bool replay = false) {
        cpu.pc = entry; cpu.lr = ret; cpu.gpr[3] = 0x8038FD68u; cpu.gpr[4] = player; cpu.gpr[30] = player;
        const auto old = bytes;
        std::array<std::uint8_t,sizeof(CPUState)> regs{};
        std::memcpy(regs.data(),&cpu,sizeof cpu);
        bluewake_game_events_dispatch(&cpu, entry);
        CHECK(bytes == old && !std::memcmp(&cpu, regs.data(), sizeof cpu));
        if (replay) { bluewake_game_events_dispatch(&cpu, entry); CHECK(bytes == old && !std::memcmp(&cpu, regs.data(), sizeof cpu)); }
        cpu.pc = ret;
        std::array<std::uint8_t,sizeof(CPUState)> returned_regs{};
        std::memcpy(returned_regs.data(),&cpu,sizeof cpu);
        bluewake_game_events_dispatch(&cpu, ret);
        CHECK(bytes == old && !std::memcmp(&cpu, returned_regs.data(), sizeof cpu));
    }
};
BwInventoryCollectorHostFlags flags() {
    BwInventoryCollectorHostFlags f{};
    f.source_initialized = f.explicit_headless_no_ui = f.rel_lifecycle_known = true;
    return f;
}
BwInventoryCollectorHistoricalDiagnostic diagnostic() {
    BwInventoryCollectorHistoricalDiagnostic d{}; CHECK(bw_inventory_collector_host_diagnostic(&d));
    CHECK(d.authorization_expired); return d;
}
bool foreign_callback_copied = false;
BwGameEvent saved_event{};
void foreign_callback(const BwGameEvent* e, void*) {
    if (e->kind != BW_GAME_EVENT_PLAYER_UPDATED) return;
    saved_event = *e;
    BwInventoryCollectorCallProof proof{};
    foreign_callback_copied |= bw_inventory_collector_copy_emitted_lease(e, &proof);
}
void descriptor_negatives(Metadata& metadata) {
    subset::ModuleFingerprint pin; Availability why;
    CHECK(validate_owned_descriptor(metadata.descriptor, &pin, &why) && pin == kQualifiedMetadata);
    auto m = metadata.descriptor;
    m.abi_version = 3; CHECK(!validate_owned_descriptor(m, &pin, &why));
    m = metadata.descriptor; m.cpu_abi_version = 5; CHECK(!validate_owned_descriptor(m, &pin, &why));
    m = metadata.descriptor; m.cpu_state_size = 0; CHECK(!validate_owned_descriptor(m, &pin, &why));
    m = metadata.descriptor; m.game_id[7] = 'X'; CHECK(!validate_owned_descriptor(m, &pin, &why));
    m = metadata.descriptor; m.dispatch = nullptr; CHECK(!validate_owned_descriptor(m, &pin, &why));
    m = metadata.descriptor; m.num_chunk_ranges = 749; CHECK(!validate_owned_descriptor(m, &pin, &why));
    m = metadata.descriptor; m.chunk_hashes = nullptr; CHECK(!validate_owned_descriptor(m, &pin, &why));
    m = metadata.descriptor; m.num_code_ranges = 0; CHECK(!validate_owned_descriptor(m, &pin, &why));
    const auto hash = metadata.hashes[0]; metadata.hashes[0] ^= 1;
    CHECK(!validate_owned_descriptor(metadata.descriptor, &pin, &why) && why == Availability::UnqualifiedModule);
    metadata.hashes[0] = hash;
}
void bridge_cases(Metadata& metadata) {
    Memory memory; memory.setup(); ppc_guest_alias_clear();
    bluewake_game_events_attach(&memory.cpu);
    const auto foreign = bluewake_game_events_subscribe(BW_GAME_EVENT_MASK(BW_GAME_EVENT_PLAYER_UPDATED), foreign_callback, nullptr);
    CHECK(foreign != 0);
    CHECK(!bw_inventory_collector_completion_enabled());
    CHECK(!bw_inventory_collector_host_start(reinterpret_cast<CPUState*>(1), &metadata.descriptor, run, false));
    CHECK(!bw_inventory_collector_host_start(&memory.cpu, &metadata.descriptor, "00000000000000000000000000000000", true));
    CHECK(!bw_inventory_collector_host_start(&memory.cpu, &metadata.descriptor, run, true));
    CHECK(!bw_inventory_collector_host_start_verified(&memory.cpu,&metadata.descriptor,run,true,nullptr,0));
    uint64_t generation=0;auto* code=bw_ic_fixture_lease(&metadata.descriptor,&generation);
    CHECK(code&&generation);
    CHECK(!bw_inventory_collector_host_start_verified(&memory.cpu,&metadata.descriptor,run,true,code,generation+1));
    CHECK(bw_inventory_collector_host_start_verified(&memory.cpu,&metadata.descriptor,run,true,code,generation));
    CHECK(!bw_inventory_collector_host_start(reinterpret_cast<CPUState*>(1), reinterpret_cast<StaticRecompModuleDesc*>(1), run, true));
    memory.execute(); // guard not initialized: real producer event, no private snapshot
    auto d = diagnostic(); CHECK(d.successful_snapshots == 0 && !d.has_bytes);
    auto f = flags(); CHECK(bw_inventory_collector_host_update_guard(&f));
    memory.execute(true); d = diagnostic();
    CHECK(d.successful_snapshots == 1 && d.has_bytes && d.has_native_frame && d.native_frame == 0);
    CHECK(!foreign_callback_copied && d.projection_status == unsigned(subset::Status::Ready));
    for (unsigned i = 0; i < 5; ++i) CHECK(d.capability_has_value[i] && !d.capability_value[i]);
    BwInventoryCollectorCallProof copied{};
    CHECK(!bw_inventory_collector_copy_emitted_lease(&saved_event, &copied));
    memory.inventory(0x22, 0x81, 0x25, 1, 1);
    memory.execute(); d = diagnostic();
    CHECK(d.has_bytes && d.inventory_bytes[1] == 0x81);
    for (unsigned i = 0; i < 5; ++i) CHECK(d.capability_has_value[i] && d.capability_value[i]);
    const auto before_exception = d.successful_snapshots;
    memory.cpu.pc=entry; memory.cpu.lr=ret; memory.cpu.gpr[3]=0x8038FD68u;
    bluewake_game_events_dispatch(&memory.cpu,entry);
    memory.cpu.pc=ret;
    fail_next_allocation = true;
    bluewake_game_events_dispatch(&memory.cpu,ret);
    CHECK(!fail_next_allocation); d=diagnostic();
    CHECK(d.successful_snapshots==before_exception && !d.has_bytes &&
        d.availability==unsigned(Availability::EvaluationFailure));
    CHECK(!bw_inventory_collector_copy_emitted_lease(&saved_event,&copied));
    memory.execute(); CHECK(diagnostic().successful_snapshots==before_exception+1);
    memory.inventory(0xFF, 1, 0x25, 1, 1); memory.execute(); d = diagnostic();
    CHECK(d.has_bytes && d.projection_status == unsigned(subset::Status::Contradictory));
    for (unsigned i = 0; i < 5; ++i) CHECK(!d.capability_has_value[i]);
    memory.inventory(0xFF, 0x80, 0xFF, 0, 0); memory.execute(); d = diagnostic();
    CHECK(d.projection_status == unsigned(subset::Status::Ready) && !d.capability_value[0]);
    const auto captures = d.successful_snapshots;
    for (auto id : {0u, UINT32_MAX-1u, UINT32_MAX}) {
        memory.b32(player+4,id); memory.execute(); CHECK(diagnostic().successful_snapshots == captures);
    }
    memory.b32(player+4,7);
    for (auto state : {0u,1u,3u,4u}) {
        memory.b8(player+0xC, std::uint8_t(state)); memory.execute(); CHECK(diagnostic().successful_snapshots == captures);
    }
    memory.b8(player+0xC,2);
    for (auto state : {0u,1u,3u,4u}) {
        memory.b8(player+0xD, std::uint8_t(state)); memory.execute(); CHECK(diagnostic().successful_snapshots == captures);
    }
    memory.b8(player+0xD,2);
    // A callback-only PID check is insufficient: the exact native entry proof
    // must reject same-address replacement before return/callback capture.
    memory.cpu.pc=entry; memory.cpu.lr=ret; memory.cpu.gpr[3]=0x8038FD68u;
    bluewake_game_events_dispatch(&memory.cpu,entry);
    memory.b32(player+4,8); memory.cpu.pc=ret;
    bluewake_game_events_dispatch(&memory.cpu,ret);
    CHECK(diagnostic().successful_snapshots==captures);
    memory.b32(player+4,7);
    // Actual counter wrap is metadata, not an invented VI/default lifetime.
    memory.b32(0x803E8140u,UINT32_MAX); memory.execute();
    CHECK(diagnostic().has_native_frame && diagnostic().native_frame==UINT32_MAX);
    memory.b32(0x803E8140u,0);
    const auto after_replacement = diagnostic().successful_snapshots;
    f.machine_capture_pending = true; CHECK(bw_inventory_collector_host_update_guard(&f));
    memory.execute(); CHECK(diagnostic().successful_snapshots == after_replacement);
    f.machine_capture_pending = false; CHECK(bw_inventory_collector_host_update_guard(&f));
    memory.execute(); CHECK(diagnostic().successful_snapshots == after_replacement+1);
    // Exactly the actual bridge path: unknown flags cannot become safe defaults.
    f.source_initialized = false; CHECK(!bw_inventory_collector_host_update_guard(&f));
    memory.execute(); CHECK(diagnostic().successful_snapshots == after_replacement+1);
    // Synthetic unchanged-DLL memory ownership recovery; code lease survives.
    bw_inventory_collector_host_suspend(BW_IC_MEMORY_REPLACE);
    CHECK(bw_inventory_collector_host_rebind(&memory.cpu,&metadata.descriptor,BW_IC_RAM_REPLACED));
    f = flags(); CHECK(bw_inventory_collector_host_update_guard(&f));
    memory.execute(); CHECK(diagnostic().has_bytes);
    // Failed STATE load remains suspended through a later read-only capture.
    bw_inventory_collector_host_suspend(BW_IC_STATE_LOAD);
    bw_inventory_collector_host_suspend(BW_IC_STATE_CAPTURE);
    CHECK(!bw_inventory_collector_host_rebind(reinterpret_cast<CPUState*>(1), &metadata.descriptor, BW_IC_STATE_CAPTURE_DONE));
    CHECK(!bw_inventory_collector_completion_enabled());
    CHECK(bw_inventory_collector_host_rebind(&memory.cpu,&metadata.descriptor,BW_IC_STATE_LOADED));
    CHECK(bw_inventory_collector_host_update_guard(&f)); memory.execute();
    d = diagnostic(); CHECK(d.provenance == unsigned(Provenance::MachineStateLoad));
    const auto state_lifetime = d.memory_lifetime;
    bw_inventory_collector_host_suspend(BW_IC_STATE_CAPTURE);
    CHECK(bw_inventory_collector_host_rebind(&memory.cpu,&metadata.descriptor,BW_IC_STATE_CAPTURE_DONE));
    CHECK(!bw_inventory_collector_host_rebind(&memory.cpu,&metadata.descriptor,BW_IC_STATE_CAPTURE_DONE));
    CHECK(bw_inventory_collector_host_update_guard(&f)); memory.execute(); d=diagnostic();
    CHECK(d.provenance == unsigned(Provenance::MachineStateLoad) && d.memory_lifetime > state_lifetime);
    // Synthetic unchanged-DLL memory ownership recovery; code lease survives.
    bw_inventory_collector_host_suspend(BW_IC_MEMORY_REPLACE);
    CHECK(bw_inventory_collector_host_rebind(&memory.cpu,&metadata.descriptor,BW_IC_RAM_REPLACED));
    CHECK(bw_inventory_collector_host_update_guard(&f)); memory.execute();
    CHECK(diagnostic().provenance == unsigned(Provenance::MachineStateLoad));
    bw_inventory_collector_host_suspend(BW_IC_STATE_CAPTURE);
    CHECK(!bw_inventory_collector_host_rebind(reinterpret_cast<CPUState*>(1),&metadata.descriptor,BW_IC_STATE_CAPTURE_DONE));
    CHECK(!bw_inventory_collector_completion_enabled());
    CHECK(!bw_inventory_collector_host_rebind(&memory.cpu,&metadata.descriptor,BW_IC_STATE_CAPTURE_DONE));
    // Synthetic unchanged-DLL memory ownership recovery; code lease survives.
    bw_inventory_collector_host_suspend(BW_IC_MEMORY_REPLACE);
    CHECK(bw_inventory_collector_host_rebind(&memory.cpu,&metadata.descriptor,BW_IC_RAM_REPLACED));
    CHECK(bw_inventory_collector_host_update_guard(&f)); memory.execute();
    CHECK(diagnostic().provenance == unsigned(Provenance::MachineStateLoad));
    // Partial fixed-field alias is reported/lifetime-renewed but refuses bytes.
    const std::uint8_t overlay = 0xFF;
    bw_inventory_collector_host_alias_before();
    CHECK(ppc_guest_alias_add(0x803C4C46u,1,&overlay));
    bw_inventory_collector_host_alias_after(g_ppc_guest_alias_generation);
    CHECK(bw_inventory_collector_host_update_guard(&f)); memory.execute(); d=diagnostic();
    CHECK(!d.has_bytes && d.availability == unsigned(Availability::AliasedFixedField));
    bw_inventory_collector_host_alias_before(); CHECK(ppc_guest_alias_remove(0x803C4C46u,1));
    bw_inventory_collector_host_alias_after(g_ppc_guest_alias_generation);
    CHECK(bw_inventory_collector_host_update_guard(&f)); memory.execute();
    CHECK(diagnostic().has_bytes && diagnostic().provenance == unsigned(Provenance::MachineStateLoad));
    // Unreported alias retirement is not repaired by capture completion.
    CHECK(ppc_guest_alias_add(0x803C4C46u,1,&overlay)); memory.execute();
    CHECK(diagnostic().availability == unsigned(Availability::Retired) || !bw_inventory_collector_completion_enabled());
    bw_inventory_collector_host_suspend(BW_IC_STATE_CAPTURE);
    CHECK(!bw_inventory_collector_host_rebind(&memory.cpu,&metadata.descriptor,BW_IC_STATE_CAPTURE_DONE));
    CHECK(ppc_guest_alias_remove(0x803C4C46u,1));
    // Synthetic unchanged-DLL memory ownership recovery; code lease survives.
    bw_inventory_collector_host_suspend(BW_IC_MEMORY_REPLACE);
    CHECK(bw_inventory_collector_host_rebind(&memory.cpu,&metadata.descriptor,BW_IC_RAM_REPLACED));
    CHECK(bw_inventory_collector_host_update_guard(&f)); memory.execute(); CHECK(diagnostic().has_bytes);
    // Genuine module-trust loss retires the pending synthetic native return.
    const auto before_code_loss=diagnostic().successful_snapshots;
    memory.cpu.pc=entry;memory.cpu.lr=ret;memory.cpu.gpr[3]=player;
    bluewake_game_events_dispatch(&memory.cpu,entry);
    // Trust loss alone MUST block registry-backed completion before the host's
    // explicit lifecycle suspension. Otherwise a gate-free providers() patch
    // could appear to pass while borrowing bytes from a revoked module.
    bw_ic_code_revoke(code,generation);
    CHECK(!bw_ic_code_is_live(code,generation,&metadata.descriptor));
    memory.cpu.pc=ret;bluewake_game_events_dispatch(&memory.cpu,ret);
    CHECK(diagnostic().successful_snapshots==before_code_loss);
    bw_inventory_collector_host_suspend(BW_IC_MODULE_RELOAD);
    for(auto action:{BW_IC_MODULE_RELOAD_DONE,BW_IC_STATE_LOADED,BW_IC_NATIVE_CARD_LOADED,BW_IC_ALIAS_REBUILD_DONE})
        CHECK(!bw_inventory_collector_host_rebind(&memory.cpu,&metadata.descriptor,action));
    CHECK(!bw_inventory_collector_host_update_guard(&f));
    bw_inventory_collector_host_shutdown(); CHECK(!bw_inventory_collector_completion_enabled());
    bw_ic_code_destroy(code,generation);
    const auto old_generation=generation;auto* fresh=bw_ic_fixture_lease(&metadata.descriptor,&generation);
    CHECK(fresh==code&&generation>old_generation); // intentional same-address mock reuse
    CHECK(!bw_inventory_collector_host_start_verified(&memory.cpu,&metadata.descriptor,run,true,fresh,old_generation));
    bw_ic_code_revoke(fresh,old_generation);bw_ic_code_destroy(fresh,old_generation);
    CHECK(bw_ic_code_is_live(fresh,generation,&metadata.descriptor));
    CHECK(bw_inventory_collector_host_start_verified(&memory.cpu,&metadata.descriptor,run,true,fresh,generation));
    CHECK(bw_inventory_collector_host_update_guard(&f));memory.execute();CHECK(diagnostic().has_bytes);
    bw_inventory_collector_host_shutdown();bw_ic_code_destroy(fresh,generation);
    CHECK(bluewake_game_events_unsubscribe(foreign)); bluewake_game_events_reset(nullptr,BW_GAME_RESET_MODULE_RELOAD);
    ppc_guest_alias_clear();
}
bool fixture_read_gate(void* opaque) noexcept { return *static_cast<bool*>(opaque); }
void read_gate_cases(Metadata& metadata) {
    Memory memory; memory.setup(); ppc_guest_alias_clear();
    bool live=false; OwnedBindingRegistry registry;
    CHECK(!registry.install_read_gate(nullptr,fixture_read_gate));
    CHECK(!registry.install_read_gate(&live,nullptr));
    CHECK(!registry.install_read_gate(&live,fixture_read_gate));
    live=true; CHECK(registry.install_read_gate(&live,fixture_read_gate));
    CHECK(!registry.install_read_gate(&live,fixture_read_gate));
    CHECK(registry.bind_after_actual_action(BindingAction::InitialBind,memory.cpu,
        memory.cpu.ram,memory.cpu.ram_size,kQualifiedMetadata,Provenance::FreshNativeBoot));
    HostGuard guard;guard.valid=true;guard.explicit_headless_no_ui=true;
    CHECK(registry.update_host_guard(guard));
    BindingLease binding;HostGuard copied;
    CHECK(registry.copy_registered_binding(&binding)&&registry.copy_host_guard(&copied));
    const CPUState before=memory.cpu;const auto bytes=memory.bytes;
    live=false;
    CHECK(!registry.copy_registered_binding(&binding)&&!registry.copy_host_guard(&copied));
    CHECK(!registry.update_host_guard(guard));
    CHECK(!registry.native_card_load_completed());
    CHECK(!registry.observe_owned_alias_change(g_ppc_guest_alias_generation));
    CHECK(!registry.install_read_gate(&live,fixture_read_gate));
    CHECK(!std::memcmp(&before,&memory.cpu,sizeof before)&&bytes==memory.bytes);
    registry.suspend_before_actual_action(SuspendReason::MemoryReplace);
    CHECK(!registry.bind_after_actual_action(BindingAction::RamReplaced,memory.cpu,
        memory.cpu.ram,memory.cpu.ram_size,kQualifiedMetadata,Provenance::FreshNativeBoot));
}
void direct_registry_cases(Metadata& metadata) {
    auto memory = std::make_unique<Memory>(); memory->setup();
    bluewake_game_events_attach(&memory->cpu);
    OwnedBindingRegistry registry;
    CHECK(registry.bind_after_actual_action(BindingAction::InitialBind,memory->cpu,memory->cpu.ram,
        memory->cpu.ram_size,kQualifiedMetadata,Provenance::FreshNativeBoot));
    OwnedBindingRegistry competitor;
    BindingLease before_competitor, after_competitor;
    CHECK(registry.copy_registered_binding(&before_competitor));
    CHECK(!competitor.bind_after_actual_action(BindingAction::InitialBind,
        memory->cpu,nullptr,0,kQualifiedMetadata,Provenance::FreshNativeBoot));
    CHECK(registry.copy_registered_binding(&after_competitor) &&
        before_competitor.registration_revision == after_competitor.registration_revision &&
        before_competitor.cpu_lifetime == after_competitor.cpu_lifetime);
    CHECK(!bw_inventory_collector_host_start(reinterpret_cast<CPUState*>(1),&metadata.descriptor,run,true));
    RunId id{}; id[0]=1;
    Collector collector(id,registry.providers(),bluewake::randomizer::imported_catalog());
    CHECK(collector.enable(metadata.descriptor));
    Collector second(id,registry.providers(),bluewake::randomizer::imported_catalog());
    CHECK(!second.enable(metadata.descriptor));
    HostGuard guard; CHECK(!registry.update_host_guard(guard));
    guard.valid=true; guard.explicit_headless_no_ui=true; CHECK(registry.update_host_guard(guard));
    memory->execute(); CHECK(collector.diagnostic_copy().successful_snapshots==1);
    registry.suspend_before_actual_action(SuspendReason::MachineStateLoad);
    registry.suspend_before_actual_action(SuspendReason::MachineStateCapture);
    CHECK(!registry.bind_after_actual_action(BindingAction::MachineStateCaptureCompleted,memory->cpu,
        memory->cpu.ram,memory->cpu.ram_size,kQualifiedMetadata,Provenance::FreshNativeBoot));
    CHECK(!collector.resume());
    // Destroy genuinely no-longer-owned CPU; private seams must not touch it.
    const auto* dead = &memory->cpu; memory.reset();
    BwInventoryCollectorCallProof proof{};
    CHECK(!bw_inventory_collector_call_begin(dead,123,1,1,stack,player,&proof));
    CHECK(!bw_inventory_collector_call_replay(dead,&proof));
    bluewake_game_events_reset(nullptr,BW_GAME_RESET_MEMORY_REPLACED);
    collector.disable(); ppc_guest_alias_clear();
}
} // namespace
int main(int argc,char**argv) {
    CHECK(argc==2);
    Metadata metadata(argv[1]); descriptor_negatives(metadata);
    bridge_cases(metadata); read_gate_cases(metadata); direct_registry_cases(metadata);
    std::cout << "SYNTHETIC_ISSUER_GATED_MOCK_LEASE_BRIDGE checks=" << assertions << "; no actual image/native qualification\n";
}
