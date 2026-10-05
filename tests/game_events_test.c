/* Real observer code against isolated big-endian guest RAM. No game module,
 * UI, physical input, graphics, CARD files or personal saves are involved.
 * These fixtures verify event semantics; optimized reachability additionally
 * needs the private translated call audit and a traced game run. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game_events.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RAM_SIZE = 24 * 1024 * 1024 };
static const uint32_t INFO = 0x803C4C08u;
static const uint32_t PLAYER = 0x80010000u;
static const uint32_t OTHER_PLAYER = 0x80020000u;
static const uint32_t PLAYER_SLOT = 0x803CA74Cu;
static const uint32_t STAGE = 0x803C9D3Cu;
static const uint32_t NEXT_STAGE = 0x803C9D48u;
static const uint32_t ROOM = 0x803F6A78u;
static const uint32_t OVERLAP = 0x803F6160u;
static const uint32_t PAUSE = 0x803F7097u;
static const uint32_t EVENT_MODE = 0x803C9EA2u;
static const uint32_t METHODS = 0x80008000u;
static const uint32_t STACK = 0x80050000u;
static const uint32_t CARD_BUFFER = 0x80060000u;
static const uint32_t CARD_CONTROL = 0x803B39A0u;
static const uint32_t EXECUTE_METHOD = 0x8003EF38u;
static const uint32_t EXECUTE_RETURN = 0x80023960u;
static const uint32_t ITEM_GET = 0x800C2DFCu;
static const uint32_t ITEM_GET_UNCACHED = 0xC00C2DFCu;
static const uint32_t ITEM_RETURN = 0x800F64B8u;
static const uint32_t DEMO_ITEM_RETURN = 0xC0770A08u;
static const uint32_t SERIALIZE = 0x8005E780u;
static const uint32_t SERIALIZE_RETURN = 0x801D8994u;
static const uint32_t SAVE_SYNC = 0x8001931Cu;
static const uint32_t SAVE_RETURN = 0x8018D674u;
static const uint32_t DESERIALIZE = 0x8005EA24u;
static const uint32_t DESERIALIZE_RETURN = 0x80231B08u;
static CPUState cpu;
static BwGameEvent events[4096];
static unsigned event_count;
static BwGameEventSubscription observer;

static void put8(uint32_t address, uint8_t value) {
    assert(address >= 0x80000000u && address - 0x80000000u < cpu.ram_size);
    cpu.ram[address - 0x80000000u] = value;
}
static void put32(uint32_t address, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) put8(address + i, (uint8_t)(value >> ((3u - i) * 8u)));
}
static void put_float(uint32_t address, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    put32(address, bits);
}
static void record(const BwGameEvent* event, void* user) {
    (void)user;
    assert(event_count < sizeof events / sizeof events[0]);
    if (event_count != 0) assert(event->sequence > events[event_count - 1].sequence);
    events[event_count++] = *event;
    if (!event->scene.active || !event->scene.player_valid) {
        assert(event->scene.player == 0);
        for (unsigned i = 0; i < 3; ++i) assert(event->scene.position[i] == 0);
    }
}
static unsigned count(BwGameEventKind kind) {
    unsigned n = 0;
    for (unsigned i = 0; i < event_count; ++i) if (events[i].kind == kind) ++n;
    return n;
}
static BwGameEvent last(BwGameEventKind kind) {
    for (unsigned i = event_count; i != 0; --i) if (events[i - 1].kind == kind) return events[i - 1];
    assert(!"expected event missing");
    return events[0];
}
static void tick(void) { bluewake_game_events_retrace(&cpu); }
static BwGameScene scene(void) {
    BwGameScene result;
    bluewake_game_events_scene(&result, NULL, NULL);
    return result;
}
static void stage_name(uint32_t address, const char* name) {
    for (unsigned i = 0; i < 8; ++i) put8(address + i, 0xCC);
    for (unsigned i = 0; i <= strlen(name); ++i) put8(address + i, (uint8_t)name[i]);
}
static void actor(uint32_t player) {
    put32(PLAYER_SLOT, player);
    put32(player + 0x498u, player + 0x1F8u);
    put_float(player + 0x1F8u, 12.0f);
    put_float(player + 0x1FCu, 200.0f);
    put_float(player + 0x200u, -44.0f);
}
static void fixture(void) {
    memset(cpu.ram, 0, cpu.ram_size);
    memset(cpu.gpr, 0, sizeof cpu.gpr);
    cpu.pc = 0x80003100u;
    cpu.lr = 0;
    cpu.exception = 0;
    cpu.downcount = -123;
    cpu.gpr[1] = STACK;
    stage_name(STAGE, "sea");
    put8(STAGE + 10, 44);
    put8(STAGE + 11, 0);
    put8(ROOM, 44);
    actor(PLAYER);
    put32(METHODS + 8, 0x80122D30u);
    for (unsigned i = 0; i < 256; ++i) put32(0x803888C8u + i * 4u, 0x800C6374u);
    /* Synthetic implemented handlers; the observer excludes no-entry cells. */
    put32(0x803888C8u + 0x06u * 4u, 0x800C2F10u);
    put32(0x803888C8u + 0x25u * 4u, 0x800C2E7Cu);
    put32(0x803888C8u + 0x27u * 4u, 0x800C2E98u);
    bluewake_game_events_attach(&cpu);
    tick();
    assert(scene().active && scene().player_valid && scene().controls_ready);
    assert(strcmp(scene().stage, "sea") == 0);
    event_count = 0;
}
static void edge(uint32_t address) {
    cpu.pc = address;
    CPUState before = cpu;
    bluewake_game_events_dispatch(&cpu, address);
    /* No register/PC/budget/CPU bookkeeping mutations are allowed. */
    assert(memcmp(&cpu, &before, sizeof cpu) == 0);
}
static void player_start(void) {
    cpu.gpr[1] = STACK;
    cpu.gpr[3] = METHODS;
    cpu.gpr[4] = PLAYER;
    cpu.lr = EXECUTE_RETURN;
    edge(EXECUTE_METHOD);
}
static void item_start_at(uint32_t address, uint32_t return_address,
                          uint32_t stack, uint32_t item) {
    cpu.gpr[1] = stack;
    cpu.gpr[3] = item;
    cpu.lr = return_address;
    edge(address);
}
static void item_start(uint32_t stack, uint8_t item) {
    item_start_at(ITEM_GET, ITEM_RETURN, stack, item);
}
static void save_start(void) {
    cpu.gpr[1] = STACK;
    cpu.gpr[3] = CARD_CONTROL;
    cpu.lr = SAVE_RETURN;
    edge(SAVE_SYNC);
}
static void card_start(uint32_t address, uint32_t return_address) {
    cpu.gpr[1] = STACK;
    cpu.gpr[3] = INFO;
    cpu.gpr[4] = CARD_BUFFER;
    cpu.gpr[5] = 1;
    cpu.lr = return_address;
    edge(address);
}

