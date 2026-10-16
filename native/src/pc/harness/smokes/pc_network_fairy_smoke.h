// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit private native STEP diagnostics; ordinary callers are no-ops.
#pragma once
namespace pc {
void networkFairyFrame(unsigned int frames);
void networkFairyBeforeCardWrite();
bool networkFairySaveComplete(unsigned int frames);
bool networkFairyLoadComplete(unsigned int frames);
} // namespace pc
