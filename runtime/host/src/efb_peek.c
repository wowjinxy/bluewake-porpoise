// SPDX-License-Identifier: GPL-3.0-or-later
#include "efb_peek.h"
#include <string.h>

void bluewake_efb_peek_reset(BluewakeEfbPeek* state) {
    if (state != NULL)
        state->alpha_read = 0u;
}

bool bluewake_efb_alpha_register(uint32_t address, uint8_t size) {
    return address == BLUEWAKE_PE_ALPHA_READ && size == 2u;
}

bool bluewake_efb_color_address(uint32_t address, uint8_t size,
                              uint16_t* x, uint16_t* y) {
    // Bit 22 selects depth; bit 23 selects the unsupported combined read.
    // Each pixel is a naturally aligned word in the color half of the window.
    if (size != 4u || (address & 0xFFC00003u) != 0xC8000000u ||
        x == NULL || y == NULL)
        return false;
    *x = (uint16_t)((address >> 2u) & 0x3FFu);
    *y = (uint16_t)((address >> 12u) & 0x3FFu);
    return true;
}

uint32_t bluewake_efb_color_read(const BluewakeEfbPeek* state,
                               uint16_t x, uint16_t y,
                               BluewakeEfbColorReader read, void* user,
                               bool* available) {
    uint32_t argb = 0u;
    const bool live = state != NULL && read != NULL && x < 640u && y < 528u &&
        read(user, x, y, state->alpha_read, &argb);
    if (available != NULL)
        *available = live;
    // A failed provider must not publish a partial/stale pixel as current.
    return live ? argb : 0u;
}

bool bluewake_efb_peek_state_valid(const void* data, uint64_t size) {
    return data != NULL ? size == sizeof(uint16_t) : size == 0u;
}

bool bluewake_efb_peek_restore(BluewakeEfbPeek* state,
                             const void* data, uint64_t size) {
    if (state == NULL || !bluewake_efb_peek_state_valid(data, size))
        return false;
    uint16_t value = 0u;
    if (data != NULL)
        memcpy(&value, data, sizeof value);
    state->alpha_read = value;
    return true;
}
