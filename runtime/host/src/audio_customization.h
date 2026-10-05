// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_AUDIO_CUSTOMIZATION_H
#define BLUEWAKE_AUDIO_CUSTOMIZATION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Music/SFX gain runs on individual native HLE voices,
 * after native resampling/filter history and before dry/reverb mixing. Master
 * runs exactly once on a COPIED final stereo buffer; never guest PCM or VPBs.
 * Music includes native main/background sub BGM and owned streams. Verified
 * reward IDs in the sub handle retain fanfare category unity.
 * Fanfares/unknown owners retain category unity; voices inherit SFX.
 * Category controls are not implemented for LLE. Unity bypasses arithmetic. */
typedef enum BwAudioCategory {
    BW_AUDIO_UNKNOWN, BW_AUDIO_MUSIC, BW_AUDIO_SFX, BW_AUDIO_FANFARE,
    BW_AUDIO_STREAM
} BwAudioCategory;
typedef struct BwAudioConfiguration {
    unsigned master_percent, music_percent, sfx_percent;
    bool muted;
    uint32_t generation;
} BwAudioConfiguration;
typedef struct BwAudioVoiceInfo {
    BwAudioCategory category;
    uint32_t dsp_channel, logical_channel, track, root_track, sound, sound_id;
    uint64_t epoch;
} BwAudioVoiceInfo;
enum { BW_AUDIO_DIAGNOSTIC_IDENTITIES = 32, BW_AUDIO_CATEGORY_COUNT = 5 };
typedef struct BwAudioDiagnosticIdentity {
    uint32_t ucode_crc, vpb_base;
    uint16_t voice_id;
    bool classified;
    BwAudioVoiceInfo owner; /* UNKNOWN may contain only validated partial evidence. */
    uint64_t callbacks;
} BwAudioDiagnosticIdentity;
typedef struct BwAudioDiagnostics {
    bool enabled;
    uint64_t epoch, callbacks, classifications;
    uint64_t category_callbacks[BW_AUDIO_CATEGORY_COUNT];
    uint64_t unrecorded_callbacks;
    unsigned identity_count;
    BwAudioDiagnosticIdentity identities[BW_AUDIO_DIAGNOSTIC_IDENTITIES];
} BwAudioDiagnostics;
/* Reader must be alias-aware, bounds-check the complete range, and return
 * original big-endian bytes. Epoch identifies the actual CPU/RAM lifetime and
 * changes before reset/load/reattach/alias replacement. Zero is invalid.
 * Attach/reset/detach/classify/customize execute on the SAME game thread as
 * synchronous HLE rendering. No UI thread guest reads or cached raw pointers.
 * The epoch is checked around every classification; no ownership is cached.
 * The adapter already permits only one active HLE instance. */
typedef bool (*BwAudioGuestReadFn)(void* user, uint32_t address, void* out, size_t size);
typedef uint64_t (*BwAudioEpochFn)(void* user);
bool bluewake_audio_configure(unsigned master, unsigned music, unsigned sfx, bool muted);
void bluewake_audio_configuration(BwAudioConfiguration* out); /* UI safe */
void bluewake_audio_attach(BwAudioGuestReadFn read, BwAudioEpochFn epoch, void* user);
void bluewake_audio_reset(void); /* Cancels reader ownership; attach again. */
/* Opt-in bounded ownership evidence. Enable/snapshot are GAME THREAD ONLY,
 * on the same thread as attach/reset/the synchronous HLE callback. No logging,
 * allocation, guest writes or UI reads occur here. Enabling/disabling, attach,
 * reset and an observed epoch change clear the window; enable policy survives
 * attach/reset. Copy a snapshot on that thread before sending it elsewhere.
 * Standalone classify calls do not count as real mixer callbacks. With this
 * disabled and category unity, customize returns after one atomic config load
 * with no guest reads, classification or sample arithmetic. */
void bluewake_audio_diagnostics_enable(bool enabled);
void bluewake_audio_diagnostics(BwAudioDiagnostics* out);
bool bluewake_audio_classify_voice(uint32_t ucode_crc, uint32_t vpb_base,
                                  uint16_t voice_id, BwAudioVoiceInfo* out);
/* Called once per REAL donor AddVoice scratch buffer, after both filters.
 * count must be the donor's 0x50 samples. Reapplying to the same scratch buffer
 * is forbidden: this is a mixer callback, not a replayable instruction hook.
 * Returns true only when samples changed; false also means valid unity. */
bool bluewake_audio_customize_voice(uint32_t ucode_crc, uint32_t vpb_base,
                                   uint16_t voice_id, int16_t* samples, size_t count);
/* Final DMA read-copy boundary, after R/L normalization and before capture /
 * PCM decode / platform queue. Also covers reverb tails, unknown voices, LLE
 * and future replacement PCM. Never call again in the platform sink.
 * Returns true only when samples changed; false also means valid unity. */
bool bluewake_audio_customize_output_be16(uint8_t* stereo, size_t bytes);
#ifdef __cplusplus
}
#endif
#endif
