// SPDX-License-Identifier: GPL-3.0-or-later
// Experimental game-thread bridge. Compiled only by explicit collector opt-in.
// Every function is game-thread-only. No UI guest reader or cross-thread
// mailbox is provided; historical diagnostics carry expired authority.
#ifndef BW_INVENTORY_COLLECTOR_HOST_H
#define BW_INVENTORY_COLLECTOR_HOST_H
#include "StaticRecompABI.h"
#include "loaded_code_admission.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
#define BW_IC_HOST_NOEXCEPT noexcept
extern "C" {
#else
#define BW_IC_HOST_NOEXCEPT
#endif
typedef struct BwInventoryCollectorHostFlags {
    // All flags must be sourced by actual main, never inferred from missing data.
    bool source_initialized, explicit_headless_no_ui, rel_lifecycle_known;
    bool initialized_controls, input_blocked, menu_open;
    uint64_t input_generation;
    bool machine_capture_pending, machine_load_pending, reset_pending;
    bool rel_lifecycle_pending, module_replace_pending, relaunch_pending, shutdown_pending;
    bool native_autosave_active, quick_door_active, quick_items_overlay_active, card_callback_active;
} BwInventoryCollectorHostFlags;
typedef enum BwInventoryCollectorHostReason {
    BW_IC_CPU_REPLACE, BW_IC_MEMORY_REPLACE, BW_IC_STATE_CAPTURE, BW_IC_STATE_LOAD,
    BW_IC_MACHINE_RESET, BW_IC_MODULE_RELOAD, BW_IC_ALIAS_REBUILD, BW_IC_RELAUNCH, BW_IC_SHUTDOWN
} BwInventoryCollectorHostReason;
typedef enum BwInventoryCollectorHostAction {
    BW_IC_INITIAL_BIND, BW_IC_CPU_REPLACED, BW_IC_RAM_REPLACED, BW_IC_NATIVE_CARD_LOADED,
    BW_IC_STATE_LOADED, BW_IC_MACHINE_RESET_DONE, BW_IC_MODULE_RELOAD_DONE,
    BW_IC_ALIAS_REBUILD_DONE, BW_IC_STATE_CAPTURE_DONE
} BwInventoryCollectorHostAction;
typedef struct BwInventoryCollectorHistoricalDiagnostic {
    uint32_t availability;
    uint64_t completed_attempts, successful_snapshots, unavailable_reads;
    uint64_t unstable_snapshots, lifecycle_invalidations;
    bool has_history, authorization_expired, has_bytes, has_native_frame, has_owner;
    uint8_t run[16], inventory_bytes[5];
    uint64_t sequence, native_call;
    uint32_t source_address, actor_id, native_frame, provenance, projection_status;
    uint64_t cpu_lifetime, memory_lifetime, module_generation, alias_binding;
    uint64_t native_epoch, scene_generation;
    char stage[9];
    uint32_t capability_status[5], capability_work[5];
    bool capability_has_value[5], capability_value[5];
} BwInventoryCollectorHistoricalDiagnostic;
// Explicit diagnostic headless-only opt-in and nonzero unique 32-hex run ID.
// Invoked after actual event attach/module binding. No file loading or logging.
bool bw_inventory_collector_host_start(const CPUState*, const StaticRecompModuleDesc*,
    const char* run_id_32_hex, bool actual_headless_no_ui) BW_IC_HOST_NOEXCEPT;
// Descriptor-only capture startup is intentionally unavailable.
// Only an actual loader-issued opaque lease may start this bridge.
bool bw_inventory_collector_host_start_verified(const CPUState*, const StaticRecompModuleDesc*,
    const char* run_id_32_hex, bool actual_headless_no_ui, BwIcLoadedCode*, uint64_t expected_generation) BW_IC_HOST_NOEXCEPT;
bool bw_inventory_collector_host_enabled(void) BW_IC_HOST_NOEXCEPT;
bool bw_inventory_collector_host_update_guard(const BwInventoryCollectorHostFlags*) BW_IC_HOST_NOEXCEPT;
void bw_inventory_collector_host_suspend(BwInventoryCollectorHostReason) BW_IC_HOST_NOEXCEPT;
bool bw_inventory_collector_host_rebind(const CPUState*, const StaticRecompModuleDesc*,
    BwInventoryCollectorHostAction) BW_IC_HOST_NOEXCEPT;
void bw_inventory_collector_host_alias_before(void) BW_IC_HOST_NOEXCEPT;
void bw_inventory_collector_host_alias_after(uint32_t actual_raw_generation) BW_IC_HOST_NOEXCEPT;
void bw_inventory_collector_host_shutdown(void) BW_IC_HOST_NOEXCEPT;
// Copied past facts only. Does not sample RAM, refresh Context or log/write files.
bool bw_inventory_collector_host_diagnostic(BwInventoryCollectorHistoricalDiagnostic*) BW_IC_HOST_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#endif
