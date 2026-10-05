// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "gxruntime/platform.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Camera context normally comes from the native camera and selected-device
// reader. Only those inputs and SDL's queue are substituted; the block, discard,
// event and PAD functions are extracted verbatim from production by CMake.
static bool g_blocked, g_click, g_stick_on, g_stick_mapped,g_enabled=true;
static bool g_stick_click_down, g_stick_zooms, g_stick_owns, g_stick_aims, g_first_person;
static bool g_click_release_guard,g_stick_click_release_guard,g_captured;
static SDL_WindowID g_window;
static unsigned long long g_exit_from, g_retrace;
static int g_subject_step;
static double g_sum_x, g_sum_y, g_wheel;
static double g_aim_yaw_rest;
static const double kStickInUse = 0.05;
static double sample_x, sample_y, sample_left_x, sample_left_y;
static bool sample_click, inverted_camera;
static unsigned stick_reads, release_calls;
static SDL_Event queued[8];
static unsigned queue_count,filter_calls;
void SDL_FilterEvents(SDL_EventFilter filter,void* user) {
    ++filter_calls;
    unsigned kept=0;
    for(unsigned i=0;i<queue_count;++i)if(filter(user,&queued[i]))queued[kept++]=queued[i];
    queue_count=kept;
}

static void set_captured(bool captured) {
    assert(!captured);
    ++release_calls;
    // A missing or uncaptured window does not clear state in set_captured.
    // The block function itself must discard queued synthetic input.
}

static void read_stick_left(double* x, double* y, bool* click, int* zoom,
                            double* left_x, double* left_y) {
    ++stick_reads;
    *x = sample_x; *y = sample_y; *click = sample_click;
    if (zoom != NULL) *zoom = 0;
    if (left_x != NULL) *left_x = sample_left_x;
    if (left_y != NULL) *left_y = sample_left_y;
}

static bool bluewake_game_options_invert_camera_x(void) { return inverted_camera; }
static bool bluewake_settings_menu_event(const void* e){(void)e;return false;}
static void bluewake_jump_button_event(const void* e){(void)e;}
static void bluewake_save_state_hotkey(bool load){(void)load;assert(false);}

#include "mouse_camera_block_under_test.inc"
#include "mouse_camera_discard_under_test.inc"
#include "mouse_camera_event_under_test.inc"
#include "mouse_camera_pad_under_test.inc"

static void reset(void) {
    g_blocked = g_click = g_stick_click_down = false;
    g_click_release_guard=g_stick_click_release_guard=g_captured=false;g_window=0;
    g_stick_on = g_stick_mapped = true;
    g_stick_zooms = g_stick_owns = g_stick_aims = g_first_person = false;
    g_exit_from = 0; g_retrace = 10; g_subject_step = 0;
    g_sum_x = g_sum_y = g_wheel = 0;
    g_aim_yaw_rest=0;queue_count=filter_calls=0;
    sample_x = sample_y = sample_left_x = sample_left_y = 0;
    sample_click = inverted_camera = false;
    stick_reads = release_calls = 0;
}

static DolPadState native_pad(void);
static void assert_unchanged(const DolPadState* expected,const DolPadState* actual);
static void autosave_discards_only_host_gestures(void) {
    reset();g_window=42;g_captured=true;g_stick_owns=true;sample_click=true;
    g_click=true;g_sum_x=12;g_sum_y=-9;g_wheel=3;g_aim_yaw_rest=.8;g_exit_from=8;
    queued[0]=(SDL_Event){0};queued[0].type=SDL_EVENT_MOUSE_MOTION;queued[0].motion.windowID=42;
    queued[1]=(SDL_Event){0};queued[1].type=SDL_EVENT_KEY_DOWN;queued[1].key.scancode=SDL_SCANCODE_F8;
    queued[2]=(SDL_Event){0};queued[2].type=SDL_EVENT_MOUSE_MOTION;queued[2].motion.windowID=99;
    queued[3]=(SDL_Event){0};queued[3].type=SDL_EVENT_MOUSE_WHEEL;queued[3].wheel.windowID=42;
    queued[4]=(SDL_Event){0};queued[4].type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    queued[4].button.windowID=42;queued[4].button.button=SDL_BUTTON_LEFT;
    queued[5]=(SDL_Event){0};queued[5].type=SDL_EVENT_MOUSE_BUTTON_UP;
    queued[5].button.windowID=42;queued[5].button.button=SDL_BUTTON_LEFT;
    queue_count=6;
    bluewake_mouse_camera_discard_input();
    assert(!g_blocked&&g_captured&&g_window==42&&release_calls==0&&filter_calls==1);
    assert(!g_click&&!g_stick_click_down&&g_click_release_guard&&g_stick_click_release_guard);
    assert(g_exit_from==0&&g_sum_x==0&&g_sum_y==0&&g_wheel==0&&g_aim_yaw_rest==0);
    assert(queue_count==3&&queued[0].type==SDL_EVENT_KEY_DOWN&&queued[0].key.scancode==SDL_SCANCODE_F8);
    assert(queued[1].motion.windowID==99&&queued[2].type==SDL_EVENT_MOUSE_BUTTON_UP);
    SDL_Event click={0};click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;click.button.button=SDL_BUTTON_LEFT;
    click.button.windowID=42;observe(&click,NULL);assert(!g_click&&g_click_release_guard);
    click.type=SDL_EVENT_MOUSE_BUTTON_UP;observe(&click,NULL);assert(!g_click&&!g_click_release_guard);
    bluewake_mouse_camera_discard_input();assert(!g_click_release_guard);
    click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;observe(&click,NULL);assert(g_click);
    bluewake_mouse_camera_discard_input();assert(!g_click&&g_click_release_guard);
    // Held camera click cannot invent first-person input after resuming.
    DolPadState pad=native_pad(),expected=pad;
    bluewake_mouse_camera_pad(&pad);assert_unchanged(&expected,&pad);
    sample_click=false;pad=native_pad();bluewake_mouse_camera_pad(&pad);
    assert(!g_stick_click_release_guard);
    sample_click=true;pad=native_pad();bluewake_mouse_camera_pad(&pad);assert(pad.substick_y==127);
    // Repeated drains cannot give the settings menu's input away or remove
    // its queue, and cannot call any capture/window helper.
    g_blocked=true;const unsigned before=filter_calls;
    bluewake_mouse_camera_discard_input();
    assert(g_blocked&&g_captured&&filter_calls==before&&queue_count==3&&release_calls==0);
}

