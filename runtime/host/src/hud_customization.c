#include "hud_customization.h"

#include <float.h>
#include <math.h>
#include <string.h>

#define MEM1_BASE UINT32_C(0x80000000)
#define SCREEN_GLOBAL UINT32_C(0x803F7078)
#define METER_METHODS UINT32_C(0x8039350C)
#define VT_MY_SCREEN UINT32_C(0x80391360)
#define VT_SCREEN UINT32_C(0x8039D6B8)
#define VT_MY_PICTURE UINT32_C(0x80372648)
#define VT_PICTURE UINT32_C(0x8039D730)
#define VT_TEXT UINT32_C(0x8039D770)
enum {
    MEM1_SIZE = 0x01800000u,
    BP_CONTROL = 0x6Au, BP_FIRST = 0x6Bu, BP_LAST = 0x79u,
    WIRE_VERSION = 1u, WIRE_BEGIN = 1u, WIRE_END = 2u, WIRE_RESET = 3u,
    WIRE_MASK = 0xFFFFFFu, WIRE_FIELDS = 0x7FFFu,
};

#define TAG(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))

static bool finite_range(float f, float lo, float hi) {
    return isfinite(f) && f >= lo && f <= hi;
}

void bw_hud_config_identity(BwHudConfig* config) {
    if (!config) return;
    memset(config, 0, sizeof *config);
    for (unsigned i = 0; i < BW_HUD_GROUP_COUNT; ++i) {
        config->groups[i].scale = config->groups[i].opacity = 1.f;
        memset(config->groups[i].tint, 255, 4);
        config->groups[i].visible = true;
    }
}

static bool group_valid(const BwHudGroupConfig* c) {
    return c && finite_range(c->offset_x, -2048.f, 2048.f) &&
        finite_range(c->offset_y, -2048.f, 2048.f) &&
        finite_range(c->scale, .25f, 4.f) && finite_range(c->opacity, 0.f, 1.f) &&
        finite_range(c->anchor_x, 0.f, 1.f) && finite_range(c->anchor_y, 0.f, 1.f);
}

bool bw_hud_config_valid(const BwHudConfig* config) {
    if (!config) return false;
    for (unsigned i = 0; i < BW_HUD_GROUP_COUNT; ++i)
        if (!group_valid(&config->groups[i])) return false;
    return true;
}

static bool group_identity(const BwHudGroupConfig* c) {
    return c->offset_x == 0.f && c->offset_y == 0.f && c->scale == 1.f &&
        c->opacity == 1.f && c->visible && c->tint[0] == 255 &&
        c->tint[1] == 255 && c->tint[2] == 255 && c->tint[3] == 255;
}

bool bw_hud_config_is_identity(const BwHudConfig* config) {
    if (!bw_hud_config_valid(config)) return false;
    for (unsigned i = 0; i < BW_HUD_GROUP_COUNT; ++i)
        if (!group_identity(&config->groups[i])) return false;
    return true;
}

const char* bw_hud_group_name(BwHudGroup group) {
    static const char* const names[] = {"Hearts", "Magic", "Buttons", "Rupees", "Keys"};
    return (unsigned)group < BW_HUD_GROUP_COUNT ? names[group] : "Unknown";
}

static bool span(const BwHudRuntime* r, uint32_t p, size_t size) {
    return r && r->memory.bytes && p >= MEM1_BASE && !(p & 3u) &&
        size <= r->memory.size && (size_t)(p - MEM1_BASE) <= r->memory.size - size;
}