static void test_lifecycle(void) {
    fixture();
    uint64_t epoch, generation;
    assert(bluewake_game_events_scene(NULL, &epoch, &generation));
    for (unsigned i = 0; i < 200; ++i) tick();
    assert(count(BW_GAME_EVENT_GAME_TICK) == 200);
    assert(count(BW_GAME_EVENT_PLAYER_UPDATED) == 0);
    assert(count(BW_GAME_EVENT_SCENE_ENTERED) == 0);
    put8(PAUSE, 1); tick(); assert(scene().active && !scene().controls_ready);
    put8(PAUSE, 0); put8(EVENT_MODE, 1); tick(); assert(!scene().controls_ready);
    put8(EVENT_MODE, 0); put8(PLAYER + 0x305u, 1); tick(); assert(!scene().controls_ready);
    put8(PLAYER + 0x305u, 0); tick(); assert(scene().controls_ready);
    event_count = 0;
    stage_name(NEXT_STAGE, "sea"); put8(NEXT_STAGE + 10, 45); put8(NEXT_STAGE + 12, 1);
    tick();
    assert(count(BW_GAME_EVENT_SCENE_LEAVING) == 1);
    assert(count(BW_GAME_EVENT_TRANSITION_STARTED) == 1);
    assert(last(BW_GAME_EVENT_TRANSITION_STARTED).next_scene.room == 45);
    assert(!scene().active && scene().player == 0);
    for (unsigned i = 0; i < 3; ++i) tick();
    assert(count(BW_GAME_EVENT_TRANSITION_STARTED) == 1);
    put8(NEXT_STAGE + 12, 0); put32(OVERLAP, 0x80007000u); tick();
    assert(count(BW_GAME_EVENT_TRANSITION_STARTED) == 1);
    put32(OVERLAP, 0); tick();
    assert(count(BW_GAME_EVENT_SCENE_ENTERED) == 1);
    assert(last(BW_GAME_EVENT_SCENE_ENTERED).scene_generation > generation);
    event_count = 0;
    put8(ROOM, 45); tick();
    assert(count(BW_GAME_EVENT_SCENE_LEAVING) == 1 && count(BW_GAME_EVENT_SCENE_ENTERED) == 1);
    assert(scene().room == 44 && scene().stay_room == 45);
    event_count = 0;
    actor(OTHER_PLAYER); tick();
    assert(count(BW_GAME_EVENT_SCENE_LEAVING) == 1 && count(BW_GAME_EVENT_SCENE_ENTERED) == 1);
    assert(scene().player == OTHER_PLAYER);
    event_count = 0;
    put_float(OTHER_PLAYER + 0x1F8, NAN); tick();
    assert(count(BW_GAME_EVENT_SCENE_LEAVING) == 1 && !scene().active);
    tick(); assert(count(BW_GAME_EVENT_SCENE_LEAVING) == 1);
    actor(OTHER_PLAYER); tick(); assert(scene().active);
    put32(PLAYER_SLOT, 0x817FFFFCu); tick(); assert(!scene().active);
    actor(OTHER_PLAYER); put32(OTHER_PLAYER + 0x498, PLAYER + 0x1F8); tick();
    assert(!scene().active);
    actor(PLAYER); stage_name(STAGE, "bad name"); tick(); assert(!scene().active);
    stage_name(STAGE, "sea"); tick(); assert(scene().active);
    event_count = 0;
    put8(INFO + 0x624, 0x44);
    bluewake_game_events_reset(&cpu, BW_GAME_RESET_STATE_LOAD);
    assert(count(BW_GAME_EVENT_RESET) == 1 && !scene().active);
    assert(last(BW_GAME_EVENT_RESET).epoch > epoch);
    tick();
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 0 && count(BW_GAME_EVENT_PROGRESSION_CHANGED) == 0);
}