static DolPadState native_pad(void) {
    DolPadState pad = {0};
    pad.button = 0x0020;
    pad.stick_x = 23; pad.stick_y = -42;
    pad.substick_x = 19; pad.substick_y = -17;
    pad.trigger_left = 31; pad.trigger_right = 87;
    pad.analog_a = 11; pad.analog_b = 9;
    return pad;
}

static void assert_unchanged(const DolPadState* expected, const DolPadState* actual) {
    assert(memcmp(expected,actual,sizeof *actual) == 0);
}

static void normal_click_and_exit(void) {
    reset();
    DolPadState pad = native_pad();
    g_stick_on = false; g_click = true;
    bluewake_mouse_camera_pad(&pad);
    assert(pad.button == 0x0120 && pad.substick_y == -17);
    assert(!g_click_release_guard&&!g_stick_click_release_guard);

    reset();
    pad = native_pad(); g_stick_owns = true; sample_click = true;
    bluewake_mouse_camera_pad(&pad);
    assert(pad.substick_x == 0 && pad.substick_y == 127 && g_exit_from == 0);

    reset();
    g_first_person = true; sample_click = true;
    pad = native_pad(); bluewake_mouse_camera_pad(&pad);
    assert(g_exit_from == 10 && pad.substick_x == 0 && pad.substick_y == -30);
    sample_click = false; g_subject_step = 1; ++g_retrace;
    pad = native_pad(); bluewake_mouse_camera_pad(&pad);
    assert(g_exit_from == 10 && pad.substick_x == 0 && pad.substick_y == -127);
    g_subject_step = 2; ++g_retrace;
    pad = native_pad(); const DolPadState expected = pad;
    bluewake_mouse_camera_pad(&pad);
    assert(g_exit_from == 0); assert_unchanged(&expected,&pad);

    // A fresh click still starts a later exit, and a stalled native transition
    // cannot inject synthetic C-stick input beyond the existing timeout.
    g_subject_step = 0; sample_click = true; ++g_retrace;
    pad = native_pad(); bluewake_mouse_camera_pad(&pad);
    assert(g_exit_from == 13 && pad.substick_y == -30);
    sample_click = false; g_retrace += 30;
    pad = native_pad(); const DolPadState after_timeout = pad;
    bluewake_mouse_camera_pad(&pad);
    assert(g_exit_from == 0); assert_unchanged(&after_timeout,&pad);
}

static void menu_cancels_synthetic_input(void) {
    reset();
    g_first_person = true; sample_click = true;
    DolPadState pad = native_pad(); bluewake_mouse_camera_pad(&pad);
    assert(g_exit_from != 0 && pad.substick_y == -30);
    g_click = true; g_sum_x = 7; g_sum_y = -4; g_wheel = 2;
    const unsigned before_reads = stick_reads;
    bluewake_mouse_camera_block(true);
    assert(g_blocked && !g_click && !g_stick_click_down && g_exit_from == 0);
    assert(g_sum_x == 0 && g_sum_y == 0 && g_wheel == 0 && release_calls == 1);

    // Even a held first-person action and moved sticks cannot restart the exit
    // while the menu owns input. Existing PAD content is left untouched.
    sample_x = sample_left_y = 1;
    pad = native_pad(); const DolPadState expected = pad;
    bluewake_mouse_camera_pad(&pad); assert_unchanged(&expected,&pad);
    memset(&pad,0,sizeof pad); const DolPadState neutral = pad;
    bluewake_mouse_camera_pad(&pad); assert_unchanged(&neutral,&pad);
    assert(stick_reads == before_reads && g_exit_from == 0);

    // Closing without a fresh click cannot revive the cancelled click or exit.
    sample_click = false; sample_x = sample_left_y = 0;
    bluewake_mouse_camera_block(false);
    pad = native_pad(); bluewake_mouse_camera_pad(&pad);
    assert_unchanged(&expected,&pad);
    assert(!g_blocked && !g_click && g_exit_from == 0 && release_calls == 1);

    sample_click = true; ++g_retrace;
    pad = native_pad(); bluewake_mouse_camera_pad(&pad);
    assert(pad.substick_y == -30 && g_exit_from == g_retrace);
}

int main(void) {
    normal_click_and_exit();
    menu_cancels_synthetic_input();
    autosave_discards_only_host_gestures();
    puts("Camera PAD preserves normal input; menu ownership and paused gesture/release cleanup passed");
    return 0;
}
