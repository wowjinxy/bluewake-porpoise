// SPDX-License-Identifier: GPL-3.0-or-later
// Host-only external-file music preview. No native music calls.
#ifndef BLUEWAKE_AUDIO_PREVIEW_H
#define BLUEWAKE_AUDIO_PREVIEW_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BwAudioPreview BwAudioPreview;
typedef enum BwAudioPreviewState {
    BW_PREVIEW_IDLE, BW_PREVIEW_LOADING, BW_PREVIEW_READY, BW_PREVIEW_PLAYING,
    BW_PREVIEW_FINISHED, BW_PREVIEW_STOPPED, BW_PREVIEW_CANCELLED,
    BW_PREVIEW_RESET, BW_PREVIEW_FAILED, BW_PREVIEW_RATE_MISMATCH,
    BW_PREVIEW_SHUTDOWN
} BwAudioPreviewState;
enum { BW_PREVIEW_PATH_BYTES=4096, BW_PREVIEW_ERROR_BYTES=256,
       BW_PREVIEW_MAX_OUTPUT_BYTES=16384, BW_PREVIEW_MAX_SECONDS=300 };
typedef struct BwAudioPreviewStatus {
    BwAudioPreviewState state;
    bool accepting_requests, looping;
    uint64_t generation, total_frames, played_frames, cursor_frame;
    uint64_t loop_start, loop_end;
    unsigned sample_rate, observed_output_rate;
    char path_utf8[BW_PREVIEW_PATH_BYTES], error[BW_PREVIEW_ERROR_BYTES];
} BwAudioPreviewStatus;

/* Startup/UI lifecycle. One handle has one decoder worker and ONE copied-PCM
 * consumer, normally the game thread. No HLE instance, guest memory, SDL,
 * audio device or native bgm function is used. Newly created handles are OFF.
 * play/stop/cancel/status may run on UI while the single consumer runs.
 * reset cancels current/pending preview before game RAM/alias/state mutation.
 * shutdown cancels publications and joins worker; handle remains inert/valid.
 * destroy only after UI and copied-output consumers have stopped. */
BwAudioPreview* bluewake_audio_preview_create(void);
bool bluewake_audio_preview_play_utf8(BwAudioPreview*, const char* path_utf8);
/* Optional loop plays intro once, then [start,end) repeats. Bounds are stereo
 * frame indices at the file's native rate; worker validates against PCM size. */
bool bluewake_audio_preview_play_loop_utf8(BwAudioPreview*, const char* path_utf8,
                                          uint64_t start, uint64_t end);
void bluewake_audio_preview_stop(BwAudioPreview*);
void bluewake_audio_preview_cancel(BwAudioPreview*);
void bluewake_audio_preview_reset(BwAudioPreview*);
void bluewake_audio_preview_shutdown(BwAudioPreview*);
void bluewake_audio_preview_destroy(BwAudioPreview*);
bool bluewake_audio_preview_status(BwAudioPreview*, BwAudioPreviewStatus*);

/* ONCE per copied output, AFTER native BE R,L→L,R normalization and BEFORE
 * bluewake_audio_customize_output_be16/capture/platform sink. Output remains
 * BE stereo L,R. Music is sampled from the existing atomic audio config and
 * applies once to preview. Master/mute apply once downstream to final output.
 * No mutex, allocation, destruction, file I/O or guest reads/writes here.
 * Off/pending/failed/stale/rate-mismatch leaves bytes bitwise unchanged.
 * Default play is one pass; optional loop is explicitly validated. No
 * resampling. Song end leaves the rest of that copied block native. Stop
 * cancels future/uncommitted publications;
 * already committed/queued output cannot be retroactively removed.
 * Returns whether copied samples changed, not whether preview is playing. */
bool bluewake_audio_preview_mix_be16(BwAudioPreview*, uint8_t* stereo,
                                    size_t bytes, unsigned output_rate);
#ifdef __cplusplus
}
#endif
#endif
