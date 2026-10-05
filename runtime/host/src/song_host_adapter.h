#ifndef BLUEWAKE_SONG_HOST_ADAPTER_H
#define BLUEWAKE_SONG_HOST_ADAPTER_H
#include "song_rel_owner.h"
#include "StaticRecompABI.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define BW_SONG_HOST_SLOTS 512u

/* Copied projections of the existing native host structures. Their saved
 * stride does not change. Tokens are an independent parallel runtime array. */
typedef struct BwSongHostAlias { uint32_t raw_start,raw_end,linked_start,text_size; } BwSongHostAlias;
typedef struct BwSongHostSlot { uint32_t owner,address,capacity; } BwSongHostSlot;
typedef bool (*BwSongHostFixedBacking)(void*,const uint8_t**,uint32_t* generation);
typedef uint32_t (*BwSongHostAliasGeneration)(void*);
typedef struct BwSongHostAdapter {
    const CPUState* cpu;
    const uint8_t* ram;
    uint32_t ram_size;
    const StaticRecompModuleDesc* module;
    const uint8_t* fixed_data;
    BwSongHostFixedBacking backing;
    BwSongHostAliasGeneration generation;
    void* alias_user;
    BwSongRelSection sections[19];
    uint64_t load_tokens[BW_SONG_HOST_SLOTS];
    uint64_t sequence;
    bool active,exhausted;
} BwSongHostAdapter;
/* Game-thread lifecycle only. Never reset sequence on revoke/rebind/STATE.
 * Initialize by zeroed static storage. No field is part of saved HOSTVARS. */
void bw_song_host_revoke(BwSongHostAdapter*);
bool bw_song_host_bind(BwSongHostAdapter*,const CPUState*,const StaticRecompModuleDesc*,
                       BwSongHostFixedBacking,BwSongHostAliasGeneration,void*);
void bw_song_host_clear_slot(BwSongHostAdapter*,uint32_t index);
/* Called only AFTER real bytes commit. Cache hits and budget reentry do not
 * call this. Counter exhaustion disables observation, never native loading. */
void bw_song_host_materialized(BwSongHostAdapter*,const CPUState*,uint32_t index);
bool bw_song_host_restore_slots(BwSongHostAdapter*,const CPUState*,const BwSongHostSlot*,uint32_t count);
bool bw_song_host_query(const BwSongHostAdapter*,const CPUState*,
                        const BwSongHostAlias*,uint32_t alias_count,
                        const BwSongHostSlot*,uint32_t slot_count,BwSongOwner*);
/* Strict observer eligibility for saved native loader fields. The native
 * host deserializer still owns the file and its ordinary behavior. */
bool bw_song_host_state_fields(const uint8_t* blob,uint64_t size);
#ifdef __cplusplus
}
#endif
#endif
