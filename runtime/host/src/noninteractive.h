#ifndef BLUEWAKE_NONINTERACTIVE_H
#define BLUEWAKE_NONINTERACTIVE_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Explicit GPU diagnostics: render guest frames without opening input/audio
 * devices or exposing a window. Never selected by a normal player launch. */
static inline bool bluewake_noninteractive_requested(void) {
    const char* renderer = getenv("BLUEWAKE_RENDERER");
    return renderer != NULL && strcmp(renderer, "aurora-noninteractive") == 0;
}

/* Check before defaults or settings can choose the player's own files. The
 * runner supplies disposable data/CARD/SRAM/state/cache paths and hashes them. */
static inline const char* bluewake_noninteractive_error(void) {
    static const char* const paths[] = {
        "BLUEWAKE_DATA_DIR", "BLUEWAKE_CARD_PATH", "BLUEWAKE_SRAM",
        "BLUEWAKE_STATE_DIR", "DOL_AURORA_CACHE_DIR",
    };
    for (size_t i = 0; i < sizeof paths / sizeof paths[0]; ++i) {
        const char* value = getenv(paths[i]);
        if (value == NULL || value[0] == '\0') return paths[i];
    }
    const char* pad = getenv("BLUEWAKE_LIVE_PAD");
    if (pad == NULL || strcmp(pad, "0") != 0) return "BLUEWAKE_LIVE_PAD=0";
    const char* dialog = getenv("BLUEWAKE_NO_DIALOG");
    if (dialog == NULL || strcmp(dialog, "1") != 0) return "BLUEWAKE_NO_DIALOG=1";
    const char* settings = getenv("BLUEWAKE_SETTINGS");
    if (settings == NULL || strcmp(settings, "none") != 0) return "BLUEWAKE_SETTINGS=none";
    return NULL;
}

#endif
