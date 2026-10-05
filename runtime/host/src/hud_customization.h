#ifndef BLUEWAKE_HUD_CUSTOMIZATION_H
#define BLUEWAKE_HUD_CUSTOMIZATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Guarded GZLE01 pane customization. Game-thread APIs consume a read-only MEM1 view;
 * renderer APIs consume copied, ordered metadata only. No guest writes, CPU
 * calls, environment/settings writes, GPU, window or game-event dependency. */
#define BW_HUD_ABI_GZLE01 1u
#define BW_HUD_MAX_PANES 160u
#define BW_HUD_METER_CAPTURE 0x80204404u
#define BW_HUD_METER_DELETE 0x80204C28u
#define BW_HUD_METER1_DRAW 0x801F0608u
#define BW_HUD_METER2_DRAW 0x801F06CCu
#define BW_HUD_METER_RETURN 0x800865A4u
#define BW_HUD_MY_PICTURE_DRAW 0x8002AC1Cu
#define BW_HUD_PICTURE_DRAW 0x802D3CECu
#define BW_HUD_TEXT_DRAW 0x802D5C4Cu
#define BW_HUD_LEAF_RETURN 0x802D04CCu

typedef enum BwHudGroup {
    BW_HUD_HEARTS,
    BW_HUD_MAGIC,
    BW_HUD_BUTTONS,
    BW_HUD_RUPEES,
    BW_HUD_KEYS,
    BW_HUD_GROUP_COUNT
} BwHudGroup;

/* All units are native logical 2D units, relative to the CURRENT aspect's
 * native layout. anchor chooses the pivot in the captured native group bounds.
 * visible=true permits native presentation; it never shows a hidden pane.
 * tint and opacity multiply the FINAL native fragment, retaining fades/flashes.
 * Native particles/minimap/timers/menus are intentionally outside this MVP. */
typedef struct BwHudGroupConfig {
    float offset_x, offset_y;       /* [-2048, 2048], Q8 on the wire. */
    float scale;                    /* [0.25, 4], Q12 on the wire. */
    float opacity;                  /* [0, 1], Q16 on the wire. */
    float anchor_x, anchor_y;        /* [0, 1]. */
    uint8_t tint[4];                 /* Native identity = 255,255,255,255. */
    bool visible;
} BwHudGroupConfig;

typedef struct BwHudConfig {
    BwHudGroupConfig groups[BW_HUD_GROUP_COUNT];
} BwHudConfig;

/* Optional host resolver: cached native MEM1 only, nonfaulting, read-only.
 * A borrowed span must remain valid and byte-contiguous through this lookup;
 * host verifies current CPU/RAM/aliases/lifecycle before and after resolving. */
typedef const uint8_t* (*BwHudResolve)(void*,uint32_t address,uint32_t size);
typedef struct BwHudMemory {
    const uint8_t* bytes;            /* Native big-endian MEM1 bytes. */
    size_t size;                    /* Exactly 24 MiB for this audited ABI. */
    uintptr_t cpu_identity;
    uint32_t abi;                   /* Caller must also verify module identity. */
    uint64_t epoch, generation;
    BwHudResolve resolve;
    void* resolve_user;
} BwHudMemory;

typedef struct BwHudEdge {
    uintptr_t cpu_identity;
    uintptr_t memory_identity;       /* current cpu->ram, checked before reads */
    uint32_t address, stack, object, lr; /* copied PC, r1, r3 and LR */
    uint64_t epoch, generation;      /* current lifecycle, not saved in a STATE */
} BwHudEdge;

/* Each callback writes ONE dedicated BP word into the actual ordered FIFO.
 * 0x6A..0x79 are separate from existing count/tag registers 0x7B..0x7E.
 * BEGIN copies a full checksummed descriptor, then commits at 0x6A. END is
 * explicit even for zero draws; RESET clears ownership at lifecycle changes.
 * Do not enqueue host pointers, rely on primitive counts, or let UI emit tags. */
typedef void (*BwHudEmitBp)(uint8_t reg, uint32_t value24, void* user);

