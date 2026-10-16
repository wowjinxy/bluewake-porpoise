// SPDX-License-Identifier: GPL-3.0-or-later
// Off unless explicitly selected. Two independent private room profiles:
//   COS_SMOKE=network-fairy COS_NETWORK_FAIRY_CASE=gift|duplicate
//   COS_BOOT_STAGE=Fairy01:0:0:-1 COS_TARGET_FPS=30
// No boot items/events/preset/input script. The gift process hosts and walks
// into the real fairy. The duplicate process joins, receives wallet1 outside
// the trigger, then completes its OWN same fairy (which naturally selects the
// wallet2 item). The authored STEP must settle back to wallet1 without a room
// revision advance. No actor, item, event, effect or completion is fabricated.
// Both cases save through the existing native save-screen sweep. The host
// stays alive until the private peer-save synchronization marker AND peer
// disconnect. Markers are choreography, never peer acceptance; aggregation
// MUST require BOTH processes' QUALIFIED/PASS and native saved-file markers.
// Fresh offline proof: COS_SMOKE=save-load COS_NETWORK_FAIRY_CASE=observe,
// no boot stage; ordinary title/file-select input belongs to save-load.
// All modes require COS_NETWORK_FAIRY_PRIVATE_ROOT beneath build/, containing
// network-fairy.allow with exactly "BlueWake.Native.Fairy.Private.v1\n".
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc_network_fairy_smoke.h"
#include "pc_internal.h"
#include "pc/pc_network_game.h"
#include "pc/network/pc_network_client_store.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "d/actor/d_a_bigelf.h"
#include "d/actor/d_a_demo_item.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_manager.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "../../network/atomic_file.h"
#include <dolphin/pad.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace pc {
namespace {
namespace fs = std::filesystem;
enum class Case { Off, Gift, Duplicate, Observe };
enum class Step { Waiting, Connecting, Receiving, Approach, Event, Settling,
                  Saving, SavedDrain, HostWaitPeer, Observing, Done };
constexpr u16 kFlag = 0x3020;
constexpr u32 kFairyParameter = 0x00ffff00;
// authoredId("Fairy01", room0, FAIRY3, flag3020, BIGELF0173, type0, no name).
constexpr u64 kReward = UINT64_C(0x039abbd4b55b1acb);
constexpr u32 kWallet = PC_PERMANENT_WALLET;
Case sCase = Case::Off;
Step sStep = Step::Waiting;
bool sChecked = false, sCaptured = false, sConnected = false;
bool sAccepted = false, sItemSeen = false, sClaimSeen = false, sSaveCompared = false;
bool sLoadCompared = false, sSeenPeer = false, sQuestWrite = false;
unsigned sFrame = 0, sStartFrame = 0, sSince = 0, sStable = 0;
uint64_t sStartNs = 0, sGeneration = 0, sRevision = 0, sInvocation = 0;
uint64_t sSavesBefore = 0, sSaveGeneration = 0;
PcProgressIdentity sIdentity{};
u32 sFairyId = UINT32_MAX;
u8 sSlots[3]{}, sItems[3]{};
int sResources[5]{};
char sRoom[33]{}, sPeer[33]{};
std::string sGci, sSavedHash, sPrivate;
char sLocalPlayer[33]{};

const char* caseName() {
    return sCase == Case::Gift ? "gift" : sCase == Case::Duplicate ? "duplicate" : "observe";
}
void drive(u16 buttons = 0, int8_t x = 0, int8_t y = 0) {
    setDrivenPad(true, buttons, x, y);
    setDrivenQuickItemModifier(false);
}
[[noreturn]] void fail(const char* reason) {
    if (sCase != Case::Observe) drive();
    writef(STDERR_FILENO, "[cos] network-fairy: FAIL case=%s step=%d frame=%u age=%u: %s\n",
           caseName(), static_cast<int>(sStep), sFrame, sSince, reason);
    pc_exit(PC_EXIT_CHECK_FAILED);
}
[[noreturn]] void usage(const char* reason) {
    writef(STDERR_FILENO, "[cos] network-fairy: usage: %s\n", reason);
    pc_exit(PC_EXIT_USAGE);
}
void need(bool condition, const char* reason) { if (!condition) fail(reason); }
void enter(Step step) {
    if (sCase != Case::Observe) drive();
    sStep = step; sSince = sStable = 0;
}
PcNetworkGameSnapshot snapshot() {
    PcNetworkGameSnapshot view{}; pc_network_game_snapshot(&view); return view;
}
std::string utf8(const fs::path& path) {
    const auto value = path.u8string(); return {value.begin(), value.end()};
}
fs::path absolute(const char* value) {
    if (!value || !*value || std::strlen(value) >= 1024) usage("bounded explicit private paths required");
    return fs::absolute(fs::u8path(value)).lexically_normal();
}
bool samePart(const fs::path& a, const fs::path& b) {
#ifdef _WIN32
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
#else
    return a == b;
#endif
}
bool inside(const fs::path& path, const fs::path& parent) {
    auto a = path.begin();
    for (auto b = parent.begin(); b != parent.end(); ++a, ++b)
        if (a == path.end() || !samePart(*a, *b)) return false;
    return path != parent;
}
void validatePrivate() {
    const auto root = absolute(std::getenv("COS_NETWORK_FAIRY_PRIVATE_ROOT"));
    bool belowBuild = false;
    for (const auto& part : root.parent_path()) if (part == "build") belowBuild = true;
    if (!belowBuild || !fs::is_directory(root) || fs::is_symlink(fs::symlink_status(root)))
        usage("private fairy root must be an existing directory beneath build/");
    // Resolve all ancestors, so a lexical build/ path cannot conceal a junction.
    if (!samePart(fs::weakly_canonical(root), root)) usage("private root must have no linked ancestors");
    const auto marker = root / "network-fairy.allow";
    if (!fs::is_regular_file(fs::symlink_status(marker)) || fs::hard_link_count(marker) != 1)
        usage("private opt-in marker must be one regular file");
    const char expected[] = "BlueWake.Native.Fairy.Private.v1\n";
    FILE* file = pc_net_atomic_open(utf8(marker).c_str(), "rb");
    if (!file) usage("cannot read private opt-in marker");
    char bytes[sizeof expected]{};
    const bool matched = std::fread(bytes, 1, sizeof expected - 1, file) == sizeof expected - 1 &&
        std::fgetc(file) == EOF && !std::ferror(file) &&
        !std::memcmp(bytes, expected, sizeof expected - 1);
    const bool closed = std::fclose(file) == 0;
    if (!matched || !closed) usage("private opt-in marker mismatch");
    const auto run = absolute(gConfig.runDir);
    const auto storage = absolute(std::getenv("COS_NETWORK_ROOT"));
    const auto prefs = absolute(std::getenv("COS_NETWORK_PROFILE"));
    const auto mounted = absolute(pc_network_game_card_base());
    if (!inside(run, root) || !inside(storage, root) || !inside(prefs, root) || !inside(mounted, storage))
        usage("run/profile/storage/mounted CARD must be beneath the explicit private root");
    for (const auto& path : {run, storage, prefs, mounted})
        if (!samePart(fs::weakly_canonical(path), path)) usage("private paths must have no linked ancestors");
    const char* card = std::getenv("COS_CARD_DIR");
    if (card && *card) usage("room diagnostic refuses a personal CARD override");
    sGci = utf8(mounted / "USA" / "Card A" / "01-GZLE-gczelda.gci");
    if (!runCardGciPath() || !samePart(absolute(runCardGciPath()), absolute(sGci.c_str())))
        usage("actual harness GCI differs from the mounted room Card A");
    sPrivate = utf8(root);
    if (sCase != Case::Observe &&
        (fs::exists(root / "network-fairy.receiver-ready") || fs::exists(root / "network-fairy.receiver-saved")))
        usage("use a fresh private root without previous fairy synchronization markers");
}
fs::path signalPath(bool saved) {
    return fs::u8path(sPrivate) / (saved ? "network-fairy.receiver-saved" : "network-fairy.receiver-ready");
}
std::string signalText(bool saved, const char* player) {
    char text[256];
    const int size = std::snprintf(text, sizeof text,
        "BlueWake.Native.Fairy.Sync.v1\nphase=%s\nroom=%s\nsource=%016llx\nrevision=%u\nplayer=%s\n",
        saved ? "saved" : "ready", sRoom, static_cast<unsigned long long>(kReward), saved ? 1 : 0, player);
    need(size > 0 && size < static_cast<int>(sizeof text), "private synchronization record overflow");
    return text;
}
void writeSignal(bool saved) {
    const auto path = signalPath(saved);
    need(!fs::exists(fs::symlink_status(path)), "private synchronization record already exists");
    const auto text = signalText(saved, sLocalPlayer);
    const auto final = utf8(path);
    char* pending = pc_net_atomic_path(final.c_str());
    need(pending != nullptr, "cannot allocate bounded synchronization path");
    FILE* file = pc_net_atomic_open(pending, "wbx");
    if (!file) { std::free(pending); fail("cannot create private synchronization pending file"); }
    bool ok = std::fwrite(text.data(), 1, text.size(), file) == text.size() && pc_net_atomic_flush(file);
    if (std::fclose(file)) ok = false;
    if (ok) {
#ifdef _WIN32
        ok = pc_net_atomic_move(pending, final.c_str(), false) == 0;
#else
        ok = link(pending, final.c_str()) == 0;
#endif
    }
    pc_net_atomic_remove(pending); std::free(pending);
    need(ok, "cannot atomically publish private peer synchronization marker");
}
bool readSignal(bool saved) {
    const auto path = signalPath(saved);
    if (!fs::exists(fs::symlink_status(path))) return false;
    need(fs::is_regular_file(fs::symlink_status(path)) && fs::hard_link_count(path) == 1,
         "peer synchronization marker is linked/nonregular");
    const auto expected = signalText(saved, sPeer);
    FILE* file = pc_net_atomic_open(utf8(path).c_str(), "rb");
    need(file != nullptr, "cannot read private peer synchronization marker");
    char bytes[256]{};
    const bool matched = std::fread(bytes, 1, expected.size(), file) == expected.size() &&
        std::fgetc(file) == EOF && !std::ferror(file) && !std::memcmp(bytes, expected.data(), expected.size());
    const bool closed = std::fclose(file) == 0;
    need(matched && closed, "private peer synchronization room/source/revision/identity mismatch");
    return true;
}
bool selected() {
    if (sChecked) return sCase != Case::Off;
    sChecked = true;
    const char* mode = std::getenv("COS_NETWORK_FAIRY_CASE");
    const bool fairy = gConfig.smoke && !std::strcmp(gConfig.smoke, "network-fairy");
    const bool observe = gConfig.smoke && !std::strcmp(gConfig.smoke, "save-load") && mode && *mode;
    if (!fairy && !observe) return false;
    if (observe && !std::strcmp(mode, "observe")) sCase = Case::Observe;
    else if (fairy && mode && !std::strcmp(mode, "gift")) sCase = Case::Gift;
    else if (fairy && mode && !std::strcmp(mode, "duplicate")) sCase = Case::Duplicate;
    else usage("network-fairy needs FAIRY_CASE=gift|duplicate; save-load needs FAIRY_CASE=observe");
    JKRPcHostAllocScope host;
    if (gConfig.frames || gConfig.milestone || gConfig.input || gConfig.uncapped)
        usage("omit frame/milestone/input/uncapped overrides; observed milestones own completion");
    const char* fps = std::getenv("COS_TARGET_FPS");
    if (!fps || std::strcmp(fps, "30")) usage("requires explicit COS_TARGET_FPS=30");
    const char* recovery = std::getenv("COS_NETWORK_RECOVERY");
    const char* legacyCase = std::getenv("COS_NETWORK_CASE");
    const char* autoStart = std::getenv("COS_NETWORK_AUTOSTART");
    if ((recovery && *recovery) || (legacyCase && *legacyCase) || (autoStart && *autoStart))
        usage("omit other network diagnostic/auto-start selectors");
    const auto view = snapshot();
    if (!view.prepared || !view.room_mode) usage("requires a frozen validated native room mount");
    PcNetworkProfile profile{};
    if (!pc_network_game_profile(&profile)) usage("frozen room profile unavailable");
    std::strcpy(sLocalPlayer, profile.config.player_id);
    validatePrivate();
    unsigned short list[64];
    if (pc_boot_items(list, 64) || pc_boot_event_bits(list, 64) || bootPreset())
        usage("boot item/event grants and story presets are forbidden");
    const auto* boot = pc_boot_stage();
    if (sCase == Case::Observe) {
        if (boot) usage("offline observe must load through title/file select, without BOOT_STAGE");
        sStep = Step::Observing;
    } else {
        if (!boot || std::strcmp(boot->stage, "Fairy01") || boot->room || boot->point || boot->layer != -1)
            usage("genuine fairy fixture needs COS_BOOT_STAGE=Fairy01:0:0:-1");
        drive();
    }
    sStartFrame = sFrame; sStartNs = monotonicNs();
    writef(STDERR_FILENO, "[cos] network-fairy: starting case=%s expected-source=%016llx native-PAD-only=1\n",
           caseName(), static_cast<unsigned long long>(kReward));
    return true;
}
void* findPlay(void* proc, void*) { return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr; }
void* findFairy(void* proc, void*) { return fpcM_GetName(proc) == fpcNm_BIGELF_e ? proc : nullptr; }
void* findDemoItem(void* proc, void*) { return fpcM_GetName(proc) == fpcNm_Demo_Item_e ? proc : nullptr; }
daPy_lk_c* readyPlayer(bool wholeRoom) {
    auto* player = dComIfGp_getPlayer(0);
    if (!fpcM_Search(findPlay, nullptr) || !player || fpcM_GetName(player) != fpcNm_PLAYER_e ||
        fpcM_IsCreating(fpcM_GetID(player)) || !fpcM_IsExecuting(fpcM_GetID(player)) ||
        (wholeRoom && stageRoomReady(fopAcM_GetRoomNo(player), nullptr))) return nullptr;
    return static_cast<daPy_lk_c*>(player);
}
daBigelf_c* fairy() {
    auto* actor = static_cast<daBigelf_c*>(fpcM_Search(findFairy, nullptr));
    if (!actor || fpcM_IsCreating(fpcM_GetID(actor)) || !fpcM_IsExecuting(fpcM_GetID(actor))) return nullptr;
    return actor;
}
void capture(const PcNetworkGameSnapshot& view, daPy_lk_c* player, daBigelf_c* actor) {
    need(!std::strcmp(dComIfGp_getStartStageName(), "Fairy01") && fopAcM_GetRoomNo(player) == 0,
         "native boot did not enter the authored Fairy01 room");
    need(actor && fopAcM_GetParam(actor) == kFairyParameter && actor->getType() == 0 &&
         actor->getEventFlag() == kFlag && fopAcM_GetRoomNo(actor) == 0 &&
         std::fabs(actor->home.pos.x) < 1 && std::fabs(actor->home.pos.z + 2200) < 1 &&
         std::fabs(actor->home.pos.y - 81.29f) < 1, "authored fairy identity/parameters/placement mismatch");
    need(!dComIfGs_isEventBit(kFlag) && !dComIfGs_getWalletSize() && !view.progress.facts[kWallet] &&
         !view.local_awards && !view.remote_applies && !view.native_loads && !view.native_saves,
         "fixture must start before any fairy award, receipt, saved quest or upgrade");
    need((actor->current.pos - player->current.pos).absXZ() > 1200 && !dComIfGp_event_runCheck(),
         "baseline entered the fairy trigger before capture");
    sIdentity = view.progress.identity; sFairyId = fpcM_GetID(actor);
    for (int i = 0; i != 3; ++i) { sSlots[i] = dComIfGs_getSelectItem(i); sItems[i] = dComIfGp_getSelectItem(i); }
    sResources[0] = dComIfGs_getRupee(); sResources[1] = dComIfGs_getLife();
    sResources[2] = dComIfGs_getMagic(); sResources[3] = dComIfGs_getArrowNum(); sResources[4] = dComIfGs_getBombNum();
    need(sResources[1] == dComIfGs_getMaxLife() && sResources[2] == dComIfGs_getMaxMagic(),
         "fresh fixture must already have full life/magic before native fairy healing");
    sCaptured = true;
    writef(STDERR_FILENO, "[cos] network-fairy: baseline case=%s stage=Fairy01 room=0 profile=%s quest=%llu slot=%d "
           "player=%u scene=%u fairy=%u wallet=0 flag3020=0 resources=%d/%d/%d/%d/%d\n",
           caseName(), sIdentity.profile_id, static_cast<unsigned long long>(sIdentity.quest_generation),
           sIdentity.quest_slot, sIdentity.player_id, sIdentity.scene_id, sFairyId,
           sResources[0], sResources[1], sResources[2], sResources[3], sResources[4]);
}
void invariant(const PcNetworkGameSnapshot& view, daPy_lk_c* player) {
    if (!sCaptured) return;
    const auto& id = view.progress.identity;
    need(player && id.scene_id == sIdentity.scene_id && id.player_id == sIdentity.player_id &&
         id.session_id == sIdentity.session_id && id.quest_generation == sIdentity.quest_generation &&
         id.quest_slot == sIdentity.quest_slot && !std::strcmp(id.profile_id, sIdentity.profile_id),
         "native scene/player/quest/profile changed during the isolated reward case");
    for (int i = 0; i != 3; ++i)
        need(dComIfGs_getSelectItem(i) == sSlots[i] && dComIfGp_getSelectItem(i) == sItems[i],
             "saved/cached XYZ selection changed");
    need(dComIfGs_getRupee() == sResources[0] && dComIfGs_getLife() == sResources[1] &&
         dComIfGs_getMagic() == sResources[2] && dComIfGs_getArrowNum() == sResources[3] &&
         dComIfGs_getBombNum() == sResources[4], "local resources changed during wallet-only sharing");
    need(!view.progress.event_overflow && !view.progress.recovery_required, "native event/claim recovery required");
    if (sConnected) {
        need(view.session.status == PC_NETWORK_CONNECTED && view.session.generation == sGeneration &&
             !std::strcmp(view.session.room_identity, sRoom) && !view.session.reconnects,
             "room identity/generation changed or connection rejoined during qualification");
        need(view.session.peer_count <= 2, "unrelated peer joined the isolated two-player fixture");
        if (sRevision) need(view.session.server_revision == sRevision, "same fairy advanced the canonical room revision twice");
    }
}
void observeProducer(const PcNetworkGameSnapshot& view, daPy_lk_c* player, daBigelf_c* actor) {
    if (dComIfGp_event_runCheck()) {
        auto* control = g_dComIfG_gameInfo.play.getEvent();
        auto* data = control->mEventId >= 0 ? dComIfGp_getPEvtManager()->getEventData(control->mEventId) : nullptr;
        if (actor && actor->eventInfo.checkCommandDemoAccrpt() && control->getPt1() == actor &&
            control->getPt2() == player && data && !std::strcmp(data->getName(), "BIGELF_ARRIVAL")) {
            if (!sAccepted) writef(STDERR_FILENO, "[cos] network-fairy: accepted actual BIGELF_ARRIVAL fairy=%u type=0 slot=3020\n", sFairyId);
            sAccepted = true;
        }
    }
    if (!sAccepted) return;
    auto* item = static_cast<daDitem_c*>(fpcM_Search(findDemoItem, nullptr));
    if (item && !fpcM_IsCreating(fpcM_GetID(item))) {
        const unsigned expected = sCase == Case::Gift ? 0xab : 0xac;
        need(item->getItemNo() == expected, "native fairy chose an unexpected item");
        if (!sItemSeen) writef(STDERR_FILENO, "[cos] network-fairy: observed actual Demo_Item=%02x actor=%u (no award injection)\n",
                               expected, fpcM_GetID(item));
        sItemSeen = true;
    }
    PcProgressClaim claims[PC_PROGRESS_MAX_PENDING_CLAIMS]{};
    const unsigned count = pc_progress_copy_claims(claims, PC_PROGRESS_MAX_PENDING_CLAIMS);
    need(count <= 1, "unexpected competing native progressive claim");
    for (unsigned i = 0; i < count; ++i) {
        const auto& claim = claims[i];
        need(claim.authored_reward_id == kReward && claim.key == kWallet &&
             claim.operation == PC_PERMANENT_STEP && claim.value == 1 && claim.invocation_id &&
             claim.identity.epoch == view.progress.identity.epoch &&
             claim.identity.scene_id == sIdentity.scene_id && claim.identity.player_id == sIdentity.player_id &&
             claim.identity.quest_generation == sIdentity.quest_generation &&
             !std::strcmp(claim.identity.profile_id, sIdentity.profile_id),
             "native claim did not bind the exact authored fairy/quest/one-step operation");
        need(!sInvocation || sInvocation == claim.invocation_id, "the same presentation produced another invocation");
        need(claim.state == PC_PROGRESS_CLAIM_ARMED || claim.state == PC_PROGRESS_CLAIM_COMPLETED,
             "native award was cancelled/stale instead of naturally completed");
        if (!sClaimSeen) writef(STDERR_FILENO, "[cos] network-fairy: authored claim source=%016llx key=%u STEP=1 invocation=%llu\n",
            static_cast<unsigned long long>(kReward), kWallet, static_cast<unsigned long long>(claim.invocation_id));
        sInvocation = claim.invocation_id; sClaimSeen = true;
    }
}
bool settled(const PcNetworkGameSnapshot& view) {
    return view.authorized && view.progress.safe_to_apply && !dComIfGp_event_runCheck() &&
        !view.pending_local && !view.pending_remote && !view.progress.pending_claims &&
        !view.session.pending_outgoing && !view.session.pending_incoming &&
        view.progress.facts[kWallet] == 1 && dComIfGs_getWalletSize() == 1 && dComIfGs_isEventBit(kFlag);
}
std::string gciHash() {
    char sha[65]{}, error[192]{};
    need(pc_network_client_hash_gci(sGci.c_str(), sha, error, sizeof error), "cannot hash actual private native GCI");
    return sha;
}
void diagnostics(const PcNetworkGameSnapshot& view, daPy_lk_c* player) {
    if (sFrame % 300) return;
    auto* event = g_dComIfG_gameInfo.play.getEvent();
    auto* data = dComIfGp_event_runCheck() && event->mEventId >= 0 ?
        dComIfGp_getPEvtManager()->getEventData(event->mEventId) : nullptr;
    writef(STDERR_FILENO, "[cos] network-fairy: frame=%u case=%s step=%d age=%u session=%d revision=%llu peers=%u "
           "wallet=%u native=%u flag=%u awards=%llu applies=%llu claims=%u pending=%d/%d/%u/%u event=%d/%s msg=%u "
           "pos=%.1f/%.1f/%.1f\n", sFrame, caseName(), static_cast<int>(sStep), sSince, view.session.status,
           static_cast<unsigned long long>(view.session.server_revision), view.session.peer_count,
           view.progress.facts[kWallet], dComIfGs_getWalletSize(), dComIfGs_isEventBit(kFlag),
           static_cast<unsigned long long>(view.local_awards), static_cast<unsigned long long>(view.remote_applies),
           view.progress.pending_claims, view.pending_local, view.pending_remote, view.session.pending_outgoing,
           view.session.pending_incoming, event->mEventId, data ? data->getName() : "none", dComIfGp_getMesgStatus(),
           player ? player->current.pos.x : 0, player ? player->current.pos.y : 0, player ? player->current.pos.z : 0);
}
void complete(const PcNetworkGameSnapshot& view) {
    need(sSaveCompared && sAccepted && sItemSeen && sClaimSeen && sSeenPeer && settled(view),
         "qualification lacks actual producer/completion/native save/drain evidence");
    need(gciHash() == sSavedHash, "GCI changed after the verified native save");
    if (sCase == Case::Duplicate) writeSignal(true);
    writef(STDERR_FILENO, "[cos] network-fairy: QUALIFIED case=%s source=%016llx native-item=%02x "
           "actual-accepted-event=1 completed-native-awards=1 wallet=1 flag3020=1 XYZ/resources-unchanged=1 "
           "same-room=%s revision=%llu duplicate-revision-advance=0 native-commit=1 both-save-copies-compared=1 "
           "drained=1 GCI-sha256=%s\n", caseName(), static_cast<unsigned long long>(kReward),
           sCase == Case::Gift ? 0xab : 0xac, sRoom, static_cast<unsigned long long>(sRevision), sSavedHash.c_str());
    writef(STDERR_FILENO, "[cos] network-fairy: PASS case=%s (peer success requires its separate QUALIFIED/PASS receipt)\n", caseName());
    enter(Step::Done); pc_exit(PC_EXIT_REACHED);
}
} // namespace

