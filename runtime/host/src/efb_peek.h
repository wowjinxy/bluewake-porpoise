// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_EFB_PEEK_H
#define BLUEWAKE_EFB_PEEK_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define BLUEWAKE_PE_ALPHA_READ 0xCC001008u

typedef struct BluewakeEfbPeek {
    uint16_t alpha_read;
} BluewakeEfbPeek;

typedef bool (*BluewakeEfbColorReader)(void* user, uint16_t x, uint16_t y,
                                     uint16_t alpha_read, uint32_t* argb);

// PE power-on value. GXInit subsequently selects the game's alpha-read mode.
void bluewake_efb_peek_reset(BluewakeEfbPeek* state);
bool bluewake_efb_alpha_register(uint32_t address, uint8_t size);
bool bluewake_efb_color_address(uint32_t address, uint8_t size,
                              uint16_t* x, uint16_t* y);
uint32_t bluewake_efb_color_read(const BluewakeEfbPeek* state,
                               uint16_t x, uint16_t y,
                               BluewakeEfbColorReader read, void* user,
                               bool* available);

// An absent optional state chunk is an older state: reset rather than retain
// the running game's register. Reject malformed chunks before changing state.
bool bluewake_efb_peek_state_valid(const void* data, uint64_t size);
bool bluewake_efb_peek_restore(BluewakeEfbPeek* state,
                             const void* data, uint64_t size);

#endif
