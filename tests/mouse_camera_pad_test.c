// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "gxruntime/platform.h"
#include "core/cpu.h"
#include <stdlib.h>
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
static bool g_stick_conducting;
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

// Real CPU/BE helpers; only the alias provider and native camera inputs are authored.
bool g_ppc_guest_aliases_overlap_mem1;
bool ppc_guest_alias_resolve(u32 address,u32 size,u8** pointer,u32* offset) {
    (void)address;(void)size;(void)pointer;(void)offset;assert(false);return false;
}
enum {kPlayerPointer=0x803CA74Cu,kPlayerCurrentProc=0x31D8u,
      kProcTactWait=0x9Au,kProcTactOriginal=0x9Du,
      kCameraBody=0x244u,kSubjectStep=0x3C4u,kStyleFlags=0x80u,kStyleZoom=0x10u,kReady=0x100u};
static bool g_held,g_aim_ran;
static unsigned g_aim_wait;
static double g_zoom_live;
static bool authored_aiming,authored_control,authored_free;
static bool aiming_view(CPUState* cpu,u32 camera){(void)cpu;(void)camera;return authored_aiming;}
static u32 camera_style(CPUState* cpu,u32 camera){(void)cpu;(void)camera;return 0x80600000u;}
static bool player_in_control(CPUState* cpu){(void)cpu;return authored_control;}
static bool camera_free(CPUState* cpu,u32 camera){(void)cpu;(void)camera;return authored_free;}
static void zoom_frame(CPUState* cpu,u32 camera,bool free_camera){(void)cpu;(void)camera;(void)free_camera;}
#include "mouse_camera_context_under_test.inc"
#include "mouse_camera_frame_under_test.inc"

static void reset(void) {
    g_blocked = g_click = g_stick_click_down = false;
    g_click_release_guard=g_stick_click_release_guard=g_captured=false;g_window=0;
    g_stick_on = g_stick_mapped = true;
    g_stick_conducting = false;
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

static unsigned conducting_checks;
static void conducting_preserves_native_directions(void) {
    u8* ram=calloc(1,GC_MAIN_RAM_SIZE);u8* snapshot=malloc(GC_MAIN_RAM_SIZE);assert(ram&&snapshot);
    CPUState cpu={0};cpu.ram=ram;cpu.ram_size=GC_MAIN_RAM_SIZE;
    const u32 player=0x80400000u;
    write_be32(ram+(kPlayerPointer-GC_RAM_BASE),player);
    const s8 notes[][2]={{-127,0},{127,0},{0,127},{0,-127},{0,0},{-128,19},{127,-128}};
    for(u32 proc=kProcTactWait;proc<=kProcTactOriginal;++proc) {
        write_be32(ram+(player-GC_RAM_BASE)+kPlayerCurrentProc,proc);
        memcpy(snapshot,ram,GC_MAIN_RAM_SIZE);
        for(unsigned invert=0;invert<2;++invert)for(unsigned i=0;i<sizeof notes/sizeof notes[0];++i) {
            reset();inverted_camera=invert!=0;
            authored_aiming=authored_control=authored_free=false;
            const CPUState before=cpu;
            camera_frame(&cpu,0x80500000u);
            assert(g_stick_conducting&&!g_stick_owns&&!g_stick_aims&&!g_stick_zooms);
            assert(memcmp(&before,&cpu,sizeof cpu)==0);
            sample_x=notes[i][0]/128.0;sample_y=notes[i][1]/128.0;
            DolPadState pad=native_pad();pad.substick_x=notes[i][0];pad.substick_y=notes[i][1];
            const DolPadState expected=pad;
            bluewake_mouse_camera_pad(&pad);assert_unchanged(&expected,&pad);++conducting_checks;
        }
        assert(memcmp(snapshot,ram,GC_MAIN_RAM_SIZE)==0);++conducting_checks;
        // The snapshot also refreshes before the aiming early-return branch.
        reset();authored_aiming=authored_control=true;
        camera_frame(&cpu,0x80500000u);assert(g_stick_conducting);++conducting_checks;
        authored_aiming=authored_control=false;
    }
    reset();sample_x=1;inverted_camera=true;
    DolPadState pad=native_pad(),expected=pad;
    bluewake_mouse_camera_pad(&pad);assert_unchanged(&expected,&pad);++conducting_checks;
    reset();sample_x=sample_y=0;pad=native_pad();expected=pad;
    bluewake_mouse_camera_pad(&pad);assert_unchanged(&expected,&pad);++conducting_checks;
    // Native return/cancellation refreshes context and restores the existing
    // boat/target/swimming correction, including the signed -128 boundary.
    const u32 other_procs[]={0,0x99,0x9E,0xFFFFFFFFu};
    for(unsigned p=0;p<sizeof other_procs/sizeof other_procs[0];++p) {
        write_be32(ram+(player-GC_RAM_BASE)+kPlayerCurrentProc,other_procs[p]);
        for(unsigned i=0;i<2;++i) {
            reset();g_stick_conducting=true;authored_control=true;authored_free=false;
            camera_frame(&cpu,0x80500000u);assert(!g_stick_conducting);
            sample_x=i?1:-1;DolPadState pad=native_pad(),expected=pad;
            pad.substick_x=expected.substick_x=i?127:-128;
            expected.substick_x=i?-127:127;
            bluewake_mouse_camera_pad(&pad);assert_unchanged(&expected,&pad);++conducting_checks;
        }
    }
    // No Link, incomplete MEM1 or an actor with an incomplete procedure field
    // cannot invent conducting context or access an external/alias provider.
    assert(!player_conducting(NULL));++conducting_checks;
    CPUState missing={0};assert(!player_conducting(&missing));++conducting_checks;
    const u32 bad_players[]={0,0x7FFFFFFFu,0x80400001u,0x817FFFFCu,0x817FFFFFu,0x81800000u,0xC0400000u};
    for(unsigned i=0;i<sizeof bad_players/sizeof bad_players[0];++i) {
        write_be32(ram+(kPlayerPointer-GC_RAM_BASE),bad_players[i]);
        assert(!player_conducting(&cpu));++conducting_checks;
    }
    write_be32(ram+(kPlayerPointer-GC_RAM_BASE),player);
    cpu.ram_size=kPlayerPointer-GC_RAM_BASE+3u;assert(!player_conducting(&cpu));++conducting_checks;
    cpu.ram_size=player-GC_RAM_BASE+kPlayerCurrentProc+3u;assert(!player_conducting(&cpu));++conducting_checks;
    free(snapshot);free(ram);
}

int main(void) {
    conducting_preserves_native_directions();
    normal_click_and_exit();
    menu_cancels_synthetic_input();
    autosave_discards_only_host_gestures();
    printf("CAMERA_PAD_CONDUCTING_CHECKS %u; gestures, menu and paused release cleanup passed\n",conducting_checks);
    return 0;
}
