#include "fps_watch.h"
#include "gxruntime/aurora_backend.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <io.h>
#define test_dup _dup
#define test_dup2 _dup2
#define test_close _close
#define test_fileno _fileno
#else
#include <unistd.h>
#define test_dup dup
#define test_dup2 dup2
#define test_close close
#define test_fileno fileno
#endif

static bool g_fast, g_smooth;
static int g_steps = 1;
static DolAuroraFrameTiming g_timing;
static unsigned g_pipelines, g_timing_reads, g_wall_reads, g_cpu_reads;
static unsigned long long g_wall = 1000000ull, g_thread_cpu = 500000ull;

bool bluewake_fast_load_fast_forward(void) { return g_fast; }
bool aurora_get_frame_interpolation(void) { return g_smooth; }
int aurora_get_frame_interp_steps(void) { return g_steps; }
void dol_aurora_frame_timing(DolAuroraFrameTiming* out) { ++g_timing_reads; *out = g_timing; }
unsigned bluewake_host_pipelines_created(void) { return g_pipelines; }

#ifdef BLUEWAKE_FPS_WATCH_TEST
unsigned long long bluewake_fps_watch_test_now_us(int clock) {
    if (clock == CLOCK_THREAD_CPUTIME_ID) {
        ++g_cpu_reads;
        return g_thread_cpu;
    }
    assert(clock == CLOCK_MONOTONIC);
    ++g_wall_reads;
    return g_wall;
}
#endif

static void test_env(const char* name, const char* value) {
#if defined(_WIN32)
    assert(_putenv_s(name, value) == 0);
#else
    assert(setenv(name, value, 1) == 0);
#endif
}

typedef struct LogCapture { FILE* file; int saved_stderr; } LogCapture;

static LogCapture capture_begin(void) {
    LogCapture capture = {tmpfile(), -1};
    assert(capture.file != NULL);
    fflush(stderr);
    capture.saved_stderr = test_dup(test_fileno(stderr));
    assert(capture.saved_stderr >= 0);
    assert(test_dup2(test_fileno(capture.file), test_fileno(stderr)) >= 0);
    return capture;
}

static long capture_position(const LogCapture* capture) {
    fflush(stderr);
    const long position = ftell(capture->file);
    assert(position >= 0);
    return position;
}

static char* capture_end(LogCapture* capture) {
    const long size = capture_position(capture);
    assert(size < 65536);
    assert(test_dup2(capture->saved_stderr, test_fileno(stderr)) >= 0);
    test_close(capture->saved_stderr);
    assert(fseek(capture->file, 0, SEEK_SET) == 0);
    char* result = malloc((size_t)size + 1u);
    assert(result != NULL);
    assert(fread(result, 1, (size_t)size, capture->file) == (size_t)size);
    result[size] = '\0';
    fclose(capture->file);
    return result;
}

static unsigned occurrences(const char* text, const char* needle) {
    unsigned count = 0;
    for (const char* at = text; (at = strstr(at, needle)) != NULL; at += strlen(needle)) ++count;
    return count;
}

static void stage(CPUState* cpu, const char* name) {
    for (unsigned i = 0; i < 8; ++i)
        mem_write8(cpu, 0x803C9D3Cu + i, i < strlen(name) ? (u8)name[i] : 0);
}

static void tick(void) {
    // Match main.c: service runs before this retrace's fresh one-second sample.
    bluewake_fps_watch_service();
    bluewake_fps_watch_retrace();
}

static void advance_second(void) {
    const unsigned long long wall_before = g_wall, cpu_before = g_thread_cpu;
    for (unsigned i = 1; i <= 60; ++i) {
        g_wall = wall_before + (unsigned long long)i * 1000000ull / 60ull;
        g_thread_cpu = cpu_before + (g_wall - wall_before) / 2ull;
        ++g_timing.shown;
        if (i % 2u == 0u) {
            ++g_timing.presents;
            ++g_timing.interp_frames;
            ++g_timing.interp_interpolated;
            g_timing.interp_draws += 10;
            g_timing.draws += 10;
            ++g_timing.display_copies;
        }
        g_timing.gx_worker_cpu_us += 1000;
        g_timing.interp_helper_cpu_us += 100;
        g_timing.render_worker_cpu_us += 200;
        g_timing.drain_us += 100;
        g_timing.present_us += 200;
        g_timing.end_frame_us += 50;
        if (i == 60) ++g_pipelines;
        tick();
    }
}

static int cause_is(const char* cause, const char* expected) { return strcmp(cause, expected) == 0; }

