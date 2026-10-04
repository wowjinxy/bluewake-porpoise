#ifndef BLUEWAKE_OVERLAP_OBSERVATION_H
#define BLUEWAKE_OVERLAP_OBSERVATION_H

#include <stdbool.h>
#include <string.h>

/* The overlap phase is a host milestone, not guest state. Ordinary desktop
 * sessions can sample once per turn. Acceptance routes and diagnostics need
 * the original per-block cadence even if a launcher default requests less. */
static inline bool bluewake_overlap_observation_enabled(
    const char* requested, bool diagnostic) {
    return diagnostic || requested == NULL || strcmp(requested, "0") != 0;
}

#endif
