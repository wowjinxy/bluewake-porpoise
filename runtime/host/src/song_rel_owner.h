#ifndef BLUEWAKE_SONG_REL_OWNER_H
#define BLUEWAKE_SONG_REL_OWNER_H
#include "core/cpu.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Called only by the game thread for exact Hr ItemAward
 * admission/return. Resolver must be bounded, read-only and non-faulting:
 * no mem_read helpers which can mutate CPU exception state. It may resolve
 * raw host-extended MEM1 and fixed linked REL data backing. */
typedef const uint8_t* (*BwSongReadOnlyResolve)(void*, uint32_t, uint32_t);
typedef struct BwSongRelAlias {
    uint32_t raw_start, raw_end, linked_start, text_size;
} BwSongRelAlias;
typedef struct BwSongRelSlot {
    uint32_t owner, address, capacity;
    uint64_t load_token; /* New, monotonic per materialization; never zero. */
} BwSongRelSlot;
typedef struct BwSongRelSection {
    uint32_t module_id, section_index, linked_start, size;
} BwSongRelSection;
typedef struct BwSongOwner {
    uint64_t load_token;
    uint32_t module_header, loader_owner, raw_text;
} BwSongOwner;
typedef struct BwSongRelView {
    BwSongReadOnlyResolve resolve;
    void* user;
    const BwSongRelAlias* aliases;
    uint32_t alias_count;
    const BwSongRelSlot* slots;
    uint32_t slot_count;
    const BwSongRelSection* sections;
    uint32_t section_count;
} BwSongRelView;
/* Exact module257/sec1 and native Hr data, not an arbitrary REL LR query. */
bool bw_song_rel_owner(const BwSongRelView*, BwSongOwner*);

/* Game-events ownership bridge; no CPU/RAM is given to subscribers. A NULL
 * callback fails the new Hr path closed. Query supplies a copied owner proof
 * after the host validates module/slot/alias/profile/method liveness. */
typedef bool (*BwSongOwnerQuery)(void*, const CPUState*, BwSongOwner*);
void bluewake_game_events_set_song_owner_query(BwSongOwnerQuery, void*);
#ifdef __cplusplus
}
#endif
#endif
