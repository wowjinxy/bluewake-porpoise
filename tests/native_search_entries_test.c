/* End-to-end qualification of the real prepared search chunks and their
 * certified native_entries_v1 hooks. Copy verified raw composite sources and
 * run normal Windows preparation with --native-entries before compiling these
 * five chunks. tests/native_search_entries_test.py supplies production flags.
 * No generated game source is distributed.
 *
 * Compare the original private module with the copied chunks, natives enabled
 * and disabled: strcmp, stage-name search and one JudgeFilter call; then stage
 * search, JudgeFilter and translated cNdIt_Judge actor walks, turn by turn in
 * randomly bounded scheduler windows. All CPU bytes, observation suffixes,
 * touched RAM and aliases match, with entire images checked periodically.
 * The quiet fixture host continues only known register helpers and individual
 * JudgeFilter boundaries; no native batched host walk exists. */
#include "native_search.h"
#include "direct_calls.h"
#include "native_entries.h"
#include "gather_pipe.h"
#include "StaticRecompABI.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* The hooked chunks (their translated functions), by table index. */
void func_800256E0(CPUState*);
void func_8003D6E0(CPUState*);
void func_802416E0(CPUState*);
void func_803256E0(CPUState*);
void func_8032D6E0(CPUState*);
static const struct {
    unsigned index;
    u32 start;
    BwChunkFn fn;
} CHUNKS[] = {{9, 0x800256E0u, func_800256E0},
              {15, 0x8003D6E0u, func_8003D6E0},
              {144, 0x802416E0u, func_802416E0},
              {201, 0x803256E0u, func_803256E0},
              {203, 0x8032D6E0u, func_8032D6E0}};

/* Chunk 0144's other certified hook (native_game_math.py, at 0x80245674),
 * off as in the module here (BLUEWAKE_NATIVE_MATH=0). */
int bluewake_native_game_math_enabled;
int bluewake_native_game_math_try(CPUState* cpu, u32 address) {
    (void)cpu;
    (void)address;
    return 0;
}

static void missing_chunk(CPUState* cpu) {
    fprintf(stderr, "a call into a chunk the test does not link (pc %08X)\n", cpu->pc);
    exit(1);
}
static BwChunkFn s_table[256];
BwChunkFn* const bw_chunk_fns = s_table;

BwChunkFn bw_find_chunk(u32 address) {
    for (unsigned i = 0; i < sizeof CHUNKS / sizeof CHUNKS[0]; ++i)
        if (address - CHUNKS[i].start < 0x4000u)
            return CHUNKS[i].fn;
    return NULL;
}

static bool can_skip(void* user, const CPUState* cpu, u32 address) {
    (void)user; (void)cpu; (void)address; return true;
}

/* The module's own leaf natives (direct_calls.h) are not on these paths. */
int bw_native_call(CPUState* cpu, u32 address) {
    (void)cpu;
    (void)address;
    return 0;
}

extern CPUState bw_guest_cpu;
extern u8 bw_guest_mem1[];
int bluewake_composite_direct_calls(bool enabled, const bool* sources_dirty, const bool* decrementer_pending,
                                    const u32* pi_cause, const u32* pi_mask);
int bluewake_composite_edge_filter(bool enabled);

#define AREA 0x80100000u /* strings, stacks, nodes, actors */
#define AREA_BYTES 0x10000u
#define TABLE_AREA 0x80372000u
#define TABLE_AREA_BYTES 0x4000u
#define TABLE 0x80372818u
#define ENTRIES 825u
#define ALIAS 0xC1F00000u /* a guest alias, as a REL module's linked data */
#define ALIAS_BYTES 0x1000u
#define RETURN_ADDRESS 0xFFFFFFFCu
#define FIND_OBJECT 0x8002833Cu
#define NDIT_LOOP 0x80244F78u /* cNdIt_Judge's call block */
#define PRM (AREA + 0x5000u)
#define FILTER (AREA + 0x5020u)
#define NODES (AREA + 0x5100u)
#define ACTORS (AREA + 0x6000u)
#define ACTOR_BYTES 0x200u
#define MAX_NODES 24u

static u32 seed = 0x1B873593u;
static u32 next(void) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

static u8 s_alias_routed[ALIAS_BYTES], s_alias_module[ALIAS_BYTES], s_alias_source[ALIAS_BYTES];

