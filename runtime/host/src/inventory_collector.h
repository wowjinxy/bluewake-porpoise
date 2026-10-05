// SPDX-License-Identifier: GPL-3.0-or-later
// Experimental read-only collector; no reward writes or live tracker API.
#pragma once
#include "native_inventory_context.h"
#include "game_events.h"
#include "StaticRecompABI.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace bluewake::inventory_collector {
namespace subset = bluewake::randomizer::native_subset;
using RunId = std::array<std::uint8_t, 16>;
inline constexpr subset::ModuleFingerprint kQualifiedMetadata = {
    0x73,0x62,0x37,0x28,0x9c,0x58,0x2a,0xc5,0xf1,0x5c,0x3e,0xa1,0x9e,0x20,0xb3,0x97,
    0xc3,0xe5,0xc0,0x49,0xa1,0xca,0x40,0x0f,0x87,0xe9,0xd1,0xe7,0xe6,0x8e,0x3e,0xa1};
enum class Availability {
    Disabled, Unbound, Suspended, Retired, UnknownGuard, HostBusy,
    UnsafeNativeContext, MissingCompletionProof, WrongActor, ReadUnavailable,
    AliasedFixedField, UnstableCapture, StaleEvent, InvalidDescriptor,
    UnqualifiedModule, SubscriptionUnavailable, IssuerExhausted, WrongThread,
    OwnerAlreadyActive, EvaluationFailure, Captured
};
enum class Provenance { FreshNativeBoot, NativeCardLoad, MachineStateLoad };
enum class SuspendReason {
    CpuReplace, MemoryReplace, MachineStateCapture, MachineStateLoad,
    MachineReset, ModuleReload, AliasRebuild, Relaunch, Shutdown
};
enum class BindingAction {
    InitialBind, CpuReplaced, RamReplaced, NativeCardLoadCompleted,
    MachineStateLoadCompleted, MachineResetCompleted, ModuleReloadCompleted,
    AliasRebuildCompleted, MachineStateCaptureCompleted
};
// InitialBind/MachineResetCompleted establish fresh content; native CARD and
// STATE completion identify their actual source. CPU/RAM/module/alias/capture
// ownership-only rebinds retain prior content provenance, never relabel STATE.
struct BindingLease {
    bool live = false;
    std::uint64_t cpu_lifetime = 0, memory_lifetime = 0, module_generation = 0;
    std::uint64_t alias_binding = 0, registration_revision = 0;
    std::uint32_t raw_alias_generation = 0;
    const CPUState* cpu = nullptr;
    const std::uint8_t* registered_ram = nullptr;
    std::uint32_t registered_ram_size = 0;
    subset::ModuleFingerprint module{};
    Provenance provenance = Provenance::FreshNativeBoot;
};
// update_host_guard takes copied actual flags. valid is explicit availability,
// never inferred from all-zero flags; the nonzero revision is issuer-owned.
struct HostGuard {
    bool valid = false;
    std::uint64_t revision = 0, input_generation = 0;
    bool initialized_controls = false, explicit_headless_no_ui = false;
    bool input_blocked = false, menu_open = false;
    bool machine_capture_pending = false, machine_load_pending = false;
    bool reset_pending = false, rel_lifecycle_pending = false;
    bool module_replace_pending = false, relaunch_pending = false;
    bool shutdown_pending = false, native_autosave_active = false;
    bool quick_door_active = false, quick_items_overlay_active = false;
    bool card_callback_active = false;
};
struct HostProviders {
    void* host_registration = nullptr;
    bool (*binding_copy)(void*, BindingLease*) noexcept = nullptr;
    bool (*guard_copy)(void*, HostGuard*) noexcept = nullptr;
};
struct History {
    RunId run{};
    Provenance provenance = Provenance::FreshNativeBoot;
    std::uint64_t sequence = 0, native_call = 0;
    std::uint32_t source_address = 0, actor_id = 0;
    std::optional<std::uint32_t> native_frame;
    std::optional<subset::OwnerStamp> owner;
    std::optional<std::array<std::uint8_t, 5>> bytes;
    std::array<subset::CapabilityResult, 5> immediate{};
    subset::Status projection_status = subset::Status::Incomplete;
    Availability availability = Availability::Unbound;
    bool authorization_expired = true;
};
struct Diagnostic {
    Availability availability = Availability::Disabled;
    std::uint64_t completed_attempts = 0, successful_snapshots = 0;
    std::uint64_t unavailable_reads = 0, unstable_snapshots = 0;
    std::uint64_t lifecycle_invalidations = 0;
    std::optional<History> history;
};
struct RegistryState;
struct CollectorState;
// Actual-loader-owned descriptor only. Reads host metadata; never calls exports
// or guest helpers. Caller separately pins loaded file bytes before/after use.
bool validate_owned_descriptor(const StaticRecompModuleDesc&,
    subset::ModuleFingerprint*, Availability*) noexcept;
// Admission check for the actual private C startup bridge, before it even
// evaluates cpu->ram arguments. It grants no binding or capture authority.
bool owner_available_for_start() noexcept;
class OwnedBindingRegistry final {
public:
    OwnedBindingRegistry();
    ~OwnedBindingRegistry();
    // Trusted source-only ownership predicate, not a CPU/fact provider. Install
    // once BEFORE initial bind; cannot replace/clear an active registry gate.
    // Its context must outlive this registry; false denies every lease/guard
    // copy before any borrowed CPU is returned. Default synthetic API unchanged.
    using TrustedLivenessGate = bool (*)(void*) noexcept;
    bool install_read_gate(void* trusted_context, TrustedLivenessGate) noexcept;
    OwnedBindingRegistry(const OwnedBindingRegistry&) = delete;
    OwnedBindingRegistry& operator=(const OwnedBindingRegistry&) = delete;
    bool bind_after_actual_action(BindingAction, const CPUState&,
        const std::uint8_t* actual_owned_ram, std::uint32_t ram_size,
        const subset::ModuleFingerprint&, Provenance) noexcept;
    void suspend_before_actual_action(SuspendReason) noexcept;
    bool observe_owned_alias_change(std::uint32_t actual_raw_generation) noexcept;
    bool update_host_guard(const HostGuard& copied_actual_flags) noexcept;
    bool native_card_load_completed() noexcept; // genuine GameLoad RESET only
    bool copy_registered_binding(BindingLease*) const noexcept;
    bool copy_host_guard(HostGuard*) const noexcept;
    HostProviders providers() const noexcept;
private:
    std::unique_ptr<RegistryState> state_;
};
class Collector final {
public:
    Collector(RunId, HostProviders, bluewake::randomizer::Catalog exact_catalog);
    ~Collector();
    Collector(const Collector&) = delete;
    Collector& operator=(const Collector&) = delete;
    bool enable(const StaticRecompModuleDesc&) noexcept;
    void suspend(SuspendReason) noexcept;
    bool resume() noexcept;
    void disable() noexcept;
    Diagnostic diagnostic_copy() const noexcept;
private:
    std::unique_ptr<CollectorState> state_;
    static void registered_event(const BwGameEvent*, void*) noexcept;
};
} // namespace bluewake::inventory_collector
