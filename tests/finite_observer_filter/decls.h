/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BLUEWAKE_FINITE_OBSERVER_TEST_DECLS_H
#define BLUEWAKE_FINITE_OBSERVER_TEST_DECLS_H
#include "core/cpu.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef size_t (*BwTestPcInventory)(uint32_t*, size_t);
static inline size_t test_copy_pcs(uint32_t* out, size_t cap, const uint32_t* values, size_t count) {
    assert(out != NULL && count <= cap);
    memcpy(out, values, count * sizeof *out);
    return count;
}
void test_hud_state(CPUState*, unsigned, uint32_t);
size_t test_hud_pcs(uint32_t*, size_t);
void test_health_state(CPUState*, unsigned, uint32_t);
size_t test_health_pcs(uint32_t*, size_t);
void test_quick_state(CPUState*, unsigned, uint32_t);
size_t test_quick_pcs(uint32_t*, size_t);
void test_dialogue_state(CPUState*, unsigned, uint32_t);
size_t test_dialogue_pcs(uint32_t*, size_t);
void test_enhancement_state(CPUState*, unsigned, uint32_t);
size_t test_enhancement_pcs(uint32_t*, size_t);
void test_autosave_state(CPUState*, unsigned, uint32_t);
size_t test_autosave_pcs(uint32_t*, size_t);
size_t test_health_lrs(uint32_t*, size_t);
size_t test_enhancement_lrs(uint32_t*, size_t);
#endif