static void put(u8* ram, u32 address, u32 value) { write_be32(ram + (address - GC_RAM_BASE), value); }
static u32 get(const u8* ram, u32 address) { return read_be32(ram + (address - GC_RAM_BASE)); }
/* The byte behind a guest address of the source image: MEM1 or the alias. */
static u8* at(u8* ram, u32 address) {
    if (address - ALIAS < ALIAS_BYTES)
        return s_alias_source + (address - ALIAS);
    return ram + (address - GC_RAM_BASE);
}

/* The module's edge service: nothing to do at JudgeFilter's boundary (where
 * the host runs its actor-search natives); anywhere else - the return
 * address, another boundary the module watches - it ends the run there. */
static unsigned s_service_count;
static u32 s_service_address;
static int edge_service(void* user, CPUState* cpu, u32 address) {
    (void)user;
    (void)cpu;
    if (address == 0x80328F40u || address == 0x80328F8Cu)
        return 0;
    if (address == BLUEWAKE_SEARCH_JUDGE_FILTER)
        return 0;
    if (s_service_count++ == 0u)
        s_service_address = address;
    return 1;
}

static const u32 FUNCTIONS[] = {BLUEWAKE_SEARCH_STRCMP, BLUEWAKE_SEARCH_STAGE_NAME, BLUEWAKE_SEARCH_JUDGE_FILTER};
static const char* const NAMES[] = {"strcmp", "dStage_searchName", "cTgIt_JudgeFilter"};
#define FUNCTION_COUNT 3u

static u8 character(unsigned style) {
    switch (style) {
    case 0: return (u8)(0x21u + next() % 94u);
    case 1: return (u8)("ab01_"[next() % 5u]);
    default: return (u8)(next() % 4u ? 0x41u + next() % 26u : 0x81u + next() % 127u);
    }
}

static void write_string(u8* ram, u32 address, u32 length, unsigned style) {
    for (u32 i = 0; i < length; ++i)
        *at(ram, address + i) = character(style);
    *at(ram, address + length) = 0u;
}

/* The table (random names, or the stage's shape: first letters that differ
 * from "ikada_h"'s but for three before it at 383) and a name in MEM1 or the
 * alias; the entry it finds, or 0. */
static u32 build_search(u8* ram, unsigned scenario, bool shaped, u32* name_out) {
    const unsigned style = next() % 3u;
    for (u32 i = 0; i < TABLE_AREA_BYTES; i += 4u)
        put(ram, TABLE_AREA + i, next());
    for (u32 e = 0; e < ENTRIES; ++e) {
        u8* entry = at(ram, TABLE + 12u * e);
        const u32 length = 1u + next() % 8u;
        for (u32 i = 0; i < 8u; ++i)
            entry[i] = i < length ? character(style) : 0u;
        if (shaped)
            entry[0] = (u8)('A' + e % 26u);
    }
    const u32 name = scenario % 3u == 1u ? ALIAS + next() % 0x800u : AREA + 0x100u + next() % 0x3000u;
    if (shaped) {
        for (u32 e = 0; e < 3u; ++e)
            memcpy(at(ram, TABLE + 12u * (100u * e + 7u)), "itemFLY", 8u);
        const u32 index = scenario % 4u == 0u ? 824u : 383u;
        memcpy(at(ram, TABLE + 12u * index), scenario % 4u == 0u ? "ikada_x" : "ikada_h", 8u);
        for (u32 i = 0; i < 8u; ++i)
            *at(ram, name + i) = (u8)"ikada_h"[i];
    } else if (scenario % 3u == 0u) {
        write_string(ram, name, 1u + next() % 8u, next() % 3u);
    } else {
        const u8* entry = at(ram, TABLE + 12u * (next() % ENTRIES));
        u32 n = 0;
        while (n < 12u && entry[n] != 0u)
            n++;
        for (u32 i = 0; i < n; ++i)
            *at(ram, name + i) = entry[i];
        *at(ram, name + n) = 0u;
    }
    *name_out = name;
    for (u32 e = 0; e < ENTRIES; ++e)
        if (strncmp((const char*)at(ram, TABLE + 12u * e), (const char*)at(ram, name), 12) == 0 &&
            strnlen((const char*)at(ram, name), 12) < 12u)
            return TABLE + 12u * e;
    return 0u;
}

