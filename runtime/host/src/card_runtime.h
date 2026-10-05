#ifndef BLUEWAKE_CARD_RUNTIME_H
#define BLUEWAKE_CARD_RUNTIME_H

#include "core/cpu.h"
#include "gxruntime/memory_card.h"

#include <stdbool.h>

// Opens BlueWake's default-on slot-A card. An explicit path is intended for
// isolated tests; NULL selects the per-user macOS application-support path.
bool bluewake_card_runtime_open(const char* explicit_path);
void bluewake_card_runtime_close(void);
void bluewake_card_runtime_suspend_writes(bool suspend);
const char* bluewake_card_runtime_path(void);
// UI snapshot bridge. A successful begin holds the dispatch/close mutex;
// end must run on the same thread on every success or failure path. The raw
// expected path must match the active backend. This protects persistent bytes,
// rather than establishing completion of a multi-write native game save.
bool bluewake_card_runtime_begin_snapshot(const char* expected_path);
void bluewake_card_runtime_end_snapshot(void);

// How much the guest has asked the card to write. The in-game save is the
// guest's own write of its own buffer. These count write attempts, including
// failures; they do not establish a completed save. Check the guest's save
// acknowledgement and the card file contents separately.
u64 bluewake_card_runtime_write_calls(void);
u64 bluewake_card_runtime_write_bytes(void);

// Services queued asynchronous completions and GZLE01's public CARD SDK
// entry points while leaving the retail synchronous wrappers in control.
void bluewake_card_runtime_service_callback(CPUState* cpu);
bool bluewake_card_runtime_intercepts(u32 address);
bool bluewake_card_runtime_dispatch(CPUState* cpu);

#endif
