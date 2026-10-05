#include "game_events.h"
#include "song_rel_owner.h"
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
#include "inventory_completion_internal.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GZLE01 USA rev0. Field layouts are verified against zeldaret/tww's
 * d_com_inf_game.h, d_save.h, m_Do_MemCard.h and their implementations.
 * https://github.com/zeldaret/tww/blob/main/include/d/d_save.h
 * https://github.com/zeldaret/tww/blob/main/src/m_Do/m_Do_MemCard.cpp
 * These are the native RAM layouts: packed memory-card offsets differ. */
/* Keep guest addresses unsigned even in MSVC-compatible C, whose enum
 * constants above INT_MAX otherwise become signed int. RAM offsets must be
 * subtracted as integers before adding them to the host allocation. */
static const uint32_t kGameInfo = 0x803C4C08u;
static const uint32_t kPlayerPointer = 0x803CA74Cu;
static const uint32_t kCurrentStage = 0x803C9D3Cu;
static const uint32_t kNextStage = 0x803C9D48u;
static const uint32_t kStayRoom = 0x803F6A78u;
static const uint32_t kOverlapRequest = 0x803F6160u;
static const uint32_t kEventMode = 0x803C9EA2u;
static const uint32_t kMenuPause = 0x803F7097u;
static const uint32_t kCardControl = 0x803B39A0u;
static const uint32_t kExecuteMethod = 0x8003EF38u;
static const uint32_t kActorExecuteReturn = 0x80023960u;
static const uint32_t kPlayerExecute = 0x80122D30u;
static const uint32_t kItemGet = 0x800C2DFCu;
/* REL imports use this exact uncached DOL entry. The module dispatcher maps
 * it to kItemGet after host observation; linked REL PCs must not be generally
 * canonicalized, because their synthetic C0xxxxxx addresses identify code. */
static const uint32_t kItemGetUncached = 0xC00C2DFCu;
static const uint32_t kItemFunctionTable = 0x803888C8u;
static const uint32_t kItemNoEntry = 0x800C6374u;
static const uint32_t kMemoryToCard = 0x8005E780u;
static const uint32_t kCardToMemory = 0x8005EA24u;
static const uint32_t kSaveSync = 0x8001931Cu;
enum {
    kPosition = 0x1F8u,
    kAcchPosition = 0x498u,
    kDemoType = 0x304u,
    kDemoMode = 0x314u,
    kPlayerSpan = 0x361Cu,
    kCardControlSpan = 0x1698u,
    kCardGameDataSize = 0x770u,
    kSaveDataSpan = 0x79Cu,
    kSubscribers = 16u,
    kPendingCalls = 16u,
    kPendingMaxTicks = 600u
};

typedef struct Subscriber {
    BwGameEventSubscription token;
    uint64_t mask;
    BwGameEventCallback callback;
    void* user;
} Subscriber;

typedef enum CallKind { CALL_PLAYER, CALL_ITEM, CALL_SERIALIZE, CALL_LOAD, CALL_SAVE } CallKind;
typedef struct PendingCall {
    bool active;
    CallKind kind;
    uint32_t return_address;
    uint32_t stack;
    uint32_t player;
    uint8_t item;
    int8_t slot;
    uint64_t token;
    uint64_t epoch;
    uint64_t generation;
    uint64_t tick;
    bool host_owned_save;
    uint32_t host_scratch;
    bool hr_song;
    uint32_t hr_actor, hr_id;
    int32_t hr_staff;
    BwSongOwner hr_owner;
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
    BwInventoryCollectorCallProof inventory_proof;
#endif
} PendingCall;

typedef struct FactRange {
    uint16_t offset;
    uint16_t size;
    BwGameFactKind fact;
    BwGameEventKind event;
} FactRange;
static const FactRange kFacts[] = {
    {0x03C, 0x15, BW_GAME_FACT_ITEM_SLOT, BW_GAME_EVENT_INVENTORY_CHANGED},
    {0x051, 0x15, BW_GAME_FACT_ITEM_OBTAINED, BW_GAME_EVENT_INVENTORY_CHANGED},
    {0x066, 0x08, BW_GAME_FACT_ITEM_COUNT, BW_GAME_EVENT_INVENTORY_CHANGED},
    {0x06E, 0x08, BW_GAME_FACT_ITEM_CAPACITY, BW_GAME_EVENT_INVENTORY_CHANGED},
    {0x076, 0x18, BW_GAME_FACT_BAG_ITEM, BW_GAME_EVENT_INVENTORY_CHANGED},
    {0x090, 0x0C, BW_GAME_FACT_BAG_OBTAINED, BW_GAME_EVENT_INVENTORY_CHANGED},
    {0x09C, 0x18, BW_GAME_FACT_BAG_COUNT, BW_GAME_EVENT_INVENTORY_CHANGED},
    {0x0B4, 0x0D, BW_GAME_FACT_COLLECTED, BW_GAME_EVENT_INVENTORY_CHANGED},
    {0x0C4, 0x84, BW_GAME_FACT_MAP, BW_GAME_EVENT_PROGRESSION_CHANGED},
    /* dSv_memBit's last two bytes are padding, not progression. */
    {0x380, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x3A4, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x3C8, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x3EC, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x410, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x434, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x458, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x47C, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x4A0, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x4C4, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x4E8, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x50C, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x530, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x554, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x578, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x59C, 0x22, BW_GAME_FACT_SAVED_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x5C0, 0x64, BW_GAME_FACT_OCEAN, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x624, 0x100, BW_GAME_FACT_EVENT, BW_GAME_EVENT_PROGRESSION_CHANGED},
    {0x778, 0x22, BW_GAME_FACT_CURRENT_STAGE, BW_GAME_EVENT_PROGRESSION_CHANGED}
};

