// SPDX-License-Identifier: GPL-3.0-or-later
// Native Telescope pilot: authored Aryll gift completion, copied lifecycle
// events, and an idempotent remote permanent effect. All functions run on the
// game thread. The network worker sees only copies drained by its integration.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_progression.h"
#include "pc/pc_progression_logic.h"
#include "pc_internal.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_item.h"
#include "d/d_meter.h"
#include "d/d_s_play.h"
#include "f_op/f_op_overlap_mng.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include <cstring>
#include <unistd.h>

namespace {
constexpr uint32_t kAbsent = UINT32_MAX;
constexpr unsigned kQueueSize = 64;
PcProgressIdentity sIdentity = {};
PcNativeEvent sEvents[kQueueSize] = {};
unsigned sHead = 0, sCount = 0;
bool sOverflow = false;
bool sRemote = false;
bool sPlayerValid = false;
bool sFrameStarted = false;
uint64_t sInvocation = 0;

pc::progression::Award sAward;
struct PreparedSave {
    bool active = false;
    uint64_t epoch = 0;
    int32_t slot = -1;
    uint32_t telescope = 0;
} sPrepared;
struct CardTransaction {
    bool active = false, prepared = false;
    uint64_t invocation = 0;
    PcProgressIdentity identity = {};
    uint32_t telescope = 0;
} sWrite, sRead;

void* playScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

uint32_t telescopeOwned() {
    return dComIfGs_getItem(dInvSlot_TELESCOPE_e) == dItemNo_TELESCOPE_e &&
           dComIfGs_checkGetItem(dItemNo_TELESCOPE_e) ? 1 : 0;
}

PcNativeEvent eventValue(uint32_t kind, uint32_t origin = PC_NATIVE_SYSTEM) {
    PcNativeEvent event = {};
    event.kind = kind;
    event.origin = origin;
    event.identity = sIdentity;
    const char* stage = dComIfGp_getStartStageName();
    if (stage != nullptr)
        std::strncpy(event.stage, stage, sizeof(event.stage) - 1);
    fopAc_ac_c* player = sPlayerValid ? dComIfGp_getPlayer(0) : nullptr;
    event.room = player != nullptr ? fopAcM_GetRoomNo(player) : -1;
    return event;
}

void push(const PcNativeEvent& event) {
    if (sCount == kQueueSize) {
        sOverflow = true;
        sHead = sCount = 0;
        pc::writef(STDERR_FILENO, "[cos] progression: event queue overflow; full reconciliation required\n");
    }
    sEvents[(sHead + sCount++) % kQueueSize] = event;
}

void cancelAward() {
    if (!sAward.active)
        return;
    PcNativeEvent event = eventValue(PC_NATIVE_AWARD_CANCELLED, PC_NATIVE_LOCAL);
    event.reward_id = PC_NATIVE_REWARD_ARYLL_TELESCOPE;
    event.producer_id = sAward.producer;
    event.invocation_id = sAward.invocation;
    push(event);
    sAward = {};
}

void revoke(uint32_t kind) {
    cancelAward();
    sPrepared = {};
    ++sIdentity.epoch;
    push(eventValue(kind));
}

void refresh() {
    if (sIdentity.session_id == 0) {
        sIdentity.session_id = pc::monotonicNs();
        if (sIdentity.session_id == 0) sIdentity.session_id = 1;
        std::strcpy(sIdentity.profile_id, "personal");
    }
    // Startup profile binding and save initialization precede FPC startup.
    base_process_class* scene = sFrameStarted ? fpcM_Search(playScene, nullptr) : nullptr;
    fopAc_ac_c* player = sFrameStarted ? dComIfGp_getPlayer(0) : nullptr;
    bool valid = scene != nullptr && player != nullptr &&
                 player == dComIfGp_getLinkPlayer() && fpcM_GetName(player) == fpcNm_PLAYER_e;
    uint32_t sceneId = scene != nullptr ? fpcM_GetID(scene) : kAbsent;
    uint32_t playerId = valid ? fpcM_GetID(player) : kAbsent;
    valid = valid && !fpcM_IsErrorID(sceneId) && !fpcM_IsErrorID(playerId) &&
            fpcM_IsExecuting(sceneId) && fpcM_IsExecuting(playerId) &&
            !fpcM_IsCreating(sceneId) && !fpcM_IsCreating(playerId);
    if (!valid) playerId = kAbsent;
    const uint64_t serial = dComIfGs_getMemCardCheckID();
    const int32_t slot = dComIfGs_getDataNum();
    char name[17] = {};
    std::memcpy(name, dComIfGs_getPlayerName(), sizeof(name) - 1);
    const bool changed = sIdentity.scene_id != sceneId || sIdentity.player_id != playerId ||
                         sIdentity.card_serial != serial || sIdentity.quest_slot != slot ||
                         std::memcmp(sIdentity.player_name, name, sizeof(name)) != 0;
    if (changed) {
        // Cancel with the old copied identity before replacing it.
        cancelAward();
        sPrepared = {};
        sIdentity.scene_id = sceneId;
        sIdentity.player_id = playerId;
        sIdentity.card_serial = serial;
        sIdentity.quest_slot = slot;
        std::memcpy(sIdentity.player_name, name, sizeof(name));
        sPlayerValid = valid;
        ++sIdentity.epoch;
        push(eventValue(PC_NATIVE_LIFECYCLE_CHANGED));
    } else sPlayerValid = valid;
}

bool safeToApply() {
    if (!sPlayerValid || sIdentity.quest_slot < 0 || sIdentity.quest_slot > 2 ||
        sAward.active || sWrite.active || sRead.active || pc::menuOpen() || dMenu_flag() ||
        dScnPly_ply_c::pauseTimer != 0 || dScnPly_ply_c::nextPauseTimer != 0 ||
        fopOvlpM_IsPeek() || fopOvlpM_IsDoingReq() || dComIfGp_isEnableNextStage() ||
        dComIfGp_event_runCheck() || dComIfGp_getMiniGameType() != 0 ||
        !dComIfGp_2dShowCheck() || dComIfGp_isHeapLockFlag() == 6 ||
        dComIfGp_getOperateWind() == 2 || dComIfGs_getLife() == 0 ||
        daPy_dmEcallBack_c::checkCurse())
        return false;
    daPy_lk_c* player = static_cast<daPy_lk_c*>(dComIfGp_getPlayer(0));
    return !player->checkPlayerDemoMode() && !player->checkFreezeState() &&
           !player->checkModeFlg(daPy_lk_c::ModeFlg_DAMAGE | daPy_lk_c::ModeFlg_CAUGHT);
}

void finishAward() {
    if (!sAward.ready(sIdentity.epoch, telescopeOwned() != 0))
        return;
    PcNativeEvent event = eventValue(PC_NATIVE_AWARD_COMPLETED, PC_NATIVE_LOCAL);
    event.reward_id = PC_NATIVE_REWARD_ARYLL_TELESCOPE;
    event.fact_key = PC_PROGRESS_FACT_TELESCOPE;
    event.fact_value = 1;
    event.producer_id = sAward.producer;
    event.invocation_id = sAward.invocation;
    push(event);
    pc::writef(STDERR_FILENO,
               "[cos] progression: genuine Aryll Telescope completed invocation=%llu epoch=%llu\n",
               static_cast<unsigned long long>(sAward.invocation),
               static_cast<unsigned long long>(sIdentity.epoch));
    sAward = {};
}
} // namespace

