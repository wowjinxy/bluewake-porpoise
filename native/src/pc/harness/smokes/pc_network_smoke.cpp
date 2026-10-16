// SPDX-License-Identifier: GPL-3.0-or-later
// COS_SMOKE=network-progress, COS_NETWORK_CASE=gift|receive.
// Uses ordinary native PAD and the mounted room profile. No item, event, actor
// action or save buffer is fabricated. Final success belongs to saveSweepStart:
// real native SaveSync completion and comparison with both GCI save copies.
// gift: sea:44:206, COS_BOOT_EVENTS=2A80, no COS_BOOT_ITEMS/preset. Normal Aryll
//       omedeto -> get_telescope supplies the sole outgoing earned award.
// receive: sea:11:9, no boot events/items/preset. Joins AFTER the unchanged
//          selectors/resources baseline is captured. Optional
//          COS_NETWORK_DEFER_MENU=1 uses native START/B to prove paused deferral.
// COS_NETWORK_AUTOSTART=host|join overrides the case's default host/join role;
// leave unset for receive so no grant precedes its measured baseline.
// COS_SMOKE=save-load + COS_NETWORK_CASE=observe passively checks actual fresh
// quest activation, Telescope ownership and no new award/apply, before the
// existing save-load harness independently completes its card comparison.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc_internal.h"
#include "pc/pc_network_game.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_manager.h"
#include "d/d_meter.h"
#include "d/d_s_play.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include <dolphin/pad.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {
namespace {
enum Case { kGift, kReceive, kObserve };
enum Step { kOff, kWaitPlayer, kOpenMenu, kWaitDeferred, kCloseMenu, kReward, kSaving, kObserved };
Case sCase = kGift;
const char* sCaseName = "gift";
Step sStep = kOff;
bool sChecked = false, sDeferMenu = false, sHost = true;
bool sCaptured = false, sConnected = false, sIndependentPeer = false;
unsigned sSince = 0, sUp = 0, sStable = 0, sDeferred = 0, sStartFrame = 0;
uint64_t sStartNs = 0;
uint32_t sScene = 0, sPlayer = 0;
u8 sSlots[3], sItems[3];
int sRupees = 0, sLife = 0, sMagic = 0, sArrows = 0, sBombs = 0;

void drive(u16 buttons = 0) {
    setDrivenPad(true, buttons, 0, 0);
    setDrivenQuickItemModifier(false);
}

[[noreturn]] void fail(const char* message) {
    if (sCase != kObserve) drive();
    writef(STDERR_FILENO, "[cos] network-progress: FAIL case=%s step=%d age=%u: %s\n",
           sCaseName, static_cast<int>(sStep), sSince, message);
    pc_exit(PC_EXIT_CHECK_FAILED);
}

[[noreturn]] void usage(const char* message) {
    writef(STDERR_FILENO, "[cos] network-progress: %s\n", message);
    pc_exit(PC_EXIT_USAGE);
}

void enter(Step step) {
    drive();
    sStep = step;
    sSince = 0;
}

void* playScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

daPy_lk_c* readyPlayer(bool requireWholeRoom) {
    base_process_class* scene = fpcM_Search(playScene, nullptr);
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (scene == nullptr || player == nullptr || fpcM_GetName(player) != fpcNm_PLAYER_e ||
        fpcM_IsCreating(fpcM_GetID(player)) || !fpcM_IsExecuting(fpcM_GetID(player)) ||
        (requireWholeRoom && stageRoomReady(fopAcM_GetRoomNo(player), nullptr) != nullptr))
        return nullptr;
    return static_cast<daPy_lk_c*>(player);
}

bool nativeMenuOpen() {
    return dMenu_flag() || dScnPly_ply_c::pauseTimer != 0 || dScnPly_ply_c::nextPauseTimer != 0;
}

void capture(daPy_lk_c* player, const PcNetworkGameSnapshot& view) {
    if (view.local_awards != 0 || view.remote_applies != 0 || view.progress.telescope_owned ||
        dComIfGs_getItem(dInvSlot_TELESCOPE_e) != dItemNo_NONE_e)
        fail("reward/receipt occurred before the measured empty-Telescope baseline");
    sScene = fpcM_GetID(fpcM_Search(playScene, nullptr));
    sPlayer = fpcM_GetID(player);
    for (int i = 0; i != 3; ++i) {
        sSlots[i] = dComIfGs_getSelectItem(i);
        sItems[i] = dComIfGp_getSelectItem(i);
    }
    sRupees = dComIfGs_getRupee(); sLife = dComIfGs_getLife();
    sMagic = dComIfGs_getMagic(); sArrows = dComIfGs_getArrowNum(); sBombs = dComIfGs_getBombNum();
    sCaptured = true;
    writef(STDERR_FILENO,
           "[cos] network-progress: baseline case=%s stage=%s room=%d player=%u scene=%u "
           "XYZslots=%02x/%02x/%02x XYZitems=%02x/%02x/%02x resources=%d/%d/%d/%d/%d\n",
           sCaseName, dComIfGp_getStartStageName(), fopAcM_GetRoomNo(player), sPlayer, sScene,
           sSlots[0], sSlots[1], sSlots[2], sItems[0], sItems[1], sItems[2],
           sRupees, sLife, sMagic, sArrows, sBombs);
}

void invariant(daPy_lk_c* player) {
    if (!sCaptured) return;
    base_process_class* scene = fpcM_Search(playScene, nullptr);
    if (player == nullptr || scene == nullptr || fpcM_GetID(scene) != sScene || fpcM_GetID(player) != sPlayer) {
        writef(STDERR_FILENO, "[cos] network-progress: identity expected=%u/%u actual=%u/%u\n",
            sScene, sPlayer, scene ? fpcM_GetID(scene) : UINT32_MAX, player ? fpcM_GetID(player) : UINT32_MAX);
        fail("native scene/player was replaced during reward qualification");
    }
    for (int i = 0; i != 3; ++i)
        if (dComIfGs_getSelectItem(i) != sSlots[i] || dComIfGp_getSelectItem(i) != sItems[i])
            fail("native saved/cached X/Y/Z selector changed");
    if (dComIfGs_getRupee() != sRupees || dComIfGs_getLife() != sLife ||
        dComIfGs_getMagic() != sMagic || dComIfGs_getArrowNum() != sArrows ||
        dComIfGs_getBombNum() != sBombs)
        fail("local rupees/life/magic/arrows/bombs changed during permanent reward sharing");
}

void connect(const PcNetworkGameSnapshot& view) {
    if (sConnected) return;
    if (view.session.status != PC_NETWORK_CONNECTING && view.session.status != PC_NETWORK_CONNECTED &&
        view.session.status != PC_NETWORK_RECONNECTING && !pc_network_game_start(sHost))
        fail("Host/Join failed for the immutable mounted room");
    sConnected = true;
    writef(STDERR_FILENO, "[cos] network-progress: %s requested after baseline capture\n", sHost ? "Host" : "Join");
}

void diagnostics(unsigned frames, const PcNetworkGameSnapshot& view) {
    if (frames % 300 != 0) return;
    dEvt_control_c* event = g_dComIfG_gameInfo.play.getEvent();
    dEvDtEvent_c* data = dComIfGp_event_runCheck() && event->mEventId != -1
        ? dComIfGp_getPEvtManager()->getEventData(event->mEventId) : nullptr;
    writef(STDERR_FILENO,
           "[cos] network-progress: frame=%u case=%s step=%d age=%u session=%d rev=%llu "
           "out=%u in=%u peers=%u awards=%llu applies=%llu loads=%llu saves=%llu owned=%u safe=%u "
           "pending=%d/%d authorized=%d event=%d/%s mode=%u msg=%u\n",
           frames, sCaseName, static_cast<int>(sStep), sSince, view.session.status,
           static_cast<unsigned long long>(view.session.server_revision), view.session.pending_outgoing,
           view.session.pending_incoming, view.session.peer_count,
           static_cast<unsigned long long>(view.local_awards), static_cast<unsigned long long>(view.remote_applies),
           static_cast<unsigned long long>(view.native_loads), static_cast<unsigned long long>(view.native_saves),
           view.progress.telescope_owned, view.progress.safe_to_apply, view.pending_local, view.pending_remote, view.authorized,
           event->mEventId, data != nullptr ? data->getName() : "none", event->getMode(), dComIfGp_getMesgStatus());
}
} // namespace

