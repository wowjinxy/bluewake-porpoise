/* SPDX-License-Identifier: GPL-3.0-or-later
 * Conservative address-only rejection for six finite observer families.
 * A hit grants nothing: all original dynamic predicates still run in order.
 * Canonicalizing only this filter deliberately permits extra raw-PC hits.
 * Inventory and a real-predicate oracle accompany every source revision. */
#ifndef BLUEWAKE_FINITE_OBSERVER_FILTER_H
#define BLUEWAKE_FINITE_OBSERVER_FILTER_H
#include <stdbool.h>
#include <stdint.h>
static inline bool bluewake_finite_observer_maybe(uint32_t address) {
    static const uint64_t words[4] = {
        UINT64_C(0x0004020400200886),
        UINT64_C(0x0C00200200882010),
        UINT64_C(0x90200020A0801500),
        UINT64_C(0x2C08284090419800),
    };
    const uint32_t bit = ((address & ~UINT32_C(0x40000000)) * UINT32_C(0x9E3779B1)) >> 24;
    return ((words[bit >> 6] >> (bit & 63u)) & 1u) != 0;
}
#endif
