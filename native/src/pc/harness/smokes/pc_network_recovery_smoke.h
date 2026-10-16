// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit private Windows diagnostics; ordinary save/load callers are no-ops.
#pragma once
namespace pc {
void networkRecoveryFrame(unsigned int frames);
bool networkRecoveryLoadComplete(unsigned int frames);
void networkRecoveryBeforeCardRead();
bool networkRecoveryReadMayPoll();
bool networkRecoveryCardReadResult(int result);
void networkRecoveryBeforeCardWrite();
bool networkRecoveryWriteMayRetry();
bool networkRecoveryWriteMayPoll();
bool networkRecoveryCardWriteResult(int result);
bool networkRecoverySaveComplete(unsigned int frames);
} // namespace pc