static const uint8_t* read_span(const BwHudRuntime* r,uint32_t p,uint32_t size) {
    /* Reads may be unaligned bytes, unlike the aligned object span checks. */
    if(!r||!r->memory.bytes||!size||p<MEM1_BASE||size>r->memory.size||
       (size_t)(p-MEM1_BASE)>r->memory.size-size) {
        if(r)((BwHudRuntime*)r)->read_failed=true;return NULL;
    }
    const uint8_t* b=r->memory.resolve?
        r->memory.resolve(r->memory.resolve_user,p,size):r->memory.bytes+(p-MEM1_BASE);
    if(!b)((BwHudRuntime*)r)->read_failed=true;
    return b;
}
static uint32_t load_be32(const uint8_t* b) {
    return ((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3];
}
static uint32_t read32(const BwHudRuntime* r,uint32_t p) {
    const uint8_t* b=read_span(r,p,4);return b?load_be32(b):0;
}
static uint8_t read8(const BwHudRuntime* r,uint32_t p) {
    const uint8_t* b=read_span(r,p,1);return b?*b:0;
}

static float read_float(const BwHudRuntime* r, uint32_t p) {
    const uint32_t u = read32(r, p);
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

static bool root_valid(const BwHudRuntime* r, uint32_t p, unsigned part) {
    return span(r, p, 0xCCu) &&
        read32(r, p) == (part == 0 ? VT_MY_SCREEN : VT_SCREEN) &&
        read32(r, p + 0xBCu) == p && read32(r, p + 0xC0u) == 0u &&
        read32(r, p + 0xB8u) <= 256u;
}

/* JSUTree has list at +0, then link at +C: owner+C, parent-list+10,
 * prev+14, next+18. List head/tail/next point at the LINK (+C), not the tree!
 * Validate both ancestry AND parent's bounded sibling list,
 * so a forged parent pointer alone cannot admit an unrelated menu pane. */
static bool sibling_member(const BwHudRuntime* r, uint32_t parent, uint32_t child) {
    if (!span(r, parent, 0xCCu)) return false;
    const uint32_t count = read32(r, parent + 0xB8u);
    if (!count || count > 256u) return false;
    uint32_t link = read32(r, parent + 0xB0u), prev = 0u;
    bool found = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (!span(r, link, 16u)) return false;
        /* This synchronous traversal runs on the game thread. Resolve the
         * complete link record once; retain no borrowed pointer across edges.
         * All four fields still require one contiguous, admitted guest span. */
        const uint8_t* record = read_span(r, link, 16u);
        if (!record || load_be32(record + 4u) != parent + 0xB0u ||
            load_be32(record + 8u) != prev) return false;
        const uint32_t owner = load_be32(record);
        if (!span(r, owner, 0xCCu) || owner + 0xBCu != link) return false;
        if (owner == child) found = true;
        prev = link;
        link = load_be32(record + 0xCu);
    }
    return found && !link && read32(r, parent + 0xB4u) == prev;
}

static bool ancestry(const BwHudRuntime* r, uint32_t pane, uint32_t root,
                     bool* visible) {
    uint32_t visited[32];
    *visible = true;
    for (unsigned n = 0; n < 32u; ++n) {
        if (!span(r, pane, 0xCCu) || read32(r, pane + 0xBCu) != pane) return false;
        for (unsigned j = 0; j < n; ++j) if (visited[j] == pane) return false;
        visited[n] = pane;
        if (!read8(r, pane + 0xAAu)) *visible = false;
        if (pane == root) return true;
        const uint32_t tree = read32(r, pane + 0xC0u);
        if (!span(r, tree, 0x1Cu)) return false;
        const uint32_t parent = read32(r, tree + 0xCu);
        if (!span(r, parent, 0xCCu) || parent + 0xB0u != tree ||
            !sibling_member(r, parent, pane)) return false;
        pane = parent;
    }
    return false;
}

static bool leaf_class(uint32_t vt, uint32_t magic) {
    return ((vt == VT_MY_PICTURE || vt == VT_PICTURE) && magic == TAG('P','I','C','1')) ||
        (vt == VT_TEXT && magic == TAG('T','B','X','1'));
}

static bool actor_valid(const BwHudRuntime* r, uint32_t actor) {
    return span(r, actor, 0x3030u) && (read32(r, actor + 8u) >> 16) == 0x1F1u &&
        read32(r, actor + 0xD8u) == METER_METHODS;
}

static bool add_pane(BwHudRuntime* r, unsigned offset, uint32_t tag,
                     BwHudGroup group, unsigned part) {
    /* Identity groups need neither registration nor per-leaf hooks. */
    if (group_identity(&r->config.groups[group])) return true;
    if (r->stats.pane_count >= BW_HUD_MAX_PANES) return false;
    const uint32_t wrapper = r->actor + offset, pane = read32(r, wrapper);
    if (!span(r, pane, 0x124u) || read32(r, pane + 8u) != tag ||
        !leaf_class(read32(r, pane), read32(r, pane + 4u))) return false;
    bool visible;
    if (!ancestry(r, pane, r->roots[part], &visible)) return false;
    for (unsigned i = 0; i < r->stats.pane_count; ++i)
        if (r->panes[i].pane == pane) return false;
    const float x = read_float(r, wrapper + 0xCu), y = read_float(r, wrapper + 0x10u);
    const float w = read_float(r, wrapper + 0x2Cu), h = read_float(r, wrapper + 0x30u);
    if (!finite_range(x, -4096.f, 4096.f) || !finite_range(y, -4096.f, 4096.f) ||
        !finite_range(w, 0.f, 4096.f) || !finite_range(h, 0.f, 4096.f)) return false;
    float* b = r->bounds[group];
    b[0] = fminf(b[0], x); b[1] = fminf(b[1], y);
    b[2] = fmaxf(b[2], x + w); b[3] = fmaxf(b[3], y + h);
    BwHudPaneCapture* c = &r->panes[r->stats.pane_count++];
    c->pane = pane; c->wrapper = wrapper; c->tag = tag; c->vtable = read32(r, pane);
    c->root = r->roots[part]; c->group = group;
    return true;
}

static bool capture(BwHudRuntime* r, uint32_t actor) {
    r->stats.pane_count = 0;
    r->actor = 0;
    if (!actor_valid(r, actor)) return false;
    for (unsigned i = 0; i < 3u; ++i) {
        r->roots[i] = read32(r, SCREEN_GLOBAL + 4u * i);
        if (!root_valid(r, r->roots[i], i)) return false;
    }
    r->actor = actor; r->actor_id = read32(r, actor + 4u);
    for (unsigned i = 0; i < BW_HUD_GROUP_COUNT; ++i) {
        r->bounds[i][0] = r->bounds[i][1] = FLT_MAX;
        r->bounds[i][2] = r->bounds[i][3] = -FLT_MAX;
    }
#define ADD(offset, tag, group, part) do { if (!add_pane(r, offset, tag, group, part)) goto invalid; } while (0)
    /* Exact dMeter_screenDataSet source wrapper/tag pairs. No whole-screen,
     * magic-address scan, ordinal draw counts or unrelated timer/reticle scope. */
    for (unsigned i = 0; i < 20u; ++i) {
        ADD(0x640u + i * 0x38u, TAG('h','t','0' + i / 10u,'0' + i % 10u), BW_HUD_HEARTS, 0);
        ADD(0xAA0u + i * 0x38u, TAG('h','k','0' + i / 10u,'0' + i % 10u), BW_HUD_HEARTS, 0);
    }
    ADD(0xF00, TAG('h','t','f','l'), BW_HUD_HEARTS, 0);
    for (unsigned i = 0; i < 8u; ++i)
        ADD(0xF38u + i * 0x38u, TAG('m','b','r','1' + i), BW_HUD_MAGIC, 0);
    static const struct { unsigned offset; uint32_t tag; } magic[] = {
        {0x10F8,TAG('m','b','b','3')},{0x1130,TAG('m','b','b','2')},
        {0x1168,TAG('m','b','k','2')},{0x11A0,TAG('m','b','b','1')},
        {0x11D8,TAG('m','b','k','1')},{0x1210,TAG('c','m','c','v')},
        {0x1248,TAG('c','m','m','g')},{0x1280,TAG('c','m','m','k')},
        {0x12B8,TAG('c','m','r','g')},{0x12F0,TAG('c','m','b','1')},
    };
    for (unsigned i = 0; i < sizeof magic / sizeof magic[0]; ++i)
        ADD(magic[i].offset, magic[i].tag, BW_HUD_MAGIC, 0);
    static const struct { unsigned offset; uint32_t tag; } buttons[] = {
        {0x1D00,TAG('b','a','w','d')},{0x1D38,TAG('b','a','w','p')},
        {0x1DA8,TAG(0,'b','a','a')},{0x1DE0,TAG('b','a','a','2')},
        {0x1E18,TAG('b','a','w','e')},{0x1E50,TAG('b','a','t','2')},
        {0x2518,TAG(0,'b','a','z')},{0x2550,TAG('b','a','x','1')},
        {0x2588,TAG(0,'b','a','y')},{0x25C0,TAG('b','0','0','1')},
        {0x25F8,TAG('b','0','0','2')},{0x2630,TAG('b','a','0','1')},
        {0x2668,TAG('b','a','0','2')},{0x26A0,TAG('b','a','r','1')},
        {0x26D8,TAG('b','a','0','r')},{0x2748,TAG(0,'a','1','0')},
        {0x2780,TAG(0,'a','0','1')},{0x27B8,TAG(0,'y','u','m')},
        {0x27F0,TAG('y','u','m','k')},{0x2828,TAG('w','e','i','t')},
        {0x2860,TAG('w','i','t','k')},{0x2898,TAG(0,'b','a','b')},
        {0x28D0,TAG('b','a','a','t')},
    };
    for (unsigned i = 0; i < sizeof buttons / sizeof buttons[0]; ++i)
        ADD(buttons[i].offset, buttons[i].tag, BW_HUD_BUTTONS, 0);
    for (unsigned i = 0; i < 3u; ++i) {
        const unsigned char name = (unsigned char)(i == 0 ? 'x' : i == 1 ? 'y' : 'z');
        ADD(0x1F30u+i*0x38u,TAG(0,name,'1','0'),BW_HUD_BUTTONS,0);
        ADD(0x1FD8u+i*0x38u,TAG(0,name,'0','1'),BW_HUD_BUTTONS,0);
        ADD(0x2080u+i*0x38u,TAG(name,'i','t','m'),BW_HUD_BUTTONS,0);
        ADD(0x2128u+i*0x38u,TAG(name,'i','t','k'),BW_HUD_BUTTONS,0);
        ADD(0x2320u+i*0x38u,TAG('b','l',name,'1'),BW_HUD_BUTTONS,0);
        ADD(0x23C8u+i*0x38u,TAG('b','l',name,'2'),BW_HUD_BUTTONS,0);
        ADD(0x2470u+i*0x38u,TAG('b','l',name,'3'),BW_HUD_BUTTONS,0);
    }
    ADD(0x2A20,TAG('r','u','p','1'),BW_HUD_RUPEES,1);
    ADD(0x2A58,TAG('r','u','p','2'),BW_HUD_RUPEES,1);
    for (unsigned i = 0; i < 4u; ++i) {
        ADD(0x19F0u+i*0x38u,TAG('n','m','0','0'+i),BW_HUD_RUPEES,1);
        ADD(0x1B40u+i*0x38u,TAG('n','m','0','4'+i),BW_HUD_RUPEES,1);
    }
    ADD(0x1980,TAG('k','y','l','1'),BW_HUD_KEYS,0);
    ADD(0x19B8,TAG('k','y','l','2'),BW_HUD_KEYS,0);
    ADD(0x1AD0,TAG('n','m','0','3'),BW_HUD_KEYS,0);
    ADD(0x1B08,TAG('n','m','0','4'),BW_HUD_KEYS,0);
    ADD(0x1C20,TAG('n','m','0','8'),BW_HUD_KEYS,0);
    ADD(0x1C58,TAG('n','m','0','9'),BW_HUD_KEYS,0);
    ADD(0x1C90,TAG('k','e','y','0'),BW_HUD_KEYS,0);
#undef ADD
    if(r->read_failed)goto invalid;
    ++r->stats.captures;
    return true;
invalid:
    r->actor = 0; r->stats.pane_count = 0;
    return false;
}

static void emit(BwHudRuntime* r, uint8_t reg, uint32_t value) {
    if (r->emit) r->emit(reg, value & WIRE_MASK, r->emit_user);
}

static void end_leaf(BwHudRuntime* r, bool cancelled) {
    if (!r->stats.leaf_active) return;
    emit(r, BP_FIRST, r->sequence);
    emit(r, BP_CONTROL, (WIRE_VERSION << 16) | WIRE_END);
    r->stats.leaf_active = false;
    ++r->stats.ends;
    if (cancelled) ++r->stats.cancellations;
}

void bw_hud_cancel(BwHudRuntime* r) {
    if (!r) return;
    end_leaf(r, true);
    r->stats.meter_active = false;
    r->stats.pane_count = 0;
    r->actor = 0;
}

static void reset_wire(BwHudRuntime* r) {
    if (!r->stats.enabled) return;
    emit(r, BP_CONTROL, (WIRE_VERSION << 16) | WIRE_RESET);
    ++r->stats.resets;
}

void bw_hud_init(BwHudRuntime* r, BwHudEmitBp emitter, void* user) {
    if (!r) return;
    memset(r, 0, sizeof *r);
    bw_hud_config_identity(&r->config);
    r->emit = emitter; r->emit_user = user; r->revision = 1u;
}

bool bw_hud_set_config(BwHudRuntime* r, const BwHudConfig* c) {
    if (!r || !bw_hud_config_valid(c)) return false;
    const bool was_enabled = r->stats.enabled;
    bw_hud_cancel(r);
    reset_wire(r);
    r->config = *c;
    r->revision = (r->revision + 1u) & WIRE_MASK;
    if (!r->revision) r->revision = 1u;
    r->stats.enabled = !bw_hud_config_is_identity(c) && r->emit != NULL;
    if (r->stats.enabled && !was_enabled) reset_wire(r);
    return true;
}

bool bw_hud_attach(BwHudRuntime* r, const BwHudMemory* m) {
    if (!r) return false;
    bw_hud_cancel(r); reset_wire(r);
    memset(&r->memory, 0, sizeof r->memory);
    if (!m || !m->bytes || m->size != MEM1_SIZE || !m->cpu_identity ||
        m->abi != BW_HUD_ABI_GZLE01 || !m->epoch || !m->generation) return false;
    r->memory = *m;
    return true;
}

void bw_hud_detach(BwHudRuntime* r) {
    if (!r) return;
    bw_hud_cancel(r); reset_wire(r);
    memset(&r->memory, 0, sizeof r->memory);
}

void bw_hud_frame(BwHudRuntime* r, uint64_t frame) {
    if (!r || r->frame == frame) return;
    bw_hud_cancel(r); reset_wire(r); r->frame = frame;
}

bool bw_hud_observes(const BwHudRuntime* r, uint32_t address) {
    if (!r || !r->stats.enabled || !r->memory.bytes) return false;
    if (address == BW_HUD_METER_CAPTURE || address == BW_HUD_METER_DELETE) return true;
    if (r->actor && (address == BW_HUD_METER1_DRAW || address == BW_HUD_METER2_DRAW)) return true;
    if (r->stats.meter_active && (address == BW_HUD_METER_RETURN ||
        address == BW_HUD_MY_PICTURE_DRAW || address == BW_HUD_PICTURE_DRAW ||
        address == BW_HUD_TEXT_DRAW)) return true;
    return r->stats.leaf_active && address == BW_HUD_LEAF_RETURN;
}

static uint32_t checksum(const uint32_t* words) {
    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < 14u; ++i)
        for (unsigned b = 0; b < 3u; ++b) {
            hash ^= (words[i] >> (8u*b)) & 255u;
            hash *= 16777619u;
        }
    return hash & WIRE_MASK;
}

