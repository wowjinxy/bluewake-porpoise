// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_NETWORK_MENU_H
#define BLUEWAKE_NETWORK_MENU_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// Before CARD management/backend startup. A nonempty returned path is the
// selected isolated room card; an empty path leaves the personal route alone.
bool bw_network_menu_prepare(const char* data_dir, const char* module_path,
                             char* card_path, unsigned path_size);
const char* bw_network_menu_error(void);
void bw_network_menu_draw(void);
bool bw_network_menu_restart_needed(void);
void bw_network_menu_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
