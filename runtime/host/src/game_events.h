#ifndef BLUEWAKE_GAME_EVENTS_H
#define BLUEWAKE_GAME_EVENTS_H

#include "core/cpu.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GZLE01 observers. All APIs, subscriptions and callbacks belong to the game
 * thread. Callbacks receive copied facts, never a mutable guest CPU. Copy the
 * event to a synchronized queue before using it from the UI/network thread.
 * Callbacks may unsubscribe, but must not call a lifecycle/tick/dispatch API.
 * Observation never changes guest memory, registers, PC, or timing budgets. */
typedef enum BwGameEventKind {
    BW_GAME_EVENT_RESET,
    BW_GAME_EVENT_GAME_TICK,       /* One actual game-thread VI retrace (60 Hz). */
    BW_GAME_EVENT_SCENE_LEAVING,   /* Previous scene identity; player is invalid. */
    BW_GAME_EVENT_SCENE_ENTERED,
    BW_GAME_EVENT_TRANSITION_STARTED,
    BW_GAME_EVENT_PLAYER_UPDATED,  /* Completed native execute, never a VI alias. */
    BW_GAME_EVENT_INVENTORY_CHANGED,
    BW_GAME_EVENT_PROGRESSION_CHANGED,
    BW_GAME_EVENT_ITEM_AWARDED,    /* Audited execItemGet return; no-entry IDs excluded. */
    BW_GAME_EVENT_SAVE_SERIALIZED, /* memory_to_card succeeded; no disk claim. */
    BW_GAME_EVENT_SAVE_COMPLETED,  /* Guest SaveSync acknowledged store success. */
    BW_GAME_EVENT_COUNT
} BwGameEventKind;

#define BW_GAME_EVENT_MASK(kind) (UINT64_C(1) << (kind))
#define BW_GAME_EVENT_ALL ((UINT64_C(1) << BW_GAME_EVENT_COUNT) - UINT64_C(1))

typedef enum BwGameResetReason {
    BW_GAME_RESET_ATTACH,
    BW_GAME_RESET_STATE_LOAD,
    BW_GAME_RESET_GAME_LOAD,
    BW_GAME_RESET_MACHINE_RESET,
    BW_GAME_RESET_MODULE_RELOAD,
    BW_GAME_RESET_MEMORY_REPLACED
} BwGameResetReason;

typedef struct BwGameScene {
    char stage[9];
    uint16_t spawn;
    int8_t room;
    int8_t layer;
    int8_t stay_room;
    uint32_t player;
    float position[3];
    bool active;
    bool player_valid;
    bool controls_ready;
    bool paused;
    bool event_running;
    bool transitioning;
} BwGameScene;

/* Native in-memory dSv offsets, not packed card offsets. A delta is a byte
 * fact with an explicit namespace/index. Event flags include registers and
 * reversible flags; consumers must whitelist permanent shared progression.
 * Current-stage memory is not the same namespace as the saved stage table.
 * Temporary room/zone flags, config, padding and recollection copies are not
 * observed. Resource counts may decrease; these are not item-award events. */
typedef enum BwGameFactKind {
    BW_GAME_FACT_ITEM_SLOT,
    BW_GAME_FACT_ITEM_OBTAINED,
    BW_GAME_FACT_ITEM_COUNT,
    BW_GAME_FACT_ITEM_CAPACITY,
    BW_GAME_FACT_BAG_ITEM,
    BW_GAME_FACT_BAG_OBTAINED,
    BW_GAME_FACT_BAG_COUNT,
    BW_GAME_FACT_COLLECTED,
    BW_GAME_FACT_MAP,
    BW_GAME_FACT_SAVED_STAGE,
    BW_GAME_FACT_OCEAN,
    BW_GAME_FACT_EVENT,
    BW_GAME_FACT_CURRENT_STAGE
} BwGameFactKind;

typedef struct BwGameEvent {
    BwGameEventKind kind;
    uint64_t sequence;             /* Monotonic across reloads. */
    uint64_t epoch;                /* Changes on attach/reset/load. */
    uint64_t scene_generation;     /* Changes on transition/actor/room changes. */
    uint64_t tick;
    uint64_t native_call;          /* Stable token for one observed invocation. */
    BwGameScene scene;
    BwGameScene next_scene;        /* Transition target, when known. */
    uint32_t source_address;       /* Audited return site, or zero for VI facts. */
    BwGameResetReason reset_reason;
    BwGameFactKind fact;
    uint16_t index;
    uint8_t before;
    uint8_t after;
    uint8_t item_id;
    int8_t save_slot;              /* -1 if guest completion does not name it. */
    int32_t result;                /* Raw return r3; execItemGet is void, not a status. */
} BwGameEvent;

typedef void (*BwGameEventCallback)(const BwGameEvent* event, void* user);
typedef uint64_t BwGameEventSubscription;

/* Bounded (16 subscribers); zero means invalid mask/callback/full. Existing
 * subscriptions survive resets. A removed subscriber will not be invoked
 * later in the same emission; newly added subscribers start on the next one. */
BwGameEventSubscription bluewake_game_events_subscribe(uint64_t mask,
                                                      BwGameEventCallback callback,
                                                      void* user);
bool bluewake_game_events_unsubscribe(BwGameEventSubscription subscription);

void bluewake_game_events_attach(CPUState* cpu);
/* Host must call after restoring a state/resetting a machine/reloading its
 * module, even if RAM/player addresses did not change. Native card_to_memory
 * completion also resets automatically. NULL detaches and invalidates state. */
void bluewake_game_events_reset(CPUState* cpu, BwGameResetReason reason);
void bluewake_game_events_retrace(CPUState* cpu);

/* Integrate in host_can_skip_observation AND the first-PC/edge service, outside
 * feature_dispatch's geometry interval. The native entry and armed caller
 * return must both remain observable under optimized direct calls. Audited
 * item coverage includes the exact uncached DOL entry used by REL imports and
 * module 131's demo-item Delete return; other REL sites require a new audit. */
bool bluewake_game_events_observes(uint32_t address);
void bluewake_game_events_dispatch(CPUState* cpu, uint32_t address);
/* Narrow game-thread bridge for an audited host-owned native save invocation.
 * Call only after the autosave owner validates its live SERIALIZE/POLL phase,
 * at the genuine native entry with LR 8180FFE0. Ordinary native entries never
 * admit that token. Observe its validated return BEFORE autosave consumes r3.
 * No register/RAM changes or synthetic save success are made by this API. */
bool bluewake_game_events_arm_host_save(CPUState* cpu, uint32_t native_entry,
                                       int8_t slot, uint32_t scratch);
bool bluewake_game_events_scene(BwGameScene* scene, uint64_t* epoch,
                               uint64_t* generation);

typedef struct BwGameEventStats {
    uint64_t epoch;
    uint64_t scene_generation;
    uint64_t ticks;
    uint64_t emitted[BW_GAME_EVENT_COUNT];
    uint64_t pending_overflow;
    uint64_t cancelled_calls;
} BwGameEventStats;
void bluewake_game_events_stats(BwGameEventStats* stats);
const char* bluewake_game_event_name(BwGameEventKind kind);

#ifdef __cplusplus
}
#endif
#endif
