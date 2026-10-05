#ifndef BLUEWAKE_SPRINT_INPUT_H
#define BLUEWAKE_SPRINT_INPUT_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum BwSprintMode { BW_SPRINT_HOLD=0, BW_SPRINT_TOGGLE=1 } BwSprintMode;
// UI-thread publication; invalid values leave the current pair unchanged.
bool bluewake_sprint_configure_modes(BwSprintMode keyboard, BwSprintMode controller);
void bluewake_sprint_modes(BwSprintMode* keyboard, BwSprintMode* controller);
typedef struct BwSprintInput {
    uint64_t generation;
    bool blocked, keyboard_down, keyboard_pressed, controller_down, controller_pressed, touch_down;
    float controller_tilt;
} BwSprintInput;
typedef struct BwSprintInputState {
    uint32_t configuration;
    uint64_t generation;
    bool initialized, keyboard_latched, controller_latched;
    bool suppress_keyboard, suppress_controller, suppress_touch;
    unsigned controller_idle;
} BwSprintInputState;
// Game thread only. Reset/menu/epoch changes cancel latches and gate held input.
void bw_sprint_input_reset(BwSprintInputState* state);
// Exactly once per VI. Keyboard Toggle lasts until the next keyboard press;
// controller Toggle also stops after eight consecutive retraces below .25 tilt.
// Touch remains Hold independent of desktop preferences.
bool bw_sprint_input_step(BwSprintInputState* state, const BwSprintInput* input);
#ifdef __cplusplus
}
#endif
#endif
