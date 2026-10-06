// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_RANDOMIZER_REL_OWNER_H
#define BLUEWAKE_RANDOMIZER_REL_OWNER_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Game-thread, read-only ownership of the two audited GZLE01 reward RELs.
 * The caller must hold a live CPU/RAM/code lease before providing this view
 * and recheck it after the query. resolve must be bounded, nonfaulting and
 * side-effect free, and reject partially shadowed/noncontiguous native spans.
 * Linked .data must resolve to the current compiled module's actual backing,
 * not the raw relocated .data copy. No mem_read/guest helper is appropriate.
 * Copied tables and tokens alone do not prove a real native registration. */
typedef const uint8_t* (*BwRandomizerRelResolve)(void*, uint32_t, uint32_t);
typedef struct BwRandomizerRelAlias {
    uint32_t raw_start, raw_end, linked_start, text_size;
} BwRandomizerRelAlias;
typedef struct BwRandomizerRelSlot {
    uint32_t owner, address, capacity;
    /* Caller-issued monotonic token, minted after genuine materialization;
     * invalidated on reuse/replacement/unload. Never a pointer/PID/hash. */
    uint64_t materialization;
} BwRandomizerRelSlot;
typedef struct BwRandomizerRelSection {
    uint32_t module_id, section_index, linked_start, size;
} BwRandomizerRelSection;
typedef struct BwRandomizerRelView {
    BwRandomizerRelResolve resolve;
    void* user;
    const BwRandomizerRelAlias* aliases;
    uint32_t alias_count;
    const BwRandomizerRelSlot* slots;
    uint32_t slot_count;
    const BwRandomizerRelSection* sections;
    uint32_t section_count;
} BwRandomizerRelView;
typedef enum BwRandomizerRelKind {
    BW_RANDOMIZER_REL_TBOX = 113,
    BW_RANDOMIZER_REL_DEMO_ITEM = 131
} BwRandomizerRelKind;
typedef struct BwRandomizerRelOwner {
    uint64_t materialization;
    uint32_t module_id, module_header, loader_owner, raw_text, raw_data;
    uint32_t fixed_profile, fixed_methods;
} BwRandomizerRelOwner;
/* No allocations, token minting, actor proof, CPU mutation or subscriptions.
 * On failure, every output field is zero. Only these two kinds are accepted. */
bool bw_randomizer_rel_owner(const BwRandomizerRelView*, BwRandomizerRelKind,
                            BwRandomizerRelOwner*);
#ifdef __cplusplus
}
#endif
#endif