static void test_placement_survives_player_update(void) {
    test_env("BLUEWAKE_TEST_PLACE", "1:10:20:30");
    test_env("BLUEWAKE_FPS_WATCH", "0");
    CPUState cpu;
    assert(cpu_init(&cpu));
    const u32 player = 0x80010000u;
    mem_write32(&cpu, 0x803CA74Cu, player);
    bluewake_fps_watch_attach(&cpu);
    assert(bluewake_fps_watch_marker_status() == BLUEWAKE_FPS_MARKER_UNAVAILABLE);
    assert(!bluewake_fps_watch_mark_slowdown());
    const unsigned reads = g_wall_reads + g_cpu_reads + g_timing_reads;
    for (unsigned tick = 0; tick < 8; ++tick)
        bluewake_fps_watch_retrace();
    const u32 expected[3] = {0x41200000u, 0x41A00000u, 0x41F00000u};
    for (u32 axis = 0; axis < 3; ++axis) {
        // Link restores the retail position cache at the start of execute.
        mem_write32(&cpu, player + 0x1F8u + axis * 4u,
                    mem_read32(&cpu, 0x803E440Cu + axis * 4u));
        assert(mem_read32(&cpu, player + 0x1F8u + axis * 4u) == expected[axis]);
        assert(mem_read32(&cpu, player + 0x1E4u + axis * 4u) == expected[axis]);
    }
    // Once the bounded placement ends, normal player movement stays free.
    mem_write32(&cpu, player + 0x1F8u, 0x42200000u);
    bluewake_fps_watch_retrace();
    assert(mem_read32(&cpu, player + 0x1F8u) == 0x42200000u);
    bluewake_fps_watch_service();
    assert(g_wall_reads + g_cpu_reads + g_timing_reads == reads);
    bluewake_fps_watch_attach(NULL);
    cpu_free(&cpu);
}

#ifdef BLUEWAKE_FPS_WATCH_TEST
static void test_marker_history_and_deadline(void) {
    test_env("BLUEWAKE_TEST_PLACE", "");
    test_env("BLUEWAKE_FPS_WATCH", "1");
    CPUState cpu;
    assert(cpu_init(&cpu));
    stage(&cpu, "sea");
    mem_write32(&cpu, 0x803CA74Cu, 0x80010000u);
    mem_write8(&cpu, 0x803F6A78u, 13);
    mem_write8(&cpu, 0x803C9EA2u, 2);
    // Large valid floats must leave the trailing context flags intact.
    mem_write32(&cpu, 0x800101F8u, 0x7f7fffffu);
    memset(&g_timing, 0, sizeof g_timing);
    g_timing.audio_queued_ms = 25;
    g_pipelines = g_timing_reads = g_wall_reads = g_cpu_reads = 0;
    g_wall = 1000000ull;
    g_thread_cpu = 500000ull;
    g_fast = false;
    g_smooth = true;
    g_steps = 1;
    LogCapture capture = capture_begin();
    bluewake_fps_watch_attach(&cpu);
    assert(bluewake_fps_watch_marker_status() == BLUEWAKE_FPS_MARKER_IDLE);
    const unsigned idle_wall_reads = g_wall_reads;
    bluewake_fps_watch_service();
    assert(g_wall_reads == idle_wall_reads && g_timing_reads == 0 && g_cpu_reads == 0);
    tick(); // initial timing baseline
    for (unsigned i = 0; i < 16; ++i) advance_second();
    assert(g_timing_reads == 17 && g_cpu_reads == 17);
    assert(bluewake_fps_watch_mark_slowdown());
    assert(bluewake_fps_watch_marker_status() == BLUEWAKE_FPS_MARKER_PENDING);
    assert(bluewake_fps_watch_marker_count() == 1);
    assert(!bluewake_fps_watch_mark_slowdown());
    const long before = capture_position(&capture);
    bluewake_fps_watch_service();
    assert(capture_position(&capture) - before < 20 * 1024);
    assert(bluewake_fps_watch_marker_status() == BLUEWAKE_FPS_MARKER_CAPTURING);
    assert(!bluewake_fps_watch_mark_slowdown());
    assert(g_timing_reads == 17 && g_cpu_reads == 17);
    for (unsigned i = 0; i < 5; ++i) advance_second();
    // At the exact deadline service ran first, then the fifth sample completed it.
    assert(bluewake_fps_watch_marker_status() == BLUEWAKE_FPS_MARKER_COMPLETE);
    assert(g_timing_reads == 22 && g_cpu_reads == 22);
    assert(bluewake_fps_watch_marker_count() == 1);
    assert(bluewake_fps_watch_mark_slowdown());
    assert(bluewake_fps_watch_marker_count() == 2);
    const unsigned timing_reads = g_timing_reads, cpu_reads = g_cpu_reads;
    bluewake_fps_watch_test_finish(); // pending request during early quit
    assert(g_timing_reads == timing_reads && g_cpu_reads == cpu_reads);
    assert(bluewake_fps_watch_marker_status() == BLUEWAKE_FPS_MARKER_COMPLETE);
    char* log = capture_end(&capture);
    assert(occurrences(log, "id=1 phase=before") == 16);
    assert(occurrences(log, "id=1 phase=after") == 5);
    assert(strstr(log, "[slowdown] end id=1 after=5 partial=0 reason=complete") != NULL);
    assert(strstr(log, "[slowdown] end id=2 after=0 partial=1 reason=exit-partial") != NULL);
    const char* oldest = strstr(log, "wall_us=2000000 interval_ms=1000.0");
    const char* newest = strstr(log, "wall_us=17000000 interval_ms=1000.0");
    assert(oldest != NULL && newest != NULL && oldest < newest);
    assert(strstr(log, "vi_hz=60.0 game_fps=30.0 speed_pct=100.1 shown_fps=60.0") != NULL);
    assert(strstr(log, "max_retrace_wall_ms=16.7") != NULL);
    assert(strstr(log, "smooth=1 steps=1 target_fps=60 interp_frames=30 interpolated=30") != NULL);
    assert(strstr(log, "stage=sea room=13 event=2 pos=3.40282e+38,0,0") != NULL);
    assert(strstr(log, "reset=0 crosses_marker=0\n") != NULL);
    assert(strstr(log, "sim_fps=") == NULL);
    free(log);
    bluewake_fps_watch_attach(NULL);
    cpu_free(&cpu);
}