static void test_facts(void) {
    fixture();
    put8(INFO + 0x03C, 0x25); put8(INFO + 0x051, 1);
    put8(INFO + 0x066, 12); put8(INFO + 0x624, 0x40);
    put8(INFO + 0x090, 0x20); put8(INFO + 0x414, 2); put8(INFO + 0x77C, 4);
    /* Alignment/config/temporary dungeon state must never look like progress. */
    put8(INFO + 0x08E, 7); put8(INFO + 0x0C1, 7);
    put8(INFO + 0x3A2, 7); put8(INFO + 0x1A4, 7); put8(INFO + 0x79C, 7);
    tick();
    assert(count(BW_GAME_EVENT_INVENTORY_CHANGED) == 4);
    assert(count(BW_GAME_EVENT_PROGRESSION_CHANGED) == 3);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 0);
    bool saved = false, current = false, story = false;
    for (unsigned i = 0; i < event_count; ++i) {
        if (events[i].kind != BW_GAME_EVENT_PROGRESSION_CHANGED) continue;
        if (events[i].fact == BW_GAME_FACT_SAVED_STAGE)
            saved = events[i].index == 0x94 && events[i].after == 2;
        if (events[i].fact == BW_GAME_FACT_CURRENT_STAGE)
            current = events[i].index == 4 && events[i].after == 4;
        if (events[i].fact == BW_GAME_FACT_EVENT)
            story = events[i].index == 0 && events[i].after == 0x40;
    }
    assert(saved && current && story);
    event_count = 0;
    tick(); assert(count(BW_GAME_EVENT_INVENTORY_CHANGED) == 0 && count(BW_GAME_EVENT_PROGRESSION_CHANGED) == 0);
    put8(INFO + 0x066, 11); put8(INFO + 0x624, 0); tick();
    assert(last(BW_GAME_EVENT_INVENTORY_CHANGED).before == 12);
    assert(last(BW_GAME_EVENT_PROGRESSION_CHANGED).before == 0x40);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 0);
    event_count = 0;
    for (unsigned i = 0; i < 16; ++i) put8(INFO + 0x380 + i * 0x24 + 0x22, 0xAA);
    tick(); assert(count(BW_GAME_EVENT_PROGRESSION_CHANGED) == 0);
    put8(INFO + 0x618, 0x40); tick();
    assert(last(BW_GAME_EVENT_PROGRESSION_CHANGED).fact == BW_GAME_FACT_OCEAN);
}