static uint32_t fixed_signed(float f) {
    const float scaled = f * 256.f;
    return (uint32_t)(int32_t)(scaled < 0.f ? scaled - .5f : scaled + .5f) & WIRE_MASK;
}

static void begin_leaf(BwHudRuntime* r, const BwHudPaneCapture* c, uint32_t stack) {
    const BwHudGroupConfig* g = &r->config.groups[c->group];
    const float* b = r->bounds[c->group];
    const float px = b[0] + (b[2] - b[0]) * g->anchor_x;
    const float py = b[1] + (b[3] - b[1]) * g->anchor_y;
    r->sequence = (r->sequence + 1u) & WIRE_MASK;
    if (!r->sequence) { reset_wire(r); r->sequence = 1u; }
    uint32_t w[15] = {
        r->sequence, r->revision, (uint32_t)c->group | (g->visible ? 0x100u : 0u),
        fixed_signed(g->offset_x), fixed_signed(g->offset_y),
        (uint32_t)(g->scale * 4096.f + .5f), (uint32_t)(g->opacity * 65536.f + .5f),
        ((uint32_t)g->tint[0] << 16) | ((uint32_t)g->tint[1] << 8) | g->tint[2], g->tint[3],
        fixed_signed(px), fixed_signed(py), (c->pane - MEM1_BASE) >> 2,
        (uint32_t)(r->memory.epoch % WIRE_MASK) + 1u,
        (uint32_t)(r->memory.generation % WIRE_MASK) + 1u, 0u,
    };
    w[14] = checksum(w);
    for (unsigned i = 0; i < 15u; ++i) emit(r, (uint8_t)(BP_FIRST + i), w[i]);
    emit(r, BP_CONTROL, (WIRE_VERSION << 16) | WIRE_BEGIN);
    r->leaf_stack = stack; r->stats.leaf_active = true; ++r->stats.begins;
}