void networkProgressFrame(unsigned frames) {
    if (!sChecked) {
        sChecked = true;
        const char* selected = std::getenv("COS_NETWORK_CASE");
        const bool progress = gConfig.smoke != nullptr && std::strcmp(gConfig.smoke, "network-progress") == 0;
        const bool observe = gConfig.smoke != nullptr && std::strcmp(gConfig.smoke, "save-load") == 0 &&
                             selected != nullptr && std::strcmp(selected, "observe") == 0;
        if (!progress && !observe) return;
        if (observe) { sCase = kObserve; sCaseName = "observe"; }
        else if (selected != nullptr && std::strcmp(selected, "receive") == 0) { sCase = kReceive; sCaseName = "receive"; }
        else if (selected != nullptr && selected[0] != '\0' && std::strcmp(selected, "gift") != 0)
            usage("COS_NETWORK_CASE must be gift|receive (observe accompanies COS_SMOKE=save-load)");
        if (gConfig.frames != 0 || gConfig.milestone != nullptr)
            usage("omit COS_FRAMES/COS_MILESTONE; observed reward and real native save/load own completion");
        PcNetworkGameSnapshot view{};
        pc_network_game_snapshot(&view);
        if (!view.prepared || !view.room_mode) usage("requires a validated mounted room profile");
        if (sCase != kObserve) {
            unsigned short items[64], events[64];
            if (pc_boot_items(items, 64) != 0 || bootPreset() != nullptr)
                usage("boot item grants and story presets are forbidden for reward qualification");
            const int count = pc_boot_event_bits(events, 64);
            const PcBootStage* boot = pc_boot_stage();
            if (boot == nullptr || std::strcmp(boot->stage, "sea") != 0 ||
                (sCase == kGift ? boot->room != 44 || boot->point != 206 || count != 1 || events[0] != 0x2A80
                                : boot->room != 11 || boot->point != 9 || count != 0))
                usage("gift needs sea:44:206+BOOT_EVENTS=2A80; receive needs sea:11:9 and no boot events");
            sHost = sCase == kGift;
            const char* autoStart = std::getenv("COS_NETWORK_AUTOSTART");
            if (autoStart != nullptr && autoStart[0] != '\0') {
                if (std::strcmp(autoStart, "host") == 0) sHost = true;
                else if (std::strcmp(autoStart, "join") == 0) sHost = false;
                else usage("COS_NETWORK_AUTOSTART must be host|join");
            }
            if (sCase == kReceive && sHost) usage("receive must join the mounted room, not host it");
            const char* defer = std::getenv("COS_NETWORK_DEFER_MENU");
            if (defer != nullptr && defer[0] != '\0') {
                if (std::strcmp(defer, "1") == 0) sDeferMenu = true;
                else if (std::strcmp(defer, "0") != 0) usage("COS_NETWORK_DEFER_MENU must be 0|1");
            }
            if (sDeferMenu && sCase != kReceive) usage("menu deferral probe is a receive case only");
            enter(kWaitPlayer);
        } else sStep = kWaitPlayer; // passive: leave the file-select driver's PAD untouched
        sStartFrame = frames;
        sStartNs = monotonicNs();
        writef(STDERR_FILENO, "[cos] network-progress: starting case=%s native PAD/producer/save evidence\n", sCaseName);
    }
    if (sStep == kOff) return;
    if (gConfig.runDir && frames % 30 == 0) {
        char flag[1024];
        const int length = std::snprintf(flag, sizeof flag, "%s/network-abort.flag", gConfig.runDir);
        if (length > 0 && length < (int)sizeof flag && access(flag, F_OK) == 0)
            fail("owned peer qualification failed; stopping this diagnostic case");
    }
    ++sSince;
    PcNetworkGameSnapshot view{};
    pc_network_game_snapshot(&view);
    if (view.session.peer_count >= 2) sIndependentPeer = true;
    diagnostics(frames, view);
    if (view.failed || view.session.status == PC_NETWORK_FAILED || view.session.status == PC_NETWORK_REJECTED)
        fail("native networking or compatibility entered a failed/rejected state");
    if (frames - sStartFrame > 30000 || monotonicNs() - sStartNs > 600000000000ULL)
        fail("bounded wait expired before native milestones completed");
    if (sCase == kObserve) {
        if (view.local_awards != 0 || view.remote_applies != 0)
            fail("fresh native load emitted a local award or a received grant");
        if (sStep != kObserved && view.native_loads != 0) {
            if (!view.progress.telescope_owned) fail("freshly activated quest lacks saved Telescope");
            writef(STDERR_FILENO,
                   "[cos] network-progress: QUALIFIED fresh native quest load with Telescope; local_awards=0 remote_applies=0; "
                   "existing save-load still must complete its card comparison\n");
            sStep = kObserved;
        }
        return;
    }
    if (sStep == kSaving) return; // the real save sweep owns input and the terminal result
    drive();
    // Asset creation during the genuine item demo does not replace Link. Only
    // initial setup waits for every room actor; the invariant still checks the
    // same live native player and scene throughout the reward transaction.
    daPy_lk_c* player = readyPlayer(sStep == kWaitPlayer);
    invariant(player);
    if (view.local_awards > 1 || view.remote_applies > 1 ||
        (sCase == kGift && view.remote_applies != 0) || (sCase == kReceive && view.local_awards != 0))
        fail("duplicate or wrong-origin native reward/receipt");

    switch (sStep) {
    case kWaitPlayer:
        if (player != nullptr) {
            if (++sUp >= 90) {
                capture(player, view);
                if (sDeferMenu) enter(kOpenMenu);
                else { connect(view); enter(kReward); }
            }
        } else sUp = 0;
        break;
    case kOpenMenu:
        if (sSince <= 2) drive(PAD_BUTTON_START);
        if (sSince > 3 && nativeMenuOpen() && !view.progress.safe_to_apply) {
            connect(view);
            enter(kWaitDeferred);
        }
        if (sSince > 240) fail("normal START did not open a native pause/menu context");
        break;
    case kWaitDeferred:
        if (!nativeMenuOpen() || view.progress.safe_to_apply || view.remote_applies != 0 || view.progress.telescope_owned)
            fail("incoming progress bypassed native pause/menu deferral");
        if (view.pending_remote && ++sDeferred >= 12) {
            writef(STDERR_FILENO, "[cos] network-progress: canonical receipt deferred for 12 native paused frames with no grant\n");
            enter(kCloseMenu);
        } else if (!view.pending_remote) sDeferred = 0;
        break;
    case kCloseMenu:
        if (sSince % 40 == 2 || sSince % 40 == 3) drive(PAD_BUTTON_B);
        if (!nativeMenuOpen() && view.progress.safe_to_apply) enter(kReward);
        if (sSince > 300) fail("normal B did not close native pause/menu context");
        break;
    case kReward: {
        if (sCase == kGift && (view.local_awards == 0 || dComIfGp_event_runCheck()) && sSince % 10 < 2)
            drive(PAD_BUTTON_B); // include Aryll's ordinary post-award instructions
        const bool earned = sCase == kGift ? view.local_awards == 1 && view.remote_applies == 0
                                         : view.remote_applies == 1 && view.local_awards == 0;
        if (earned && view.progress.telescope_owned && view.progress.safe_to_apply &&
            !dComIfGp_event_runCheck() && view.session.status == PC_NETWORK_CONNECTED &&
            !view.pending_local && view.session.pending_outgoing == 0 && view.session.server_revision != 0) {
            // Keep the owned host alive while the independent receiver can
            // finish its pause/receive/save path. This is ordinary idle input.
            if (++sStable >= (sCase == kGift ? 300u : 45u) &&
                (sCase != kGift || sIndependentPeer)) {
                if (sCase == kReceive && !view.unsaved_remote)
                    fail("native receipt was not marked pending an actual save");
                writef(STDERR_FILENO,
                       "[cos] network-progress: QUALIFIED case=%s local_awards=%llu remote_applies=%llu "
                       "Telescope=1 XYZ/resources unchanged native-event-finished=1 revision=%llu; "
                       "beginning actual native save and GCI verification\n",
                       sCaseName, static_cast<unsigned long long>(view.local_awards),
                       static_cast<unsigned long long>(view.remote_applies),
                       static_cast<unsigned long long>(view.session.server_revision));
                drive();
                sStep = kSaving;
                saveSweepStart("network-progress");
            }
        } else sStable = 0;
        break;
    }
    case kOff:
    case kSaving:
    case kObserved:
        break;
    }
}
} // namespace pc