extern "C" {
int pc_progress_bind_profile(const char* identity) {
    if (identity == nullptr) return 0;
    const size_t len = std::strlen(identity);
    if (std::strcmp(identity, "personal") != 0) {
        if (len != 64) return 0;
        for (size_t i = 0; i < len; ++i)
            if (!((identity[i] >= '0' && identity[i] <= '9') ||
                  (identity[i] >= 'a' && identity[i] <= 'f'))) return 0;
    }
    refresh();
    if (std::strcmp(sIdentity.profile_id, identity) != 0) {
        cancelAward();
        std::memset(sIdentity.profile_id, 0, sizeof(sIdentity.profile_id));
        std::memcpy(sIdentity.profile_id, identity, len);
        revoke(PC_NATIVE_LIFECYCLE_CHANGED);
    }
    return 1;
}

void pc_progress_frame(void) {
    sFrameStarted = true;
    refresh();
    // Native endCheck and item deletion may land in adjacent execute frames.
    // An unrelated later Telescope effect cannot revive an expired invocation.
    if (sAward.active) {
        const dEvt_control_c* event = g_dComIfG_gameInfo.play.getEvent();
        if (sAward.mustCancel(sIdentity.epoch, dComIfGp_event_runCheck() != 0,
                             event->mEventId, pc_frame_count()))
            cancelAward();
    }
}

void pc_progress_capture(PcProgressSnapshot* out) {
    if (out == nullptr) return;
    refresh();
    *out = {};
    out->identity = sIdentity;
    out->telescope_owned = telescopeOwned();
    out->safe_to_apply = safeToApply() ? 1 : 0;
    out->event_overflow = sOverflow ? 1 : 0;
}

int pc_progress_pop_event(PcNativeEvent* out) {
    if (out == nullptr) return 0;
    if (sOverflow) { sOverflow = false; return -1; }
    if (sCount == 0) return 0;
    *out = sEvents[sHead];
    sHead = (sHead + 1) % kQueueSize;
    --sCount;
    return 1;
}

int pc_progress_apply(uint32_t key, uint32_t value, uint64_t expected_epoch) {
    if (key != PC_PROGRESS_FACT_TELESCOPE || value != 1) return PC_PROGRESS_REJECTED;
    refresh();
    const int gate = pc::progression::applyReadiness(key, value, expected_epoch,
        sIdentity.epoch, safeToApply(), telescopeOwned() != 0);
    if (gate != PC_PROGRESS_APPLIED) return gate;
    sRemote = true;
    item_func_telescope(); // only permanent obtained bit + inventory slot; no fanfare/refill/XYZ
    sRemote = false;
    if (!telescopeOwned()) return PC_PROGRESS_REJECTED;
    PcNativeEvent event = eventValue(PC_NATIVE_FACT_APPLIED, PC_NATIVE_REMOTE);
    event.fact_key = key;
    event.fact_value = value;
    push(event);
    return PC_PROGRESS_APPLIED;
}

void pc_progress_aryll_begin(uint32_t producer, int32_t native_event) {
    refresh();
    if (sRemote || telescopeOwned() || !sPlayerValid || native_event < 0) return;
    if (sAward.same(producer, native_event, sIdentity.epoch)) return;
    cancelAward();
    sAward.begin(producer, native_event, sIdentity.epoch, ++sInvocation);
    PcNativeEvent event = eventValue(PC_NATIVE_AWARD_BEGAN, PC_NATIVE_LOCAL);
    event.reward_id = PC_NATIVE_REWARD_ARYLL_TELESCOPE;
    event.producer_id = producer;
    event.invocation_id = sAward.invocation;
    push(event);
}

void pc_progress_item_message_completed(uint32_t actor, uint32_t item) {
    if (sRemote || item != dItemNo_TELESCOPE_e) return;
    refresh();
    sAward.messageCompleted(actor, sIdentity.epoch,
        g_dComIfG_gameInfo.play.getEvent()->mEventId);
}

void pc_progress_item_effect(uint32_t actor, uint32_t item) {
    if (sRemote || item != dItemNo_TELESCOPE_e) return;
    refresh();
    if (telescopeOwned() && sAward.effectApplied(actor, sIdentity.epoch)) {
        finishAward();
    }
}

void pc_progress_aryll_end(uint32_t producer) {
    refresh();
    if (sAward.eventEnded(producer, sIdentity.epoch, pc_frame_count())) {
        finishAward();
    }
}

void pc_progress_aryll_cancel(uint32_t producer) {
    if (sAward.active && sAward.producer == producer) cancelAward();
}

void pc_progress_quest_initialized(void) {
    refresh();
    sIdentity.save_generation = 0;
    sWrite = {}; sRead = {};
    revoke(PC_NATIVE_QUEST_INITIALIZED);
}

void pc_progress_quest_loaded(int32_t slot, int32_t success) {
    refresh();
    sIdentity.quest_slot = slot;
    sIdentity.save_generation = 0;
    sWrite = {}; sRead = {};
    revoke(success ? PC_NATIVE_QUEST_LOADED : PC_NATIVE_LIFECYCLE_CHANGED);
}

void pc_progress_save_prepared(int32_t slot) {
    refresh();
    sPrepared.active = slot >= 0 && slot <= 2 && slot == sIdentity.quest_slot;
    sPrepared.slot = slot;
    sPrepared.epoch = sIdentity.epoch;
    sPrepared.telescope = telescopeOwned();
}

void pc_progress_card_request(int32_t save) {
    refresh();
    CardTransaction& tx = save ? sWrite : sRead;
    tx.active = true;
    tx.invocation = ++sInvocation;
    tx.identity = sIdentity;
    tx.prepared = save && sPrepared.active && sPrepared.epoch == sIdentity.epoch;
    tx.telescope = tx.prepared ? sPrepared.telescope : 0;
    if (save) sPrepared = {};
    PcNativeEvent event = eventValue(save ? PC_NATIVE_SAVE_REQUESTED : PC_NATIVE_CARD_READ_REQUESTED);
    event.invocation_id = tx.invocation;
    event.fact_key = tx.prepared ? PC_PROGRESS_FACT_TELESCOPE : 0;
    event.fact_value = tx.telescope;
    push(event);
}

void pc_progress_card_result(int32_t save, int32_t result) {
    if (result == 0) return; // native busy; no new event or claimed success
    refresh();
    CardTransaction& tx = save ? sWrite : sRead;
    if (!tx.active) return;
    const bool success = result == 1;
    // A write of a different slot, file creation, photos or a copied buffer is
    // not proof that this active quest's permanent facts were committed.
    if (save && pc::progression::confirmsActiveQuestSave(tx.prepared, result, tx.identity, sIdentity))
        ++sIdentity.save_generation;
    PcNativeEvent event = eventValue(save ? (success ? PC_NATIVE_SAVE_COMMITTED : PC_NATIVE_SAVE_FAILED)
                                            : (success ? PC_NATIVE_CARD_READ_COMPLETED : PC_NATIVE_CARD_READ_FAILED));
    event.identity = tx.identity;
    event.identity.save_generation = sIdentity.save_generation;
    event.invocation_id = tx.invocation;
    event.fact_key = tx.prepared ? PC_PROGRESS_FACT_TELESCOPE : 0;
    event.fact_value = tx.telescope;
    push(event);
    tx = {};
}
} // extern "C"
