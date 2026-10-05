// Fixture-only synchronization; not present in production configuration.
#ifndef BLUEWAKE_AUDIO_PREVIEW_TEST_API_H
#define BLUEWAKE_AUDIO_PREVIEW_TEST_API_H
#ifndef BLUEWAKE_AUDIO_PREVIEW_TEST
#error Test API is fixture-only
#endif
#include "audio_preview.h"
#ifdef __cplusplus
extern "C" {
#endif
void bluewake_audio_preview_test_hold_decode(BwAudioPreview*, bool);
bool bluewake_audio_preview_test_decode_waiting(BwAudioPreview*);
void bluewake_audio_preview_test_hold_mix(BwAudioPreview*, bool);
bool bluewake_audio_preview_test_mix_waiting(BwAudioPreview*);
#ifdef __cplusplus
}
#endif
#endif