static void test_player_updates(void) {
    fixture();
    assert(bluewake_game_events_observes(EXECUTE_METHOD));
    assert(!bluewake_game_events_observes(0x80122D30u));
    assert(!bluewake_game_events_observes(0x80122D40u));
    player_start(); player_start();
    assert(bluewake_game_events_observes(EXECUTE_RETURN));
    cpu.gpr[1] = STACK + 16; cpu.gpr[3] = 1; cpu.gpr[30] = PLAYER;
    edge(EXECUTE_RETURN); assert(count(BW_GAME_EVENT_PLAYER_UPDATED) == 0);
    cpu.gpr[1] = STACK; edge(EXECUTE_RETURN);
    assert(count(BW_GAME_EVENT_PLAYER_UPDATED) == 1);
    const BwGameEvent first = last(BW_GAME_EVENT_PLAYER_UPDATED);
    assert(first.source_address == EXECUTE_RETURN && first.native_call != 0);
    assert(first.scene.player == PLAYER && first.result == 1);
    edge(EXECUTE_RETURN); assert(count(BW_GAME_EVENT_PLAYER_UPDATED) == 1);
    assert(!bluewake_game_events_observes(EXECUTE_RETURN));
    player_start(); cpu.gpr[3] = 0; cpu.gpr[30] = PLAYER; edge(EXECUTE_RETURN);
    assert(count(BW_GAME_EVENT_PLAYER_UPDATED) == 2);
    assert(last(BW_GAME_EVENT_PLAYER_UPDATED).native_call != first.native_call);
    put8(PLAYER + 0x305, 1); player_start(); cpu.gpr[3] = 1; edge(EXECUTE_RETURN);
    assert(count(BW_GAME_EVENT_PLAYER_UPDATED) == 3);
    assert(!last(BW_GAME_EVENT_PLAYER_UPDATED).scene.controls_ready);
    put32(METHODS + 8, 0x80100000u); player_start();
    assert(!bluewake_game_events_observes(EXECUTE_RETURN));
    put32(METHODS + 8, 0x80122D30u); player_start();
    put8(ROOM, 45); tick(); cpu.gpr[3] = 1; edge(EXECUTE_RETURN);
    assert(count(BW_GAME_EVENT_PLAYER_UPDATED) == 3);
}

static void test_awards(void) {
    fixture();
    item_start(STACK, 0x25); item_start(STACK, 0x25);
    assert(bluewake_game_events_observes(ITEM_RETURN));
    assert(!bluewake_game_events_observes(0x800C2E20u));
    item_start(STACK - 64, 0x27);
    cpu.gpr[3] = 0; edge(ITEM_RETURN);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 1);
    assert(last(BW_GAME_EVENT_ITEM_AWARDED).item_id == 0x27);
    cpu.gpr[1] = STACK; edge(ITEM_RETURN);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 2);
    assert(last(BW_GAME_EVENT_ITEM_AWARDED).item_id == 0x25);
    edge(ITEM_RETURN); assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 2);
    item_start(STACK, 0xFF);
    assert(!bluewake_game_events_observes(ITEM_RETURN));
    /* Unknown/private REL return sites require an audit before enabling. */
    cpu.gpr[3] = 0x25; cpu.lr = 0x80500004u; edge(ITEM_GET);
    assert(!bluewake_game_events_observes(cpu.lr));
    item_start(STACK, 0x25); bluewake_game_events_reset(&cpu, BW_GAME_RESET_MODULE_RELOAD);
    cpu.gpr[3] = 0; edge(ITEM_RETURN);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 2);
    tick(); item_start(STACK, 0x25);
    for (unsigned i = 0; i < 601; ++i) tick();
    assert(!bluewake_game_events_observes(ITEM_RETURN));
    edge(ITEM_RETURN); assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 2);
    BwGameEventStats before, after;
    bluewake_game_events_stats(&before);
    for (unsigned i = 0; i < 17; ++i) item_start(STACK - i * 64, 0x25);
    bluewake_game_events_stats(&after);
    assert(after.pending_overflow == before.pending_overflow + 1);
    bluewake_game_events_reset(&cpu, BW_GAME_RESET_MACHINE_RESET);
    assert(!bluewake_game_events_observes(ITEM_RETURN));
}

