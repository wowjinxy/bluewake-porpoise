// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "core/cpu.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Production functions are extracted verbatim at configure time. Only their
// host device levels and unrelated trace consumer are supplied by this fixture.
static atomic_uint g_presses;
static atomic_bool g_touch_down;
static bool g_pending,g_enabled=true,g_trace,g_touch_was_down,g_bumper_was_down,g_targeting;
static unsigned g_presses_seen,g_follow,g_test_count;
static unsigned long long g_retrace,g_deadline,g_test[32],g_target_start,g_target_length;
static CPUState* g_cpu;
static const unsigned long long kWindow=6;
static const u32 kMenuPause=0x803F7097u;
static const Uint16 kNsoGameCubeProduct=0x2073;
bool bluewake_jump_button_armed;
static bool lb_down;
static Uint16 product;
static unsigned device_reads;
static int fake_device;
static void update_armed(void){bluewake_jump_button_armed=g_pending||g_targeting;}
static void follow(void){assert(false);}
SDL_JoystickID* SDL_GetGamepads(int* count){
    ++device_reads;*count=1;SDL_JoystickID* ids=malloc(2*sizeof *ids);assert(ids);ids[0]=3;ids[1]=0;return ids;
}
SDL_Gamepad* SDL_GetGamepadFromID(SDL_JoystickID id){assert(id==3);return (SDL_Gamepad*)&fake_device;}
Uint16 SDL_GetGamepadProduct(SDL_Gamepad* pad){assert(pad==(SDL_Gamepad*)&fake_device);return product;}
bool SDL_GetGamepadButton(SDL_Gamepad* pad,SDL_GamepadButton button){
    assert(pad==(SDL_Gamepad*)&fake_device&&button==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);return lb_down;
}
void SDL_free(void* memory){free(memory);}
#ifdef BLUEWAKE_AUTOSAVE_ISOLATED_MEMORY
bool g_ppc_guest_aliases_overlap_mem1;
PPCMemWriteJournal g_mem_write_journal;
void* g_mem_write_journal_user;
bool ppc_guest_alias_resolve(u32 a,u32 n,u8** p,u32* o){(void)a;(void)n;(void)p;(void)o;return false;}
#endif
#include "jump_touch_under_test.inc"
#include "jump_event_under_test.inc"
#include "jump_discard_under_test.inc"
#include "jump_retrace_under_test.inc"

static void fixture(void){
    atomic_store(&g_presses,0);atomic_store(&g_touch_down,false);g_presses_seen=0;
    g_pending=g_touch_was_down=g_bumper_was_down=g_targeting=g_trace=bluewake_jump_button_armed=false;
    g_enabled=true;g_cpu=NULL;g_follow=g_test_count=0;g_retrace=g_deadline=g_target_start=g_target_length=0;
    lb_down=false;product=0;device_reads=0;
}
static void press_space(void){SDL_Event e={0};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_SPACE;
    bluewake_jump_button_event(&e);}
static void native_pending(void){bluewake_jump_button_retrace();assert(bluewake_jump_button_armed&&g_pending);}
int main(void){
    fixture();press_space();native_pending();bluewake_jump_button_discard_input();
    assert(!bluewake_jump_button_armed&&!g_pending);bluewake_jump_button_retrace();assert(!bluewake_jump_button_armed);
    press_space();bluewake_jump_button_discard_input();bluewake_jump_button_retrace();assert(!bluewake_jump_button_armed);
    press_space();native_pending(); // A fresh post-resume edge still works.
    fixture();bluewake_jump_button_touch(true);bluewake_jump_button_discard_input();
    bluewake_jump_button_retrace();assert(!bluewake_jump_button_armed);
    bluewake_jump_button_touch(false);bluewake_jump_button_discard_input();
    bluewake_jump_button_touch(true);native_pending();
    fixture();lb_down=true;bluewake_jump_button_discard_input();
    bluewake_jump_button_retrace();assert(!bluewake_jump_button_armed&&g_bumper_was_down);
    lb_down=false;bluewake_jump_button_discard_input();lb_down=true;native_pending();
    fixture();product=kNsoGameCubeProduct;lb_down=true;bluewake_jump_button_discard_input();
    bluewake_jump_button_retrace();assert(!bluewake_jump_button_armed);
    assert(device_reads==2); // NSO native L remains distinct from free modern LB.
    fixture();g_targeting=true;bluewake_jump_button_discard_input();
    assert(bluewake_jump_button_armed&&g_targeting); // Discard leaves diagnostic/native targeting ownership intact.
    puts("Jump discard drops pending Space/touch/LB edges; held inputs require a fresh edge; no guest/UI access");
    return 0;
}
