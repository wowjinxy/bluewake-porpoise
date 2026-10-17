#pragma once

#include <stdint.h>

// Copied values only. Version 1 joints are FINAL WORLD affine matrices, already
// containing actor base/scale. The renderer must not multiply base a second time.
// This is an internal diagnostic boundary, not a network/wire representation.
enum { PC_REMOTE_BODY_JOINTS = 42, PC_REMOTE_BODY_POSE_VERSION = 1 };
typedef struct PcRemoteBodyPose {
    uint32_t version;
    uint32_t joint_count;
    uint64_t scene_epoch;
    uint32_t sample_frame;
    char stage[9];
    int8_t room;
    int8_t layer;
    uint8_t reserved;
    float base[3][4];
    float base_scale[3];
    float joints[PC_REMOTE_BODY_JOINTS][3][4];
    uint8_t scale_flags[PC_REMOTE_BODY_JOINTS];
} PcRemoteBodyPose;

// Internal copied draw selection, NOT a wire/appearance contract. Unknown means
// no added hands. External HANDS grip models are a separate later slice.
enum {
    PC_REMOTE_VISUAL_VERSION = 1,
    PC_REMOTE_VISUAL_LEFT_OPEN = 1,
    PC_REMOTE_VISUAL_RIGHT_OPEN = 2
};
typedef struct PcRemoteBodyVisual {
    uint32_t version;
    uint64_t scene_epoch;
    uint32_t sample_frame;
    char stage[9];
    int8_t room, layer;
    uint8_t open_hands;
    uint8_t reserved;
} PcRemoteBodyVisual;

#ifdef __cplusplus
class J3DModel;
class daPy_lk_c;
class dKy_tevstr_c;

namespace pc_remote_body {
// Game/render owner thread only; prepare never retains the local model pointer.
bool prepare_private(J3DModel* local);
// Default callers (including network-body) never inherit receiver appearance.
// True is reserved for a pose+visibility pair sampled in one native local draw.
bool stage(const PcRemoteBodyPose& pose, bool local_native_visual = false);
bool copy_native_visual(const PcRemoteBodyVisual&);
void hide();
// Current draw's receiver lighting only, copied to stable private storage.
// Passing nullptr invalidates an unsafe/stale capture without retaining pointers.
void copy_receiver_lighting(const PcRemoteBodyPose& receiver_scene, const dKy_tevstr_c*);
void before_p1(const PcRemoteBodyPose& receiver_scene);
}

// Read-only native context/final-pose capture shared by the two opt-in diagnostics.
bool pc_remote_body_capture_native(J3DModel*, daPy_lk_c*, PcRemoteBodyPose*);
bool pc_remote_body_context_native(PcRemoteBodyPose*);

// Narrow diagnostic controls, separate from the renderer. No player-facing
// settings, input injection, network session or multiplayer authority.
void pc_remote_body_diagnostic_capture(J3DModel*, daPy_lk_c*);
void pc_remote_body_diagnostic_lighting(const dKy_tevstr_c&, daPy_lk_c*);
// Observes actual CL hand flags after native selection/camera hiding, before P1.
void pc_remote_body_diagnostic_visuals(J3DModel*, daPy_lk_c*);
void pc_remote_body_diagnostic_before_p1();
#endif
