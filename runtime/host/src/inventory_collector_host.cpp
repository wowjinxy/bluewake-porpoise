// SPDX-License-Identifier: GPL-3.0-or-later
#include "inventory_collector_host.h"
#include "inventory_collector.h"
#include "inventory_completion_internal.h"
#include <memory>
#include <cstring>
#include <thread>

using namespace bluewake::inventory_collector;
namespace {
struct Host {
    const std::thread::id thread = std::this_thread::get_id();
    bool headless = false, alias_open = false, capture_can_resume = false;
    BindingLease capture_origin;
    BwIcLoadedCode* code = nullptr;
    std::uint64_t code_generation = 0;
    const StaticRecompModuleDesc* descriptor = nullptr;
    std::unique_ptr<OwnedBindingRegistry> registry;
    std::unique_ptr<Collector> collector;
};
// Access only on the actual game thread. Core itself rejects off-thread reads.
std::unique_ptr<Host> host;
bool owner_thread() noexcept { return host && host->thread == std::this_thread::get_id(); }
bool code_live(const Host& h) noexcept {
    return bw_ic_code_is_live(h.code, h.code_generation, h.descriptor);
}
bool registry_code_live(void* opaque) noexcept {
    auto* h = static_cast<Host*>(opaque);
    return h && h->thread == std::this_thread::get_id() && code_live(*h);
}
bool parse_run(const char* text, RunId& out) noexcept {
    if (!text) return false;
    auto digit = [](char c) -> unsigned { return c >= '0' && c <= '9' ? unsigned(c-'0') :
        c >= 'a' && c <= 'f' ? unsigned(c-'a'+10) : c >= 'A' && c <= 'F' ? unsigned(c-'A'+10) : 16; };
    bool nonzero = false;
    for (unsigned i = 0; i < 32; ++i) if (!text[i]) return false;
    if (text[32]) return false;
    for (unsigned i = 0; i < 16; ++i) {
        const auto a = digit(text[2*i]), b = digit(text[2*i+1]);
        if (a > 15 || b > 15) return false;
        out[i] = static_cast<std::uint8_t>((a<<4)|b); nonzero |= out[i] != 0;
    }
    return nonzero;
}
bool reason_valid(BwInventoryCollectorHostReason r) noexcept {
    return r >= BW_IC_CPU_REPLACE && r <= BW_IC_SHUTDOWN;
}
bool action_valid(BwInventoryCollectorHostAction a) noexcept {
    return a >= BW_IC_INITIAL_BIND && a <= BW_IC_STATE_CAPTURE_DONE;
}
} // namespace
extern "C" bool bw_inventory_collector_host_start(const CPUState*,
    const StaticRecompModuleDesc*, const char*, bool) noexcept {
    return false; // cannot bypass actual loaded-code admission in this private revision
}
extern "C" bool bw_inventory_collector_host_start_verified(const CPUState* cpu,
    const StaticRecompModuleDesc* descriptor, const char* run, bool headless, BwIcLoadedCode* code, std::uint64_t expected_generation) noexcept {
    try {
        // Never replace an active issuer/collector, including a suspended one.
        if (host || !headless || !cpu || !descriptor || !owner_available_for_start()) return false;
        const auto code_generation = bw_ic_code_generation(code);
        if (code_generation != expected_generation || !bw_ic_code_is_live(code, expected_generation, descriptor)) return false;
        RunId run_id{}; if (!parse_run(run, run_id)) return false;
        subset::ModuleFingerprint fingerprint; Availability why;
        if (!validate_owned_descriptor(*descriptor, &fingerprint, &why)) return false;
        auto next = std::make_unique<Host>(); next->headless = true;
        next->code = code; next->code_generation = code_generation; next->descriptor = descriptor;
        next->registry = std::make_unique<OwnedBindingRegistry>();
        if (!next->registry->install_read_gate(next.get(), registry_code_live)) return false;
        // CPU is actual caller-owned at this startup boundary, not a retained probe.
        if (!next->registry->bind_after_actual_action(BindingAction::InitialBind, *cpu,
            cpu->ram, cpu->ram_size, fingerprint, Provenance::FreshNativeBoot)) return false;
        const HostProviders verified = next->registry->providers();
        next->collector = std::make_unique<Collector>(run_id, verified,
            bluewake::randomizer::imported_catalog());
        if (!next->collector->enable(*descriptor)) return false;
        host = std::move(next); return true;
    } catch (...) { return false; }
}
extern "C" bool bw_inventory_collector_host_enabled(void) noexcept { return owner_thread(); }
extern "C" bool bw_inventory_collector_host_update_guard(const BwInventoryCollectorHostFlags* f) noexcept {
    try {
        if (!owner_thread() || !code_live(*host) || !f || !f->source_initialized || !f->rel_lifecycle_known ||
            !host->headless || !f->explicit_headless_no_ui) {
            if (owner_thread()) {
                host->collector->suspend(SuspendReason::MachineReset);
                host->registry->suspend_before_actual_action(SuspendReason::MachineReset);
            }
            return false;
        }
        HostGuard guard; guard.valid = true; // every required source was explicitly known above
        guard.initialized_controls = f->initialized_controls;
        guard.explicit_headless_no_ui = host->headless && f->explicit_headless_no_ui;
        guard.input_generation = f->input_generation;
        guard.input_blocked = f->input_blocked; guard.menu_open = f->menu_open;
        guard.machine_capture_pending = f->machine_capture_pending;
        guard.machine_load_pending = f->machine_load_pending;
        guard.reset_pending = f->reset_pending; guard.rel_lifecycle_pending = f->rel_lifecycle_pending;
        guard.module_replace_pending = f->module_replace_pending; guard.relaunch_pending = f->relaunch_pending;
        guard.shutdown_pending = f->shutdown_pending; guard.native_autosave_active = f->native_autosave_active;
        guard.quick_door_active = f->quick_door_active; guard.quick_items_overlay_active = f->quick_items_overlay_active;
        guard.card_callback_active = f->card_callback_active;
        return host->registry->update_host_guard(guard);
    } catch (...) {
        if (owner_thread()) host->collector->suspend(SuspendReason::MachineReset);
        return false;
    }
}
extern "C" void bw_inventory_collector_host_suspend(BwInventoryCollectorHostReason reason) noexcept {
    try {
        if (!owner_thread()) return;
        const auto r = reason_valid(reason) ? static_cast<SuspendReason>(reason) : SuspendReason::MachineReset;
        if (reason == BW_IC_MODULE_RELOAD || reason == BW_IC_RELAUNCH || reason == BW_IC_SHUTDOWN)
            bw_ic_code_revoke(host->code, host->code_generation);
        // A failed/unsupported STATE load must not be repaired by a later
        // read-only capture. Capture completion renews only a previously live owner.
        BindingLease binding;
        const auto status = host->collector->diagnostic_copy().availability;
        host->capture_can_resume = reason == BW_IC_STATE_CAPTURE &&
            host->registry->copy_registered_binding(&binding) &&
            binding.raw_alias_generation == g_ppc_guest_alias_generation &&
            status != Availability::Retired && status != Availability::Suspended;
        host->capture_origin = host->capture_can_resume ? binding : BindingLease{};
        // Registry records the live pre-capture lease before revoking it.
        host->registry->suspend_before_actual_action(r); host->collector->suspend(r);
    } catch (...) { bw_inventory_collector_retire_pending_proofs(); }
}
extern "C" bool bw_inventory_collector_host_rebind(const CPUState* cpu,
    const StaticRecompModuleDesc* descriptor, BwInventoryCollectorHostAction action) noexcept {
    try {
        if (!owner_thread() || !cpu || !descriptor || !action_valid(action) ||
            !bw_ic_code_is_live(host->code, host->code_generation, descriptor)) return false;
        if (action == BW_IC_STATE_CAPTURE_DONE && !host->capture_can_resume) return false;
        if (action == BW_IC_STATE_CAPTURE_DONE && cpu != host->capture_origin.cpu) {
            host->capture_can_resume = false; host->capture_origin = {};
            host->registry->suspend_before_actual_action(SuspendReason::MemoryReplace);
            host->collector->suspend(SuspendReason::MemoryReplace);
            return false; // reject mismatched probe BEFORE any cpu->ram argument
        }
        subset::ModuleFingerprint fingerprint; Availability why;
        if (!validate_owned_descriptor(*descriptor, &fingerprint, &why)) return false;
        auto provenance = action == BW_IC_STATE_LOADED ? Provenance::MachineStateLoad :
            action == BW_IC_NATIVE_CARD_LOADED ? Provenance::NativeCardLoad : Provenance::FreshNativeBoot;
        const auto* ram = action == BW_IC_STATE_CAPTURE_DONE ? host->capture_origin.registered_ram : cpu->ram;
        const auto size = action == BW_IC_STATE_CAPTURE_DONE ? host->capture_origin.registered_ram_size : cpu->ram_size;
        if (!host->registry->bind_after_actual_action(static_cast<BindingAction>(action),
            *cpu, ram, size, fingerprint, provenance)) return false;
        host->alias_open = false; host->capture_can_resume = false; host->capture_origin = {};
        return host->collector->resume();
    } catch (...) { return false; }
}
extern "C" void bw_inventory_collector_host_alias_before(void) noexcept {
    try {
        if (!owner_thread()) return;
        // Retire before a real registry mutation. CPU/RAM/module stay owned.
        host->alias_open = true; host->collector->suspend(SuspendReason::AliasRebuild);
        bw_inventory_collector_retire_pending_proofs();
    } catch (...) { bw_inventory_collector_retire_pending_proofs(); }
}
extern "C" void bw_inventory_collector_host_alias_after(std::uint32_t raw) noexcept {
    try {
        if (!owner_thread() || !host->alias_open || !code_live(*host)) return;
        host->alias_open = false;
        if (!host->registry->observe_owned_alias_change(raw) || !host->collector->resume())
            host->registry->suspend_before_actual_action(SuspendReason::AliasRebuild);
    } catch (...) {
        if (owner_thread()) host->registry->suspend_before_actual_action(SuspendReason::AliasRebuild);
    }
}
extern "C" void bw_inventory_collector_host_shutdown(void) noexcept {
    try {
        if (!owner_thread()) return;
        host->collector->disable(); host->registry->suspend_before_actual_action(SuspendReason::Shutdown);
        host->collector.reset(); host->registry.reset(); host.reset();
    } catch (...) {
        // If an unexpected failure leaves owned objects, keep revoked records
        // alive rather than leave a subscriber with freed state.
        if (owner_thread()) { host->collector->suspend(SuspendReason::Shutdown); (void)host.release(); }
    }
}
extern "C" bool bw_inventory_collector_host_diagnostic(BwInventoryCollectorHistoricalDiagnostic* out) noexcept {
    if (out) *out = {};
    try {
        if (!owner_thread() || !out) return false;
        const auto d = host->collector->diagnostic_copy();
        out->availability = static_cast<std::uint32_t>(d.availability);
        out->completed_attempts = d.completed_attempts; out->successful_snapshots = d.successful_snapshots;
        out->unavailable_reads = d.unavailable_reads; out->unstable_snapshots = d.unstable_snapshots;
        out->lifecycle_invalidations = d.lifecycle_invalidations; out->authorization_expired = true;
        if (!d.history) return true;
        const auto& h = *d.history; out->has_history = true;
        std::memcpy(out->run, h.run.data(), 16); out->sequence = h.sequence; out->native_call = h.native_call;
        out->source_address = h.source_address; out->actor_id = h.actor_id;
        out->provenance = static_cast<std::uint32_t>(h.provenance);
        out->projection_status = static_cast<std::uint32_t>(h.projection_status);
        out->has_bytes = h.bytes.has_value();
        if (h.bytes) std::memcpy(out->inventory_bytes, h.bytes->data(), 5);
        out->has_native_frame = h.native_frame.has_value();
        if (h.native_frame) out->native_frame = *h.native_frame;
        out->has_owner = h.owner.has_value();
        if (h.owner) {
            const auto& o = *h.owner; out->cpu_lifetime = o.cpu_lifetime; out->memory_lifetime = o.memory_lifetime;
            out->module_generation = o.module_generation; out->alias_binding = o.alias_generation;
            out->native_epoch = o.native_epoch; out->scene_generation = o.scene_generation;
            std::memcpy(out->stage, o.stage.data(), 9);
        }
        for (unsigned i = 0; i < 5; ++i) {
            const auto& r = h.immediate[i]; out->capability_status[i] = static_cast<std::uint32_t>(r.status);
            out->capability_work[i] = r.work; out->capability_has_value[i] = r.value.has_value();
            if (r.value) out->capability_value[i] = *r.value;
        }
        return true;
    } catch (...) { if (out) *out = {}; return false; }
}