static bool current_capture(const BwHudRuntime* r) {
    if (!r->actor || !actor_valid(r, r->actor) || read32(r, r->actor + 4u) != r->actor_id)
        return false;
    for (unsigned i = 0; i < 3u; ++i)
        if (read32(r, SCREEN_GLOBAL + i*4u) != r->roots[i] || !root_valid(r, r->roots[i], i))
            return false;
    return !r->read_failed;
}

void bw_hud_dispatch(BwHudRuntime* r, const BwHudEdge* e) {
    if (!r || !e || !bw_hud_observes(r, e->address)) return;
    r->read_failed=false;
    if (e->cpu_identity != r->memory.cpu_identity || e->memory_identity != (uintptr_t)r->memory.bytes ||
        e->epoch != r->memory.epoch || e->generation != r->memory.generation || !span(r, e->stack, 16u)) {
        ++r->stats.rejected_edges; bw_hud_cancel(r); return;
    }
    if (e->address == BW_HUD_METER_DELETE) { bw_hud_cancel(r); reset_wire(r); return; }
    if (e->address == BW_HUD_METER_CAPTURE) {
        bw_hud_cancel(r);
        if (!capture(r, e->object)) { ++r->stats.rejected_captures; bw_hud_cancel(r); }
        return;
    }
    if (!current_capture(r)) { ++r->stats.rejected_edges; bw_hud_cancel(r); return; }
    if (e->address == BW_HUD_METER_RETURN) {
        const bool good = r->stats.meter_active && e->stack == r->meter_stack;
        end_leaf(r, true); r->stats.meter_active = false;
        if (!good) ++r->stats.rejected_edges;
        return;
    }
    if (e->address == BW_HUD_LEAF_RETURN) {
        const bool good = r->stats.leaf_active && e->stack == r->leaf_stack;
        end_leaf(r, !good);
        if (!good) { ++r->stats.rejected_edges; bw_hud_cancel(r); }
        return;
    }
    if (e->address == BW_HUD_METER1_DRAW || e->address == BW_HUD_METER2_DRAW) {
        /* Only native virtual list dispatch, never an arbitrary direct call. */
        /* Entry service may replay at a zero-budget first-PC boundary. Only the
         * exact still-armed invocation is idempotent; no new scope/sequence. */
        const bool replay=r->stats.meter_active && e->address==r->meter_address &&
            e->stack==r->meter_stack && e->object==r->meter_object && e->lr==BW_HUD_METER_RETURN;
        if ((r->stats.meter_active&&!replay) || e->lr != BW_HUD_METER_RETURN ||
            !span(r, e->object, 4u) || !span(r, read32(r, e->object), 16u) ||
            read32(r, read32(r, e->object) + 0xCu) != e->address || r->read_failed) {
            ++r->stats.rejected_edges; bw_hud_cancel(r); return;
        }
        if(replay)return;
        r->stats.meter_active = true; r->meter_address = e->address; r->meter_stack = e->stack; r->meter_object=e->object;
        return;
    }
    const bool replay=r->stats.leaf_active && e->address==r->leaf_address && e->object==r->leaf_object &&
        e->stack==r->leaf_stack && e->lr==BW_HUD_LEAF_RETURN;
    if (!r->stats.meter_active || (r->stats.leaf_active&&!replay) || e->lr != BW_HUD_LEAF_RETURN ||
        e->stack >= r->meter_stack || r->meter_stack - e->stack > 0x10000u) {
        ++r->stats.rejected_edges; bw_hud_cancel(r); return;
    }
    for (unsigned i = 0; i < r->stats.pane_count; ++i) {
        const BwHudPaneCapture* c = &r->panes[i];
        if (c->pane != e->object) continue;
        const uint32_t expected = c->vtable == VT_MY_PICTURE ? BW_HUD_MY_PICTURE_DRAW :
            c->vtable == VT_PICTURE ? BW_HUD_PICTURE_DRAW : BW_HUD_TEXT_DRAW;
        const unsigned part = c->root == r->roots[1] ? 1u : 0u;
        bool visible;
        if (e->address != expected ||
            (r->meter_address == BW_HUD_METER2_DRAW) != (part == 1u) ||
            read32(r, c->wrapper) != c->pane || read32(r, c->pane) != c->vtable ||
            read32(r, c->pane + 8u) != c->tag ||
            !leaf_class(c->vtable, read32(r, c->pane + 4u)) ||
            !ancestry(r, c->pane, c->root, &visible)) {
            ++r->stats.rejected_edges; bw_hud_cancel(r); return;
        }
        const uint8_t alpha=read8(r,c->pane+0xADu);
        if(r->read_failed){++r->stats.rejected_edges;bw_hud_cancel(r);return;}
        if (!visible || !alpha) { if(replay)bw_hud_cancel(r);++r->stats.hidden_leaves; return; }
        if(replay)return;
        r->leaf_object=e->object;r->leaf_address=e->address;
        begin_leaf(r, c, e->stack);
        return;
    }
    /* A valid meter also draws unowned timers/compass/choice panes. */
}