static void test_rel_awards(void) {
    fixture();
    assert(bluewake_game_events_observes(ITEM_GET_UNCACHED));
    assert(!bluewake_game_events_observes(ITEM_GET_UNCACHED + 4));
    assert(!bluewake_game_events_observes(DEMO_ITEM_RETURN));
    edge(DEMO_ITEM_RETURN);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 0);

    /* Only the audited linked return is accepted. A different module's LR,
     * adjacent instruction, cached-looking REL PC or raw load address is not. */
    const uint32_t unknown_returns[] = {
        0xC0770A04u, 0xC0770A0Cu, 0xC0780A08u, 0x80770A08u, 0x81832EE8u
    };
    for (unsigned i = 0; i < sizeof unknown_returns / sizeof unknown_returns[0]; ++i) {
        item_start_at(ITEM_GET_UNCACHED, unknown_returns[i], STACK, 0x06);
        assert(!bluewake_game_events_observes(unknown_returns[i]));
        edge(unknown_returns[i]);
    }
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 0);
    item_start_at(ITEM_GET_UNCACHED, DEMO_ITEM_RETURN, STACK, 0xFF);
    item_start_at(ITEM_GET_UNCACHED, DEMO_ITEM_RETURN, STACK, 0x100);
    item_start_at(ITEM_GET_UNCACHED, DEMO_ITEM_RETURN, STACK + 1, 0x06);
    assert(!bluewake_game_events_observes(DEMO_ITEM_RETURN));

    uint8_t* snapshot = malloc(cpu.ram_size);
    assert(snapshot != NULL);
    memcpy(snapshot, cpu.ram, cpu.ram_size);
    item_start_at(ITEM_GET_UNCACHED, DEMO_ITEM_RETURN, STACK, 0x06);
    item_start_at(ITEM_GET_UNCACHED, DEMO_ITEM_RETURN, STACK, 0x06);
    /* A budget replay after DOL aliasing is the same SP/LR invocation. */
    item_start_at(ITEM_GET, DEMO_ITEM_RETURN, STACK, 0x06);
    assert(bluewake_game_events_observes(DEMO_ITEM_RETURN));
    item_start_at(ITEM_GET_UNCACHED, DEMO_ITEM_RETURN, STACK - 64, 0x27);
    cpu.gpr[1] = STACK - 60; edge(DEMO_ITEM_RETURN);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 0);
    /* execItemGet is void. r3 is copied unchanged, never treated as status. */
    cpu.gpr[1] = STACK - 64; cpu.gpr[3] = (uint32_t)-7; edge(DEMO_ITEM_RETURN);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 1);
    const BwGameEvent nested = last(BW_GAME_EVENT_ITEM_AWARDED);
    assert(nested.item_id == 0x27 && nested.result == -7);
    cpu.gpr[1] = STACK; cpu.gpr[3] = 0x803C4C08u; edge(DEMO_ITEM_RETURN);
    const BwGameEvent chest = last(BW_GAME_EVENT_ITEM_AWARDED);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 2);
    assert(chest.item_id == 0x06 && chest.source_address == DEMO_ITEM_RETURN);
    assert(chest.native_call != 0 && chest.native_call != nested.native_call);
    assert(chest.scene.player == PLAYER && chest.scene.active);
    assert((uint32_t)chest.result == 0x803C4C08u);
    edge(DEMO_ITEM_RETURN);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 2);
    assert(!bluewake_game_events_observes(DEMO_ITEM_RETURN));
    tick();
    assert(memcmp(snapshot, cpu.ram, cpu.ram_size) == 0);
    free(snapshot);

    item_start_at(ITEM_GET_UNCACHED, DEMO_ITEM_RETURN, STACK, 0x06);
    put8(NEXT_STAGE + 12, 1); tick();
    assert(!bluewake_game_events_observes(DEMO_ITEM_RETURN));
    cpu.gpr[3] = 0; edge(DEMO_ITEM_RETURN);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 2);
    put8(NEXT_STAGE + 12, 0); tick();
    item_start_at(ITEM_GET_UNCACHED, DEMO_ITEM_RETURN, STACK, 0x06);
    bluewake_game_events_reset(&cpu, BW_GAME_RESET_MODULE_RELOAD);
    assert(!bluewake_game_events_observes(DEMO_ITEM_RETURN));
    cpu.gpr[3] = 0; edge(DEMO_ITEM_RETURN);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 2);
    tick();
    item_start_at(ITEM_GET_UNCACHED, DEMO_ITEM_RETURN, STACK, 0x06);
    for (unsigned i = 0; i < 601; ++i) tick();
    assert(!bluewake_game_events_observes(DEMO_ITEM_RETURN));
    cpu.gpr[3] = 0; edge(DEMO_ITEM_RETURN);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 2);
}

