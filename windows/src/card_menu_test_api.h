// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_CARD_MENU_TEST_API_H
#define BLUEWAKE_CARD_MENU_TEST_API_H
#if !defined(BLUEWAKE_CARD_MENU_TEST)
#error This header is only for the offscreen card-menu regression.
#endif
#ifdef __cplusplus
extern "C" {
#endif
// The next actual Import button click consumes this path, or an empty path
// simulates cancellation. The fixture never compiles or invokes a file picker.
void bw_card_menu_test_set_import_path(const char* path);
#ifdef __cplusplus
}
#endif
#endif