static void test_marker_reset_context_and_partial(void) {
    CPUState cpu;
    assert(cpu_init(&cpu));
    stage(&cpu, "sea_T");
    mem_write32(&cpu, 0x803CA74Cu, 0u); // no Link: position must not be dereferenced
    mem_write8(&cpu, 0x803F6A78u, 255u);
    mem_write8(&cpu, 0x803C9EA2u, 3u);
    memset(&g_timing, 0, sizeof g_timing);
    g_timing.presents = g_timing.shown = g_timing.draws = g_timing.display_copies = 1000;
    g_timing.interp_frames = g_timing.interp_interpolated = g_timing.interp_draws = 1000;
    g_timing.interp_rejected = g_timing.interp_unmatched = 1000;
    g_timing.drain_us = g_timing.present_us = g_timing.end_frame_us = 1000;
    g_timing.gx_worker_cpu_us = g_timing.interp_helper_cpu_us = g_timing.render_worker_cpu_us = 1000;
    g_timing.audio_throttles = g_timing.audio_dropped = g_timing.held_us = 1000;
    g_pipelines = 100;
    g_wall = 100000000ull;
    g_thread_cpu = 500000ull;
    g_smooth = false;
    g_fast = false;
    LogCapture capture = capture_begin();
    bluewake_fps_watch_attach(&cpu);
    tick();
    memset(&g_timing, 0, sizeof g_timing); // worker/backend restart decreases every cumulative field
    g_pipelines = 0;
    g_thread_cpu = 0;
    g_wall += 1000000ull;
    tick();
    g_fast = true;
    g_timing.held_us += 200000ull;
    advance_second();
    assert(bluewake_fps_watch_mark_slowdown());
    bluewake_fps_watch_service();
    const unsigned timing_reads = g_timing_reads, cpu_reads = g_cpu_reads;
    // Held retraces service the request but cannot collect a new sample.
    g_wall += 6000000ull;
    bluewake_fps_watch_service();
    assert(g_timing_reads == timing_reads && g_cpu_reads == cpu_reads);
    assert(bluewake_fps_watch_marker_status() == BLUEWAKE_FPS_MARKER_COMPLETE);
    char* log = capture_end(&capture);
    assert(occurrences(log, "id=1 phase=before") == 2);
    assert(strstr(log, "game_fps=0.0") != NULL);
    assert(strstr(log, "cpu=0.0 gx_cpu=0.0 interp_cpu=0.0 render_cpu=0.0") != NULL);
    assert(strstr(log, "gx_wait_ms=0.0 present_ms=0.0 submit_drawable_ms=0.0") != NULL);
    assert(strstr(log, "max_retrace_wall_ms=1000.0") != NULL);
    assert(strstr(log, "stage=sea_T room=-1 event=3 pos=0,0,0") != NULL);
    assert(strstr(log, "held=0 fast_forward=0 title_stage=1 no_link=1 no_present=1 reset=1") != NULL);
    assert(strstr(log, "held=1 fast_forward=1 title_stage=1 no_link=1") != NULL);
    assert(strstr(log, "[slowdown] end id=1 after=0 partial=1 reason=deadline-no-sample-partial") != NULL);
    free(log);
    assert(bluewake_fps_watch_mark_slowdown());
    capture = capture_begin();
    bluewake_fps_watch_service();
    bluewake_fps_watch_test_finish(); // already capturing early quit
    log = capture_end(&capture);
    assert(strstr(log, "end id=2 after=0 partial=1 reason=exit-partial") != NULL);
    free(log);
    bluewake_fps_watch_attach(NULL);
    cpu_free(&cpu);
    g_fast = false;
}
#endif

