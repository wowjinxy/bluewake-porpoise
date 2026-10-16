// Developer-only local-offset mirror. This is not a multiplayer renderer test.
// There is no PAD driver, game-effect call, network session or player-facing row.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_remote_body.h"
#include "pc_internal.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter.h"
#include "d/d_s_play.h"
#include "f_op/f_op_overlap_mng.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
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

void pc_remote_body_diagnostic_capture(J3DModel* model, daPy_lk_c* player) {
    if (!selected()) return; // Default: no actor search, loader, heap or SDL work.
    PcRemoteBodyPose pose{};
    if (!model || model->getMtxCalcMode() != 0 || !model->getModelData() || model->getModelData()->getJointNum() != 42 ||
        !receiver_scene(pose, player)) { pc_remote_body::hide(); return; }
    if (!pc_remote_body::prepare_private(model)) {
        static bool reported;
        if (!reported) {
            std::fprintf(stderr, "[cos] remote-body: REFUSED private resource/model isolation; native play unchanged\n");
            reported = true;
        }
        return;
    }
    // The final 42 matrices are already in world space (native model mode 0).
    // Translate each final joint and the metadata root exactly once, by +150X.
    constexpr float offset = 150.0f;
    std::memcpy(pose.base, model->getBaseTRMtx(), sizeof(pose.base));
    pose.base[0][3] += offset;
    const Vec* scale = model->getBaseScale();
    pose.base_scale[0] = scale->x; pose.base_scale[1] = scale->y; pose.base_scale[2] = scale->z;
    for (int i = 0; i < PC_REMOTE_BODY_JOINTS; ++i) {
        std::memcpy(pose.joints[i], model->getAnmMtx(i), sizeof(pose.joints[i]));
        pose.joints[i][0][3] += offset;
        pose.scale_flags[i] = model->getScaleFlag(i);
    }
    pc_remote_body::stage(pose);
}

void pc_remote_body_diagnostic_before_p1() {
    if (!selected()) return;
    PcRemoteBodyPose receiver{};
    if (!receiver_scene(receiver)) { pc_remote_body::hide(); return; }
    pc_remote_body::before_p1(receiver);
}
