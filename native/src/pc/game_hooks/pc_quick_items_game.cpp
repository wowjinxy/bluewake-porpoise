// Safe native actor context for the opt-in quick-item input filter. Only reads
// state: gameplay changes remain in the player's and boat's normal action paths.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_quick_items.h"
#include "pc_internal.h"

#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter.h"
#include "d/d_s_play.h"
#include "f_op/f_op_overlap_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

namespace {

void* executingPlay(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

} // namespace

extern "C" uint64_t pc_quick_items_context(void) {
    if (pc::menuOpen() || dMenu_flag() || dScnPly_ply_c::pauseTimer != 0 ||
        dScnPly_ply_c::nextPauseTimer != 0 || fopOvlpM_IsPeek() ||
        fopOvlpM_IsDoingReq() || dComIfGp_isEnableNextStage() ||
        dComIfGp_event_runCheck() || dComIfGp_getMiniGameType() != 0 ||
        !dComIfGp_2dShowCheck() || dComIfGp_isHeapLockFlag() == 6 ||
        dComIfGp_getOperateWind() == 2 ||
        dComIfGs_getLife() == 0 || daPy_dmEcallBack_c::checkCurse())
    {
        return 0;
    }

    base_process_class* scene = fpcM_Search(executingPlay, nullptr);
    fopAc_ac_c* actor = dComIfGp_getPlayer(0);
    if (scene == nullptr || actor == nullptr || actor != dComIfGp_getLinkPlayer() ||
        fpcM_GetName(actor) != fpcNm_PLAYER_e)
    {
        return 0;
    }
    const fpc_ProcID sceneId = fpcM_GetID(scene);
    const fpc_ProcID playerId = fpcM_GetID(actor);
    if (fpcM_IsErrorID(sceneId) || fpcM_IsErrorID(playerId) ||
        !fpcM_IsExecuting(sceneId) || !fpcM_IsExecuting(playerId) ||
        fpcM_IsCreating(sceneId) || fpcM_IsCreating(playerId))
    {
        return 0;
    }
    daPy_lk_c* player = static_cast<daPy_lk_c*>(actor);
    if (player->checkPlayerDemoMode() || player->checkFreezeState() ||
        player->checkModeFlg(daPy_lk_c::ModeFlg_DAMAGE | daPy_lk_c::ModeFlg_CAUGHT))
    {
        return 0;
    }

    // Process IDs, rather than addresses, invalidate held input across actor or
    // scene replacement even if an allocator reuses the old object's address.
    return (static_cast<uint64_t>(static_cast<uint32_t>(sceneId)) << 32) |
           static_cast<uint32_t>(playerId);
}