int main(void) {
    assert(bluewake_fps_watch_cpu_percent(750000, 250000, 1000000) == 50.0);
    assert(bluewake_fps_watch_cpu_percent(250000, 250000, 1000000) == 0.0);
    assert(bluewake_fps_watch_cpu_percent(10, 250000, 1000000) == 0.0);
    assert(bluewake_fps_watch_cpu_percent(750000, 250000, 0) == 0.0);
    assert(bluewake_fps_watch_reason(29.3, 1.0, false, 3, 0, 0) == NULL);
    assert(bluewake_fps_watch_reason(59.0, 1.0, true, 1, 30, 30) == NULL);
    assert(bluewake_fps_watch_reason(118.0, 1.0, true, 3, 30, 30) == NULL);
    assert(strcmp(bluewake_fps_watch_reason(20, 0.7, false, 1, 0, 0),
                  "game below full speed") == 0);
    assert(strcmp(bluewake_fps_watch_reason(30, 1.0, true, 1, 30, 0),
                  "frames not interpolated") == 0);
    assert(strcmp(bluewake_fps_watch_reason(50, 1.0, true, 1, 30, 30),
                  "presents late") == 0);
    assert(strcmp(bluewake_fps_watch_reason(60, 1.0, true, 3, 30, 30),
                  "presents late") == 0); // does not confuse 60 with requested 120
    assert(bluewake_fps_watch_reason(59, 1.0, true, 0, 30, 30) == NULL);
    assert(bluewake_fps_watch_reason(118, 1.0, true, 99, 30, 30) == NULL);
    // The iPad with Smooth Motion dropped: 60 shown (each frame twice), none of the
    // 30 game frames interpolated. Previously no dip because shown looked fine.
    assert(strcmp(bluewake_fps_watch_reason(60, 1.0, true, 1, 30, 0), "frames not interpolated") == 0);
    assert(strcmp(bluewake_fps_watch_reason(118, 1.0, true, 3, 30, 0), "frames not interpolated") == 0);
    // ... and the same while the game itself was slow (the reason the frames were dropped).
    assert(strcmp(bluewake_fps_watch_reason(51, 0.86, true, 1, 26, 0), "game below full speed") == 0);
    // Smooth Motion off: repeated frames are expected, no dip.
    assert(bluewake_fps_watch_reason(30, 1.0, false, 1, 30, 0) == NULL);
    // The Forsaken Fortress dips in a player's 0.4.0 log: game at 75%, GX worker
    // 95%, game thread about half, 450 ms of each second waiting on the worker.
    assert(cause_is(bluewake_fps_watch_cause(0.75, 55, 95, 0, 20, 450, 3, 0), "gx-worker"));
    // The same wait while pipelines were made is a shader compile.
    assert(cause_is(bluewake_fps_watch_cause(0.75, 55, 95, 0, 20, 450, 3, 4), "shader-compile"));
    // Pipelines made in the background, the game not waiting: not a compile stall.
    assert(cause_is(bluewake_fps_watch_cause(0.80, 95, 30, 0, 20, 10, 3, 4), "game-thread"));
    // BLUEWAKE_TEST_STALL on a Mac: 150 ms and 900 ms holds on the game thread
    // (speed 86% and 47%, the thread 73% and 85% busy, almost no waiting).
    assert(cause_is(bluewake_fps_watch_cause(0.86, 73, 33, 0, 6, 5, 2, 0), "game-thread"));
    assert(cause_is(bluewake_fps_watch_cause(0.47, 85, 19, 0, 4, 22, 1, 0), "game-thread"));
    assert(cause_is(bluewake_fps_watch_cause(1.0, 40, 30, 0, 20, 5, 400, 0), "gpu-present"));
    assert(cause_is(bluewake_fps_watch_cause(1.0, 40, 30, 0, 90, 5, 40, 0), "render-worker"));
    assert(cause_is(bluewake_fps_watch_cause(1.0, 40, 30, 92, 20, 5, 40, 0), "interp-helper"));
    // Full speed, nothing saturated: presents late for no visible reason.
    assert(cause_is(bluewake_fps_watch_cause(1.0, 40, 30, 0, 20, 5, 40, 0), "unclear"));
    test_placement_survives_player_update();
#ifdef BLUEWAKE_FPS_WATCH_TEST
    test_marker_history_and_deadline();
    test_marker_reset_context_and_partial();
#endif
    return 0;
}
