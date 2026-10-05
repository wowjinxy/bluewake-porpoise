#ifndef BLUEWAKE_SPRINT_H
#define BLUEWAKE_SPRINT_H

#include "core/cpu.h"
#include "sprint_input.h"

#ifdef __cplusplus
extern "C" {
#endif

// Sprint speeds native running and its animation. Desktop preferences choose
// Hold or Toggle independently: keyboard defaults to Hold; controller defaults
// to Toggle until the next click or eight idle retraces. Touch stays Hold.
//
//   BLUEWAKE_SPRINT_SPEED=1.5          how much faster (1: off)
//   BLUEWAKE_SPRINT_TRACE=1            log it and Link's speed
//   BLUEWAKE_SPRINT_TEST=retrace:n     testing: Shift held for n retraces

void bluewake_sprint_attach(CPUState* cpu);
// Once per retrace, on the thread that pumps SDL's events.
void bluewake_sprint_retrace(void);
void bluewake_sprint_touch(bool down);
// Game-thread cancellation before suspended input or a machine mutation.
void bluewake_sprint_cancel(void);
// Clears baselines/latches on a reset. NULL detaches without accessing old RAM.
void bluewake_sprint_reset(CPUState* cpu);
// UI-safe factor publication; the game thread applies cancellation at retrace.
void bluewake_sprint_reload(void);

#ifdef __cplusplus
}
#endif

#endif
