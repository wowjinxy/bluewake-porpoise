// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "efb_peek.h"
#include <assert.h>
#include <stdio.h>

typedef struct Reader {
    unsigned calls;
    uint16_t x, y, mode;
    uint32_t pixel;
    bool success;
} Reader;

static bool read_color(void* user, uint16_t x, uint16_t y,
                       uint16_t mode, uint32_t* argb) {
    Reader* r = user;
    ++r->calls;
    r->x = x; r->y = y; r->mode = mode;
    *argb = r->pixel;
    return r->success;
}

int main(void) {
    BluewakeEfbPeek state = {0xFFFFu};
    bluewake_efb_peek_reset(&state);
    assert(state.alpha_read == 0u);
    assert(bluewake_efb_alpha_register(0xCC001008u, 2u));
    assert(!bluewake_efb_alpha_register(0xCC001009u, 2u));
    assert(!bluewake_efb_alpha_register(0xCC00100Au, 2u));
    assert(!bluewake_efb_alpha_register(0xCC001008u, 1u));
    assert(!bluewake_efb_alpha_register(0xCC001008u, 4u));

    uint16_t x = 99, y = 99;
    for (uint32_t iy = 0; iy < 1024u; ++iy) {
        for (uint32_t ix = 0; ix < 1024u; ++ix) {
            const uint32_t address = 0xC8000000u | (iy << 12u) | (ix << 2u);
            assert(bluewake_efb_color_address(address, 4u, &x, &y));
            assert(x == ix && y == iy);
        }
    }
    const uint32_t wrong[] = {0xC8400000u, 0xC8800000u, 0xC8C00000u,
                             0xC9000000u, 0x08000000u, 0xC8000001u};
    for (unsigned i = 0; i < sizeof wrong / sizeof wrong[0]; ++i) {
        x = 99; y = 77;
        assert(!bluewake_efb_color_address(wrong[i], 4u, &x, &y));
        assert(x == 99 && y == 77);
    }
    assert(!bluewake_efb_color_address(0xC8000000u, 2u, &x, &y));
    assert(!bluewake_efb_color_address(0xC8000000u, 4u, NULL, &y));

    Reader r = {.pixel = 0xFC123456u, .success = true};
    bool available = false;
    state.alpha_read = 6u; // Native GXPokeAlphaRead(READ_NONE) also sets bit 2.
    assert(bluewake_efb_color_read(&state, 639u, 527u, read_color, &r, &available) == r.pixel);
    assert(available && r.calls == 1 && r.x == 639 && r.y == 527 && r.mode == 6);
    r.success = false; // Even a provider writing a partial value cannot publish it.
    assert(bluewake_efb_color_read(&state, 10u, 20u, read_color, &r, &available) == 0);
    assert(!available && r.calls == 2);
    assert(bluewake_efb_color_read(&state, 640u, 0u, read_color, &r, &available) == 0);
    assert(bluewake_efb_color_read(&state, 0u, 528u, read_color, &r, &available) == 0);
    assert(bluewake_efb_color_read(&state, 0u, 0u, NULL, &r, &available) == 0);
    assert(bluewake_efb_color_read(NULL, 0u, 0u, read_color, &r, NULL) == 0);
    assert(r.calls == 2 && !available);

    uint16_t saved = 0xFFF6u;
    assert(bluewake_efb_peek_restore(&state, &saved, sizeof saved));
    assert(state.alpha_read == saved); // Preserve the complete register, not only its mode.
    assert(!bluewake_efb_peek_restore(&state, &saved, 1u));
    assert(!bluewake_efb_peek_restore(&state, &saved, 3u));
    assert(!bluewake_efb_peek_restore(&state, NULL, 2u));
    assert(!bluewake_efb_peek_restore(&state, &saved, 0u));
    assert(state.alpha_read == saved);
    assert(bluewake_efb_peek_restore(&state, NULL, 0u));
    assert(state.alpha_read == 0u); // Older states cannot retain the future register.
    assert(!bluewake_efb_peek_restore(NULL, &saved, sizeof saved));
    puts("EFB color addresses, register state, bounds and provider failures pass");
    return 0;
}