void bw_hud_stats(const BwHudRuntime* r, BwHudStats* out) {
    if (out) { if (r) *out = r->stats; else memset(out, 0, sizeof *out); }
}

static float signed_fixed(uint32_t value) {
    const int32_t integer = (int32_t)(value & 0x7FFFFFu) - (int32_t)(value & 0x800000u);
    return (float)integer / 256.f;
}

bool bw_hud_descriptor_decode(const uint32_t words[16], BwHudDescriptor* out) {
    if (!words || !out || words[0] != ((WIRE_VERSION << 16) | WIRE_BEGIN)) return false;
    const uint32_t* w = words + 1;
    for (unsigned i = 0; i < 15u; ++i) if (w[i] & ~WIRE_MASK) return false;
    if (!w[0] || !w[1] || (w[2] & ~0x1FFu) || (w[2] & 255u) >= BW_HUD_GROUP_COUNT ||
        w[8] > 255u || w[11] >= MEM1_SIZE / 4u || !w[12] || !w[13] || w[14] != checksum(w))
        return false;
    BwHudDescriptor d;
    memset(&d, 0, sizeof d);
    d.sequence = w[0]; d.revision = w[1]; d.group = (BwHudGroup)(w[2] & 255u);
    d.config.visible = (w[2] & 0x100u) != 0;
    d.config.offset_x = signed_fixed(w[3]); d.config.offset_y = signed_fixed(w[4]);
    d.config.scale = (float)w[5] / 4096.f; d.config.opacity = (float)w[6] / 65536.f;
    d.config.tint[0] = (uint8_t)(w[7] >> 16); d.config.tint[1] = (uint8_t)(w[7] >> 8);
    d.config.tint[2] = (uint8_t)w[7]; d.config.tint[3] = (uint8_t)w[8];
    d.pivot_x = signed_fixed(w[9]); d.pivot_y = signed_fixed(w[10]);
    d.pane = MEM1_BASE + w[11] * 4u; d.epoch = w[12]; d.generation = w[13];
    if (!group_valid(&d.config) || !finite_range(d.pivot_x, -8192.f, 8192.f) ||
        !finite_range(d.pivot_y, -8192.f, 8192.f)) return false;
    *out = d;
    return true;
}

