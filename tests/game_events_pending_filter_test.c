/* SPDX-License-Identifier: GPL-3.0-or-later
 * White-box comparison with the previous live-slot predicate. The real source
 * is compiled here once so arbitrary authored LRs can exercise its arm path;
 * this creates no native game, CARD, actor or input authority. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../runtime/host/src/game_events.c"
#include <assert.h>

static CPUState cpu;
static unsigned checks, callbacks;
static uint32_t random_state = UINT32_C(0x76543210);
static BwGameEventSubscription subscription;

static uint32_t random_word(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
static bool old_observes(uint32_t address) {
    if (g_cpu == NULL || g_mask == 0) return false;
    if ((address == kExecuteMethod && (g_mask & BW_GAME_EVENT_MASK(BW_GAME_EVENT_PLAYER_UPDATED))) ||
        (item_entry(address) && (g_mask & BW_GAME_EVENT_MASK(BW_GAME_EVENT_ITEM_AWARDED))) ||
        (address == kMemoryToCard && (g_mask & BW_GAME_EVENT_MASK(BW_GAME_EVENT_SAVE_SERIALIZED))) ||
        (address == kSaveSync && (g_mask & BW_GAME_EVENT_MASK(BW_GAME_EVENT_SAVE_COMPLETED))) ||
        address == kCardToMemory) return true;
    for (unsigned i = 0; i < kPendingCalls; ++i)
        if (g_pending[i].active && g_pending[i].return_address == address) return true;
    return false;
}
static void check(uint32_t address) {
    const CPUState before = cpu;
    assert(bluewake_game_events_observes(address) == old_observes(address));
    assert(memcmp(&cpu, &before, sizeof cpu) == 0);
    ++checks;
}
static void check_state(void) {
    const uint32_t entries[] = {kExecuteMethod, kItemGet, kItemGetUncached,
        kMemoryToCard, kSaveSync, kCardToMemory};
    for (unsigned i = 0; i < sizeof entries / sizeof entries[0]; ++i) check(entries[i]);
    for (unsigned i = 0; i < kPendingCalls; ++i) {
        check(g_pending[i].return_address);
        check(g_pending[i].return_address ^ UINT32_C(0x40000000));
        check(g_pending[i].return_address + 1u);
    }
    for (unsigned i = 0; i < 256; ++i) check(random_word());
}
static void record(const BwGameEvent* event, void* user) {
    (void)event; (void)user; ++callbacks; check_state();
}
static void start(uint32_t target, uint32_t stack, CallKind kind) {
    cpu.lr = target; cpu.gpr[1] = stack;
    assert(arm(&cpu, kind, 0, 6, 0));
    check_state();
}
static void reset(void) {
    bluewake_game_events_reset(&cpu, BW_GAME_RESET_MACHINE_RESET);
    check_state();
    for (unsigned i = 0; i < 4; ++i) assert(g_pending_return_buckets[i] == 0);
}
static void arbitrary_returns(void) {
    for (unsigned round = 0; round < 512; ++round) {
        reset();
        for (unsigned i = 0; i < kPendingCalls; ++i) {
            const uint32_t target = random_word(); /* Includes noncanonical/unaligned LRs. */
            start(target, UINT32_C(0x80010000) + i * 16u, CALL_ITEM);
            const uint64_t token = g_native_token;
            assert(arm(&cpu, CALL_ITEM, 0, 6, 0)); /* Budget replay does not duplicate. */
            assert(g_native_token == token);
        }
        const uint64_t overflow = g_stats.pending_overflow;
        cpu.gpr[1] = UINT32_C(0x80020000); cpu.lr = random_word();
        assert(!arm(&cpu, CALL_ITEM, 0, 6, 0));
        assert(g_stats.pending_overflow == overflow + 1);
        check_state();
        for (unsigned i = 0; i < kPendingCalls; ++i) {
            cpu.gpr[1] = g_pending[i].stack;
            returned(&cpu, g_pending[i].return_address);
            check_state();
        }
    }
}
static void collisions_and_saturation(void) {
    reset();
    unsigned occupied = 0;
    for (uint32_t target = 0; occupied != 256; ++target) {
        const unsigned bucket = return_bucket(target);
        if (g_pending_return_buckets[bucket >> 6] & (UINT64_C(1) << (bucket & 63u))) continue;
        start(target, UINT32_C(0x80010000), CALL_ITEM);
        cpu.gpr[3] = 0;
        returned(&cpu, target); /* Individual cancellation intentionally retains the bit. */
        ++occupied;
    }
    for (unsigned i = 0; i < 4; ++i) assert(g_pending_return_buckets[i] == UINT64_MAX);
    check_state(); /* A saturated filter is still exactly the old predicate. */
    reset();
    const uint32_t target = UINT32_C(0xFFFFFFFF);
    start(target, UINT32_C(0x80010000), CALL_ITEM);
    uint32_t collision = 0;
    while (collision == target || return_bucket(collision) != return_bucket(target)) ++collision;
    assert(!bluewake_game_events_observes(collision)); check(collision);
    start(target, UINT32_C(0x80010010), CALL_ITEM); /* Shared return, distinct invocations. */
    cpu.gpr[1] = UINT32_C(0x80010000); returned(&cpu, target);
    assert(bluewake_game_events_observes(target)); check_state();
    cpu.gpr[1] = UINT32_C(0x80010010); returned(&cpu, target);
    assert(!bluewake_game_events_observes(target)); check_state();
}
static void cancellations(void) {
    reset(); start(UINT32_C(0xC0770A08), UINT32_C(0x80010000), CALL_ITEM);
    g_stats.ticks += kPendingMaxTicks + 1;
    bluewake_game_events_retrace(&cpu); /* Real expiry path. */
    assert(!bluewake_game_events_observes(UINT32_C(0xC0770A08))); check_state();
    start(UINT32_C(0x8180FFE0), UINT32_C(0x80010000), CALL_SAVE);
    assert(bluewake_game_events_unsubscribe(subscription)); subscription = 0;
    check_state(); assert(!g_pending[0].active);
    subscription = bluewake_game_events_subscribe(BW_GAME_EVENT_ALL, record, NULL);
    assert(subscription); check_state();
    start(UINT32_C(0x80231B08), UINT32_C(0x80010000), CALL_LOAD);
    assert(bluewake_game_events_unsubscribe(subscription)); subscription = 0;
    assert(!g_pending[0].active); check_state();
    subscription = bluewake_game_events_subscribe(BW_GAME_EVENT_ALL, record, NULL);
    assert(subscription);
    start(UINT32_C(0x80023960), UINT32_C(0x80010000), CALL_PLAYER);
    bluewake_game_events_reset(NULL, BW_GAME_RESET_MODULE_RELOAD); check_state();
    bluewake_game_events_reset(&cpu, BW_GAME_RESET_ATTACH); check_state();
}
static void mutation_callback(const BwGameEvent* event, void* user) {
    (void)user;
    if (event->kind != BW_GAME_EVENT_SAVE_SERIALIZED) return;
    ++callbacks;
    assert(!g_pending[0].active); /* Consumption precedes callbacks. */
    check_state();
    assert(bluewake_game_events_unsubscribe(subscription)); subscription = 0;
    check_state(); /* Unsubscription cancels another live shared-return call. */
    subscription = bluewake_game_events_subscribe(BW_GAME_EVENT_ALL, record, NULL);
    assert(subscription); check_state();
}
static void callback_mutation(void) {
    reset();
    assert(bluewake_game_events_unsubscribe(subscription));
    subscription = bluewake_game_events_subscribe(BW_GAME_EVENT_ALL, mutation_callback, NULL);
    assert(subscription);
    start(UINT32_C(0x800F64B8), UINT32_C(0x80010000), CALL_SERIALIZE);
    start(UINT32_C(0x800F64B8), UINT32_C(0x80010010), CALL_SERIALIZE);
    cpu.gpr[1] = UINT32_C(0x80010000);
    cpu.gpr[3] = 0;
    returned(&cpu, UINT32_C(0x800F64B8));
    assert(!bluewake_game_events_observes(UINT32_C(0x800F64B8))); check_state();
    start(UINT32_C(0xC0123457), UINT32_C(0x80010000), CALL_ITEM);
    check_state(); reset();
}
int main(void) {
    cpu.ram_size = UINT32_C(0x01800000); cpu.ram = calloc(1, cpu.ram_size);
    uint8_t* original = calloc(1, cpu.ram_size);
    assert(cpu.ram && original);
    subscription = bluewake_game_events_subscribe(BW_GAME_EVENT_ALL, record, NULL);
    assert(subscription); reset();
    arbitrary_returns(); collisions_and_saturation(); cancellations(); callback_mutation();
    assert(callbacks != 0 && memcmp(cpu.ram, original, cpu.ram_size) == 0);
    assert(bluewake_game_events_unsubscribe(subscription));
    bluewake_game_events_reset(NULL, BW_GAME_RESET_MACHINE_RESET);
    free(original); free(cpu.ram);
    printf("game_events_pending_filter_test: %u oracle checks passed\n", checks);
    return 0;
}
