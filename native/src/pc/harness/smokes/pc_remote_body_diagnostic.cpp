// Developer-only local-offset mirror. This is not a multiplayer renderer test.
// There is no PAD driver, game-effect call, network session or player-facing row.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_remote_body.h"
#include "pc/pc_network_body.h"
#include "pc/pc_remote_body_visual_policy.h"
#include "pc_internal.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter.h"
#include "d/d_s_play.h"
#include "f_op/f_op_overlap_mng.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "res/Object/Link.h"
#include <cstdio>
#include <cstring>

namespace {
bool selected() {
    return pc::gConfig.smoke && std::strcmp(pc::gConfig.smoke, "remote-body") == 0;
}
void* play_scene(void* process, void*) {
    return fpcM_GetName(process) == fpcNm_PLAY_SCENE_e ? process : nullptr;
}
bool receiver_scene(PcRemoteBodyPose& pose, daPy_lk_c* expected = nullptr) {
    if (pc::menuOpen() || dMenu_flag() || dScnPly_ply_c::pauseTimer || dScnPly_ply_c::nextPauseTimer ||
        fopOvlpM_IsPeek() || fopOvlpM_IsDoingReq() || dComIfGp_isEnableNextStage() ||
        dComIfGp_event_runCheck()) return false;
    base_process_class* scene = fpcM_Search(play_scene, nullptr);
    fopAc_ac_c* actor = dComIfGp_getPlayer(0);
    if (!scene || !actor || actor != dComIfGp_getLinkPlayer() || fpcM_GetName(actor) != fpcNm_PLAYER_e ||
        (expected && actor != expected)) return false;
    const fpc_ProcID sceneID = fpcM_GetID(scene), actorID = fpcM_GetID(actor);
    if (fpcM_IsErrorID(sceneID) || fpcM_IsErrorID(actorID) || !fpcM_IsExecuting(sceneID) ||
        !fpcM_IsExecuting(actorID) || fpcM_IsCreating(sceneID) || fpcM_IsCreating(actorID)) return false;
    daPy_lk_c* player = static_cast<daPy_lk_c*>(actor);
    if (player->checkPlayerNoDraw() || player->checkPlayerDemoMode() || player->checkFreezeState()) return false;
    const char* name = dComIfGp_getStartStageName();
    const size_t length = name ? std::strlen(name) : 0;
    const int room = fopAcM_GetRoomNo(player);
    // Start-layer -1 means native day/story resolution, not an invalid layer.
    const int layer = dComIfG_play_c::getLayerNo(room);
    if (!length || length > 8 || room < 0 || room > 63 || layer < 0 || layer > 15) return false;
    pose = {};
    pose.version = PC_REMOTE_BODY_POSE_VERSION;
    pose.joint_count = PC_REMOTE_BODY_JOINTS;
    pose.scene_epoch = (uint64_t(uint32_t(sceneID)) << 32) | uint32_t(actorID);
    pose.sample_frame = pc_frame_count();
    std::memcpy(pose.stage, name, length);
    pose.room = int8_t(room); pose.layer = int8_t(layer);
    return true;
}
} // namespace

bool pc_remote_body_context_native(PcRemoteBodyPose* pose) {
    return pose && receiver_scene(*pose);
}

bool pc_remote_body_capture_native(J3DModel* model, daPy_lk_c* player, PcRemoteBodyPose* out) {
    if (!out) return false;
    *out = {};
    if (!model || !model->getModelData() || model->getModelData()->getJointNum() != 42 ||
        model->getMtxCalcMode() != 0 || !receiver_scene(*out, player)) return false;
    std::memcpy(out->base, model->getBaseTRMtx(), sizeof out->base);
    const Vec* scale = model->getBaseScale();
    out->base_scale[0] = scale->x; out->base_scale[1] = scale->y; out->base_scale[2] = scale->z;
    for (int i = 0; i < PC_REMOTE_BODY_JOINTS; ++i) {
        std::memcpy(out->joints[i], model->getAnmMtx(i), sizeof out->joints[i]);
        out->scale_flags[i] = model->getScaleFlag(i);
    }
    return true; // Final world pose, without any diagnostic/remote offset.
}

void pc_remote_body_diagnostic_lighting(const dKy_tevstr_c& lighting, daPy_lk_c* player) {
    // Keep the normal player draw path dormant unless one diagnostic is selected.
    if (!selected() && !pc_network_body_diagnostic_selected()) return;
    PcRemoteBodyPose receiver{};
    if (!receiver_scene(receiver, player)) {
        pc_remote_body::copy_receiver_lighting(receiver, nullptr);
        return;
    }
    pc_remote_body::copy_receiver_lighting(receiver, &lighting);
}

void pc_remote_body_diagnostic_capture(J3DModel* model, daPy_lk_c* player) {
    if (pc_network_body_diagnostic_selected()) {
        pc_network_body_diagnostic_capture(model, player); return;
    }
    if (!selected()) return;
    PcRemoteBodyPose context{};
    if (!pc_remote_body_capture_native(model, player, &context)) { pc_remote_body::hide(); return; }
    // Preserve the existing execute-time private allocation/isolation phase.
    // Matrices for local display are paired with visibility in the draw below.
    if (!pc_remote_body::prepare_private(model)) {
        static bool reported;
        if (!reported) {
            std::fprintf(stderr, "[cos] remote-body: REFUSED private resource/model isolation; native play unchanged\n");
            reported = true;
        }
    }
}

void pc_remote_body_diagnostic_visuals(J3DModel* model, daPy_lk_c* player) {
    if (!selected()) return; // Never substitute receiver appearance for a network peer.
    PcRemoteBodyPose pose{};
    if (!pc_remote_body_capture_native(model, player, &pose)) { pc_remote_body::hide(); return; }
    J3DModelData* data = model->getModelData();
    J3DMaterial* left = data->getJointNodePointer(CL_JNT_CL_LHANDA_e)->getMesh();
    J3DMaterial* right = data->getJointNodePointer(CL_JNT_CL_RHANDA_e)->getMesh();
    if (!left || !right || !left->getShape() || !right->getShape()) { pc_remote_body::hide(); return; }
    PcRemoteBodyVisual visual{};
    visual.version = PC_REMOTE_VISUAL_VERSION;
    visual.scene_epoch = pose.scene_epoch; visual.sample_frame = pose.sample_frame;
    std::memcpy(visual.stage, pose.stage, sizeof visual.stage);
    visual.room = pose.room; visual.layer = pose.layer;
    if (!left->getShape()->checkFlag(J3DShpFlag_Hide)) visual.open_hands |= PC_REMOTE_VISUAL_LEFT_OPEN;
    if (!right->getShape()->checkFlag(J3DShpFlag_Hide)) visual.open_hands |= PC_REMOTE_VISUAL_RIGHT_OPEN;
    constexpr float offset = 150.0f;
    pose.base[0][3] += offset;
    for (int i = 0; i < PC_REMOTE_BODY_JOINTS; ++i) pose.joints[i][0][3] += offset;
    if (!pc_remote_body::copy_native_visual(visual) || !pc_remote_body::stage(pose, true)) pc_remote_body::hide();
}

void pc_remote_body_diagnostic_before_p1() {
    if (pc_network_body_diagnostic_selected()) { pc_network_body_diagnostic_before_p1(); return; }
    if (!selected()) return;
    PcRemoteBodyPose receiver{};
    if (!pc_remote_body_context_native(&receiver)) { pc_remote_body::hide(); return; }
    pc_remote_body::before_p1(receiver);
}