/* Audited slow AND fast translated cross-chunk call/return guards in the
 * private mods-integration composite. Same-chunk local gotos bypass host
 * observation: never watch the native player wrapper's internal return or
 * execItemGet's 800C2E20 item-function return and assume coverage.
 * fopAc_Execute 8002395C -> fpcMtd_Execute -> 80023960 is real native execution.
 * Item returns: chunks 0023,0025,0061. Serialization: 0098,0117.
 * SaveSync returns: 0097,0098,0117,0139. Card load: 0140.
 * REL 131 (d_a_demo_item), section 1 C07700EC..C0771084: daDitem_Delete
 * C07709E0 calls the uncached DOL entry at C0770A04 and returns to C0770A08.
 * This call exits the translated REL chunk, and its return is a dispatchable
 * block. The loader maps module/section identity to these fixed linked PCs;
 * no raw REL range or arbitrary caller LR is admitted here.
 * Unknown donor/REL call sites need a separate reachability audit. */
static const uint32_t kItemReturns[] = {
    0x8006068Cu, 0x80068D6Cu,
    0x800F64B8u, 0x800F64F0u, 0x800F6528u, 0x800F6560u,
    0x800F6598u, 0x800F65D0u, 0x800F6608u, 0x800F6640u,
    0x800F66E8u, 0x800F6720u, 0x800F6758u, 0x800F679Cu,
    0x800F680Cu, 0x800F690Cu, 0x800F697Cu, 0x800F69ECu,
    0x800F6A5Cu, 0x800F6ACCu, 0x800F6B3Cu, 0x800F6BACu,
    0x800F6C1Cu, 0x800F6C8Cu,
    0xC0770A08u
};
static const uint32_t kSerializeReturns[] = {0x8018D5C4u, 0x801D8994u};
static const uint32_t kSaveReturns[] = {
    0x80187C1Cu, 0x80187D18u, 0x8018D674u, 0x801D83E0u,
    0x801D89FCu, 0x801D8A6Cu, 0x802312A4u
};

static BwSongOwnerQuery g_song_owner_query;
static void* g_song_owner_user;
static CPUState* g_cpu;
static const uint8_t* g_ram;
static uint32_t g_ram_size;
static bool g_busy, g_transition, g_facts_valid, g_loading;
static BwGameScene g_scene;
static uint8_t g_facts[kSaveDataSpan];
static Subscriber g_subscribers[kSubscribers];
static PendingCall g_pending[kPendingCalls];
static uint64_t g_mask, g_subscription_token, g_sequence, g_native_token;
static BwGameEventStats g_stats;
static BwGameEventSubscription g_trace_subscription;
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
void bw_inventory_collector_retire_pending_proofs(void) {
    for (unsigned i = 0; i < kPendingCalls; ++i)
        memset(&g_pending[i].inventory_proof, 0, sizeof g_pending[i].inventory_proof);
    bw_inventory_collector_emit_end();
}
#endif

