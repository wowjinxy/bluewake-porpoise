// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_WINDOWS_CARD_MENU_H
#define BLUEWAKE_WINDOWS_CARD_MENU_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Before the runtime opens its card: apply a queued replacement, preserving
// the current card first. Do not open the card when this returns false.
bool bw_card_menu_prepare(const char* card_path);
const char* bw_card_menu_error(void);
// Draw inside the existing settings window. Listings are cached until Refresh
// or a successful operation, and imports/restores apply at the next launch.
void bw_card_menu_draw(void);
bool bw_card_menu_restart_needed(void);
// Only after the game workers have stopped and the runtime card has closed.
void bw_card_menu_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