void bw_hud_stream_init(BwHudStream* s) { if (s) memset(s, 0, sizeof *s); }

void bw_hud_stream_frame(BwHudStream* s) {
    if (!s) return;
    s->active = false; s->mask = 0;
    memset(&s->descriptor, 0, sizeof s->descriptor);
}

bool bw_hud_stream_bp(BwHudStream* s, uint8_t reg, uint32_t value) {
    if (!s || reg < BP_CONTROL || reg > BP_LAST) return false;
    if (value & ~WIRE_MASK) { ++s->malformed; bw_hud_stream_frame(s); return true; }
    if (reg != BP_CONTROL) {
        s->words[reg - BP_FIRST] = value;
        s->mask |= 1u << (reg - BP_FIRST);
        return true;
    }
    const uint32_t seq = s->words[0], mask = s->mask;
    s->mask = 0;
    if (value == ((WIRE_VERSION << 16) | WIRE_RESET)) {
        bw_hud_stream_frame(s); s->last_sequence = s->last_epoch = s->last_generation = 0;
        return true;
    }
    if (value == ((WIRE_VERSION << 16) | WIRE_END) && mask == 1u && s->active &&
        seq == s->descriptor.sequence) {
        s->active = false; ++s->ends; memset(&s->descriptor, 0, sizeof s->descriptor);
        return true;
    }
    uint32_t packet[16]; packet[0] = value; memcpy(packet + 1, s->words, sizeof s->words);
    BwHudDescriptor d;
    if (s->active || mask != WIRE_FIELDS || !bw_hud_descriptor_decode(packet, &d) ||
        (s->last_sequence && (d.epoch != s->last_epoch || d.generation != s->last_generation ||
                             d.sequence <= s->last_sequence))) {
        ++s->malformed; bw_hud_stream_frame(s); return true;
    }
    s->descriptor = d; s->active = true; ++s->begins;
    s->last_sequence = d.sequence; s->last_epoch = d.epoch; s->last_generation = d.generation;
    return true;
}

