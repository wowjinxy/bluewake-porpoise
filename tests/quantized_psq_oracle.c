/* SPDX-License-Identifier: GPL-3.0-or-later
 * Canonical GXRuntime cpu.c PSQ region, unchanged except entrypoint names.
 * The runner verifies this entire region against the guarded SDK source. */
#include "core/cpu.h"
#include <math.h>
#define ppc_psq_load oracle_psq_load
#define ppc_psq_store oracle_psq_store
static s32 gqr_scale(u32 value) {
    return sign_extend(value & 0x3Fu, 6);
}


static u32 psq_type_size(u8 type) {
    switch (type) {
    case 0: return 4;
    case 4:
    case 6: return 1;
    case 5:
    case 7: return 2;
    default: return 0;
    }
}

/* Quantized load/store semantics mirror Dolphin's interpreter (the chassis
 * lockstep oracle) exactly:
 *  - psq_l/psq_st (non-indexed) require only HID2.LSQE; the indexed forms
 *    are never gated. PSE is not checked, and no alignment exceptions are
 *    raised (unaligned accesses go straight to memory).
 *  - Invalid GQR types 1-3 load 0.0 into both lanes and store nothing.
 *  - Dequantization: f32(int) * f32 power-of-two, rounded once to f32.
 *  - Quantization: round the lane to f32 first, multiply by the f32
 *    power-of-two scale, clamp in f32, truncate. NaN quantizes to 0
 *    (matching SType(NaN-after-clamp) in release Dolphin on arm64). */
static f64 psq_dequant(f64 value, s32 scale) {
    if (scale == 0)
        return (f64)(f32)value;
    return (f64)(f32)ldexp(value, -scale);
}

static f64 psq_load_value(CPUState* cpu, u32 ea, u8 type, s32 scale) {
    switch (type) {
    case 0:
        return f64_value(convert_to_double(mem_read32(cpu, ea)));
    case 4:
        return psq_dequant((f64)mem_read8(cpu, ea), scale);
    case 5:
        return psq_dequant((f64)mem_read16(cpu, ea), scale);
    case 6:
        return psq_dequant((f64)(s8)mem_read8(cpu, ea), scale);
    case 7:
        return psq_dequant((f64)(s16)mem_read16(cpu, ea), scale);
    default:
        return 0.0;
    }
}

static s64 psq_quantize_int(f64 value, s64 min_value, s64 max_value, s32 scale) {
    f32 conv = (f32)value * ldexpf(1.0f, scale);
    if (isnan(conv))
        return 0;
    if (conv <= (f32)min_value)
        return min_value;
    if (conv >= (f32)max_value)
        return max_value;
    return (s64)conv;
}

static void psq_store_value(CPUState* cpu, u32 ea, u8 type, s32 scale, f64 value) {
    switch (type) {
    case 0:
        mem_write32(cpu, ea, convert_to_single_ftz(f64_bits(value)));
        break;
    case 4:
        mem_write8(cpu, ea, (u8)psq_quantize_int(value, 0, 255, scale));
        break;
    case 5:
        mem_write16(cpu, ea, (u16)psq_quantize_int(value, 0, 65535, scale));
        break;
    case 6:
        mem_write8(cpu, ea, (u8)(s8)psq_quantize_int(value, -128, 127, scale));
        break;
    case 7:
        mem_write16(cpu, ea, (u16)(s16)psq_quantize_int(value, -32768, 32767, scale));
        break;
    }
}

static bool psq_check_enabled(CPUState* cpu, bool indexed, u32 cia) {
    if (!indexed && (cpu->hid2 & PPC_HID2_LSQE) == 0) {
        ppc_program_exception(cpu, PPC_PROGRAM_ILLEGAL, cia);
        return false;
    }
    return true;
}

bool ppc_psq_load(CPUState* cpu, u8 frD, u32 ea, bool w, u8 gqr_index, bool indexed, u32 cia) {
    if (!psq_check_enabled(cpu, indexed, cia))
        return false;

    u32 gqr = cpu->gqr[gqr_index & 7u];
    s32 scale = gqr_scale(gqr >> 24);
    u8 type = (u8)((gqr >> 16) & 7u);
    u32 size = psq_type_size(type);
    if (size == 0) { /* invalid GQR type: both lanes read as 0.0 */
        cpu->fpr[frD] = 0.0;
        cpu->ps1[frD] = 0.0;
        return true;
    }

    cpu->fpr[frD] = psq_load_value(cpu, ea, type, scale);
    cpu->ps1[frD] = w ? 1.0 : psq_load_value(cpu, ea + size, type, scale);
    return true;
}

bool ppc_psq_store(CPUState* cpu, u8 frS, u32 ea, bool w, u8 gqr_index, bool indexed, u32 cia) {
    if (!psq_check_enabled(cpu, indexed, cia))
        return false;

    u32 gqr = cpu->gqr[gqr_index & 7u];
    s32 scale = gqr_scale(gqr >> 8);
    u8 type = (u8)(gqr & 7u);
    u32 size = psq_type_size(type);
    if (size == 0) /* invalid GQR type: nothing is stored */
        return true;

    psq_store_value(cpu, ea, type, scale, cpu->fpr[frS]);
    if (!w)
        psq_store_value(cpu, ea + size, type, scale, cpu->ps1[frS]);
    return true;
}