/* fopAcM_findObjectCB's search for the name, and an actor for each node:
 * not the entry's (most), or matching it at some of its tests. */
static void build_judge(u8* ram, u32 name, u32 found, unsigned scenario, u32 node, u32 actor, bool rarely_match) {
    put(ram, PRM, name);
    put(ram, PRM + 4u, scenario % 4u == 0u ? 0u : next() % 2u ? 0xFFu : next());
    put(ram, PRM + 8u, next() % 4u);
    put(ram, FILTER, FIND_OBJECT);
    put(ram, FILTER + 4u, PRM);
    put(ram, node + 12u, actor);
    for (u32 i = 0; i < ACTOR_BYTES; i += 4u)
        put(ram, actor + i, next());
    if (found != 0u && next() % (rarely_match ? 40u : 2u) == 0u) {
        *at(ram, actor + 14u) = *at(ram, found + 8u);
        *at(ram, actor + 15u) = *at(ram, found + 9u);
        if (next() % 3u)
            *at(ram, actor + 449u) = *at(ram, found + 10u);
        if (next() % 2u)
            put(ram, actor + 176u, get(ram, PRM + 8u));
    }
}

/* The data, then each function's operands. */
static CPUState build(u8* ram, unsigned which, unsigned scenario) {
    for (u32 i = 0; i < AREA_BYTES; i += 4u)
        put(ram, AREA + i, next());
    for (u32 i = 0; i < ALIAS_BYTES; ++i)
        s_alias_source[i] = (u8)next();
    CPUState c;
    memset(&c, 0, sizeof c);
    c.ram_size = GC_MAIN_RAM_SIZE;
    for (unsigned r = 0; r < 32; ++r) {
        c.gpr[r] = next();
        c.fpr[r] = f64_value(((u64)next() << 32) | next());
        c.ps1[r] = f64_value(((u64)next() << 32) | next());
    }
    c.gpr[1] = AREA + 0xF000u - 8u * (next() % 16u);
    c.lr = RETURN_ADDRESS | (next() & 3u);
    c.pc = FUNCTIONS[which];
    c.cr = next();
    c.xer = next();
    c.ctr = next();
    c.msr = PPC_MSR_FP;
    c.fpscr = next() & 0xFFFFF000u;
    c.reserve_valid = (scenario & 1u) != 0u;
    c.reserve_addr = scenario % 4u == 1u ? (c.gpr[1] - 12u) & ~31u : next();
    c.cycle_observation_suffix = next();
    c.cycle_budget = 65536;
    c.downcount = -(s64)(next() % 64u);
    c.cycle_deadline_budget = scenario % 3u == 0u ? 0 : 1000000;
    if (scenario % 17u == 3u)
        c.cycle_deadline_budget = 8 + (s64)(next() % (which ? 9000u : 60u)); /* the natives stop or decline */
    if (scenario % 19u == 4u)
        c.downcount = -c.cycle_budget + 1 + (s64)(next() % (which ? 9000u : 60u));
    if (scenario % 23u == 6u)
        c.cycle_deadline_budget = 1 + (s64)(next() % 12u); /* before a suffix */
    if (which != 0u && scenario % 2u == 0u) { /* a window as the host gives one */
        c.downcount = 0;
        c.cycle_budget = 1 + (s64)(next() % 8000u);
        c.cycle_deadline_budget = next() % 3u ? c.cycle_budget : 0;
    }
    const unsigned style = next() % 3u;
    if (which == 0) {
        const u32 a = scenario % 5u == 2u ? ALIAS + next() % 0x600u : AREA + 0x100u + next() % 0x3000u;
        u32 b = AREA + 0x4000u + next() % 0x3000u;
        if (next() % 2u)
            b = (b & ~3u) | (a & 3u);
        const u32 length = next() % 4u == 0u ? 30u + next() % 100u : next() % 16u;
        write_string(ram, a, length, style);
        for (u32 i = 0; i <= length; ++i)
            *at(ram, b + i) = *at(ram, a + i);
        switch (scenario % 4u) {
        case 0: break;
        case 1:
            if (length != 0u)
                *at(ram, b + next() % length) = character(style);
            break;
        case 2:
            *at(ram, b + next() % (length + 1u)) = 0u;
            break;
        default:
            *at(ram, b + length) = character(style);
            *at(ram, b + length + 1u) = 0u;
            break;
        }
        c.gpr[3] = a;
        c.gpr[4] = b;
    } else {
        u32 name;
        const u32 found = build_search(ram, scenario, scenario % 7u == 0u, &name);
        c.gpr[3] = name;
        if (which == 2u) {
            build_judge(ram, name, found, scenario, NODES, ACTORS, false);
            c.gpr[3] = NODES;
            c.gpr[4] = FILTER;
        }
    }
    return c;
}