bool bw_hud_stream_snapshot(const BwHudStream* s, BwHudDescriptor* out) {
    if (!s || !out || !s->active) return false;
    *out = s->descriptor;
    return true;
}

bool bw_hud_descriptor_visible(const BwHudDescriptor* d) {
    return d && group_valid(&d->config) && d->config.visible && d->config.opacity > 0.f && d->config.tint[3];
}

void bw_hud_transform_point(const BwHudDescriptor* d, float xy[2]) {
    if (!d || !xy || !group_valid(&d->config) || group_identity(&d->config) ||
        !finite_range(d->pivot_x, -8192.f, 8192.f) || !finite_range(d->pivot_y, -8192.f, 8192.f)) return;
    xy[0] = d->pivot_x + (xy[0] - d->pivot_x) * d->config.scale + d->config.offset_x;
    xy[1] = d->pivot_y + (xy[1] - d->pivot_y) * d->config.scale + d->config.offset_y;
}

bool bw_hud_transform_rect(const BwHudDescriptor* d, float rect[4]) {
    if (!d || !rect || !group_valid(&d->config) || !isfinite(rect[0]) || !isfinite(rect[1]) ||
        !isfinite(rect[2]) || !isfinite(rect[3]) || rect[2] < rect[0] || rect[3] < rect[1] ||
        !finite_range(d->pivot_x, -8192.f, 8192.f) || !finite_range(d->pivot_y, -8192.f, 8192.f)) return false;
    if (group_identity(&d->config)) return true;
    bw_hud_transform_point(d, rect); bw_hud_transform_point(d, rect + 2);
    return true;
}

void bw_hud_final_color(const BwHudDescriptor* d, float rgba[4]) {
    if (!d || !rgba || !group_valid(&d->config) || group_identity(&d->config)) return;
    for (unsigned i = 0; i < 4u; ++i) rgba[i] *= (float)d->config.tint[i] / 255.f;
    rgba[3] *= d->config.opacity;
}
