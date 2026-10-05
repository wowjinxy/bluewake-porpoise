#include "sprint_input.h"
#include <stdatomic.h>
#include <string.h>
#include <math.h>
#if defined(BLUEWAKE_WINDOWS)
#include "controls_bindings.h"
#endif
// Low bits hold both modes; upper bits invalidate state on actual changes.
static atomic_uint_least32_t g_configuration=ATOMIC_VAR_INIT(2u);
bool bluewake_sprint_configure_modes(BwSprintMode keyboard,BwSprintMode controller) {
    if ((keyboard!=BW_SPRINT_HOLD&&keyboard!=BW_SPRINT_TOGGLE)||
        (controller!=BW_SPRINT_HOLD&&controller!=BW_SPRINT_TOGGLE)) return false;
    const uint32_t modes=(uint32_t)keyboard|((uint32_t)controller<<1u);
    uint_least32_t old=atomic_load_explicit(&g_configuration,memory_order_acquire);
    for (;;) {
        if ((old&3u)==modes) return true;
        const uint_least32_t next=((old+4u)&~3u)|modes;
        if (atomic_compare_exchange_weak_explicit(&g_configuration,&old,next,
                                                  memory_order_acq_rel,memory_order_acquire)) break;
    }
#if defined(BLUEWAKE_WINDOWS)
    bluewake_controls_cancel_actions();
#endif
    return true;
}
void bluewake_sprint_modes(BwSprintMode* keyboard,BwSprintMode* controller) {
    const uint32_t cfg=atomic_load_explicit(&g_configuration,memory_order_acquire);
    if(keyboard)*keyboard=(BwSprintMode)(cfg&1u);
    if(controller)*controller=(BwSprintMode)((cfg>>1u)&1u);
}
void bw_sprint_input_reset(BwSprintInputState* state) {
    memset(state,0,sizeof *state);
    state->suppress_keyboard=state->suppress_controller=state->suppress_touch=true;
}
bool bw_sprint_input_step(BwSprintInputState* state,const BwSprintInput* input) {
    const uint32_t cfg=atomic_load_explicit(&g_configuration,memory_order_acquire);
    if (!state->initialized || cfg!=state->configuration || input->generation!=state->generation || input->blocked) {
        bw_sprint_input_reset(state);
        state->initialized=true;state->configuration=cfg;state->generation=input->generation;
        // Even a completed pending tap from a previous epoch cannot re-arm here.
        return false;
    }
    state->suppress_keyboard=state->suppress_keyboard&&input->keyboard_down;
    state->suppress_controller=state->suppress_controller&&input->controller_down;
    state->suppress_touch=state->suppress_touch&&input->touch_down;
    bool keyboard=false,controller=false;
    if((cfg&1u)==BW_SPRINT_HOLD) keyboard=input->keyboard_down&&!state->suppress_keyboard;
    else {
        if(input->keyboard_pressed&&!state->suppress_keyboard)state->keyboard_latched=!state->keyboard_latched;
        keyboard=state->keyboard_latched;
    }
    if(((cfg>>1u)&1u)==BW_SPRINT_HOLD) controller=input->controller_down&&!state->suppress_controller;
    else {
        if(input->controller_pressed&&!state->suppress_controller) {
            state->controller_latched=!state->controller_latched;state->controller_idle=0;
        }
        if(state->controller_latched) {
            const bool moving=isfinite(input->controller_tilt)&&input->controller_tilt>=.25f;
            state->controller_idle=moving?0u:state->controller_idle+1u;
            if(state->controller_idle>=8u)state->controller_latched=false;
        }
        controller=state->controller_latched;
    }
    return keyboard||controller||(input->touch_down&&!state->suppress_touch);
}
