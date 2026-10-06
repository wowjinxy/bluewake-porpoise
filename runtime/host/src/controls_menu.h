// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_CONTROLS_MENU_H
#define BLUEWAKE_CONTROLS_MENU_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum BluewakeControlsCapture {
    BLUEWAKE_CAPTURE_KEY_BUTTON,
    BLUEWAKE_CAPTURE_KEY_AXIS,
    BLUEWAKE_CAPTURE_CONTROLLER_BUTTON,
    BLUEWAKE_CAPTURE_CONTROLLER_AXIS,
    BLUEWAKE_CAPTURE_KEY_ACTION, // slot = action * 2 + primary/alternate source.
    BLUEWAKE_CAPTURE_CONTROLLER_ACTION
} BluewakeControlsCapture;

// The portable Controls tab; call inside an existing ImGui settings window.
void bluewake_controls_menu_draw(void);
// Stacked rows for an enlarged narrow settings window; binding semantics stay unchanged.
void bluewake_controls_menu_draw_compact(void);
// Opening settings neutralizes game input without disabling keyboard mappings.
// Closing cancels capture, suppresses held input and saves changed bindings.
void bluewake_controls_menu_set_open(bool open);
// Call each UI frame on the bindings owner thread, including while settings is closed.
// Failed menu saves retry current dirty bindings at most once per second after
// the first later frame. Opening settings pauses retry; no device input is sampled.
void bluewake_controls_menu_tick(uint64_t now_ms);
// Retained persistence failure, independent of capture messages. Empty after a
// successful save/reload. Show outside the Controls page so closing cannot hide it.
const char* bluewake_controls_menu_save_error(void);
bool bluewake_controls_menu_is_open(void);
bool bluewake_controls_menu_event(const void* sdl_event);
bool bluewake_controls_menu_capturing(void);
bool bluewake_controls_menu_begin_capture(BluewakeControlsCapture kind, unsigned slot);
void bluewake_controls_menu_cancel_capture(void);
// Native hotkey hooks use this when the key would otherwise bypass SDL capture.
void bluewake_controls_menu_reject_reserved_key(void);

#ifdef __cplusplus
}
#endif
#endif