void networkFairyFrame(unsigned frames) {
    sFrame = frames;
    try {
        if (!selected()) return;
        JKRPcHostAllocScope host;
        ++sSince;
        if (frames % 30 == 0 && fs::exists(fs::u8path(gConfig.runDir) / "network-abort.flag"))
            fail("owned peer qualification failed; stopping this private diagnostic");
        need(frames - sStartFrame <= 30000 && monotonicNs() - sStartNs <= UINT64_C(600000000000),
             "bounded 600-second native qualification expired");
        const auto view = snapshot();
        need(!view.failed && view.session.status != PC_NETWORK_FAILED && view.session.status != PC_NETWORK_REJECTED,
             "native networking entered failed/rejected state");
        if (sCase == Case::Observe) {
            need(view.session.status == PC_NETWORK_OFFLINE && !view.local_awards && !view.remote_applies,
                 "offline fresh load connected or generated another native grant");
            if (view.native_loads) {
                need(view.progress.facts_valid && view.progress.facts[kWallet] == 1 &&
                     dComIfGs_getWalletSize() == 1 && dComIfGs_isEventBit(kFlag),
                     "actual fresh quest load lacks settled saved wallet1/fairy flag");
            }
            return; // File-select driver retains exclusive ownership of PAD.
        }
        auto* player = readyPlayer(sStep == Step::Waiting);
        auto* actor = fairy();
        invariant(view, player);
        diagnostics(view, player);
        need(view.local_awards <= 1 && view.remote_applies <= (sCase == Case::Gift ? 0u : 2u),
             "duplicate or wrong-origin native award/application");
        if (sStep != Step::Saving) drive();
        if (sCaptured && sStep != Step::Saving && sStep != Step::SavedDrain && sStep != Step::HostWaitPeer)
            need(actor && fpcM_GetID(actor) == sFairyId, "authored fairy actor disappeared/replaced");
        if (sStep == Step::Waiting) {
            if (player && actor && view.progress.facts_valid && view.progress.safe_to_apply) {
                if (++sStable >= 90) {
                    capture(view, player, actor);
                    need(pc_network_game_start(sCase == Case::Gift), "cannot start owned Host/Join after baseline");
                    enter(Step::Connecting);
                }
            } else sStable = 0;
        } else if (sStep == Step::Connecting) {
            if (view.session.status == PC_NETWORK_CONNECTED) {
                need(view.authorized && view.session.server_revision == 0 && view.session.room_identity[0],
                     "fixture must connect to an empty canonical room before any reward");
                sConnected = true; sGeneration = view.session.generation;
                std::strcpy(sRoom, view.session.room_identity);
                if (view.session.peer_count == 2) {
                    for (unsigned i = 0; i < view.session.peer_count; ++i)
                        if (std::strcmp(view.session.peers[i].player_id, sLocalPlayer)) {
                            need(!sPeer[0] || !std::strcmp(sPeer, view.session.peers[i].player_id),
                                 "isolated peer identity changed");
                            std::strcpy(sPeer, view.session.peers[i].player_id);
                        }
                    need(sPeer[0], "two peer entries do not identify distinct private players"); sSeenPeer = true;
                    if (++sStable >= 90 && (sCase == Case::Duplicate || readSignal(false))) {
                        writef(STDERR_FILENO, "[cos] network-fairy: connected case=%s room=%s generation=%llu independent-peer=%s\n",
                               caseName(), sRoom, static_cast<unsigned long long>(sGeneration), sPeer);
                        if (sCase == Case::Duplicate) writeSignal(false);
                        enter(sCase == Case::Gift ? Step::Approach : Step::Receiving);
                    }
                } else sStable = 0;
            }
        } else if (sStep == Step::Receiving) {
            need(!view.local_awards && !dComIfGs_isEventBit(kFlag) && !dComIfGp_event_runCheck(),
                 "stationary receiver triggered a local reward before receiving");
            if (view.remote_applies == 1 && view.progress.facts[kWallet] == 1 &&
                view.progress.safe_to_apply && !view.pending_remote && view.session.server_revision == 1) {
                if (++sStable >= 30) {
                    sRevision = view.session.server_revision;
                    writef(STDERR_FILENO, "[cos] network-fairy: received wallet1 once outside trigger; resources/XYZ unchanged; "
                           "room=%s revision=%llu; approaching same authored fairy\n", sRoom,
                           static_cast<unsigned long long>(sRevision));
                    enter(Step::Approach);
                }
            } else sStable = 0;
        } else if (sStep == Step::Approach || sStep == Step::Event) {
            observeProducer(view, player, actor);
            if (sAccepted && sStep == Step::Approach) enter(Step::Event);
            if (view.local_awards == 1) {
                need(sAccepted && sItemSeen && sClaimSeen, "completed counter lacks correlated actual fairy presentation");
                if (sCase == Case::Gift) {
                    if (view.session.server_revision == 1) sRevision = 1;
                }
                enter(Step::Settling);
            } else if (sStep == Step::Approach && !dComIfGp_event_runCheck()) {
                need(player && actor && dComIfGp_getCamera(0), "player/fairy/camera unavailable during PAD approach");
                const s16 angle = cM_atan2s(actor->current.pos.x - player->current.pos.x,
                                          actor->current.pos.z - player->current.pos.z);
                const s16 stick = static_cast<s16>(angle - 0x8000 -
                    dCam_getControledAngleY(reinterpret_cast<camera_class*>(dComIfGp_getCamera(0))));
                drive(0, static_cast<int8_t>(cM_ssin(stick) * 70.0f), static_cast<int8_t>(-cM_scos(stick) * 70.0f));
            } else if (dComIfGp_event_runCheck()) {
                drive(sSince % 20 < 2 ? PAD_BUTTON_A : 0);
            }
            need(sSince <= (sStep == Step::Approach ? 1350u : 5400u), "natural approach/event phase exceeded its bounded budget");
        } else if (sStep == Step::Settling) {
            if (dComIfGp_event_runCheck()) drive(sSince % 20 < 2 ? PAD_BUTTON_A : 0);
            if (sCase == Case::Gift && view.session.server_revision == 1) sRevision = 1;
            if (sRevision && settled(view)) {
                need(view.local_awards == 1 && view.remote_applies == (sCase == Case::Gift ? 0u : 2u),
                     "native completion/receiver correction counts differ from the two genuine fairy transactions");
                if (++sStable >= 30) {
                    writef(STDERR_FILENO, "[cos] network-fairy: real fairy completed case=%s native-item=%02x source=%016llx "
                           "wallet=1 room-revision=%llu; beginning actual native save\n", caseName(),
                           sCase == Case::Gift ? 0xab : 0xac, static_cast<unsigned long long>(kReward),
                           static_cast<unsigned long long>(sRevision));
                    enter(Step::Saving); saveSweepStart("network-fairy");
                }
            } else sStable = 0;
        } else if (sStep == Step::SavedDrain) {
            if (sQuestWrite && view.native_saves == sSavesBefore + 1 &&
                view.progress.identity.save_generation == sSaveGeneration + 1 &&
                !view.unsaved_remote && !view.provisional_save && settled(view)) {
                if (++sStable >= 3) {
                    sSavedHash = gciHash();
                    if (sCase == Case::Duplicate) complete(view);
                    writef(STDERR_FILENO, "[cos] network-fairy: host actual save/drain checked; retaining owned listener for peer native-save completion\n");
                    enter(Step::HostWaitPeer);
                }
            } else sStable = 0;
        } else if (sStep == Step::HostWaitPeer) {
            need(settled(view) && !view.unsaved_remote && !view.provisional_save, "host state changed while serving duplicate peer");
            if (readSignal(true) && view.session.peer_count == 1) complete(view);
        }
    } catch (const fs::filesystem_error&) { fail("private fixture path validation failed"); }
}