static void test_saves_and_reload(void) {
    fixture();
    card_start(SERIALIZE, SERIALIZE_RETURN); cpu.gpr[3] = (uint32_t)-1; edge(SERIALIZE_RETURN);
    assert(count(BW_GAME_EVENT_SAVE_SERIALIZED) == 0);
    card_start(SERIALIZE, SERIALIZE_RETURN); cpu.gpr[3] = 0; edge(SERIALIZE_RETURN);
    assert(count(BW_GAME_EVENT_SAVE_SERIALIZED) == 1);
    assert(last(BW_GAME_EVENT_SAVE_SERIALIZED).save_slot == 1);
    assert(count(BW_GAME_EVENT_SAVE_COMPLETED) == 0);
    edge(SERIALIZE_RETURN); assert(count(BW_GAME_EVENT_SAVE_SERIALIZED) == 1);
    const int32_t results[] = {0, 2, -5, 1};
    for (unsigned i = 0; i < sizeof results / sizeof results[0]; ++i) {
        save_start(); save_start(); cpu.gpr[3] = (uint32_t)results[i]; edge(SAVE_RETURN);
    }
    assert(count(BW_GAME_EVENT_SAVE_COMPLETED) == 1);
    assert(last(BW_GAME_EVENT_SAVE_COMPLETED).result == 1);
    assert(last(BW_GAME_EVENT_SAVE_COMPLETED).save_slot == -1);
    assert(strcmp(bluewake_game_event_name(BW_GAME_EVENT_SAVE_COMPLETED), "GuestSaveCompleted") == 0);
    edge(SAVE_RETURN); assert(count(BW_GAME_EVENT_SAVE_COMPLETED) == 1);
    card_start(DESERIALIZE, DESERIALIZE_RETURN);
    assert(!scene().active);
    const unsigned resets = count(BW_GAME_EVENT_RESET);
    put8(INFO + 0x03C, 0x2F); put8(INFO + 0x624, 0x80); tick();
    assert(count(BW_GAME_EVENT_INVENTORY_CHANGED) == 0 && count(BW_GAME_EVENT_PROGRESSION_CHANGED) == 0);
    cpu.gpr[1] = STACK; cpu.gpr[3] = 0; edge(DESERIALIZE_RETURN);
    assert(count(BW_GAME_EVENT_RESET) == resets + 1);
    assert(last(BW_GAME_EVENT_RESET).reset_reason == BW_GAME_RESET_GAME_LOAD);
    tick(); assert(scene().active);
    assert(count(BW_GAME_EVENT_ITEM_AWARDED) == 0 && count(BW_GAME_EVENT_PROGRESSION_CHANGED) == 0);
    assert(strcmp(bluewake_game_event_name(BW_GAME_EVENT_SAVE_SERIALIZED), "SaveSerialized") == 0);
}

