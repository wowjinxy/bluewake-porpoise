// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic owned-lifetime negatives; no private descriptor data or game code.
// Passing this fixture cannot qualify an accepted descriptor/native capture.
#include "inventory_collector.h"
#include "inventory_collector_host.h"
#include "inventory_completion_internal.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

using namespace bluewake::inventory_collector;
namespace {
std::uint64_t checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while (0)
constexpr char run[] = "0123456789abcdef0123456789abcdef";
int never_called(CPUState*, u32) { std::abort(); }
void descriptor_and_disabled_cases() {
    StaticRecompModuleDesc bad{};
    subset::ModuleFingerprint fingerprint{};
    Availability availability{};
    CHECK(!validate_owned_descriptor(bad, &fingerprint, &availability));
    bad.abi_version = 5; bad.cpu_abi_version = 6; bad.cpu_state_size = sizeof(CPUState);
    std::memcpy(bad.game_id, "GZLE01\0", 8); bad.dispatch = never_called;
    CHECK(!validate_owned_descriptor(bad, &fingerprint, &availability));
    StaticRecompRange malformed[] = {{0x80000000u, 0x80000000u}};
    u64 hashes[] = {0};
    bad.chunk_ranges = malformed; bad.chunk_hashes = hashes; bad.num_chunk_ranges = 1;
    bad.code_ranges = malformed; bad.num_code_ranges = 1;
    CHECK(!validate_owned_descriptor(bad, &fingerprint, &availability));
    bad.game_id[7] = 'x';
    CHECK(!validate_owned_descriptor(bad, &fingerprint, &availability));
    CHECK(!bw_inventory_collector_host_enabled());
    CHECK(!bw_inventory_collector_completion_enabled());
    CHECK(!bw_inventory_collector_host_start(reinterpret_cast<const CPUState*>(1), &bad, run, false));
    CHECK(!bw_inventory_collector_host_start(reinterpret_cast<const CPUState*>(1), &bad, run, true));
    CHECK(!bw_inventory_collector_host_start(reinterpret_cast<const CPUState*>(1), &bad,
        "00000000000000000000000000000000", true));
    CHECK(!bw_inventory_collector_host_start(nullptr, &bad, run, true));
    BwInventoryCollectorCallProof proof{};
    CHECK(!bw_inventory_collector_call_begin(reinterpret_cast<const CPUState*>(1), 1, 1, 1,
        0x80020000u, 0x80800000u, &proof));
    CHECK(!bw_inventory_collector_call_replay(reinterpret_cast<const CPUState*>(1), &proof));
    BwInventoryCollectorHistoricalDiagnostic diagnostic{};
    CHECK(!bw_inventory_collector_host_diagnostic(&diagnostic));
    bw_inventory_collector_host_shutdown();
}
void synthetic_registry_cases() {
    std::vector<std::uint8_t> ram(0x02000000u);
    CPUState cpu{}; cpu.ram = ram.data(); cpu.ram_size = static_cast<u32>(ram.size());
    const auto ram_before = ram;
    std::array<std::uint8_t, sizeof(CPUState)> cpu_before{};
    std::memcpy(cpu_before.data(), &cpu, sizeof cpu);
    const auto bind = [&](OwnedBindingRegistry& registry, BindingAction action,
                          Provenance supplied = Provenance::FreshNativeBoot) {
        // A synthetic trusted host binding, not a successfully validated module.
        return registry.bind_after_actual_action(action, cpu, ram.data(),
            static_cast<u32>(ram.size()), kQualifiedMetadata, supplied);
    };
    {
        OwnedBindingRegistry registry;
        BindingLease initial{};
        CHECK(!registry.copy_registered_binding(&initial));
        CHECK(bind(registry, BindingAction::InitialBind));
        CHECK(registry.copy_registered_binding(&initial));
        CHECK(initial.live && initial.cpu == &cpu && initial.registered_ram == ram.data());
        CHECK(initial.cpu_lifetime && initial.memory_lifetime && initial.module_generation &&
            initial.alias_binding && initial.registration_revision);
        CHECK(initial.provenance == Provenance::FreshNativeBoot);
        CHECK(!bind(registry, BindingAction::CpuReplaced));
        CHECK(registry.copy_registered_binding(&initial));
        OwnedBindingRegistry competitor;
        CHECK(!bind(competitor, BindingAction::InitialBind));
        BindingLease after_competitor{};
        CHECK(registry.copy_registered_binding(&after_competitor));
        CHECK(after_competitor.registration_revision == initial.registration_revision);
        CHECK(!bw_inventory_collector_host_start(reinterpret_cast<const CPUState*>(1),
            reinterpret_cast<const StaticRecompModuleDesc*>(1), run, true));
        HostGuard guard{};
        CHECK(!registry.update_host_guard(guard));
        guard.valid = true; guard.explicit_headless_no_ui = true;
        CHECK(registry.update_host_guard(guard));
        HostGuard guard_before{}; CHECK(registry.copy_host_guard(&guard_before));
        CHECK(guard_before.valid && guard_before.revision);
        CHECK(registry.update_host_guard(guard));
        HostGuard guard_after{}; CHECK(registry.copy_host_guard(&guard_after));
        CHECK(guard_before.revision == guard_after.revision);
        bool foreign_copy = true;
        std::thread foreign([&] { BindingLease probe{}; foreign_copy = registry.copy_registered_binding(&probe); });
        foreign.join(); CHECK(!foreign_copy);
        registry.suspend_before_actual_action(SuspendReason::MachineStateLoad);
        CHECK(!registry.copy_registered_binding(&after_competitor));
        registry.suspend_before_actual_action(SuspendReason::MachineStateCapture);
        CHECK(!bind(registry, BindingAction::MachineStateCaptureCompleted));
        CHECK(bind(registry, BindingAction::MachineStateLoadCompleted));
        BindingLease loaded{}; CHECK(registry.copy_registered_binding(&loaded));
        CHECK(loaded.provenance == Provenance::MachineStateLoad);
        CHECK(loaded.memory_lifetime > initial.memory_lifetime);
        registry.suspend_before_actual_action(SuspendReason::ModuleReload);
        CHECK(bind(registry, BindingAction::ModuleReloadCompleted));
        BindingLease module{}; CHECK(registry.copy_registered_binding(&module));
        CHECK(module.provenance == Provenance::MachineStateLoad);
        CHECK(module.module_generation > loaded.module_generation);
        registry.suspend_before_actual_action(SuspendReason::MachineStateCapture);
        CHECK(bind(registry, BindingAction::MachineStateCaptureCompleted));
        BindingLease captured{}; CHECK(registry.copy_registered_binding(&captured));
        CHECK(captured.provenance == Provenance::MachineStateLoad);
        CHECK(!bind(registry, BindingAction::MachineStateCaptureCompleted));
        CHECK(registry.copy_registered_binding(&captured));
        CHECK(!registry.observe_owned_alias_change(g_ppc_guest_alias_generation + 1));
        CHECK(registry.native_card_load_completed());
        BindingLease card{}; CHECK(registry.copy_registered_binding(&card));
        CHECK(card.provenance == Provenance::NativeCardLoad && card.memory_lifetime > captured.memory_lifetime);
        registry.suspend_before_actual_action(SuspendReason::MachineReset);
        CHECK(bind(registry, BindingAction::MachineResetCompleted, Provenance::MachineStateLoad));
        BindingLease reset{}; CHECK(registry.copy_registered_binding(&reset));
        CHECK(reset.provenance == Provenance::FreshNativeBoot);
        CHECK(reset.cpu_lifetime > card.cpu_lifetime);
        registry.suspend_before_actual_action(SuspendReason::Shutdown);
        CHECK(!registry.copy_registered_binding(&reset));
    }
    CHECK(owner_available_for_start());
    CHECK(ram == ram_before && !std::memcmp(cpu_before.data(), &cpu, sizeof cpu));
}
} // namespace
int main() {
    descriptor_and_disabled_cases(); synthetic_registry_cases();
    std::cout << "SYNTHETIC_PUBLIC_LIFETIME_NEGATIVES checks=" << checks
        << "; no accepted-descriptor/native-capture qualification\n";
}
