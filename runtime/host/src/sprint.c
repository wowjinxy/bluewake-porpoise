#include "sprint.h"
#if defined(BLUEWAKE_WINDOWS)
#include "controls_bindings.h"
#endif
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
// Native movement speed/running animation parameters; input semantics are
// separate and do not change swimming, boots, targeting or carrying parameters.
enum { kMoveParams=0x8035CED4u, kMaxSpeed=kMoveParams+0x18u,
       kRunAnimRate=kMoveParams+0x48u, kPlayerPointer=0x803CA74Cu,
       // daPy_py_c::mMaxNormalSpeed, audited tww/include/d/actor/d_a_player.h.
       kSpeedF=0x254u, kNormalSpeed=0x35BCu, kMaxNormalSpeed=0x2A8u };
static CPUState* g_cpu;
static atomic_bool g_touch_down;
static double g_factor=1.5;
static atomic_uint_least64_t g_requested_factor;
static atomic_bool g_reload_requested;
static bool g_sprinting,g_trace,g_have_base,g_stick_was_down;
static float g_base_speed,g_base_rate,g_applied_speed,g_applied_rate;
static unsigned long long g_retrace,g_test_start,g_test_length;
static BwSprintInputState g_input;
static float read_f32(CPUState* cpu,u32 address) {
    const u32 bits=mem_read32(cpu,address);float value;memcpy(&value,&bits,sizeof value);return value;
}
static void write_f32(CPUState* cpu,u32 address,float value) {
    u32 bits;memcpy(&bits,&value,sizeof bits);mem_write32(cpu,address,bits);
}
static double requested_factor(void) {
    const char* value=getenv("BLUEWAKE_SPRINT_SPEED");
    char* end=NULL;
    double factor=value&&value[0]?strtod(value,&end):1.5;
    if(!isfinite(factor)||(end&&*end))factor=1.0;
    return factor;
}
void bluewake_sprint_touch(bool down) { atomic_store_explicit(&g_touch_down,down,memory_order_relaxed); }
void bluewake_sprint_cancel(void) {
    if(g_sprinting&&g_cpu&&g_have_base) {
        // A reset may already have replaced native data. Restore only the exact
        // pair this instance wrote; never overwrite a different loaded baseline.
        if(read_f32(g_cpu,kMaxSpeed)==g_applied_speed&&read_f32(g_cpu,kRunAnimRate)==g_applied_rate) {
            write_f32(g_cpu,kMaxSpeed,g_base_speed);write_f32(g_cpu,kRunAnimRate,g_base_rate);
        }
    }
    g_sprinting=false;g_stick_was_down=false;bw_sprint_input_reset(&g_input);
#if defined(BLUEWAKE_WINDOWS)
    bluewake_controls_cancel_actions();
#endif
}
void bluewake_sprint_reset(CPUState* cpu) {
    if(cpu&&cpu==g_cpu)bluewake_sprint_cancel();
    else {g_sprinting=false;g_stick_was_down=false;bw_sprint_input_reset(&g_input);}
    g_cpu=cpu;g_have_base=false;
}
void bluewake_sprint_reload(void) {
    // Called by legacy ImGui settings too: publish only, never access CPU/RAM.
    const double factor=requested_factor();uint64_t bits;
    memcpy(&bits,&factor,sizeof bits);
    atomic_store_explicit(&g_requested_factor,bits,memory_order_release);
    atomic_store_explicit(&g_reload_requested,true,memory_order_release);
}
static void apply_requested_factor(void) {
    if(!atomic_exchange_explicit(&g_reload_requested,false,memory_order_acq_rel))return;
    bluewake_sprint_cancel();
    const uint64_t bits=atomic_load_explicit(&g_requested_factor,memory_order_acquire);
    memcpy(&g_factor,&bits,sizeof g_factor);g_have_base=false;
}
void bluewake_sprint_attach(CPUState* cpu) {
    bluewake_sprint_reset(cpu);bluewake_sprint_reload();apply_requested_factor();g_retrace=0;g_test_start=g_test_length=0;
    const char* trace=getenv("BLUEWAKE_SPRINT_TRACE");g_trace=trace&&trace[0]=='1';
    const char* test=getenv("BLUEWAKE_SPRINT_TEST");
    if(test&&sscanf(test,"%llu:%llu",&g_test_start,&g_test_length)==2)g_trace=true;
    if(g_factor>1.0)fprintf(stderr,"[sprint] Sprint enabled at %.2fx; desktop bindings are in Controls\n",g_factor);
}
static void read_pads(bool* click, float* tilt) {
    *click = false;
    *tilt = 0.0f;
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (int i = 0; ids != NULL && i < count; ++i) {
        SDL_Gamepad* pad = SDL_GetGamepadFromID(ids[i]);
        if (pad == NULL)
            continue;
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_STICK))
            *click = true;
        const float x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        const float y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        const float t = x * x + y * y;
        if (t > *tilt)
            *tilt = t;
    }
    SDL_free(ids);
    *tilt = *tilt > 0.0f ? SDL_sqrtf(*tilt) : 0.0f;
}