/* The source's areas into an image, and its alias into the given storage. */
static void copy_regions(u8* to, const u8* from) {
    memcpy(to + (AREA - GC_RAM_BASE), from + (AREA - GC_RAM_BASE), AREA_BYTES);
    memcpy(to + (TABLE_AREA - GC_RAM_BASE), from + (TABLE_AREA - GC_RAM_BASE), TABLE_AREA_BYTES);
}

static bool same_regions(const u8* a, const u8* b) {
    return memcmp(a + (AREA - GC_RAM_BASE), b + (AREA - GC_RAM_BASE), AREA_BYTES) == 0 &&
           memcmp(a + (TABLE_AREA - GC_RAM_BASE), b + (TABLE_AREA - GC_RAM_BASE), TABLE_AREA_BYTES) == 0 &&
           memcmp(s_alias_module, s_alias_routed, ALIAS_BYTES) == 0;
}

static int compare(const char* what, unsigned i, const CPUState* got, const CPUState* want) {
    if (memcmp(got, want, sizeof *got) == 0)
        return 0;
    fprintf(stderr, "case %u: %s differs\n", i, what);
    for (unsigned b = 0; b < sizeof *got; ++b)
        if (((const u8*)got)[b] != ((const u8*)want)[b])
            fprintf(stderr, "  CPU byte %u: %02X, want %02X\n", b, ((const u8*)got)[b], ((const u8*)want)[b]);
    return 1;
}

/* One turn of the hooked chunks from their pc, as the chassis loop runs
 * them: a chunk left at a boundary goes on in the chunk that has the address
 * unless the budget is spent (2) or the module's watch list names the
 * boundary (3). The quiet host continues individual JudgeFilter and known
 * register-helper boundaries; 1 at the final return address. */
static int run_turn(void) {
    CPUState* c = &bw_guest_cpu;
    for (unsigned guard = 0, first = 1; guard < 1000000u; ++guard, first = 0) {
        if ((c->pc & ~3u) == RETURN_ADDRESS)
            return 1;
        if (!first) {
            if (c->exception != 0u || (c->cycle_budget > 0 && c->downcount <= -c->cycle_budget))
                return 2;
            if (!(bw_edge_filter_enabled && bw_host_quiet(c) && bw_edge_unwatched(c->pc))) {
                if (c->pc != BLUEWAKE_SEARCH_JUDGE_FILTER &&
                    c->pc != 0x80328F40u && c->pc != 0x80328F8Cu)
                    return 3;
            }
        }
        BwChunkFn fn = bw_find_chunk(c->pc);
        if (fn == NULL) {
            fprintf(stderr, "no linked chunk at %08X\n", c->pc);
            exit(1);
        }
        fn(c);
    }
    fprintf(stderr, "a turn did not end\n");
    exit(1);
}

/* The module's run from its guest CPU's pc: 1 at the return address, 3 where
 * its edge service was asked elsewhere, 2 at a spent budget. */
