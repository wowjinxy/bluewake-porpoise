#ifndef BLUEWAKE_COMPOSITE_OBSERVATION_FACTS_H
#define BLUEWAKE_COMPOSITE_OBSERVATION_FACTS_H

#include "core/cpu.h"

/* A separate, optional contract for the chassis loop. Existing direct and
 * native calls keep their complete observation query. Facts describe this
 * exact query, never a cached answer: the loop has just checked host quiet
 * state and missed the immutable module watch table at this address. The
 * current host's observer predicates are read-only and interrupt-state
 * writers run on the game thread; neither may publish new interrupt state
 * between the quiet check and this callback. Changing that invariant needs
 * a revised contract, not reuse of this capability. */
#define BW_OBSERVATION_FACTS_V1 1u
#define BW_OBSERVATION_FACT_QUIET 1u
#define BW_OBSERVATION_FACT_NO_STATIC_INTERCEPT 2u
#define BW_OBSERVATION_FACTS_ALL \
    (BW_OBSERVATION_FACT_QUIET | BW_OBSERVATION_FACT_NO_STATIC_INTERCEPT)
/* Version 1 certifies the current host's complete static intercept set.
 * A changed host set requires a new contract, rather than a partial proof. */
#define BW_OBSERVATION_STATIC_KEY_COUNT_V1 56u

typedef bool (*BwHostObservationFactsFn)(void*, const CPUState*, u32, u32);
extern BwHostObservationFactsFn bw_host_observation_facts;
extern void* bw_host_observation_facts_user;
unsigned bluewake_composite_observation_facts_v1(
    u32 cpu_abi, u32 cpu_size, const u32* canonical_keys, u32 key_count,
    BwHostObservationFactsFn callback, void* user);

#endif