static void test_memory_and_read_only(void) {
    fixture();
    uint8_t* snapshot = malloc(cpu.ram_size);
    assert(snapshot != NULL);
    memcpy(snapshot, cpu.ram, cpu.ram_size);
    player_start(); cpu.gpr[3] = 1; cpu.gpr[30] = PLAYER; edge(EXECUTE_RETURN); tick();
    assert(memcmp(snapshot, cpu.ram, cpu.ram_size) == 0);
    free(snapshot);
    item_start(STACK, 0x25);
    const uint64_t epoch = last(BW_GAME_EVENT_GAME_TICK).epoch;
    uint8_t* original = cpu.ram;
    cpu.ram = malloc(RAM_SIZE);
    assert(cpu.ram != NULL); memcpy(cpu.ram, original, RAM_SIZE);
    tick();
    assert(last(BW_GAME_EVENT_RESET).reset_reason == BW_GAME_RESET_MEMORY_REPLACED);
    assert(last(BW_GAME_EVENT_RESET).epoch > epoch);
    assert(!bluewake_game_events_observes(ITEM_RETURN));
    free(original);
    cpu.ram_size = 1; tick(); assert(!scene().active);
    cpu.ram_size = RAM_SIZE; tick(); assert(scene().active);
    bluewake_game_events_reset(NULL, BW_GAME_RESET_MACHINE_RESET);
    assert(!scene().active && !bluewake_game_events_observes(EXECUTE_METHOD));
    tick(); assert(!scene().active);
}

static void test_host_owned_native_saves(void) {
    fixture();
    const uint32_t token = 0x8180FFE0u, main_thread = 0x803A2960u;
    put8(INFO + 0x1290u, 1);
    put32(0x800000D4u, main_thread); put32(0x800000E4u, main_thread);
    card_start(SERIALIZE, token);
    assert(!bluewake_game_events_observes(token)); // Token is not a general whitelist.
    const CPUState before = cpu;
    assert(!bluewake_game_events_arm_host_save(&cpu, ITEM_GET, 1, CARD_BUFFER));
    assert(!bluewake_game_events_arm_host_save(&cpu, SERIALIZE, 3, CARD_BUFFER));
    assert(!bluewake_game_events_arm_host_save(&cpu, SERIALIZE, 1, CARD_BUFFER + 32));
    put32(0x800000E4u, main_thread + 0x1000);
    assert(!bluewake_game_events_arm_host_save(&cpu, SERIALIZE, 1, CARD_BUFFER));
    put32(0x800000E4u, main_thread);
    assert(bluewake_game_events_arm_host_save(&cpu, SERIALIZE, 1, CARD_BUFFER));
    assert(bluewake_game_events_arm_host_save(&cpu, SERIALIZE, 1, CARD_BUFFER));
    assert(memcmp(&cpu, &before, sizeof cpu) == 0);
    cpu.gpr[3] = 0;
    put32(0x800000D4u, main_thread + 0x1000); edge(token);
    assert(count(BW_GAME_EVENT_SAVE_SERIALIZED) == 0 && bluewake_game_events_observes(token));
    put32(0x800000D4u, main_thread); edge(token); edge(token);
    assert(count(BW_GAME_EVENT_SAVE_SERIALIZED) == 1);
    assert(last(BW_GAME_EVENT_SAVE_SERIALIZED).save_slot == 1);

    cpu.pc = SAVE_SYNC; cpu.lr = token; cpu.gpr[3] = CARD_CONTROL;
    assert(bluewake_game_events_arm_host_save(&cpu, SAVE_SYNC, 1, CARD_BUFFER));
    cpu.gpr[3] = 0; edge(token);
    assert(count(BW_GAME_EVENT_SAVE_COMPLETED) == 0);
    cpu.pc = SAVE_SYNC; cpu.gpr[3] = CARD_CONTROL;
    assert(bluewake_game_events_arm_host_save(&cpu, SAVE_SYNC, 1, CARD_BUFFER));
    cpu.gpr[3] = 1; edge(token); edge(token);
    assert(count(BW_GAME_EVENT_SAVE_COMPLETED) == 1);
    assert(last(BW_GAME_EVENT_SAVE_COMPLETED).save_slot == 1);

    cpu.pc = SAVE_SYNC; cpu.gpr[3] = CARD_CONTROL;
    assert(bluewake_game_events_arm_host_save(&cpu, SAVE_SYNC, 1, CARD_BUFFER));
    bluewake_game_events_reset(&cpu, BW_GAME_RESET_STATE_LOAD);
    cpu.gpr[3] = 1; edge(token);
    assert(count(BW_GAME_EVENT_SAVE_COMPLETED) == 1 && !bluewake_game_events_observes(token));
}