static BwSprintInput read_input(void) {
    BwSprintInput input={0};
    input.touch_down=atomic_load_explicit(&g_touch_down,memory_order_relaxed)||
        (g_retrace>=g_test_start&&g_retrace-g_test_start<g_test_length);
#if defined(BLUEWAKE_WINDOWS)
    BluewakeControlsActions actions;
    if(bluewake_controls_read_actions(&actions)) {
        input.generation=actions.generation;input.blocked=actions.blocked;
        input.keyboard_down=actions.sprint_held;input.keyboard_pressed=actions.sprint_keyboard_pressed;
        input.controller_down=actions.sprint_controller_down;input.controller_pressed=actions.sprint_pressed;
        input.controller_tilt=actions.movement_tilt;return input;
    }
#endif
    int count=0;const bool* keys=SDL_GetKeyboardState(&count);
    input.keyboard_down=keys&&((count>SDL_SCANCODE_LSHIFT&&keys[SDL_SCANCODE_LSHIFT])||
                              (count>SDL_SCANCODE_RSHIFT&&keys[SDL_SCANCODE_RSHIFT]));
    // The unmanaged fallback keeps its original defaults on other platforms.
    static bool key_previous;
    input.keyboard_pressed=input.keyboard_down&&!key_previous;key_previous=input.keyboard_down;
    read_pads(&input.controller_down,&input.controller_tilt);
    input.controller_pressed=input.controller_down&&!g_stick_was_down;
    g_stick_was_down=input.controller_down;
    return input;
}
void bluewake_sprint_retrace(void) {
    ++g_retrace;apply_requested_factor();CPUState* cpu=g_cpu;
    // Disabled Sprint does not sample inputs or read/write any guest memory.
    if(cpu==NULL||g_factor<=1.0) {bw_sprint_input_reset(&g_input);return;}
    const BwSprintInput input=read_input();
    const bool sprint=bw_sprint_input_step(&g_input,&input);
    if(!g_have_base) {
        const float speed=read_f32(cpu,kMaxSpeed),rate=read_f32(cpu,kRunAnimRate);
        if(!(speed>1.f&&speed<100.f&&rate>.1f&&rate<20.f))return;
        g_base_speed=speed;g_base_rate=rate;g_have_base=true;
    }
    if(sprint!=g_sprinting) {
        const float scale=sprint?(float)g_factor:1.f;
        const float speed=g_base_speed*scale,rate=g_base_rate*scale;
        if(!isfinite(speed)||!isfinite(rate))return;
        g_sprinting=sprint;g_applied_speed=speed;g_applied_rate=rate;
        write_f32(cpu,kMaxSpeed,g_applied_speed);write_f32(cpu,kRunAnimRate,g_applied_rate);
        if(g_trace)fprintf(stderr,"[sprint] %s retrace=%llu top speed %.1f\n",sprint?"on":"off",g_retrace,g_applied_speed);
    }
    if(g_trace&&(g_retrace%20u)==0u) {
        const u32 player=mem_read32(cpu,kPlayerPointer);
        if(player>=0x80000000u&&player<0x81800000u)
            fprintf(stderr,"[sprint] retrace=%llu speedF=%.2f normal=%.2f max=%.2f\n",g_retrace,
                read_f32(cpu,player+kSpeedF),read_f32(cpu,player+kNormalSpeed),read_f32(cpu,player+kMaxNormalSpeed));
    }
}
