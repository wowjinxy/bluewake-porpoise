#ifdef NDEBUG
#undef NDEBUG
#endif
#include "sprint_input.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static BwSprintInputState state;
static BwSprintInput input;
static bool tick(void) {return bw_sprint_input_step(&state,&input);}
static void start(BwSprintMode key,BwSprintMode pad) {
    assert(bluewake_sprint_configure_modes(key,pad));bw_sprint_input_reset(&state);
    input=(BwSprintInput){.generation=1,.controller_tilt=1};
    assert(!tick());assert(!tick());
}
int main(void) {
    BwSprintMode key,pad;bluewake_sprint_modes(&key,&pad);
    assert(key==BW_SPRINT_HOLD&&pad==BW_SPRINT_TOGGLE);
    assert(!bluewake_sprint_configure_modes((BwSprintMode)2,BW_SPRINT_HOLD));
    assert(!bluewake_sprint_configure_modes(BW_SPRINT_HOLD,(BwSprintMode)-1));
    start(BW_SPRINT_HOLD,BW_SPRINT_TOGGLE);
    input.keyboard_down=true;assert(tick());assert(tick());input.keyboard_down=false;assert(!tick());
    input.controller_down=true;input.controller_pressed=true;assert(tick());
    input.controller_pressed=false;input.controller_down=false;assert(tick());
    input.controller_tilt=.249f;for(int i=0;i<7;++i)assert(tick());assert(!tick());
    input.controller_tilt=.25f;input.controller_pressed=true;assert(tick());
    input.controller_pressed=false;for(int i=0;i<20;++i)assert(tick());
    // Unmanaged SDL diagonal sticks reach sqrt(2): preserve native moving semantics.
    input.controller_tilt=sqrtf(2.f);for(int i=0;i<20;++i)assert(tick());
    input.controller_pressed=true;assert(!tick());input.controller_pressed=false;
    // Keyboard Toggle responds once to a completed tap and persists independently
    // of an idle controller; Hold never treats a completed controller tap as held.
    start(BW_SPRINT_TOGGLE,BW_SPRINT_HOLD);
    input.keyboard_pressed=true;assert(tick());input.keyboard_pressed=false;
    for(int i=0;i<20;++i)assert(tick());
    input.keyboard_pressed=true;assert(!tick());input.keyboard_pressed=false;
    input.controller_pressed=true;assert(!tick());input.controller_pressed=false;
    input.controller_down=true;assert(tick());input.controller_down=false;assert(!tick());
    input.touch_down=true;assert(tick());input.touch_down=false;assert(!tick());
    // Mode change, block, input epoch/rebind and reset each cancel all sources.
    start(BW_SPRINT_TOGGLE,BW_SPRINT_TOGGLE);
    input.keyboard_down=true;input.keyboard_pressed=true;assert(tick());input.keyboard_pressed=false;
    assert(bluewake_sprint_configure_modes(BW_SPRINT_HOLD,BW_SPRINT_HOLD));assert(!tick());assert(!tick());
    input.keyboard_down=false;assert(!tick());input.keyboard_down=true;assert(tick());
    input.controller_down=input.touch_down=true;input.blocked=true;assert(!tick());
    input.blocked=false;assert(!tick());assert(!tick());
    input.keyboard_down=input.controller_down=input.touch_down=false;assert(!tick());
    input.touch_down=true;assert(tick());++input.generation;assert(!tick());assert(!tick());
    input.touch_down=false;assert(!tick());input.touch_down=true;assert(tick());
    bw_sprint_input_reset(&state);input.keyboard_pressed=input.controller_pressed=true;
    assert(!tick());assert(!tick());input.keyboard_pressed=input.controller_pressed=false;
    input.touch_down=false;assert(!tick());input.touch_down=true;assert(tick());
    // Redundant UI publications must not cancel an existing deliberate latch.
    start(BW_SPRINT_TOGGLE,BW_SPRINT_TOGGLE);input.keyboard_pressed=true;assert(tick());
    input.keyboard_pressed=false;assert(bluewake_sprint_configure_modes(BW_SPRINT_TOGGLE,BW_SPRINT_TOGGLE));assert(tick());
    start(BW_SPRINT_HOLD,BW_SPRINT_TOGGLE);input.controller_pressed=true;assert(tick());
    input.controller_pressed=false;input.controller_tilt=INFINITY;
    for(int i=0;i<7;++i)assert(tick());assert(!tick());
    start(BW_SPRINT_HOLD,BW_SPRINT_TOGGLE);input.controller_pressed=true;assert(tick());
    input.controller_pressed=false;input.controller_tilt=NAN;
    for(int i=0;i<7;++i)assert(tick());assert(!tick());
    puts("PASS Sprint source semantics, idle8, mode/menu/epoch/reset held-release cancellation");
}