typedef struct BwHudDescriptor {
    uint32_t sequence, revision, pane;
    uint32_t epoch, generation;      /* bounded wire tokens; not CPU pointers */
    BwHudGroup group;
    BwHudGroupConfig config;
    float pivot_x, pivot_y;
} BwHudDescriptor;

typedef struct BwHudPaneCapture {
    uint32_t pane, wrapper, tag, vtable, root;
    BwHudGroup group;
} BwHudPaneCapture;

typedef struct BwHudStats {
    uint64_t captures, rejected_captures, rejected_edges, hidden_leaves;
    uint64_t begins, ends, cancellations, resets;
    uint32_t pane_count;
    bool enabled, meter_active, leaf_active;
} BwHudStats;

/* Exposed fixed-size state permits stack/static ownership without allocation.
 * Do not modify fields: use the API; all runtime operations are game-thread. */
typedef struct BwHudRuntime {
    BwHudMemory memory;
    BwHudConfig config;
    BwHudEmitBp emit;
    void* emit_user;
    BwHudPaneCapture panes[BW_HUD_MAX_PANES];
    uint32_t roots[3];
    float bounds[BW_HUD_GROUP_COUNT][4];
    uint32_t actor, actor_id, revision, sequence;
    uint32_t meter_stack, meter_address, leaf_stack;
    uint32_t meter_object, leaf_object, leaf_address;
    bool read_failed;
    uint64_t frame;
    BwHudStats stats;
} BwHudRuntime;

void bw_hud_config_identity(BwHudConfig* config);
bool bw_hud_config_valid(const BwHudConfig* config);
bool bw_hud_config_is_identity(const BwHudConfig* config);
const char* bw_hud_group_name(BwHudGroup group);
void bw_hud_init(BwHudRuntime* runtime, BwHudEmitBp emit, void* user);
bool bw_hud_set_config(BwHudRuntime* runtime, const BwHudConfig* config);
bool bw_hud_attach(BwHudRuntime* runtime, const BwHudMemory* memory);
void bw_hud_detach(BwHudRuntime* runtime);
/* Explicit calls are mandatory on every state load/module/memory/scene reset,
 * even if pointers were recycled. frame() revokes previous native capture. */
void bw_hud_frame(BwHudRuntime* runtime, uint64_t frame);
void bw_hud_cancel(BwHudRuntime* runtime);
bool bw_hud_observes(const BwHudRuntime* runtime, uint32_t address);
void bw_hud_dispatch(BwHudRuntime* runtime, const BwHudEdge* edge);
void bw_hud_stats(const BwHudRuntime* runtime, BwHudStats* out);

/* Renderer-side packet parser. It belongs to a renderer stream, never CPU/RAM.
 * Non-HUD registers are ignored. Any malformed/nested/replayed transaction
 * clears active ownership; a frame/reset clears both active and pending state.
 * Registers 6B..79 must all occur since the previous commit. */
typedef struct BwHudStream {
    uint32_t words[15], mask;
    uint32_t last_sequence, last_epoch, last_generation;
    uint64_t malformed, begins, ends;
    bool active;
    BwHudDescriptor descriptor;
} BwHudStream;
void bw_hud_stream_init(BwHudStream* stream);
bool bw_hud_stream_bp(BwHudStream* stream, uint8_t reg, uint32_t value24);
void bw_hud_stream_frame(BwHudStream* stream);
bool bw_hud_stream_snapshot(const BwHudStream* stream, BwHudDescriptor* out);
/* Decode a complete register snapshot taken AT the draw packet (not later).
 * An END/RESET snapshot has no descriptor. Useful for GXCore's pending state. */
bool bw_hud_descriptor_decode(const uint32_t words[16], BwHudDescriptor* out);

/* Pure native-logical affine/final-color operations. Identity returns without
 * touching bytes. Helpers never mutate an input descriptor/native draw state. */
void bw_hud_transform_point(const BwHudDescriptor* descriptor, float xy[2]);
bool bw_hud_transform_rect(const BwHudDescriptor* descriptor, float rect[4]);
void bw_hud_final_color(const BwHudDescriptor* descriptor, float rgba[4]);
bool bw_hud_descriptor_visible(const BwHudDescriptor* descriptor);

#ifdef __cplusplus
}
#endif
#endif