static bool span(const CPUState* cpu, uint32_t address, uint32_t size) {
    if (cpu == NULL || cpu->ram == NULL || address < 0x80000000u || address > 0x81800000u)
        return false;
    const uint32_t offset = address - 0x80000000u;
    return size <= 0x01800000u && offset <= 0x01800000u - size &&
           size <= cpu->ram_size && offset <= cpu->ram_size - size;
}
static uint8_t read8(const CPUState* cpu, uint32_t address) {
    return cpu->ram[address - 0x80000000u];
}
static uint16_t read16(const CPUState* cpu, uint32_t address) {
    return (uint16_t)(((uint16_t)read8(cpu, address) << 8u) | read8(cpu, address + 1u));
}
static uint32_t read32(const CPUState* cpu, uint32_t address) {
    return ((uint32_t)read16(cpu, address) << 16u) | read16(cpu, address + 2u);
}
static float read_float(const CPUState* cpu, uint32_t address) {
    const uint32_t bits = read32(cpu, address);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static void mask_refresh(void) {
    g_mask = 0;
    for (unsigned i = 0; i < kSubscribers; ++i)
        if (g_subscribers[i].token != 0)
            g_mask |= g_subscribers[i].mask;
    for (unsigned i = 0; i < kPendingCalls; ++i) {
        PendingCall* call = &g_pending[i];
        if (!call->active) continue;
        const BwGameEventKind kind = call->kind == CALL_PLAYER ? BW_GAME_EVENT_PLAYER_UPDATED :
                                     call->kind == CALL_ITEM ? BW_GAME_EVENT_ITEM_AWARDED :
                                     call->kind == CALL_SERIALIZE ? BW_GAME_EVENT_SAVE_SERIALIZED :
                                     BW_GAME_EVENT_SAVE_COMPLETED;
        if ((call->kind == CALL_LOAD && g_mask != 0) ||
            (call->kind != CALL_LOAD && (g_mask & BW_GAME_EVENT_MASK(kind))))
            continue;
        if (call->kind == CALL_LOAD) g_loading = false;
        call->active = false;
        ++g_stats.cancelled_calls;
    }
}
BwGameEventSubscription bluewake_game_events_subscribe(uint64_t mask,
                                                      BwGameEventCallback callback,
                                                      void* user) {
    if (callback == NULL || mask == 0 || (mask & ~BW_GAME_EVENT_ALL) != 0)
        return 0;
    for (unsigned i = 0; i < kSubscribers; ++i) {
        if (g_subscribers[i].token != 0) continue;
        if (++g_subscription_token == 0) ++g_subscription_token;
        g_subscribers[i] = (Subscriber){g_subscription_token, mask, callback, user};
        mask_refresh();
        return g_subscription_token;
    }
    return 0;
}
bool bluewake_game_events_unsubscribe(BwGameEventSubscription subscription) {
    if (subscription == 0) return false;
    for (unsigned i = 0; i < kSubscribers; ++i) {
        if (g_subscribers[i].token != subscription) continue;
        memset(&g_subscribers[i], 0, sizeof g_subscribers[i]);
        mask_refresh();
        return true;
    }
    return false;
}

static BwGameEvent event_new(BwGameEventKind kind) {
    BwGameEvent event;
    memset(&event, 0, sizeof event);
    event.kind = kind;
    event.epoch = g_stats.epoch;
    event.scene_generation = g_stats.scene_generation;
    event.tick = g_stats.ticks;
    event.scene = g_scene;
    event.save_slot = -1;
    return event;
}
static void emit(BwGameEvent event) {
    event.sequence = ++g_sequence;
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
    if (event.kind == BW_GAME_EVENT_PLAYER_UPDATED &&
        bw_inventory_collector_completion_enabled())
        bw_inventory_collector_event_scope_bind(&event);
#endif
    ++g_stats.emitted[event.kind];
    Subscriber subscribers[kSubscribers];
    memcpy(subscribers, g_subscribers, sizeof subscribers);
    for (unsigned i = 0; i < kSubscribers; ++i) {
        const Subscriber* sub = &subscribers[i];
        if (sub->token == 0 || !(sub->mask & BW_GAME_EVENT_MASK(event.kind)) ||
            g_subscribers[i].token != sub->token)
            continue;
        sub->callback(&event, sub->user);
    }
}

static void clear_pending(void) {
    for (unsigned i = 0; i < kPendingCalls; ++i) {
        if (g_pending[i].active) ++g_stats.cancelled_calls;
        g_pending[i].active = false;
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
        memset(&g_pending[i].inventory_proof, 0, sizeof g_pending[i].inventory_proof);
#endif
    }
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
    bw_inventory_collector_emit_end();
#endif
    g_loading = false;
}
static void leave_scene(void) {
    if (!g_scene.active) return;
    ++g_stats.scene_generation;
    g_scene.active = false;
    g_scene.player_valid = false;
    g_scene.controls_ready = false;
    g_scene.player = 0;
    memset(g_scene.position, 0, sizeof g_scene.position);
    g_facts_valid = false;
    clear_pending();
    emit(event_new(BW_GAME_EVENT_SCENE_LEAVING));
}
static void reset_internal(CPUState* cpu, BwGameResetReason reason) {
    leave_scene();
    clear_pending();
    g_cpu = cpu;
    g_ram = cpu != NULL ? cpu->ram : NULL;
    g_ram_size = cpu != NULL ? cpu->ram_size : 0;
    g_transition = false;
    g_facts_valid = false;
    memset(&g_scene, 0, sizeof g_scene);
    ++g_stats.epoch;
    ++g_stats.scene_generation;
    g_stats.ticks = 0;
    BwGameEvent event = event_new(BW_GAME_EVENT_RESET);
    event.reset_reason = reason;
    emit(event);
}
void bluewake_game_events_reset(CPUState* cpu, BwGameResetReason reason) {
    if (g_busy) return;
    g_busy = true;
    reset_internal(cpu, reason);
    g_busy = false;
}

const char* bluewake_game_event_name(BwGameEventKind kind) {
    static const char* const names[BW_GAME_EVENT_COUNT] = {
        "Reset", "GameTick", "SceneLeaving", "SceneEntered", "TransitionStarted",
        "PlayerUpdated", "InventoryFactChanged", "ProgressionFactChanged", "ItemAward",
        "SaveSerialized", "GuestSaveCompleted"
    };
    return kind >= 0 && kind < BW_GAME_EVENT_COUNT ? names[kind] : "Unknown";
}
static void trace_event(const BwGameEvent* event, void* user) {
    (void)user;
    const uint64_t count = g_stats.emitted[event->kind];
    if (event->kind == BW_GAME_EVENT_INVENTORY_CHANGED ||
        event->kind == BW_GAME_EVENT_PROGRESSION_CHANGED)
        return;
    if ((event->kind == BW_GAME_EVENT_GAME_TICK && count > 3 && count % 600 != 0) ||
        (event->kind == BW_GAME_EVENT_PLAYER_UPDATED && count > 3 && count % 300 != 0) ||
        (event->kind == BW_GAME_EVENT_ITEM_AWARDED && count > 8 && count % 128 != 0))
        return;
    fprintf(stderr, "[game-events] %s count=%llu epoch=%llu generation=%llu tick=%llu "
                    "stage=%.8s room=%d stay=%d player=%08X ready=%u return=%08X "
                    "call=%llu item=%u slot=%d result=%d reason=%u facts=%llu/%llu\n",
            bluewake_game_event_name(event->kind), (unsigned long long)count,
            (unsigned long long)event->epoch, (unsigned long long)event->scene_generation,
            (unsigned long long)event->tick, event->scene.stage, event->scene.room,
            event->scene.stay_room, event->scene.player, event->scene.controls_ready ? 1u : 0u,
            event->source_address, (unsigned long long)event->native_call, event->item_id,
            event->save_slot, event->result, (unsigned)event->reset_reason,
            (unsigned long long)g_stats.emitted[BW_GAME_EVENT_INVENTORY_CHANGED],
            (unsigned long long)g_stats.emitted[BW_GAME_EVENT_PROGRESSION_CHANGED]);
}
void bluewake_game_events_attach(CPUState* cpu) {
    if (g_busy) return;
    if (g_trace_subscription != 0)
        bluewake_game_events_unsubscribe(g_trace_subscription);
    g_trace_subscription = 0;
    const char* trace = getenv("BLUEWAKE_GAME_EVENTS_TRACE");
    if (trace != NULL && strcmp(trace, "1") == 0)
        g_trace_subscription = bluewake_game_events_subscribe(BW_GAME_EVENT_ALL, trace_event, NULL);
    bluewake_game_events_reset(cpu, BW_GAME_RESET_ATTACH);
}

static bool same_scene(const BwGameScene* a, const BwGameScene* b) {
    return memcmp(a->stage, b->stage, sizeof a->stage) == 0 &&
           a->spawn == b->spawn && a->room == b->room && a->layer == b->layer &&
           a->stay_room == b->stay_room && a->player == b->player;
}
static bool read_stage(const CPUState* cpu, uint32_t address, BwGameScene* scene) {
    bool ended = false;
    for (unsigned i = 0; i < 8; ++i) {
        const uint8_t ch = read8(cpu, address + i);
        if (ch == 0) ended = true;
        if (ended) { scene->stage[i] = '\0'; continue; }
        if (!(ch >= '0' && ch <= '9') && !(ch >= 'A' && ch <= 'Z') &&
            !(ch >= 'a' && ch <= 'z') && ch != '_')
            return false;
        scene->stage[i] = (char)ch;
    }
    scene->spawn = read16(cpu, address + 8);
    scene->room = (int8_t)read8(cpu, address + 10);
    scene->layer = (int8_t)read8(cpu, address + 11);
    return scene->stage[0] != '\0';
}
static bool valid_player(const CPUState* cpu, uint32_t player, BwGameScene* scene) {
    if (!span(cpu, player, kPlayerSpan) || (player & 3u) != 0 ||
        read32(cpu, player + kAcchPosition) != player + kPosition)
        return false;
    for (unsigned i = 0; i < 3; ++i) {
        scene->position[i] = read_float(cpu, player + kPosition + i * 4u);
        if (!isfinite(scene->position[i]) || fabsf(scene->position[i]) >= 1.0e32f)
            return false;
    }
    scene->player = player;
    scene->player_valid = true;
    return true;
}
static void baseline_facts(const CPUState* cpu) {
    g_facts_valid = span(cpu, kGameInfo, kSaveDataSpan);
    if (g_facts_valid)
        memcpy(g_facts, cpu->ram + (kGameInfo - 0x80000000u), sizeof g_facts);
}
static void refresh_scene(CPUState* cpu) {
    BwGameScene scene = {0}, next = {0};
    if (!span(cpu, kCurrentStage, 26) || !span(cpu, kPlayerPointer, 4) ||
        !span(cpu, kStayRoom, 1) || !span(cpu, kOverlapRequest, 4) ||
        !span(cpu, kEventMode, 1) || !span(cpu, kMenuPause, 1)) {
        leave_scene();
        memset(&g_scene, 0, sizeof g_scene);
        return;
    }
    const bool stage_valid = read_stage(cpu, kCurrentStage, &scene);
    scene.stay_room = (int8_t)read8(cpu, kStayRoom);
    scene.paused = read8(cpu, kMenuPause) != 0;
    scene.event_running = read8(cpu, kEventMode) != 0;
    scene.transitioning = read8(cpu, kNextStage + 12) != 0 ||
                          read32(cpu, kOverlapRequest) != 0;
    const bool player_valid = valid_player(cpu, read32(cpu, kPlayerPointer), &scene);
    scene.active = !g_loading && stage_valid && player_valid &&
                   scene.stay_room >= 0 && !scene.transitioning;
    scene.controls_ready = scene.active && !scene.paused && !scene.event_running &&
                           read16(cpu, scene.player + kDemoType) == 0 &&
                           read32(cpu, scene.player + kDemoMode) == 0;
    if (scene.transitioning && !g_transition) {
        leave_scene();
        clear_pending();
        ++g_stats.scene_generation;
        g_facts_valid = false;
        g_scene = scene;
        g_scene.player = 0;
        g_scene.player_valid = false;
        memset(g_scene.position, 0, sizeof g_scene.position);
        BwGameEvent event = event_new(BW_GAME_EVENT_TRANSITION_STARTED);
        if (read_stage(cpu, kNextStage, &next)) event.next_scene = next;
        emit(event);
    }
    g_transition = scene.transitioning;
    if (!scene.active) {
        leave_scene();
        scene.player = 0;
        scene.player_valid = false;
        memset(scene.position, 0, sizeof scene.position);
        g_scene = scene;
        return;
    }
    if (g_scene.active && !same_scene(&g_scene, &scene)) leave_scene();
    const bool entering = !g_scene.active;
    g_scene = scene;
    if (entering) {
        clear_pending();
        ++g_stats.scene_generation;
        baseline_facts(cpu);
        emit(event_new(BW_GAME_EVENT_SCENE_ENTERED));
    }
}
static void sample_facts(CPUState* cpu) {
    if (!g_scene.active || !span(cpu, kGameInfo, kSaveDataSpan)) return;
    if (!g_facts_valid) { baseline_facts(cpu); return; }
    for (unsigned range = 0; range < sizeof kFacts / sizeof kFacts[0]; ++range) {
        const FactRange* fact = &kFacts[range];
        for (uint16_t index = 0; index < fact->size; ++index) {
            const uint16_t offset = (uint16_t)(fact->offset + index);
            const uint8_t after = read8(cpu, kGameInfo + offset);
            const uint8_t before = g_facts[offset];
            if (after == before) continue;
            g_facts[offset] = after;
            BwGameEvent event = event_new(fact->event);
            event.fact = fact->fact;
            event.index = fact->fact == BW_GAME_FACT_SAVED_STAGE ?
                          (uint16_t)(offset - 0x380u) : index;
            event.before = before;
            event.after = after;
            emit(event);
        }
    }
}
static bool cpu_matches(CPUState* cpu) {
    if (cpu == NULL || cpu != g_cpu) return false;
    if (cpu->ram != g_ram || cpu->ram_size != g_ram_size)
        reset_internal(cpu, BW_GAME_RESET_MEMORY_REPLACED);
    return cpu->ram != NULL;
}
void bluewake_game_events_retrace(CPUState* cpu) {
    if (g_busy) return;
    g_busy = true;
    if (cpu_matches(cpu)) {
        ++g_stats.ticks;
        refresh_scene(cpu);
        sample_facts(cpu);
        for (unsigned i = 0; i < kPendingCalls; ++i) {
            if (!g_pending[i].active || g_stats.ticks - g_pending[i].tick <= kPendingMaxTicks)
                continue;
            if (g_pending[i].kind == CALL_LOAD) g_loading = false;
            g_pending[i].active = false;
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
            memset(&g_pending[i].inventory_proof, 0, sizeof g_pending[i].inventory_proof);
#endif
            ++g_stats.cancelled_calls;
        }
        emit(event_new(BW_GAME_EVENT_GAME_TICK));
    }
    g_busy = false;
}

static bool listed(uint32_t address, const uint32_t* addresses, unsigned count) {
    for (unsigned i = 0; i < count; ++i) if (addresses[i] == address) return true;
    return false;
}
#define LISTED(address, table) listed(address, table, (unsigned)(sizeof table / sizeof table[0]))

static bool item_entry(uint32_t address) {
    return address == kItemGet || address == kItemGetUncached;
}

bool bluewake_game_events_observes(uint32_t address) {
    if (g_cpu == NULL || g_mask == 0) return false;
    if ((address == kExecuteMethod && (g_mask & BW_GAME_EVENT_MASK(BW_GAME_EVENT_PLAYER_UPDATED))) ||
        (item_entry(address) && (g_mask & BW_GAME_EVENT_MASK(BW_GAME_EVENT_ITEM_AWARDED))) ||
        (address == kMemoryToCard && (g_mask & BW_GAME_EVENT_MASK(BW_GAME_EVENT_SAVE_SERIALIZED))) ||
        (address == kSaveSync && (g_mask & BW_GAME_EVENT_MASK(BW_GAME_EVENT_SAVE_COMPLETED))) ||
        address == kCardToMemory)
        return true;
    for (unsigned i = 0; i < kPendingCalls; ++i)
        if (g_pending[i].active && g_pending[i].return_address == address) return true;
    return false;
}
static bool arm(CPUState* cpu, CallKind kind, uint32_t player, uint8_t item, int8_t slot) {
    const uint32_t stack = cpu->gpr[1], target = cpu->lr;
    if ((stack & 3u) != 0 || !span(cpu, stack, 4)) return false;
    /* A resumed entry at an exhausted budget is the same invocation. Nested
     * invocations have different guest stacks; identical SP/LR is not nested. */
    for (unsigned i = 0; i < kPendingCalls; ++i)
        if (g_pending[i].active && g_pending[i].kind == kind &&
            g_pending[i].stack == stack && g_pending[i].return_address == target) {
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
            if (kind == CALL_PLAYER && bw_inventory_collector_completion_enabled() &&
                !bw_inventory_collector_call_replay(cpu, &g_pending[i].inventory_proof))
                memset(&g_pending[i].inventory_proof, 0, sizeof g_pending[i].inventory_proof);
#endif
            return true;
        }
    for (unsigned i = 0; i < kPendingCalls; ++i) {
        if (g_pending[i].active) continue;
        g_pending[i] = (PendingCall){.active=true,.kind=kind,.return_address=target,
            .stack=stack,.player=player,.item=item,.slot=slot,.token=++g_native_token,
            .epoch=g_stats.epoch,.generation=g_stats.scene_generation,.tick=g_stats.ticks};
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
        if (kind == CALL_PLAYER && bw_inventory_collector_completion_enabled())
            (void)bw_inventory_collector_call_begin(cpu, g_pending[i].token,
                g_pending[i].epoch, g_pending[i].generation, stack, player,
                &g_pending[i].inventory_proof);
#endif
        return true;
    }
    ++g_stats.pending_overflow;
    return false;
}
bool bluewake_game_events_arm_host_save(CPUState* cpu, uint32_t entry,
                                       int8_t slot, uint32_t scratch) {
    const bool serialize = entry == kMemoryToCard;
    const bool poll = entry == kSaveSync;
    const BwGameEventKind event = serialize ? BW_GAME_EVENT_SAVE_SERIALIZED : BW_GAME_EVENT_SAVE_COMPLETED;
    if (g_busy || (!serialize && !poll) || !(g_mask & BW_GAME_EVENT_MASK(event)) ||
        !cpu_matches(cpu) || cpu->exception || cpu->pc != entry || cpu->lr != 0x8180FFE0u ||
        slot < 0 || slot >= 3 || !span(cpu, scratch, 3u * kCardGameDataSize) || (scratch & 31u) ||
        !span(cpu, 0x800000D4u, 20) || read32(cpu, 0x800000D4u) != 0x803A2960u ||
        read32(cpu, 0x800000E4u) != 0x803A2960u ||
        !span(cpu, kGameInfo + 0x1290u, 1) || read8(cpu, kGameInfo + 0x1290u) != (uint8_t)slot ||
        (serialize && (cpu->gpr[3] != kGameInfo || cpu->gpr[4] != scratch || cpu->gpr[5] != (uint32_t)slot)) ||
        (poll && (cpu->gpr[3] != kCardControl || !span(cpu, kCardControl, kCardControlSpan))))
        return false;
    g_busy = true;
    refresh_scene(cpu);
    const CallKind kind = serialize ? CALL_SERIALIZE : CALL_SAVE;
    const bool armed = g_scene.active && arm(cpu, kind, 0, 0, slot);
    bool marked = false;
    if (armed) for (unsigned i = 0; i < kPendingCalls; ++i) {
        PendingCall* call = &g_pending[i];
        if (!call->active || call->kind != kind || call->stack != cpu->gpr[1] ||
            call->return_address != 0x8180FFE0u) continue;
        if (call->host_owned_save && (call->slot != slot || call->host_scratch != scratch)) break;
        call->host_owned_save = true; call->host_scratch = scratch; marked = true; break;
    }
    g_busy = false;
    return marked;
}
/* Source-bound Hr Wind's Requiem observer. This exact imported
 * call is cross-chunk even though demoProcTact0's outer caller is intrachunk.
 * Owner proof is mandatory and revalidated before emission. TALK and DEMO
 * are legitimate native event modes; controls_ready must remain false here. */
void bluewake_game_events_set_song_owner_query(BwSongOwnerQuery query, void* user) {
    if (g_busy) return;
    for (unsigned i = 0; i < kPendingCalls; ++i) {
        if (!g_pending[i].active || !g_pending[i].hr_song) continue;
        g_pending[i].active = false; ++g_stats.cancelled_calls;
    }
    g_song_owner_query = query; g_song_owner_user = user;
}
static bool hr_context(const CPUState* cpu, uint32_t actor, BwSongOwner* owner) {
    if (!g_song_owner_query || !g_scene.active || !g_scene.player_valid ||
        g_scene.paused || strcmp(g_scene.stage,"sea") != 0 || g_scene.stay_room != 13 ||
        (read8(cpu,kEventMode) != 1 && read8(cpu,kEventMode) != 2) ||
        !span(cpu,0x803CA8C8u,1) || read8(cpu,0x803CA8C8u) ||
        !span(cpu,actor,0x7C8) || (actor&3) ||
        read16(cpu,actor+8) != 0x16F || read16(cpu,actor+0xE) != 0x16F ||
        read32(cpu,actor+0x10) != 0xC0E96620u ||
        read32(cpu,actor+0xEC) != 0xC0E96600u ||
        read32(cpu,actor) == 0 || read32(cpu,actor) != read32(cpu,0x803F6A18u) ||
        read32(cpu,actor+0xC0) != read32(cpu,0x803F69D0u) ||
        read32(cpu,actor+0x14) || read8(cpu,actor+0x5C) ||
        read8(cpu,actor+0xB) || read8(cpu,actor+0x1BE) ||
        (read32(cpu,actor+0x1C8)&0xAu) != 8u ||
        (int8_t)read8(cpu,actor+0x20A) != 13 ||
        read8(cpu,actor+0x638) != 3 || read8(cpu,actor+0x63A) != 0 ||
        (read16(cpu,actor+0x608)&0x202u) != 2 ||
        (int32_t)read32(cpu,actor+0x640) < 0 ||
        read32(cpu,kItemFunctionTable+0x6Du*4) != 0x800C446Cu)
        return false;
    memset(owner,0,sizeof *owner);
    return g_song_owner_query(g_song_owner_user,cpu,owner) && owner->load_token != 0 &&
           owner->module_header != 0 && owner->loader_owner != 0 && owner->raw_text != 0;
}
static bool same_owner(const BwSongOwner* a,const BwSongOwner* b) {
    return a->load_token==b->load_token && a->module_header==b->module_header &&
           a->loader_owner==b->loader_owner && a->raw_text==b->raw_text;
}
static void arm_hr_song(CPUState* cpu) {
    if (cpu->lr != 0xC0E9116Cu || cpu->gpr[3] != 0x6D) return;
    BwSongOwner owner; const uint32_t actor = cpu->gpr[31];
    if (!hr_context(cpu,actor,&owner) || !arm(cpu,CALL_ITEM,g_scene.player,0x6D,-1))return;
    for (unsigned i=0;i<kPendingCalls;++i) {
        PendingCall* call=&g_pending[i];
        if (!call->active || call->kind!=CALL_ITEM || call->stack!=cpu->gpr[1] ||
            call->return_address!=0xC0E9116Cu) continue;
        const uint32_t id=read32(cpu,actor+4); const int32_t staff=(int32_t)read32(cpu,actor+0x640);
        /* A resumed entry cannot replace the actor/module proof of a pending
         * invocation with a newly reused actor or relinked module. */
        if(call->hr_song && (call->hr_actor!=actor || call->hr_id!=id ||
           call->hr_staff!=staff || !same_owner(&call->hr_owner,&owner))) {
            call->active=false;++g_stats.cancelled_calls;return;
        }
        call->hr_song=true;call->hr_actor=actor;call->hr_id=id;
        call->hr_staff=staff;call->hr_owner=owner;return;
    }
}

static void returned(CPUState* cpu, uint32_t address) {
    for (unsigned i = 0; i < kPendingCalls; ++i) {
        PendingCall* pending = &g_pending[i];
        if (!pending->active || pending->return_address != address ||
            pending->stack != cpu->gpr[1]) continue;
        // A foreign worker reaching this private token cannot consume the
        // main thread's pending native save. The host also checks its owner.
        if (pending->host_owned_save &&
            (!span(cpu, 0x800000D4u, 20) || read32(cpu, 0x800000D4u) != 0x803A2960u ||
             read32(cpu, 0x800000E4u) != 0x803A2960u ||
             !span(cpu, pending->host_scratch, 3u * kCardGameDataSize) ||
             !span(cpu, kGameInfo + 0x1290u, 1) ||
             read8(cpu, kGameInfo + 0x1290u) != (uint8_t)pending->slot)) continue;
        const PendingCall call = *pending;
        pending->active = false; /* Consume before callbacks or a budget replay. */
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
        memset(&pending->inventory_proof, 0, sizeof pending->inventory_proof);
#endif
        if (call.epoch != g_stats.epoch || call.generation != g_stats.scene_generation) {
            ++g_stats.cancelled_calls;
            continue;
        }
        if (call.hr_song) {
            BwSongOwner owner;
            if (cpu->gpr[31] != call.hr_actor || !hr_context(cpu,call.hr_actor,&owner) ||
                read32(cpu,call.hr_actor+4) != call.hr_id ||
                (int32_t)read32(cpu,call.hr_actor+0x640) != call.hr_staff ||
                !same_owner(&owner,&call.hr_owner) ||
                !(read8(cpu,kGameInfo+0xBD)&1u)) {
                ++g_stats.cancelled_calls;continue;
            }
        }
        const int32_t result = (int32_t)cpu->gpr[3];
        if (call.kind == CALL_LOAD) {
            g_loading = false;
            if (result == 0) reset_internal(cpu, BW_GAME_RESET_GAME_LOAD);
            else g_facts_valid = false;
            return;
        }
        if ((call.kind == CALL_PLAYER || call.kind == CALL_ITEM) &&
            (!g_scene.active || !g_scene.player_valid || g_scene.player != call.player ||
             (call.kind == CALL_PLAYER && cpu->gpr[30] != call.player))) {
            ++g_stats.cancelled_calls;
            continue;
        }
        if ((call.kind == CALL_SERIALIZE && result != 0) ||
            (call.kind == CALL_SAVE && result != 1)) continue;
        const BwGameEventKind kind = call.kind == CALL_PLAYER ? BW_GAME_EVENT_PLAYER_UPDATED :
                                     call.kind == CALL_ITEM ? BW_GAME_EVENT_ITEM_AWARDED :
                                     call.kind == CALL_SERIALIZE ? BW_GAME_EVENT_SAVE_SERIALIZED :
                                     BW_GAME_EVENT_SAVE_COMPLETED;
        BwGameEvent event = event_new(kind);
        event.native_call = call.token;
        event.source_address = address;
        event.item_id = call.item;
        event.save_slot = call.slot;
        event.result = result;
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
        if (call.kind == CALL_PLAYER && bw_inventory_collector_completion_enabled())
            bw_inventory_collector_emit_begin(cpu, &call.inventory_proof);
#endif
        emit(event);
#ifdef BW_NATIVE_INVENTORY_COLLECTOR
        if (call.kind == CALL_PLAYER) bw_inventory_collector_emit_end();
#endif
    }
}
void bluewake_game_events_dispatch(CPUState* cpu, uint32_t address) {
    if (g_busy || !bluewake_game_events_observes(address)) return;
    g_busy = true;
    if (!cpu_matches(cpu) || cpu->exception != 0) { g_busy = false; return; }
    /* Most ExecuteMethod calls belong to other actors. Reject them before
     * scanning a scene/facts snapshot. */
    if (address == kExecuteMethod && (!span(cpu, kPlayerPointer, 4) ||
        cpu->gpr[4] != read32(cpu, kPlayerPointer))) { g_busy = false; return; }
    refresh_scene(cpu);
    returned(cpu, address);
    if (address == kExecuteMethod && cpu->lr == kActorExecuteReturn &&
        g_scene.active && cpu->gpr[4] == g_scene.player &&
        span(cpu, cpu->gpr[3], 12) && read32(cpu, cpu->gpr[3] + 8) == kPlayerExecute) {
        arm(cpu, CALL_PLAYER, g_scene.player, 0, -1);
    } else if (item_entry(address) && cpu->lr == 0xC0E9116Cu) {
        arm_hr_song(cpu);
    } else if (item_entry(address) && LISTED(cpu->lr, kItemReturns) &&
               g_scene.active && cpu->gpr[3] <= 0xFFu) {
        const uint32_t item_cell = kItemFunctionTable + cpu->gpr[3] * 4u;
        if (span(cpu, item_cell, 4)) {
            const uint32_t function = read32(cpu, item_cell);
            /* Retail item_func_noentry is a genuine no-op, including ID FF;
             * a native invocation of it is not an awarded item. */
            if (function != kItemNoEntry && (function & 3u) == 0 && span(cpu, function, 4))
                arm(cpu, CALL_ITEM, g_scene.player, (uint8_t)cpu->gpr[3], -1);
        }
    } else if ((address == kMemoryToCard || address == kCardToMemory) &&
               cpu->gpr[3] == kGameInfo && cpu->gpr[5] < 3 &&
               span(cpu, cpu->gpr[4], (cpu->gpr[5] + 1u) * kCardGameDataSize)) {
        if (address == kMemoryToCard && LISTED(cpu->lr, kSerializeReturns))
            arm(cpu, CALL_SERIALIZE, 0, 0, (int8_t)cpu->gpr[5]);
        else if (address == kCardToMemory && cpu->lr == 0x80231B08u) {
            /* Freeze facts while native deserialization is in flight. */
            leave_scene();
            clear_pending();
            g_loading = true;
            if (!arm(cpu, CALL_LOAD, 0, 0, (int8_t)cpu->gpr[5])) g_loading = false;
        }
    } else if (address == kSaveSync && LISTED(cpu->lr, kSaveReturns) &&
               cpu->gpr[3] == kCardControl && span(cpu, kCardControl, kCardControlSpan)) {
        arm(cpu, CALL_SAVE, 0, 0, -1);
    }
    g_busy = false;
}
bool bluewake_game_events_scene(BwGameScene* scene, uint64_t* epoch, uint64_t* generation) {
    if (scene != NULL) *scene = g_scene;
    if (epoch != NULL) *epoch = g_stats.epoch;
    if (generation != NULL) *generation = g_stats.scene_generation;
    return g_scene.active;
}
void bluewake_game_events_stats(BwGameEventStats* stats) {
    if (stats != NULL) *stats = g_stats;
}
