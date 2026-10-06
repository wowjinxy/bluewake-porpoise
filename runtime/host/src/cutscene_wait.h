// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_CUTSCENE_WAIT_H
#define BLUEWAKE_CUTSCENE_WAIT_H
#include "core/cpu.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* The host issuer must own genuine admitted code,
 * CPU/RAM and native CARD provenance. Callbacks/tokens/descriptor constants do
 * not create that authority. No UI call receives CPU, guest data or a resolver. */
typedef enum BwTcWaitAvailability {
    BW_TC_WAIT_UNAVAILABLE_BUILD,
    BW_TC_WAIT_UNAVAILABLE_ADMISSION,
    BW_TC_WAIT_AVAILABLE
} BwTcWaitAvailability;
typedef enum BwTcWaitSuspendReason {
    BW_TC_WAIT_CONFIG, BW_TC_WAIT_SCENE, BW_TC_WAIT_TRANSITION,
    BW_TC_WAIT_CARD, BW_TC_WAIT_STATE, BW_TC_WAIT_MACHINE,
    BW_TC_WAIT_CPU, BW_TC_WAIT_RAM, BW_TC_WAIT_CODE, BW_TC_WAIT_REL,
    BW_TC_WAIT_ALIAS, BW_TC_WAIT_ACTOR, BW_TC_WAIT_SAVE,
    BW_TC_WAIT_CAPTURE, BW_TC_WAIT_INPUT_DISCARD, BW_TC_WAIT_DETACH
} BwTcWaitSuspendReason;
enum {
    BW_TC_WAIT_NATIVE_MEM1 = 0x01800000u,
    BW_TC_WAIT_HOST_RAM = 0x02000000u,
    BW_TC_WAIT_CARD_AUTHORIZED = 1u,
    BW_TC_WAIT_MUTATORS_HELD = 2u,
    BW_TC_WAIT_SAVE_ACTIVE = 4u,
    BW_TC_WAIT_CAPTURE_ACTIVE = 8u,
    BW_TC_WAIT_TRANSITION_ACTIVE = 16u
};
typedef struct BwTcWaitLease {
    /* All generations are monotonic, runtime-only issuer values, never pointer,
     * PID/hash substitutes. issuer must equal the copied binding issuer. */
    uint64_t issuer, cpu, ram, code, native_card_epoch, scene, alias, rel_table;
    uint64_t retrace;
    uint8_t* mem1;
    uint32_t native_size, host_ram_size, thread, context, flags;
} BwTcWaitLease;
typedef struct BwTcWaitOwner {
    uint64_t materialization, registration;
    uint32_t actor, pid, profile, methods, actor_tag;
    uint32_t module_header, loader_owner, raw_text, linked_text, text_size;
} BwTcWaitOwner;
typedef struct BwTcWaitHostBinding {
    void* user;
    uint64_t issuer;
    BwTcWaitAvailability availability;
    /* Must check external live issuer/CPU identity BEFORE any cpu dereference.
     * Also prove current owner thread, code lease and borrowed storage lifetime.
     * False zeros *out. No callback calls a native function or faults CPU. */
    bool (*validate_before_cpu)(void*, const CPUState*, BwTcWaitLease* out);
    /* Nonfaulting native alias-aware byte-contiguous mapping. Every span is
     * bounded/revalidated by this core before and after the callback. Linked
     * fixed .data must be actual current module backing, not raw copied data.
     * Ordinary actor/stack/event spans stay24MiB. Exact owned raw REL285 text
     * additionally maps only inside loader scratch81820000..81F80000 in the
     * 32MiB host allocation; this is not a general extension of native spans. */
    const uint8_t* (*resolve)(void*, uint32_t address, uint32_t size);
    /* Exact flat MEM1 bytes, with every timer byte unshadowed and writable.
     * Returns no pointer after issuer revocation. This is not a write callback. */
    uint8_t* (*resolve_timer_write)(void*, uint32_t address, uint32_t size);
    /* Dedicated real REL285 + registered Tc actor query. Prove bounded actual
     * OS-list/slot/alias/section/profile/method + actor queue membership, unique
     * owner/PID/init2/create2/backpointer, before copying these identities.
     * expected_pid=0 discovers; nonzero revalidates. Recheck issuer on return.
     * Revoke before OSUnlink, alias/slot reuse, cTg cut, or backing free. */
    bool (*owner)(void*, const CPUState*, uint32_t actor, uint32_t expected_pid,
                  BwTcWaitOwner* out);
} BwTcWaitHostBinding;
typedef struct BwTcWaitStatus {
    BwTcWaitAvailability availability;
    bool desired_enabled, pending;
    uint64_t desired_generation, entries, shortened, rejected, cancelled;
    uint32_t last_cut;
} BwTcWaitStatus;

/* UI-safe desired atomic config only. Idempotent values keep their generation.
 * Game-thread dispatch/retrace applies changes and cancels pending ownership;
 * disabling cannot restore an already shortened native timer. */
void bluewake_cutscene_wait_configure(bool enabled);
bool bluewake_cutscene_wait_enabled(void);
const char* bluewake_cutscene_wait_availability_name(BwTcWaitAvailability);
/* UI-safe independently atomic copied metadata; no CPU/callback access.
 * Counts may describe neighboring game-thread instants, never an authority. */
void bluewake_cutscene_wait_snapshot(BwTcWaitStatus* out);

/* Game-thread-only ownership/lifecycle. Attach copies the binding without CPU
 * reads/callbacks, including when OFF. Availability is build/code capability,
 * not current Tc eligibility. Host stops borrowing before freeing storage. */
bool bluewake_cutscene_wait_attach(CPUState*, const BwTcWaitHostBinding*);
void bluewake_cutscene_wait_suspend(BwTcWaitSuspendReason);
void bluewake_cutscene_wait_detach(void);
void bluewake_cutscene_wait_retrace(CPUState*, bool mutators_held);
bool bluewake_cutscene_wait_observes(const CPUState*, uint32_t address);
void bluewake_cutscene_wait_dispatch(CPUState*, uint32_t address, bool mutators_held);
/* Exactly six initialized TC_RESCUE WAITs; only actor+2DC BE32 may become1.
 * Entry canonical8021CC4C/importC021CC4C; real Tc return C10A3DE4 (or its
 * currently owned raw text alias). Native invocation executes normally once.
 * Return admission intentionally requires low bool byte exactly1 (the pinned
 * common-cut true return), not arbitrary nonzero low-byte values.
 * No CPU register/budget/result/event/award mutation or cutEnd helper call. */
#ifdef __cplusplus
}
#endif
#endif