void networkFairyBeforeCardWrite() {
    if (!selected()) return;
    JKRPcHostAllocScope host;
    need(sCase != Case::Observe && sStep == Step::Saving && !sQuestWrite,
         "unexpected/repeated actual quest-save preparation");
    const auto view = snapshot();
    // A fresh CARD also writes initial metadata. Count from the ACTUAL quest
    // STORE boundary, after that creation, rather than pretending it is a
    // qualified native quest save or expecting only one raw disk write.
    need(view.local_awards == 1 && dComIfGs_getWalletSize() == 1 && dComIfGs_isEventBit(kFlag) &&
         !view.pending_local && !view.pending_remote && !view.session.pending_outgoing &&
         !view.progress.pending_claims, "actual quest-save preparation still has unresolved fairy work");
    sSavesBefore = view.native_saves;
    sSaveGeneration = view.progress.identity.save_generation;
    sQuestWrite = true;
}

bool networkFairySaveComplete(unsigned frames) {
    sFrame = frames;
    if (!selected()) return false;
    JKRPcHostAllocScope host;
    need(sCase != Case::Observe && sStep == Step::Saving && sQuestWrite && !sSaveCompared,
         "unexpected repeated native save comparison callback");
    sSaveCompared = true;
    enter(Step::SavedDrain);
    return true; // Native SaveSync and BOTH GCI copies already passed upstream.
}

