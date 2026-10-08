#ifndef BLUEWAKE_FPS_WATCH_H
#define BLUEWAKE_FPS_WATCH_H

#include "core/cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

// A [fps-dip] line when presentation falls below 95% of the selected 30/60/120
// FPS mode: the frames shown, the game's speed, how many game frames
// got an in-between frame, the draws rejected or unmatched, the stage, room and
// Link's position, and why (the game below full speed, frames not
// interpolated, or presents late). Smooth Motion counts as a dip whenever fewer than
// 90% of game frames were interpolated, even if repeated frames keep the shown count
// at the target. BLUEWAKE_FPS_WATCH=0 turns it off.

void bluewake_fps_watch_attach(CPUState* cpu);
// Once per retrace.
void bluewake_fps_watch_retrace(void);

enum BluewakeFpsMarkerStatus {
    BLUEWAKE_FPS_MARKER_IDLE = 0,
    BLUEWAKE_FPS_MARKER_PENDING = 1,
    BLUEWAKE_FPS_MARKER_CAPTURING = 2,
    BLUEWAKE_FPS_MARKER_COMPLETE = 3,
    BLUEWAKE_FPS_MARKER_UNAVAILABLE = 4
};
// Thread-safe request/status only. A second press while pending/capturing is
// rejected. Count is the number of accepted marker IDs in this attachment.
bool bluewake_fps_watch_mark_slowdown(void);
unsigned bluewake_fps_watch_marker_status(void);
unsigned bluewake_fps_watch_marker_count(void);
// Game thread only, including held retraces. Consumes requests and deadlines;
// no guest reads, worker queries or clock read while idle.
void bluewake_fps_watch_service(void);

// Pure classification shared with the synthetic regression. NULL means no dip.
const char* bluewake_fps_watch_reason(double shown, double speed, bool smooth,
                                    int steps, unsigned long long frames,
                                    unsigned long long interpolated);

// Which part held a second below target, from the game's speed (1.0 = full),
// that second's CPU use (percent of one core) and the game thread's waits
// (milliseconds): "shader-compile" (it waited on the GX worker while pipelines
// were made), "gx-worker", "gpu-present", "render-worker", "interp-helper",
// "game-thread" (the game ran slow without waiting on the others: its own work,
// or a hitch on its thread) or "unclear". Pure, shared with the regression.
const char* bluewake_fps_watch_cause(double speed, double game_busy, double gx_worker, double interp_helper,
                                    double render_worker, double gx_wait_ms, double present_ms,
                                    unsigned pipelines);

// Cumulative worker counters may reset when a worker exits or is replaced.
double bluewake_fps_watch_cpu_percent(unsigned long long current,
                                     unsigned long long previous,
                                     unsigned long long wall_us);

#ifdef BLUEWAKE_FPS_WATCH_TEST
// Exercises the same terminal path as atexit without ending the fixture.
void bluewake_fps_watch_test_finish(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
