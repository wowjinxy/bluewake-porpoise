/* Internal game-thread observation view. This is not a module ABI.
 * The getter returns one process-lifetime static object, including before
 * attach. Fields stay live: no observation answers or guest pointers copied.
 * Event APIs, writers, and readers must obey game_events.h's game-thread
 * contract. Reset mutates the object in place; it never changes its address.
 */
#ifndef BLUEWAKE_GAME_EVENT_OBSERVATION_VIEW_H
#define BLUEWAKE_GAME_EVENT_OBSERVATION_VIEW_H
#include "core/cpu.h"
#include <stdint.h>
typedef struct BwGameEventObservationView {
    CPUState* owner_cpu;
    uint64_t mask;
    uint64_t pending_return_buckets[4];
} BwGameEventObservationView;
#if UINTPTR_MAX == UINT64_MAX
#ifdef __cplusplus
static_assert(sizeof(BwGameEventObservationView) == 48, "host event view layout");
#else
_Static_assert(sizeof(BwGameEventObservationView) == 48, "host event view layout");
#endif
#endif
#ifdef __cplusplus
extern "C" {
#endif
const BwGameEventObservationView* bluewake_game_events_observation_view(void);
#ifdef __cplusplus
}
#endif
#endif