bool networkFairyLoadComplete(unsigned frames) {
    sFrame = frames;
    if (!selected()) return false;
    JKRPcHostAllocScope host;
    need(sCase == Case::Observe && !sLoadCompared, "unexpected native file-select comparison callback");
    const auto view = snapshot();
    need(view.native_loads == 1 && view.authorized && view.progress.facts_valid &&
         view.progress.facts[kWallet] == 1 && dComIfGs_getWalletSize() == 1 && dComIfGs_isEventBit(kFlag) &&
         !view.local_awards && !view.remote_applies && !view.pending_local && !view.pending_remote &&
         !view.progress.pending_claims && !view.failed && view.session.status == PC_NETWORK_OFFLINE &&
         !view.provisional_save, "fresh offline activation did not retain confirmed wallet1/fairy completion");
    sLoadCompared = true;
    const auto hash = gciHash();
    writef(STDERR_FILENO, "[cos] network-fairy: QUALIFIED case=observe actual-file-select-load=1 wallet=1 flag3020=1 "
           "loaded-packed-card-compared=1 local-awards=0 remote-applies=0 offline=1 drained=1 GCI-sha256=%s\n", hash.c_str());
    writef(STDERR_FILENO, "[cos] network-fairy: PASS case=observe\n");
    return false; // Preserve save-load's own normal successful exit.
}
} // namespace pc
