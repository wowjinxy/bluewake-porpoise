// Dynamic native acceptance for COS_SMOKE=quick-items. The driven modifier is a
// diagnostic input seam, not proof of a physical keyboard/controller binding.
// No inventory, selectors, actor actions, event state or scene state are written.
// Cases (feature must already be enabled):
//   foot:    COS_BOOT_ITEMS=20,22; Wind Waker owned but not assigned to X/Y/Z.
//   missing: COS_BOOT_ITEMS=20; all three shortcut tools must be absent.
//   boat:    COS_BOOT_PRESET=sailing COS_BOOT_ITEMS=20,31,25; bombs/hook unassigned.
//   boat-missing: COS_BOOT_PRESET=sailing, no COS_BOOT_ITEMS; baton owned but
//                 bomb/hook absent. Only Left/Right are tested, never baton Up.
// The boat case needs at least two bombs. COS_FRAMES and COS_MILESTONE are
// rejected: only observed native milestones may successfully finish this smoke.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc_internal.h"
#include "pc/pc_controls.h"
#include "pc/pc_quick_items.h"

#include "d/actor/d_a_player_main.h"
#include "d/actor/d_a_ship.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_manager.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {
namespace {

enum Case { kFoot, kMissing, kBoat, kBoatMissing };
enum Step {
    kOff, kWait, kSettle, kPlainDown, kNeutralDown, kModifiedDown, kNeutralAction,
    kMissingPulse, kTactStart, kTactCenter, kTactDirection, kTactCancel,
    kCannonDeploy, kCannonShot, kCannonCooldownPress, kCannonCooldownWait,
    kCraneDeploy, kCraneHold, kCraneRaise, kCraneStow, kDone,
};
bool sChecked = false;
Case sCase = kFoot;
const char* sCaseName = "foot";
Step sStep = kOff;
unsigned sSince = 0;
unsigned sStartFrame = 0;
unsigned sSettle = 0;
unsigned sTalkAdvances = 0;
uint64_t sStartNs = 0;
uint64_t sToken = 0;
fpc_ProcID sShipId = 0;
u8 sSavedSlots[3];
u8 sPlayItems[3];
u8 sOwned[3];
u8 sReadyButton = 0;
int sCameraX = 0;
int sCameraY = 0;
int sInitialBombs = 0;
int sShots = 0;
int sLastCooldown = 0;
int sRopePeak = 0;
int sDirection = 0; // normal R/L, then camera-inverted R/L
int sMissingAction = 0;
bool sSawCooldown = false;
bool sDownSeen = false;

void drive(u16 buttons = 0, bool modifier = false, s8 subX = 0) {
    setDrivenPad(true, buttons, 0, 0, subX, 0);
    setDrivenQuickItemModifier(modifier);
}

void enter(Step step) {
    drive(); // a completed state's held sample must not leak into the next one
    sStep = step;
    sSince = 0;
}

[[noreturn]] void finish(bool success, const char* reason) {
    const Step finishedStep = sStep;
    drive();
    pc_camera_invert_x_set(sCameraX);
    pc_camera_invert_y_set(sCameraY);
    sStep = kDone;
    writef(STDERR_FILENO,
           "[cos] quick-items: %s case=%s step=%d age=%u: %s; "
           "scripted modifier only, physical bindings unverified\n",
           success ? "PASS" : "FAIL", sCaseName, static_cast<int>(finishedStep), sSince, reason);
    pc_exit(success ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

void* playScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

daPy_lk_c* playerNow() {
    fopAc_ac_c* actor = dComIfGp_getPlayer(0);
    if (actor == nullptr || fpcM_GetName(actor) != fpcNm_PLAYER_e ||
        fpcM_IsCreating(fpcM_GetID(actor)))
        return nullptr;
    return static_cast<daPy_lk_c*>(actor);
}

bool riding() {
    return dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e) != 0;
}

bool boatCase() {
    return sCase == kBoat || sCase == kBoatMissing;
}

void advanceBootTalk() {
    // The sailing preset can authentically start the boat's DEFAULT_TALK.
    // Advance only a live message that is waiting for player input. A free
    // gameplay A would disembark, so event/name/message checks are all required.
    if (!dComIfGp_event_runCheck())
        return;
    dEvt_control_c* event = g_dComIfG_gameInfo.play.getEvent();
    dEvDtEvent_c* data = event->mEventId != -1
                            ? dComIfGp_getPEvtManager()->getEventData(event->mEventId)
                            : nullptr;
    if (data == nullptr || std::strcmp(data->getName(), "DEFAULT_TALK") != 0)
        return;
    const u8 status = dComIfGp_getMesgStatus();
    if (status != fopMsgStts_STOP_e && status != fopMsgStts_CLOSE_WAIT_e &&
        status != fopMsgStts_MSG_DISPLAYED_e && status != fopMsgStts_MSG_CONTINUES_e &&
        status != fopMsgStts_MSG_ENDS_e) // dMsg_finishProc also waits for ordinary A/B
        return;
    if (sSince % 24 == 0) {
        drive(PAD_BUTTON_A); // one sample, with a neutral sample on the next frame
        if (sTalkAdvances++ == 0 || sTalkAdvances % 8 == 0)
            writef(STDERR_FILENO,
                   "[cos] quick-items: normal A advances boot DEFAULT_TALK event=%d message=%u pulse=%u\n",
                   event->mEventId, status, sTalkAdvances);
    }
}

bool assigned(u8 item, u8 slot) {
    for (int i = 0; i != 3; ++i) {
        if (sSavedSlots[i] == slot || sPlayItems[i] == item)
            return true;
    }
    return false;
}

void capture(daPy_lk_c* player) {
    sToken = pc_quick_items_context();
    for (int i = 0; i != 3; ++i) {
        sSavedSlots[i] = dComIfGs_getSelectItem(i);
        sPlayItems[i] = dComIfGp_getSelectItem(i);
    }
    sOwned[0] = dComIfGs_getItem(dInvSlot_WIND_WAKER_e);
    sOwned[1] = dComIfGs_getItem(dInvSlot_BOMB_e);
    sOwned[2] = dComIfGs_getItem(dInvSlot_GRAPPLING_HOOK_e);
    sReadyButton = player->getPcReadyItemButton();
    sInitialBombs = dComIfGs_getBombNum();
    if (sCase == kFoot &&
        (sOwned[0] != dItemNo_WIND_WAKER_e ||
         assigned(dItemNo_WIND_WAKER_e, dInvSlot_WIND_WAKER_e)))
        finish(false, "foot precondition needs owned, unassigned Wind Waker");
    if (sCase == kMissing &&
        (sOwned[0] == dItemNo_WIND_WAKER_e || sOwned[1] == dItemNo_BOMB_BAG_e ||
         sOwned[2] == dItemNo_GRAPPLING_HOOK_e))
        finish(false, "missing precondition needs all three shortcut tools absent");
    if (boatCase()) {
        daShip_c* ship = dComIfGp_getShipActor();
        const char* stage = dComIfGp_getStartStageName();
        if (!riding() || ship == nullptr ||
            stage == nullptr || std::strcmp(stage, "sea") != 0)
            finish(false, "boat case needs an actual sea scene and riding player/ship");
        if (sCase == kBoat &&
            (sOwned[1] != dItemNo_BOMB_BAG_e || sOwned[2] != dItemNo_GRAPPLING_HOOK_e ||
             assigned(dItemNo_BOMB_BAG_e, dInvSlot_BOMB_e) ||
             assigned(dItemNo_GRAPPLING_HOOK_e, dInvSlot_GRAPPLING_HOOK_e) || sInitialBombs < 2))
            finish(false, "boat precondition needs sea, riding, unassigned bombs/hook, >=2 bombs");
        if (sCase == kBoatMissing &&
            (sOwned[0] != dItemNo_WIND_WAKER_e || sOwned[1] != dItemNo_NONE_e ||
             sOwned[2] != dItemNo_NONE_e))
            finish(false, "boat-missing needs owned baton and empty bomb/hook inventory slots");
        if (sCase == kBoatMissing)
            sMissingAction = 1; // Left and Right only: the preset owns a working baton.
        sShipId = fpcM_GetID(ship);
    } else if (riding()) {
        finish(false, "on-foot case unexpectedly riding");
    }
    writef(STDERR_FILENO,
           "[cos] quick-items: ready case=%s token=%llu XYZ-slots=%02x/%02x/%02x "
           "XYZ-items=%02x/%02x/%02x ready=%u bombs=%d\n",
           sCaseName, static_cast<unsigned long long>(sToken), sSavedSlots[0], sSavedSlots[1],
           sSavedSlots[2], sPlayItems[0], sPlayItems[1], sPlayItems[2], sReadyButton, sInitialBombs);
}

void invariant(daPy_lk_c* player) {
    base_process_class* scene = fpcM_Search(playScene, nullptr);
    if (scene == nullptr || player == nullptr ||
        static_cast<u32>(fpcM_GetID(scene)) != static_cast<u32>(sToken >> 32) ||
        static_cast<u32>(fpcM_GetID(player)) != static_cast<u32>(sToken))
        finish(false, "scene/player disappeared or was replaced");
    for (int i = 0; i != 3; ++i) {
        if (dComIfGs_getSelectItem(i) != sSavedSlots[i] ||
            dComIfGp_getSelectItem(i) != sPlayItems[i])
            finish(false, "saved or cached X/Y/Z selector changed");
    }
    if (dComIfGs_getItem(dInvSlot_WIND_WAKER_e) != sOwned[0] ||
        dComIfGs_getItem(dInvSlot_BOMB_e) != sOwned[1] ||
        dComIfGs_getItem(dInvSlot_GRAPPLING_HOOK_e) != sOwned[2])
        finish(false, "shortcut changed tool ownership");
    // All tested active tools are deliberately unassigned, so native selection
    // of an already-assigned item cannot legitimately change the ready index.
    if (player->getPcReadyItemButton() != sReadyButton)
        finish(false, "unassigned shortcut changed the ready-item button");
    if (boatCase()) {
        daShip_c* ship = dComIfGp_getShipActor();
        if (!riding() || ship == nullptr || fpcM_GetID(ship) != sShipId)
            finish(false, "riding/ship identity changed");
    }
}

void bound(unsigned limit, const char* reason) {
    if (sSince > limit)
        finish(false, reason);
}

void pulse(u16 button, bool modifier = true) {
    if (sSince <= 2)
        drive(button, modifier);
}

bool forbidden(daPy_lk_c* player) {
    return player->getPcActionProc() == daPy_lk_c::daPyProc_TACT_WAIT_e ||
           player->getPcActionProc() == daPy_lk_c::daPyProc_SHIP_CANNON_e ||
           player->getPcActionProc() == daPy_lk_c::daPyProc_SHIP_CRANE_e;
}

} // namespace

void quickItemsFrame(unsigned frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || std::strcmp(gConfig.smoke, "quick-items") != 0)
            return;
        const char* testCase = std::getenv("COS_QUICK_ITEMS_CASE");
        if (testCase != nullptr && testCase[0] != '\0') {
            sCaseName = testCase;
            if (std::strcmp(testCase, "boat") == 0) sCase = kBoat;
            else if (std::strcmp(testCase, "boat-missing") == 0) sCase = kBoatMissing;
            else if (std::strcmp(testCase, "missing") == 0) sCase = kMissing;
            else if (std::strcmp(testCase, "foot") != 0) {
                writef(STDERR_FILENO, "[cos] quick-items: unknown case %s\n", testCase);
                pc_exit(PC_EXIT_USAGE);
            }
        }
        if (gConfig.frames != 0 || gConfig.milestone != nullptr) {
            writef(STDERR_FILENO, "[cos] quick-items: omit COS_FRAMES/COS_MILESTONE; native checks own completion\n");
            pc_exit(PC_EXIT_USAGE);
        }
        sCameraX = pc_camera_invert_x();
        sCameraY = pc_camera_invert_y();
        if (!pc_quick_items_enabled())
            finish(false, "feature disabled at startup; launch with COS_QUICK_ITEMS=1");
        sStartFrame = frames;
        sStartNs = monotonicNs();
        enter(kWait);
        writef(STDERR_FILENO, "[cos] quick-items: starting case=%s; dynamic native milestones\n", sCaseName);
    }
    if (sStep == kOff || sStep == kDone)
        return;
    drive(); // release any previous sample unless this state's action holds it
    ++sSince;
    if (frames - sStartFrame > 20000 || monotonicNs() - sStartNs > 300000000000ULL)
        finish(false, "overall bounded wait expired before all milestones");
    daPy_lk_c* player = playerNow();
    if (sToken != 0)
        invariant(player);
    daShip_c* ship = boatCase() && sToken != 0 ? dComIfGp_getShipActor() : nullptr;

    switch (sStep) {
    case kWait:
    case kSettle:
        if (player != nullptr && pc_quick_items_context() != 0 &&
            (boatCase() ? riding() && dComIfGp_getShipActor() != nullptr : !riding()) &&
            stageRoomReady(fopAcM_GetRoomNo(player), nullptr) == nullptr) {
            if (++sSettle >= 90) {
                capture(player);
                enter(kPlainDown);
            }
        } else {
            sSettle = 0;
            advanceBootTalk();
        }
        break;
    case kPlainDown:
    case kModifiedDown:
        if (sSince <= 4)
            drive(PAD_BUTTON_DOWN, sStep == kModifiedDown);
        if (sSince >= 2 && sSince <= 4) {
            if (!CPad_CHECK_HOLD_DOWN(0))
                finish(false, "plain Down was consumed before native controller state");
            if (pc_quick_items_pressed(PC_QUICK_ITEM_TACT) ||
                pc_quick_items_pressed(PC_QUICK_ITEM_CANNON) ||
                pc_quick_items_pressed(PC_QUICK_ITEM_CRANE) || forbidden(player))
                finish(false, "Down unexpectedly generated an equipment action");
            sDownSeen = true;
        }
        if (sSince > 4) {
            if (!sDownSeen) finish(false, "Down observation missing");
            sDownSeen = false;
            enter(sStep == kPlainDown ? kNeutralDown : kNeutralAction);
        }
        break;
    case kNeutralDown:
        if (sSince >= 4) enter(kModifiedDown);
        break;
    case kNeutralAction:
        if (pc_quick_items_context() != sToken) {
            bound(180, "safe context did not return before shortcut");
        } else if (sSince >= 4) {
            writef(STDERR_FILENO, "[cos] quick-items: plain and modified Down preserved\n");
            enter(sCase == kFoot ? kTactStart : sCase == kBoat ? kCannonDeploy : kMissingPulse);
        }
        break;
    case kMissingPulse: {
        const u16 buttons[] = {PAD_BUTTON_UP, PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT};
        pulse(buttons[sMissingAction]);
        if (forbidden(player) || dComIfGs_getBombNum() != sInitialBombs)
            finish(false, "absent tool entered native action or consumed ammunition");
        if (sSince >= 24) {
            if (++sMissingAction == 3)
                finish(true, sCase == kBoatMissing
                                 ? "riding sea Left/Right ignored with bomb/hook absent; ammo/Down/XYZ preserved"
                                 : "absent tools ignored; Down and XYZ preserved");
            enter(kMissingPulse); // twenty neutral samples already rearmed the edge
        }
        break;
    }
    case kTactStart:
        pulse(PAD_BUTTON_UP);
        if (player->checkTactWait()) {
            writef(STDERR_FILENO, "[cos] quick-items: native TACT_WAIT entered from owned unassigned baton\n");
            pc_camera_invert_x_set(0);
            pc_camera_invert_y_set(0);
            enter(kTactCenter);
        }
        bound(240, "Wind Waker shortcut did not enter native TACT_WAIT");
        break;
    case kTactCenter:
        if (!player->checkTactWait()) finish(false, "conducting ended during direction checks");
        if (sSince >= 8 && player->getTactLeftHandPos() == mDoAud_ARM_CENTER_e) {
            if (sDirection == 2) {
                pc_camera_invert_x_set(1);
                pc_camera_invert_y_set(1);
            }
            enter(kTactDirection);
        }
        bound(120, "baton did not return to center");
        break;
    case kTactDirection: {
        const bool right = (sDirection % 2) == 0;
        drive(0, false, right ? 100 : -100);
        if (!player->checkTactWait()) finish(false, "conducting ended before direction proof");
        if (sSince >= 3 &&
            (right ? CPad_GET_SUBSTICK_POS_X(0) > 0.5f : CPad_GET_SUBSTICK_POS_X(0) < -0.5f) &&
            player->getTactLeftHandPos() == (right ? mDoAud_ARM_RIGHT_e : mDoAud_ARM_LEFT_e)) {
            writef(STDERR_FILENO, "[cos] quick-items: baton raw C-stick %s correct camera-inversion=%d\n",
                   right ? "right" : "left", sDirection >= 2);
            ++sDirection;
            enter(sDirection == 4 ? kTactCancel : kTactCenter);
        }
        bound(120, "raw C-stick did not produce the native baton direction");
        break;
    }
    case kTactCancel:
        // Give the previous direction a neutral sample, then press B repeatedly
        // through the authored cancellation/event teardown, without forcing it.
        if (sSince % 30 == 4 || sSince % 30 == 5) drive(PAD_BUTTON_B);
        if (sSince > 8 && !player->checkTactWait() &&
            !dComIfGp_checkPlayerStatus1(0, daPyStts1_WIND_WAKER_CONDUCT_e) &&
            pc_quick_items_context() == sToken)
            finish(true, "native baton start, both raw directions/inversions, cancel; Down/XYZ preserved");
        bound(300, "native B did not cancel conducting and return safe context");
        break;
    case kCannonDeploy:
        pulse(PAD_BUTTON_LEFT);
        if (dComIfGs_getBombNum() != sInitialBombs)
            finish(false, "cannon deployment consumed ammunition");
        if (player->getPcActionProc() == daPy_lk_c::daPyProc_SHIP_CANNON_e &&
            ship->mCurMode == daShip_c::MODE_CANNON_e && ship->getPart() == daShip_c::PART_CANNON_e &&
            ship->mProc == &daShip_c::procCannon && ship->m037A == 0 && sSince > 5) {
            writef(STDERR_FILENO, "[cos] quick-items: native cannon deployed without assigning bombs\n");
            enter(kCannonShot);
        }
        bound(300, "cannon did not finish native deployment");
        break;
    case kCannonShot:
        pulse(PAD_BUTTON_LEFT);
        sSawCooldown = sSawCooldown || ship->m037A > 0;
        if (dComIfGs_getBombNum() < sInitialBombs - sShots - 1)
            finish(false, "cannon consumed more than one bomb per press");
        if (dComIfGs_getBombNum() == sInitialBombs - sShots - 1 && sSawCooldown && sSince > 3) {
            ++sShots;
            writef(STDERR_FILENO, "[cos] quick-items: real cannon shot=%d bombs=%u cooldown=%d\n",
                   sShots, dComIfGs_getBombNum(), ship->m037A);
            sLastCooldown = ship->m037A;
            if (sShots == 1) {
                if (sLastCooldown < 8) finish(false, "could not observe shot early enough to test cooldown");
                enter(kCannonCooldownPress);
            } else enter(kCannonCooldownWait);
        }
        bound(45, "cannon press did not consume exactly one bomb with native cooldown");
        break;
    case kCannonCooldownPress:
        if (sSince >= 3 && sSince <= 4) drive(PAD_BUTTON_LEFT, true);
        if (ship->m037A > sLastCooldown || dComIfGs_getBombNum() != sInitialBombs - 1)
            finish(false, "press during cannon cooldown reset timer or consumed another bomb");
        sLastCooldown = ship->m037A;
        if (sSince >= 7) {
            writef(STDERR_FILENO, "[cos] quick-items: repeat press rejected during native cannon cooldown\n");
            enter(kCannonCooldownWait);
        }
        break;
    case kCannonCooldownWait:
        if (dComIfGs_getBombNum() != sInitialBombs - sShots)
            finish(false, "unexpected ammunition change while waiting for cooldown");
        if (sSince >= 4 && ship->m037A == 0 && pc_quick_items_context() == sToken) {
            sSawCooldown = false;
            enter(sShots == 1 ? kCannonShot : kCraneDeploy);
        }
        bound(120, "native cannon cooldown did not expire");
        break;
    case kCraneDeploy:
        pulse(PAD_BUTTON_RIGHT);
        if (dComIfGs_getBombNum() != sInitialBombs - 2)
            finish(false, "switching to crane changed ammunition");
        if (player->getPcActionProc() == daPy_lk_c::daPyProc_SHIP_CRANE_e &&
            ship->mCurMode == daShip_c::MODE_CRANE_e && ship->getPart() == daShip_c::PART_CRANE_e &&
            ship->mProc == &daShip_c::procCrane && ship->getRopeCnt() == 20 && sSince > 8) {
            writef(STDERR_FILENO, "[cos] quick-items: native crane deployed with rope=%d\n", ship->getRopeCnt());
            enter(kCraneHold);
        }
        bound(360, "crane did not finish native deployment");
        break;
    case kCraneHold:
        // Neutral samples after deployment prevent the transition's held key
        // from becoming an accidental rope operation in the next context.
        if (sSince > 4) drive(PAD_BUTTON_RIGHT, true);
        if (ship->getRopeCnt() >= 60) {
            sRopePeak = ship->getRopeCnt();
            writef(STDERR_FILENO, "[cos] quick-items: held shortcut lowered real rope to %d\n", sRopePeak);
            enter(kCraneRaise);
        }
        bound(180, "held crane shortcut did not lower rope");
        break;
    case kCraneRaise:
        if (ship->getRopeCnt() > sRopePeak)
            finish(false, "rope continued lowering after modifier release");
        if (sSince > 4 && ship->getRopeCnt() == 20 && sRopePeak >= 60) {
            writef(STDERR_FILENO, "[cos] quick-items: modifier release raised real rope to 20\n");
            enter(kCraneStow);
        }
        bound(240, "modifier release did not retract crane rope");
        break;
    case kCraneStow:
        pulse(PAD_BUTTON_A, false);
        if (sSince > 4 && player->getPcActionProc() == daPy_lk_c::daPyProc_SHIP_PADDLE_e &&
            ship->mCurMode == daShip_c::MODE_PADDLE_MOVE_e &&
            ship->getPart() != daShip_c::PART_CANNON_e && ship->getPart() != daShip_c::PART_CRANE_e)
            finish(true, "native cannon deploy/two shots/cooldown, crane lower/raise/stow; Down/XYZ preserved");
        bound(300, "native A did not stow crane into paddling");
        break;
    case kOff:
    case kDone:
        break;
    }
}

} // namespace pc
