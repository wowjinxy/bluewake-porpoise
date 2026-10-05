// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "core/cpu.h"
#include "audio_customization.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exact production owner/reader functions, not a mirrored implementation. */
static u64 g_host_retrace_count;
#include "audio_owner_under_test.inc"

int main(void) {
    CPUState cpu = {0}, clone = {0};
    cpu.ram_size = 0x01800000u;
    cpu.ram = calloc(1, cpu.ram_size);
    assert(cpu.ram != NULL);
    ppc_guest_alias_clear();
    host_audio_owner_attach(&cpu);
    const u64 first_epoch = host_audio_owner_epoch(&cpu);
    assert(first_epoch != 0u);
    const u32 address = 0x803F7520u;
    const u8 native[] = {0x12, 0x34, 0x80, 0x00, 0xFF, 0xFE, 0x56, 0x78};
    for (u32 i = 0; i < sizeof native; ++i) mem_write8(&cpu, address + i, native[i]);
    u8 out[sizeof native] = {0};
    assert(host_audio_owner_read(&cpu, address, out, sizeof out));
    assert(memcmp(out, native, sizeof out) == 0);
    assert(host_audio_owner_epoch(&cpu) == first_epoch);
    memset(out, 0xA5, sizeof out);
    assert(!host_audio_owner_read(&cpu, address, NULL, sizeof out));
    assert(!host_audio_owner_read(&cpu, address, out, 0));
    assert(!host_audio_owner_read(&cpu, 0x7FFFFFFFu, out, sizeof out));
    assert(!host_audio_owner_read(&cpu, 0x817FFFFCu, out, sizeof out));
    assert(!host_audio_owner_read(&cpu, address, out, 0x01800001u));
    assert(!host_audio_owner_read(NULL, address, out, sizeof out));
    for (u32 i = 0; i < sizeof out; ++i) assert(out[i] == 0xA5);

    clone = cpu; /* Same RAM is insufficient to authorize a private CPU clone. */
    assert(host_audio_owner_epoch(&clone) == 0u);
    assert(!host_audio_owner_read(&clone, address, out, sizeof out));
    u8* original_ram = cpu.ram;
    cpu.ram = calloc(1, cpu.ram_size);
    assert(cpu.ram != NULL);
    assert(host_audio_owner_epoch(&cpu) == 0u);
    assert(!host_audio_owner_read(&cpu, address, out, sizeof out));
    host_audio_owner_attach(&cpu);
    assert(host_audio_owner_epoch(&cpu) != first_epoch);
    free(original_ram);

    const u64 before_alias = host_audio_owner_epoch(&cpu);
    u8 alias[] = {0x80, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    assert(ppc_guest_alias_add_shared(address, sizeof alias, alias));
    assert(host_audio_owner_epoch(&cpu) != before_alias);
    assert(host_audio_owner_read(&cpu, address, out, sizeof out));
    assert(memcmp(out, alias, sizeof out) == 0);
    const u64 active_alias = host_audio_owner_epoch(&cpu);
    ppc_guest_alias_clear();
    assert(host_audio_owner_epoch(&cpu) != active_alias);
    assert(host_audio_owner_read(&cpu, address, out, sizeof out));
    for (u32 i = 0; i < sizeof out; ++i) assert(out[i] == 0);

    /* Rotation samples the actual callback seam while retaining CPU ownership,
     * the atomic preference tuple and untouched scratch samples/guest bytes.
     * This intentionally invalid graph is not native music-owner evidence. */
    assert(bluewake_audio_configure(45, 75, 65, true));
    BwAudioConfiguration config_before, config_after;
    bluewake_audio_configuration(&config_before);
    bluewake_audio_diagnostics_enable(true);
    int16_t scratch[0x50], scratch_before[0x50];
    for (unsigned i = 0; i < 0x50; ++i) scratch_before[i] = scratch[i] = (int16_t)(i - 40);
    assert(!bluewake_audio_customize_voice(0x86840740u, 0u, 0u, scratch, 0x50));
    BwAudioDiagnostics diagnostics;
    bluewake_audio_diagnostics(&diagnostics);
    assert(diagnostics.enabled && diagnostics.callbacks == 1 && diagnostics.identity_count == 1);
    const u64 rotation_epoch = host_audio_owner_epoch(&cpu);
    const u64 first_window = g_audio_diagnostic_window;
    g_host_retrace_count = 2000;
    host_audio_diagnostics_retrace(); /* Disabled production trace is inert. */
    assert(g_audio_diagnostic_window == first_window && g_audio_diagnostic_reports == 0);
    bluewake_audio_diagnostics(&diagnostics);
    assert(diagnostics.callbacks == 1);
    g_audio_category_trace = true;
    g_host_retrace_count = 1999;
    host_audio_diagnostics_retrace();
    assert(g_audio_diagnostic_window == first_window && g_audio_diagnostic_reports == 0);
    for (unsigned i = 0; i < 8; ++i) {
        g_host_retrace_count = (i + 1u) * 2000u;
        host_audio_diagnostics_retrace();
        assert(g_audio_diagnostic_window == first_window + i + 1u && g_audio_diagnostic_reports == i + 1u);
        bluewake_audio_diagnostics(&diagnostics);
        assert(diagnostics.enabled && diagnostics.callbacks == 0 && diagnostics.identity_count == 0);
        assert(host_audio_owner_epoch(&cpu) == rotation_epoch);
        assert(host_audio_owner_read(&cpu, address, out, sizeof out));
        for (u32 j = 0; j < sizeof out; ++j) assert(out[j] == 0);
        bluewake_audio_configuration(&config_after);
        assert(config_after.master_percent == config_before.master_percent &&
               config_after.music_percent == config_before.music_percent &&
               config_after.sfx_percent == config_before.sfx_percent &&
               config_after.muted == config_before.muted && config_after.generation == config_before.generation);
        assert(!bluewake_audio_customize_voice(0x86840740u, 0u, 0u, scratch, 0x50));
    }
    g_host_retrace_count = 18000;
    host_audio_diagnostics_retrace();
    bluewake_audio_diagnostics(&diagnostics);
    assert(g_audio_diagnostic_reports == 8 && g_audio_diagnostic_window == first_window + 8 && diagnostics.callbacks == 1);
    assert(memcmp(scratch, scratch_before, sizeof scratch) == 0);
    g_audio_category_trace = false;
    bluewake_audio_diagnostics_enable(false);

    const u64 before_reset = host_audio_owner_epoch(&cpu);
    host_audio_owner_attach(NULL); /* Revocation occurs before old RAM changes. */
    assert(host_audio_owner_epoch(&cpu) == 0u);
    assert(!host_audio_owner_read(&cpu, address, out, sizeof out));
    BwAudioVoiceInfo owner;
    assert(!bluewake_audio_classify_voice(0x86840740u, 0u, 0u, &owner));
    host_audio_owner_attach(&cpu); /* Same RAM after reset has a new lifetime. */
    assert(host_audio_owner_epoch(&cpu) != before_reset);
    host_audio_owner_attach(NULL);
    free(cpu.ram);
    puts("Audio ownership: exact production reader, bounds, CPU/RAM lifetime, native aliases and revocation passed");
    return 0;
}
