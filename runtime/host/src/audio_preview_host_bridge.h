// SPDX-License-Identifier: GPL-3.0-or-later
// Windows preview lifetime bridge; bind once across UI and audio use.
#ifndef BLUEWAKE_AUDIO_PREVIEW_HOST_BRIDGE_H
#define BLUEWAKE_AUDIO_PREVIEW_HOST_BRIDGE_H
#include "audio_preview.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Bind once before UI/game/audio workers start. Clear only after they stop.
 * This pointer never changes while any consumer can access it. A null handle
 * is supported: preview allocation failure never prevents native gameplay. */
void bluewake_host_audio_preview_bind(BwAudioPreview* preview);
BwAudioPreview* bluewake_host_audio_preview_get(void);
#ifdef __cplusplus
}
#endif
#endif