static BwGameEventSubscription remove_target;
static unsigned remover_calls, removed_calls;
static void remove_other(const BwGameEvent* event, void* user) {
    (void)event; (void)user;
    ++remover_calls;
    bluewake_game_events_unsubscribe(remove_target);
}
static void removed(const BwGameEvent* event, void* user) {
    (void)event; (void)user; ++removed_calls;
}
static void test_subscriptions(void) {
    assert(bluewake_game_events_unsubscribe(observer)); observer = 0;
    fixture();
    assert(!bluewake_game_events_observes(EXECUTE_METHOD));
    assert(!bluewake_game_events_observes(ITEM_GET));
    assert(!bluewake_game_events_observes(ITEM_GET_UNCACHED));
    assert(!bluewake_game_events_observes(DEMO_ITEM_RETURN));
    assert(bluewake_game_events_subscribe(0, record, NULL) == 0);
    assert(bluewake_game_events_subscribe(UINT64_MAX, record, NULL) == 0);
    assert(bluewake_game_events_subscribe(BW_GAME_EVENT_ALL, NULL, NULL) == 0);
    const BwGameEventSubscription remover = bluewake_game_events_subscribe(
        BW_GAME_EVENT_MASK(BW_GAME_EVENT_GAME_TICK), remove_other, NULL);
    remove_target = bluewake_game_events_subscribe(
        BW_GAME_EVENT_MASK(BW_GAME_EVENT_GAME_TICK), removed, NULL);
    assert(remover != 0 && remove_target != 0);
    tick(); assert(remover_calls == 1 && removed_calls == 0);
    assert(!bluewake_game_events_unsubscribe(remove_target));
    assert(bluewake_game_events_unsubscribe(remover));
    BwGameEventSubscription subscriptions[16];
    for (unsigned i = 0; i < 16; ++i) {
        subscriptions[i] = bluewake_game_events_subscribe(BW_GAME_EVENT_ALL, record, NULL);
        assert(subscriptions[i] != 0);
    }
    assert(bluewake_game_events_subscribe(BW_GAME_EVENT_ALL, record, NULL) == 0);
    item_start_at(ITEM_GET_UNCACHED, DEMO_ITEM_RETURN, STACK, 0x06);
    for (unsigned i = 0; i < 16; ++i) assert(bluewake_game_events_unsubscribe(subscriptions[i]));
    assert(!bluewake_game_events_observes(ITEM_RETURN));
    assert(!bluewake_game_events_observes(DEMO_ITEM_RETURN));
    observer = bluewake_game_events_subscribe(BW_GAME_EVENT_ALL, record, NULL);
    assert(observer != 0);
    assert(!bluewake_game_events_observes(ITEM_RETURN));
    assert(!bluewake_game_events_observes(DEMO_ITEM_RETURN));
}

int main(void) {
#if defined(_WIN32)
    assert(_putenv_s("BLUEWAKE_GAME_EVENTS_TRACE", "") == 0);
#else
    assert(unsetenv("BLUEWAKE_GAME_EVENTS_TRACE") == 0);
#endif
    cpu.ram = calloc(1, RAM_SIZE); cpu.ram_size = RAM_SIZE;
    assert(cpu.ram != NULL);
    observer = bluewake_game_events_subscribe(BW_GAME_EVENT_ALL, record, NULL);
    assert(observer != 0);
    test_lifecycle();
    test_facts();
    test_player_updates();
    test_awards();
    test_rel_awards();
    test_saves_and_reload();
    test_host_owned_native_saves();
    test_memory_and_read_only();
    test_subscriptions();
    assert(strcmp(bluewake_game_event_name((BwGameEventKind)999), "Unknown") == 0);
    assert(bluewake_game_events_unsubscribe(observer));
    bluewake_game_events_reset(NULL, BW_GAME_RESET_MACHINE_RESET);
    free(cpu.ram);
    puts("game_events_test: passed");
    return 0;
}
