// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_RANDOMIZER_ACTOR_OWNER_H
#define BLUEWAKE_RANDOMIZER_ACTOR_OWNER_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Game-thread query over a bounded, non-faulting, contiguous native resolver.
 * The caller must establish live code/CPU/RAM/alias ownership before and after
 * the query. A supplied resolver or copied result is not native authority.
 * No pointer escapes, native function runs, or guest state changes occur. */
typedef const uint8_t* (*BwRandomizerActorResolve)(void*, uint32_t, uint32_t);
typedef enum BwRandomizerActorKind {
    BW_RANDOMIZER_ACTOR_LINK,
    BW_RANDOMIZER_ACTOR_TBOX,
    BW_RANDOMIZER_ACTOR_DELETING_ITEM
} BwRandomizerActorKind;
typedef struct BwRandomizerActorView {
    BwRandomizerActorResolve resolve;
    void* user;
} BwRandomizerActorView;
typedef struct BwRandomizerActorOwner {
    uint32_t address, pid, profile, methods, actor_tag;
    uint16_t process;
    uint8_t init_state, create_result;
    int8_t room;
} BwRandomizerActorOwner;

/* Exactly one target must belong to the actual fopAc actor queue. A nonzero
 * expected_pid is mandatory when revalidating a retained actor. Zero is for
 * initial discovery only; it cannot refresh a retained actor's lifetime.
 * Membership is an immediate fact, not an issued registration token. The
 * adapter separately issues/revokes monotonic lifetimes, and proves TBOX or
 * Demo_Item's live REL/profile backing with randomizer_rel_owner.
 * Demo_Item awards during Delete: init=3/create=2, actor tag still used.
 * Its separate delete tag has already been removed and is not inspected. */
bool bw_randomizer_actor_owner(const BwRandomizerActorView*,
    BwRandomizerActorKind, uint32_t address, uint32_t expected_pid,
    BwRandomizerActorOwner*);
#ifdef __cplusplus
}
#endif
#endif