static const StaticRecompModuleDesc* s_mod;
static CPUState* (*s_guest_cpu)(void);
static int run_module_turn(void) {
    CPUState* g = s_guest_cpu();
    if ((g->pc & ~3u) == RETURN_ADDRESS)
        return 1;
    s_service_count = 0;
    if (!s_mod->dispatch(g, g->pc)) {
        fprintf(stderr, "the translation did not run at %08X\n", g->pc);
        exit(1);
    }
    if ((g->pc & ~3u) == RETURN_ADDRESS)
        return 1;
    return s_service_count != 0u ? 3 : 2;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: native_search_entries_test MODULE.dll [CASES] [TURN_CASES]\n");
        return 2;
    }
    _putenv_s("BLUEWAKE_NATIVE_MATH", "0");
    const unsigned cases = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 20000u;
    const unsigned turn_cases = argc > 3 ? (unsigned)strtoul(argv[3], NULL, 10) : 3000u;
    HMODULE lib = LoadLibraryA(argv[1]);
    if (lib == NULL)
        return 1;
    StaticRecompGetModuleFn get_module =
        (StaticRecompGetModuleFn)(void*)GetProcAddress(lib, STATICRECOMP_GET_MODULE_SYMBOL);
    s_guest_cpu = (CPUState * (*)(void))(void*)GetProcAddress(lib, "bluewake_composite_guest_cpu");
    u8* (*mem1)(u32*) = (u8* (*)(u32*))(void*)GetProcAddress(lib, "bluewake_composite_guest_mem1");
    void (*set_edge)(int (*)(void*, CPUState*, u32), void*) =
        (void (*)(int (*)(void*, CPUState*, u32), void*))(void*)GetProcAddress(lib, "bluewake_set_edge_service");
    int (*direct)(bool, const bool*, const bool*, const u32*, const u32*, BwHostCanSkipFn, void*) =
        (int (*)(bool, const bool*, const bool*, const u32*, const u32*, BwHostCanSkipFn, void*))(void*)GetProcAddress(
            lib, "bluewake_composite_direct_calls_v2");
    int (*filter)(bool) = (int (*)(bool))(void*)GetProcAddress(lib, "bluewake_composite_edge_filter");
    bool (*module_alias_add)(u32, u32, u8*) =
        (bool (*)(u32, u32, u8*))(void*)GetProcAddress(lib, "ppc_guest_alias_add_shared");
    if (!get_module || !s_guest_cpu || !mem1 || !set_edge || !direct || !filter || !module_alias_add)
        return 1;
    s_mod = get_module();
    if (s_mod->cpu_state_size != sizeof(CPUState) || strcmp(s_mod->game_id, "GZLE01") != 0)
        return 1;
    static const bool clear = false;
    static const u32 zero = 0u;
    if (!direct(true, &clear, &clear, &zero, &zero, can_skip, NULL) || !filter(true))
        return 1;
    set_edge(edge_service, NULL);
    u32 size = 0;
    u8* reference_ram = mem1(&size);
    memset(reference_ram, 0, GC_MAIN_RAM_SIZE);

    /* The hooked side: its own chunk table, direct calls and edge filter on
     * with the module's watch list, the host quiet; MEM1 its own
     * (guest_cpu.c). The alias in both registries. */
    for (unsigned i = 0; i < 256u; ++i)
        s_table[i] = missing_chunk;
    for (unsigned i = 0; i < sizeof CHUNKS / sizeof CHUNKS[0]; ++i)
        s_table[CHUNKS[i].index] = CHUNKS[i].fn;
    static bool sources_dirty, decrementer_pending;
    static u32 pi_cause, pi_mask;
    bluewake_composite_direct_calls_v2(true, &sources_dirty, &decrementer_pending, &pi_cause, &pi_mask, can_skip, NULL);
    if (!bluewake_composite_edge_filter(true) || !bw_edge_watch_ready || bw_edge_unwatched(0x80245640u))
        return 1;
    if (!module_alias_add(ALIAS, ALIAS_BYTES, s_alias_module) ||
        !ppc_guest_alias_add_shared(ALIAS, ALIAS_BYTES, s_alias_routed) || g_ppc_guest_aliases_overlap_mem1)
        return 1;
    u8* routed_ram = bw_guest_mem1;
    memset(routed_ram, 0, GC_MAIN_RAM_SIZE);
    u8* source = calloc(1, GC_MAIN_RAM_SIZE);
    if (source == NULL)
        return 1;

    /* ---- One call ---- */
    for (unsigned which = 0; which < FUNCTION_COUNT; ++which) {
        unsigned outcomes[4] = {0};
        for (unsigned i = 0; i < cases; ++i) {
            const CPUState start = build(source, which, i);
            CPUState results[3];
            int outcome[3];
            for (unsigned run = 0; run < 3u; ++run) {
                if (run == 0u) {
                    copy_regions(reference_ram, source);
                    memcpy(s_alias_module, s_alias_source, ALIAS_BYTES);
                    CPUState* g = s_guest_cpu();
                    *g = start;
                    g->ram = reference_ram;
                    s_mod->on_state_loaded(g);
                    outcome[0] = run_module_turn();
                    results[0] = *g;
                    results[0].ram = NULL;
                } else {
                    copy_regions(routed_ram, source);
                    memcpy(s_alias_routed, s_alias_source, ALIAS_BYTES);
                    bluewake_composite_native_entries_v1(run == 1u, can_skip, NULL);
                    bw_guest_cpu = start;
                    bw_guest_cpu.ram = routed_ram;
                    ppc_fpscr_updated(&bw_guest_cpu);
                    outcome[run] = run_turn();
                    results[run] = bw_guest_cpu;
                    results[run].ram = NULL;
                    if (!same_regions(routed_ram, reference_ram)) {
                        fprintf(stderr, "case %u (%s, natives %s): RAM differs\n", i, NAMES[which],
                                run == 1u ? "on" : "off");
                        return 1;
                    }
                }
            }
            outcomes[outcome[0]]++;
            if (outcome[1] != outcome[0] || outcome[2] != outcome[0] ||
                compare(NAMES[which], i, &results[1], &results[0]) ||
                compare(NAMES[which], i, &results[2], &results[0])) {
                fprintf(stderr, "case %u (%s, seed %08X): mismatch (outcomes %d/%d/%d)\n", i, NAMES[which], seed,
                        outcome[0], outcome[1], outcome[2]);
                return 1;
            }
            if (i % 256u == 255u && memcmp(routed_ram, reference_ram, GC_MAIN_RAM_SIZE) != 0) {
                fprintf(stderr, "case %u (%s): a byte outside the compared ranges differs\n", i, NAMES[which]);
                return 1;
            }
        }
        printf("%08X %s: %u calls identical through the hooked chunks, natives on and off (%u returned, %u "
               "stopped for the budget, %u at a watched boundary)\n",
               FUNCTIONS[which], NAMES[which], cases, outcomes[1], outcomes[2], outcomes[3]);
        fflush(stdout);
    }
    bluewake_native_search_report();

    /* ---- Turns ---- */
    static const char* const TURN_NAMES[3] = {"a search from dStage_searchName's entry",
                                              "a cTgIt_JudgeFilter call",
                                              "cNdIt_Judge's walk over 1 to 24 actors"};
    for (unsigned kind = 0; kind < 3u; ++kind) {
        unsigned long long turns_total = 0;
        unsigned completed = 0;
        for (unsigned i = 0; i < turn_cases; ++i) {
            /* The work: a search, a call, or a walk from cNdIt_Judge's call
             * block (its frame above, with the return address). */
            CPUState start = build(source, kind == 0u ? 1u : 2u, i);
            if (kind == 2u) {
                u32 name = get(source, PRM);
                const u32 found = build_search(source, i, i % 3u == 0u, &name);
                const unsigned count = 1u + next() % MAX_NODES;
                for (unsigned n = 0; n < count; ++n) {
                    const u32 node = NODES + 16u * n;
                    build_judge(source, name, found, i, node, ACTORS + ACTOR_BYTES * (n % 16u), true);
                    put(source, node + 8u, n + 1u < count ? node + 16u : 0u);
                }
                start.gpr[1] = AREA + 0xE000u;
                put(source, start.gpr[1] + 36u, RETURN_ADDRESS);
                start.gpr[3] = NODES;
                start.gpr[31] = get(source, NODES + 8u);
                start.gpr[29] = BLUEWAKE_SEARCH_JUDGE_FILTER;
                start.gpr[30] = FILTER;
                start.pc = NDIT_LOOP;
            }
            start.reserve_valid = false;
            CPUState* g = s_guest_cpu();
            *g = start;
            copy_regions(reference_ram, source);
            memcpy(s_alias_module, s_alias_source, ALIAS_BYTES);
            g->ram = reference_ram;
            s_mod->on_state_loaded(g);
            CPUState sides[2];
            u8* images[2] = {routed_ram, NULL};
            static u8* off_ram;
            if (off_ram == NULL)
                off_ram = calloc(1, GC_MAIN_RAM_SIZE);
            images[1] = off_ram;
            static u8 alias_off[ALIAS_BYTES], alias_on[ALIAS_BYTES];
            for (unsigned s = 0; s < 2u; ++s) {
                copy_regions(images[s], source);
                sides[s] = start;
                sides[s].ram = images[s];
                ppc_fpscr_updated(&sides[s]);
            }
            memcpy(alias_on, s_alias_source, ALIAS_BYTES);
            memcpy(alias_off, s_alias_source, ALIAS_BYTES);
            int done = 0;
            unsigned turn = 0;
            for (; turn < 100000u && !done; ++turn) {
                /* The window: 300 to 5,000 cycles, the deadline at its end, or none. */
                const s64 window = 300 + (s64)(next() % 4701u);
                const s64 deadline = next() % 3u ? window : 0;
                g->downcount = 0;
                g->cycle_budget = window;
                g->cycle_deadline_budget = deadline;
                const int m = run_module_turn();
                int r[2];
                for (unsigned s = 0; s < 2u; ++s) {
                    /* The hooked side's MEM1 is guest_cpu.c's array: swap the
                     * side's image and alias in and out of it. */
                    memcpy(routed_ram + (AREA - GC_RAM_BASE), images[s] + (AREA - GC_RAM_BASE), AREA_BYTES);
                    memcpy(routed_ram + (TABLE_AREA - GC_RAM_BASE), images[s] + (TABLE_AREA - GC_RAM_BASE),
                           TABLE_AREA_BYTES);
                    memcpy(s_alias_routed, s == 0u ? alias_on : alias_off, ALIAS_BYTES);
                    bluewake_composite_native_entries_v1(s == 0u, can_skip, NULL);
                    bw_guest_cpu = sides[s];
                    bw_guest_cpu.ram = routed_ram;
                    bw_guest_cpu.downcount = 0;
                    bw_guest_cpu.cycle_budget = window;
                    bw_guest_cpu.cycle_deadline_budget = deadline;
                    r[s] = run_turn();
                    sides[s] = bw_guest_cpu;
                    memcpy(images[s] + (AREA - GC_RAM_BASE), routed_ram + (AREA - GC_RAM_BASE), AREA_BYTES);
                    memcpy(images[s] + (TABLE_AREA - GC_RAM_BASE), routed_ram + (TABLE_AREA - GC_RAM_BASE),
                           TABLE_AREA_BYTES);
                    memcpy(s == 0u ? alias_on : alias_off, s_alias_routed, ALIAS_BYTES);
                }

                CPUState want = *g, on = sides[0], off = sides[1];
                want.ram = on.ram = off.ram = NULL;
                if (r[0] != m || r[1] != m || compare(TURN_NAMES[kind], i, &on, &want) ||
                    compare(TURN_NAMES[kind], i, &off, &want)) {
                    fprintf(stderr, "case %u (%s), turn %u (window %lld, deadline %lld): mismatch (ends %d/%d/%d)\n",
                            i, TURN_NAMES[kind], turn, (long long)window, (long long)deadline, m, r[0], r[1]);
                    return 1;
                }
                done = m == 1;
            }
            if (!done) {
                fprintf(stderr, "case %u (%s): did not finish\n", i, TURN_NAMES[kind]);
                return 1;
            }
            for (unsigned s = 0; s < 2u; ++s)
                if (memcmp(images[s] + (AREA - GC_RAM_BASE), reference_ram + (AREA - GC_RAM_BASE), AREA_BYTES) != 0 ||
                    memcmp(images[s] + (TABLE_AREA - GC_RAM_BASE), reference_ram + (TABLE_AREA - GC_RAM_BASE),
                           TABLE_AREA_BYTES) != 0 ||
                    memcmp(s == 0u ? alias_on : alias_off, s_alias_module, ALIAS_BYTES) != 0) {
                    fprintf(stderr, "case %u (%s): RAM differs at the end (natives %s)\n", i, TURN_NAMES[kind],
                            s == 0u ? "on" : "off");
                    return 1;
                }
            turns_total += turn;
            completed++;
        }
        printf("%s, turn by turn in windows of 300 to 5,000 cycles: %u cases identical after every turn through the "
               "hooked chunks, natives on and off (%llu turns)\n",
               TURN_NAMES[kind], completed, turns_total);
        fflush(stdout);
    }
    bluewake_native_search_report();

    return 0;
}
