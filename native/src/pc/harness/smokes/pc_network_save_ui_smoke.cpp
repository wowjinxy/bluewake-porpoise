// SPDX-License-Identifier: GPL-3.0-or-later
// PRIVATE, UNQUALIFIED PROPOSAL. COS_SMOKE=network-save-ui; off by default.
// Adapted narrowly from pc_network_smoke.cpp/pc_network_recovery_smoke.cpp and
// pc_save_sweep.cpp in this repository; retain their licenses/provenance.
// Only ordinary driven PAD reaches the native Save UI. This diagnostic never
// calls CARD, Save/Load/SaveSync/LoadSync, packing or progression mutation APIs.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc_network_save_ui_smoke.h"
#include "pc_internal.h"
#include "pc/pc_network_game.h"
#include "pc/pc_save_ui_observer.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "d/d_menu_save.h"
#include "d/d_menu_window.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter.h"
#include "d/d_s_play.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_MemCardRWmng.h"
#include "../../network/atomic_file.h"
#include "../../network/network_digest.h"
#include <dolphin/pad.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace pc {
namespace {
namespace fs = std::filesystem;
static_assert(sizeof(card_gamedata) * 3 == 5712, "GZLE01 three-slot UI buffer size changed");
enum class Step {
    Off, Settle, Navigate, Deferred, Refuse, RefusalPublished, CloseRefusal,
    Apply, ReadFault, ReadFailurePublished, RetryRead, WriteFault,
    WriteFailurePublished, RetryWrite, SavePublished, CloseSuccess, Done
};
Step sStep = Step::Off;
bool sChecked = false, sCaptured = false, sRetryPhase = false;
bool sReadWaitSeen = false, sReadSuccessSeen = false, sWriteReadWaitSeen = false;
bool sWriteReadSuccessSeen = false, sSaved = false;
bool sIndependentPeerSeen = false;
unsigned sFrame = 0, sAge = 0, sStartFrame = 0, sSettle = 0, sNav = 0;
unsigned sPulseCooldown = 0, sObservedErrorAge = 0;
uint64_t sStartNs = 0, sReadsAtFault = 0, sReadsAfterWriteFault = 0;
uint64_t sRefusedAttempt = 0, sFailedWriteAttempt = 0;
PcNetworkGameSnapshot sBaseline{};
std::array<uint8_t, 5712> sExpected{};
std::string sGci, sBeforeBytes, sRoomIdentity;
u8 sSelectors[3]{}, sCachedItems[3]{};
int sResources[5]{};
Vec sPosition{};
#ifdef _WIN32
HANDLE sFault = INVALID_HANDLE_VALUE;
#endif

void drive(uint16_t buttons = 0, int8_t x = 0, int8_t y = 0) {
    setDrivenPad(true, buttons, x, y);
    setDrivenQuickItemModifier(false);
}
void unlock() {
#ifdef _WIN32
    if (sFault != INVALID_HANDLE_VALUE) { CloseHandle(sFault); sFault = INVALID_HANDLE_VALUE; }
#endif
}
[[noreturn]] void fail(const char* text) {
    unlock(); drive();
    writef(STDERR_FILENO, "[cos] network-save-ui: FAIL step=%u frame=%u age=%u: %s\n",
           static_cast<unsigned>(sStep), sFrame, sAge, text);
    pc_exit(PC_EXIT_CHECK_FAILED);
}
[[noreturn]] void usage(const char* text) {
    unlock();
    writef(STDERR_FILENO, "[cos] network-save-ui: usage: %s\n", text);
    pc_exit(PC_EXIT_USAGE);
}
void need(bool ok, const char* text) { if (!ok) fail(text); }
void enter(Step step) { drive(); sStep = step; sAge = 0; sObservedErrorAge = 0; }
bool pulse(uint16_t button, int8_t x = 0, int8_t y = 0) {
    if (sPulseCooldown) return false;
    drive(button, x, y); sPulseCooldown = 12; return true;
}
std::string utf8(const fs::path& value) {
    const auto s = value.u8string(); return {s.begin(), s.end()};
}
fs::path absolute(const char* value) {
    if (!value || !*value || std::strlen(value) >= 1024) usage("explicit bounded private paths required");
    auto p = fs::absolute(fs::u8path(value)).lexically_normal().make_preferred();
    while (p != p.root_path() && p.filename().empty()) p = p.parent_path();
    return p;
}
bool equal(const fs::path& a, const fs::path& b) {
#ifdef _WIN32
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
#else
    return a == b;
#endif
}
bool inside(const fs::path& p, const fs::path& root) {
    auto i = p.begin();
    for (auto j = root.begin(); j != root.end(); ++i, ++j)
        if (i == p.end() || !equal(*i, *j)) return false;
    return !equal(p, root);
}
// Reject junctions/reparse points throughout the owned path, including parents.
void noLinks(const fs::path& path, bool leafMayBeMissing = false) {
#ifdef _WIN32
    fs::path current;
    for (const auto& part : path) {
        current /= part;
        if (current == current.root_name()) continue;
        const std::string text = utf8(current);
        wchar_t* wide = pc_net_atomic_wide_path(text.c_str());
        if (!wide) usage("private path UTF-16 conversion failed");
        const DWORD attrs = GetFileAttributesW(wide);
        std::free(wide);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            if (leafMayBeMissing && equal(current, path)) return;
            usage("private fixture path is missing/inaccessible");
        }
        if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) usage("private paths may not contain linked ancestors");
    }
#else
    (void)path; (void)leafMayBeMissing;
    usage("controlled OS sharing-denial case is Windows-only");
#endif
}
std::string readExact(const fs::path& path, size_t bytes) {
    FILE* file = pc_net_atomic_open(utf8(path).c_str(), "rb");
    need(file != nullptr, "cannot read a private fixture");
    std::string out(bytes, '\0');
    const bool ok = std::fread(out.data(), 1, bytes, file) == bytes &&
                    std::fgetc(file) == EOF && !std::ferror(file);
    const bool closed = std::fclose(file) == 0;
    need(ok && closed, "private fixture length/read mismatch");
    return out;
}
std::string readGci() {
    noLinks(fs::u8path(sGci));
    need(fs::hard_link_count(fs::u8path(sGci)) == 1, "GCI must have exactly one filesystem link");
    return readExact(fs::u8path(sGci), 98368);
}
void unchangedGci() { need(readGci() == sBeforeBytes, "failed/refused UI save changed the existing GCI"); }
const uint8_t* slot(const std::string& bytes, int copy, int index) {
    // Native save copies are blocks1 and2. Block0 is NOT a save copy.
    return reinterpret_cast<const uint8_t*>(bytes.data()) + 0x40 + copy * 0x2000 + 8 + index * sizeof(card_gamedata);
}
void validatePrivate() {
    const auto root = absolute(std::getenv("COS_NETWORK_SAVE_UI_PRIVATE_ROOT"));
    bool buildParent = false;
    for (const auto& part : root.parent_path()) if (part == "build") buildParent = true;
    if (!buildParent || !fs::is_directory(root)) usage("private root must exist beneath build/");
    noLinks(root);
    const auto marker = root / "network-save-ui.allow";
    noLinks(marker);
    const std::string magic = "BlueWake.Native.SaveUI.Private.v1\n";
    if (fs::hard_link_count(marker) != 1 || readExact(marker, magic.size()) != magic)
        usage("private opt-in marker mismatch");
    for (const char* name : {"COS_NETWORK_PROFILE", "COS_NETWORK_ROOT", "COS_SETTINGS", "COS_CACHE_DIR"}) {
        const auto path = absolute(std::getenv(name));
        if (!inside(path, root)) usage("profile/storage/settings/cache must be explicitly private");
        noLinks(path, !std::strcmp(name, "COS_SETTINGS"));
    }
    const auto run = absolute(gConfig.runDir);
    if (!inside(run, root)) usage("run directory must be explicitly private");
    noLinks(run);
    const char* card = std::getenv("COS_CARD_DIR");
    if (card && *card) usage("room diagnostic forbids COS_CARD_DIR override");
    const auto mounted = absolute(pc_network_game_card_base());
    const auto storage = absolute(std::getenv("COS_NETWORK_ROOT"));
    if (!inside(mounted, storage)) usage("mounted room card escaped private storage");
    noLinks(mounted);
    sGci = utf8(mounted / "USA" / "Card A" / "01-GZLE-gczelda.gci");
    const char* actual = runCardGciPath();
    if (!actual || !equal(absolute(actual), absolute(sGci.c_str())))
        usage("run card differs from immutable room route");
    // No creation/copy/import is performed here. The external fixture receipt
    // must identify a previously successful REAL native blank-gamefile create.
    const auto receipt = root / "native-baseline-gci.sha256";
    noLinks(receipt);
    if (fs::hard_link_count(receipt) != 1) usage("baseline receipt must be one private file");
    const std::string hash = readExact(receipt, 65);
    for (unsigned i = 0; i < 64; ++i)
        if (!((hash[i] >= '0' && hash[i] <= '9') || (hash[i] >= 'a' && hash[i] <= 'f')))
            usage("baseline receipt needs lowercase SHA256 plus LF");
    sBeforeBytes = readGci();
    if (hash[64] != '\n' || hash.substr(0, 64) != pc_net::digest(sBeforeBytes))
        usage("baseline receipt does not match the existing native-created GCI");
    if (std::memcmp(slot(sBeforeBytes, 1, 0), slot(sBeforeBytes, 2, 0), 5712))
        usage("native baseline's two three-slot save copies differ");
}
void* findPlay(void* p, void*) { return fpcM_GetName(p) == fpcNm_PLAY_SCENE_e ? p : nullptr; }
fopAc_ac_c* readyPlayer(bool wholeRoom) {
    auto* p = dComIfGp_getPlayer(0);
    if (!fpcM_Search(findPlay, nullptr) || !p || fpcM_GetName(p) != fpcNm_PLAYER_e ||
        fpcM_IsCreating(fpcM_GetID(p)) || !fpcM_IsExecuting(fpcM_GetID(p)) ||
        (wholeRoom && stageRoomReady(fopAcM_GetRoomNo(p), nullptr) != nullptr)) return nullptr;
    return p;
}
void baseline(const PcNetworkGameSnapshot& v, fopAc_ac_c* p) {
    need(v.local_awards == 0 && v.remote_applies == 0 && !v.progress.telescope_owned &&
         v.progress.facts_valid && dComIfGs_getItem(dInvSlot_TELESCOPE_e) == dItemNo_NONE_e &&
         !v.pending_remote && !v.unsaved_remote, "baseline already contains reward/receipt");
    need(v.progress.identity.quest_slot >= 0 && v.progress.identity.quest_slot < 3,
         "active native quest slot is not0..2");
    const bool serialMatches = dComIfGs_getMemCardCheckID() == mDoMemCd_getCardSerialNo();
    const bool uiAdmissionReady = dComIfGs_getNewFile() == 1 ||
        (dComIfGs_getNoFile() == 0 && serialMatches);
    writef(STDERR_FILENO, "[cos] network-save-ui: existing-save-precondition new-file=%u no-file=%u serial-match=%u\n",
           dComIfGs_getNewFile(), dComIfGs_getNoFile(), serialMatches);
    need(mDoMemCd_getStatus(0) == 2 && uiAdmissionReady,
         "existing native GCI is not admitted by ordinary Save UI; use genuine file-select/new-game setup, never set flags/serial");
    sBaseline = v; sPosition = p->current.pos;
    for (int i = 0; i < 3; ++i) {
        sSelectors[i] = dComIfGs_getSelectItem(i); sCachedItems[i] = dComIfGp_getSelectItem(i);
    }
    sResources[0] = dComIfGs_getRupee(); sResources[1] = dComIfGs_getLife();
    sResources[2] = dComIfGs_getMagic(); sResources[3] = dComIfGs_getArrowNum();
    sResources[4] = dComIfGs_getBombNum(); sCaptured = true;
    writef(STDERR_FILENO, "[cos] network-save-ui: baseline existing-GCI=%s quest-slot=%d native-loads=%llu saves=%llu\n",
           pc_net::digest(sBeforeBytes).c_str(), v.progress.identity.quest_slot,
           static_cast<unsigned long long>(v.native_loads), static_cast<unsigned long long>(v.native_saves));
}
void invariants(const PcNetworkGameSnapshot& v, fopAc_ac_c* p) {
    if (!sCaptured) return;
    const auto& a = sBaseline.progress.identity; const auto& b = v.progress.identity;
    need(p && a.session_id == b.session_id && a.quest_generation == b.quest_generation &&
         a.quest_slot == b.quest_slot && a.scene_id == b.scene_id && a.player_id == b.player_id &&
         !std::strcmp(a.profile_id, b.profile_id), "active quest/player/profile changed during UI test");
    need(v.native_loads == sBaseline.native_loads, "controller read unexpectedly activated a quest");
    need(v.local_awards == 0 && v.remote_applies <= 1, "local echo/duplicate remote application");
    for (int i = 0; i < 3; ++i)
        need(dComIfGs_getSelectItem(i) == sSelectors[i] && dComIfGp_getSelectItem(i) == sCachedItems[i],
             "saved/cached X/Y/Z assignment changed");
    need(dComIfGs_getRupee() == sResources[0] && dComIfGs_getLife() == sResources[1] &&
         dComIfGs_getMagic() == sResources[2] && dComIfGs_getArrowNum() == sResources[3] &&
         dComIfGs_getBombNum() == sResources[4], "ordinary resources changed");
    need(p->current.pos.x == sPosition.x && p->current.pos.y == sPosition.y && p->current.pos.z == sPosition.z,
         "native Link position changed during pause/receipt/UI retry");
    need(v.progress.facts_valid, "native permanent fact capture became invalid");
    for (unsigned i = 0; i < PC_PERMANENT_FACT_COUNT; ++i)
        if (i != PC_PERMANENT_TELESCOPE)
            need(v.progress.facts[i] == sBaseline.progress.facts[i], "unexpected unrelated permanent fact changed");
}
bool paused() {
    return dMenu_flag() || dScnPly_ply_c::pauseTimer || dScnPly_ply_c::nextPauseTimer;
}
void rejectDestructiveUi(const PcSaveUiObservation& o) {
    if (!o.save_present || o.save_status != 2) return;
    need(o.proc != dMenu_save_c::PROC_MEMCARD_FORMAT &&
         o.proc != dMenu_save_c::PROC_MEMCARD_ERR_MSG_WAIT_FORMAT_SEL &&
         o.proc != dMenu_save_c::PROC_MEMCARD_ERR_MSG_WAIT_FORMAT_SEL2 &&
         o.proc != dMenu_save_c::PROC_MEMCARD_FORMAT_CHECK &&
         o.proc != dMenu_save_c::PROC_MEMCARD_MAKE_GAME_FILE_SEL &&
         o.proc != dMenu_save_c::PROC_MEMCARD_MAKE_GAME_FILE &&
         o.proc != dMenu_save_c::PROC_MEMCARD_MAKE_GAME_FILE_CHECK &&
         o.proc != dMenu_save_c::PROC_MEMCARD_ERR_GO_IPL_SEL &&
         o.proc != dMenu_save_c::PROC_MEMCARD_ERR_GO_IPL_SEL2,
         "existing GCI was routed to creation/format/system UI");
}
bool navigate(const PcSaveUiObservation& o) {
    if (o.save_present && o.save_status == 2 && o.proc == dMenu_save_c::PROC_SAVE_QUESTION)
        return true;
    if (!paused()) { pulse(PAD_BUTTON_START); return false; }
    if (o.window_proc == MENU_STATE_ITEM_MOVE) { pulse(PAD_TRIGGER_R); return false; }
    if (o.window_proc != MENU_STATE_COLLECT_MOVE || !o.collection_present || o.collection_mode != 0)
        return false;
    if (sNav == 0) {
        need(o.cursor == 0 || o.cursor == 7, "fixture needs observed fresh cursor0 or retained Save7");
        sNav = o.cursor == 7 ? 3 : 1;
    }
    if (sNav == 1 && o.cursor == 0) pulse(0, 0, -80);
    else if (sNav == 1 && o.cursor == 6) sNav = 2;
    else if (sNav == 2 && o.cursor == 6) pulse(0, 0, -80);
    else if (sNav == 2 && o.cursor == 7) sNav = 3;
    else if (sNav == 3 && o.cursor == 7) pulse(PAD_BUTTON_A);
    else need(false, "normal analog cursor did not follow0->6->7");
    return false;
}
void yes(const PcSaveUiObservation& o) {
    if (!o.save_present || o.save_status != 2) return;
    if (o.proc == dMenu_save_c::PROC_SAVE_QUESTION || o.proc == dMenu_save_c::PROC_MEMCARD_DATA_SAVE_SEL) {
        if (o.choice != 0) pulse(0, -80, 0);
        else pulse(PAD_BUTTON_A);
    }
}
void acknowledgeError(const PcSaveUiObservation& o) {
    if (o.save_present && o.save_status == 2 &&
        (o.proc == dMenu_save_c::PROC_MEMCARD_ERR_MSG_WAIT_KEY ||
         o.proc == dMenu_save_c::PROC_MEMCARD_ERR_MSG_WAIT_KEY2 ||
         o.proc == dMenu_save_c::PROC_MEMCARD_ERR_MSG_WAIT_KEY3 ||
         o.proc == dMenu_save_c::PROC_MSG_WAIT2)) pulse(PAD_BUTTON_A);
    yes(o);
}
void fault(bool read) {
#ifdef _WIN32
    need(sFault == INVALID_HANDLE_VALUE, "overlapping owned fault handle");
    unchangedGci();
    wchar_t* path = pc_net_atomic_wide_path(sGci.c_str());
    need(path != nullptr, "private GCI UTF-16 conversion failed");
    // Read fault denies ordinary native opens. Write fault has zero access and
    // permits native read/write opens, while withholding DELETE/atomic replace.
    sFault = CreateFileW(path, read ? GENERIC_READ : 0,
                        read ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    std::free(path);
    need(sFault != INVALID_HANDLE_VALUE, "could not acquire owned OS sharing denial");
    BY_HANDLE_FILE_INFORMATION info{};
    need(GetFileInformationByHandle(sFault, &info) && info.nNumberOfLinks == 1 &&
         !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
         info.nFileSizeHigh == 0 && info.nFileSizeLow == 98368,
         "locked GCI identity/size/link invariant failed");
    std::string lockedBytes(98368, '\0');
    if (read) {
        DWORD got = 0;
        need(ReadFile(sFault, lockedBytes.data(), static_cast<DWORD>(lockedBytes.size()), &got, nullptr) &&
             got == lockedBytes.size() && lockedBytes == sBeforeBytes,
             "read-denial handle did not pin the expected unchanged private GCI");
    } else {
        // This temporary reader MUST close before native r+b/atomic replace.
        path = pc_net_atomic_wide_path(sGci.c_str());
        need(path != nullptr, "private GCI reader path conversion failed");
        HANDLE reader = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        std::free(path);
        need(reader != INVALID_HANDLE_VALUE, "cannot verify zero-access replacement pin");
        DWORD got = 0;
        const bool bytesOk = ReadFile(reader, lockedBytes.data(), static_cast<DWORD>(lockedBytes.size()), &got, nullptr)
                             && got == lockedBytes.size() && lockedBytes == sBeforeBytes;
        const bool closed = CloseHandle(reader) != FALSE;
        need(bytesOk && closed, "replacement-pin verification reader failed or stayed open");
    }
#else
    (void)read; usage("OS sharing-denial is Windows-only");
#endif
}
void finalGci(const PcSaveUiObservation& o) {
    need(o.buffer_valid, "successful native UI save lacks observed prepared three-slot buffer");
    std::memcpy(sExpected.data(), o.buffered_slots, sExpected.size());
    const std::string after = readGci();
    const int active = sBaseline.progress.identity.quest_slot;
    for (int copy = 1; copy <= 2; ++copy) {
        need(!std::memcmp(slot(after, copy, 0), sExpected.data(), sExpected.size()),
             "actual GCI copy differs from the UI's real native prepared three-slot buffer");
        for (int i = 0; i < 3; ++i)
            if (i != active)
                need(!std::memcmp(slot(after, copy, i), slot(sBeforeBytes, copy, i), sizeof(card_gamedata)),
                     "nonselected quest slot changed");
        need(mDoMemCdRWm_TestCheckSumGameData(const_cast<uint8_t*>(slot(after, copy, active))),
             "selected native GCI slot checksum failed");
    }
    need(after != sBeforeBytes, "successful native UI save did not change blank baseline GCI");
    writef(STDERR_FILENO, "[cos] network-save-ui: actual-GCI=%s both-save-copies=1 nonselected-slot-bytes-unchanged=1\n",
           pc_net::digest(after).c_str());
}
bool selected() {
    if (sChecked) return sStep != Step::Off;
    sChecked = true;
    if (!gConfig.smoke || std::strcmp(gConfig.smoke, "network-save-ui")) return false;
#ifndef _WIN32
    usage("controlled actual Save UI retry case is Windows-only");
#endif
    if (gConfig.frames || gConfig.milestone || gConfig.uncapped || gConfig.input)
        usage("no frame/milestone exits, uncapped pacing or competing input script");
    for (const char* name : {"COS_NETWORK_AUTOSTART", "COS_NETWORK_CASE", "COS_NETWORK_RECOVERY"}) {
        const char* value = std::getenv(name);
        if (value && *value) usage("no competing network/save diagnostic or automatic join");
    }
    unsigned short items[64]{}, events[64]{};
    const PcBootStage* boot = pc_boot_stage();
    if (!boot || std::strcmp(boot->stage, "sea") || boot->room != 11 || boot->point != 9 ||
        pc_boot_items(items, 64) || pc_boot_event_bits(events, 64) || bootPreset())
        usage("requires ordinary sea:11:9 receiver, no boot item/event/preset grants");
    PcNetworkGameSnapshot view{}; pc_network_game_snapshot(&view);
    if (!view.prepared || !view.room_mode || view.session.status != PC_NETWORK_OFFLINE)
        usage("requires dormant validated isolated room profile before joining");
    PcNetworkProfile profile{};
    if (!pc_network_game_profile(&profile) || std::strcmp(profile.config.server, "127.0.0.1") ||
        std::strcmp(profile.bind, "127.0.0.1"))
        usage("requires a separately owned persistent loopback host, never an external service");
    validatePrivate();
    sStartNs = monotonicNs(); sStartFrame = sFrame; enter(Step::Settle);
    return true;
}
} // namespace

void networkSaveUiFrame(unsigned frames) {
    sFrame = frames;
    JKRPcHostAllocScope host;
    if (!selected()) return;
    drive();
    if (sPulseCooldown) --sPulseCooldown;
    ++sAge;
    need(monotonicNs() - sStartNs < 720000000000ULL && frames - sStartFrame < 21600,
         "bounded normally paced UI milestones did not complete");
    // The genuine sender's full native Aryll event is known to take thousands
    // of frames. UI phases remain60s; only the external real-reward wait is200s.
    need(sAge < (sStep == Step::Deferred ? 6000u : 1800u),
         "bounded native UI/real-reward phase expired");
    PcNetworkGameSnapshot v{}; pc_network_game_snapshot(&v);
    PcSaveUiObservation o{}; pc_save_ui_observe(&o);
    auto* player = readyPlayer(sStep == Step::Settle);
    need(!v.failed && v.session.status != PC_NETWORK_FAILED && v.session.status != PC_NETWORK_REJECTED,
         "network failed/rejected during native UI case");
    invariants(v, player);
    rejectDestructiveUi(o);
    if (frames % 120 == 0)
        writef(STDERR_FILENO,
            "[cos] network-save-ui: step=%u frame=%u age=%u window=%u collect=%u cursor=%u save=%u proc=%u "
            "choice=%u read=%d/%llu result=%d attempt=%llu refused=%d status=%u authorized=%d "
            "awards=%llu applies=%llu loads=%llu saves=%llu pending=%d unsaved=%d\n",
            static_cast<unsigned>(sStep), frames, sAge, o.window_proc, o.collection_mode, o.cursor,
            o.save_status, o.proc, o.choice, o.last_read_result,
            static_cast<unsigned long long>(o.completed_reads), o.last_save_result,
            static_cast<unsigned long long>(o.save_attempt),
            g_mDoMemCd_control.pcWasSaveRefused(o.save_attempt), mDoMemCd_getStatus(0), v.authorized,
            static_cast<unsigned long long>(v.local_awards), static_cast<unsigned long long>(v.remote_applies),
            static_cast<unsigned long long>(v.native_loads), static_cast<unsigned long long>(v.native_saves),
            v.pending_remote, v.unsaved_remote);

    switch (sStep) {
    case Step::Settle:
        if (player && v.progress.safe_to_apply && !paused() && !dComIfGp_event_runCheck()) {
            if (++sSettle >= 90) { baseline(v, player); sNav = 0; enter(Step::Navigate); }
        } else sSettle = 0;
        break;
    case Step::Navigate:
        if (navigate(o)) {
            if (!sRetryPhase) {
                need(!v.progress.safe_to_apply && !v.progress.telescope_owned, "native pause did not defer grants");
                need(pc_network_game_start(false), "ordinary Join of immutable room failed");
                enter(Step::Deferred);
            } else {
                sReadsAtFault = o.completed_reads; fault(true); enter(Step::ReadFault);
            }
        }
        break;
    case Step::Deferred:
        need(paused() && !v.progress.safe_to_apply && !v.progress.telescope_owned && v.remote_applies == 0,
             "canonical remote grant bypassed native paused context");
        if (v.session.status == PC_NETWORK_CONNECTED && v.session.peer_count >= 2) sIndependentPeerSeen = true;
        // A genuine giver may leave after its award/save. The separate owned
        // service and canonical grant must persist; the giver need not remain.
        if (sIndependentPeerSeen && v.pending_remote && v.session.status == PC_NETWORK_CONNECTED) {
            if (++sObservedErrorAge >= 12) {
                sRoomIdentity = v.session.room_identity; enter(Step::Refuse);
            }
        } else sObservedErrorAge = 0;
        break;
    case Step::Refuse:
        yes(o);
        if (o.last_save_result == 2 && o.save_attempt &&
            g_mDoMemCd_control.pcWasSaveRefused(o.save_attempt)) {
            sRefusedAttempt = o.save_attempt; enter(Step::RefusalPublished);
        }
        break;
    case Step::RefusalPublished:
        need(v.native_saves == sBaseline.native_saves && v.progress.identity.save_generation ==
             sBaseline.progress.identity.save_generation, "refused UI attempt committed a native quest save");
        if (sAge >= 2 && o.proc == dMenu_save_c::PROC_CAN_NOT_SAVE) {
            unchangedGci();
            need(!o.recollection_exchanged && mDoMemCd_getStatus(0) == 2,
                 "refused UI save left swapped recollection or changed controller readiness");
            writef(STDERR_FILENO, "[cos] network-save-ui: genuine-pending-save-refusal attempt=%llu UI-CAN_NOT_SAVE=1 GCI-unchanged=1\n",
                   static_cast<unsigned long long>(sRefusedAttempt));
            enter(Step::CloseRefusal);
        }
        break;
    case Step::CloseRefusal:
        if (o.save_present && o.save_status == 2 && o.proc == dMenu_save_c::PROC_CAN_NOT_SAVE) pulse(PAD_BUTTON_A);
        else if (o.save_present && o.save_status == 2 && o.proc == dMenu_save_c::PROC_SAVE_QUESTION) pulse(PAD_BUTTON_B);
        else if (o.collection_present && o.collection_mode == 0) pulse(PAD_BUTTON_B);
        if (!paused() && v.progress.safe_to_apply) enter(Step::Apply);
        break;
    case Step::Apply:
        if (v.remote_applies == 1 && v.progress.telescope_owned && v.unsaved_remote && !v.pending_remote &&
            v.session.status == PC_NETWORK_CONNECTED && v.authorized) {
            unchangedGci(); sRetryPhase = true; sNav = 0; enter(Step::Navigate);
        }
        break;
    case Step::ReadFault:
        yes(o);
        if (o.proc == dMenu_save_c::PROC_MEMCARD_DATA_LOAD_WAIT) sReadWaitSeen = true;
        if (o.completed_reads > sReadsAtFault && o.last_read_result == 2) {
            need(sReadWaitSeen && mDoMemCd_getStatus(0) == 8, "real UI read denial did not preserve IOERROR public8");
            unlock(); unchangedGci(); enter(Step::ReadFailurePublished);
        }
        break;
    case Step::ReadFailurePublished:
        need(v.native_saves == sBaseline.native_saves && v.unsaved_remote && v.remote_applies == 1,
             "failed UI read cleared unsaved receipt or committed a save");
        if (sAge >= 2) {
            need(!v.authorized, "failed native UI read left read authorization valid");
            enter(Step::RetryRead);
        }
        break;
    case Step::RetryRead:
        acknowledgeError(o);
        if (o.proc == dMenu_save_c::PROC_MEMCARD_DATA_LOAD_WAIT) sReadWaitSeen = true;
        if (o.completed_reads > sReadsAtFault + 1 && o.last_read_result == 1) sReadSuccessSeen = true;
        if (o.proc == dMenu_save_c::PROC_MEMCARD_DATA_SAVE) {
            need(sReadSuccessSeen && o.buffer_valid && !o.existing_read_required && v.authorized,
                 "ordinary retry reached write phase without real successful UI controller read");
            need(v.unsaved_remote && v.remote_applies == 1 && v.native_saves == sBaseline.native_saves,
                 "UI controller recovery replaced/committed live unsaved quest");
            fault(false); enter(Step::WriteFault);
        }
        break;
    case Step::WriteFault:
        yes(o);
        if (o.save_attempt && o.save_attempt != sRefusedAttempt && o.last_save_result == 2) {
            need(!g_mDoMemCd_control.pcWasSaveRefused(o.save_attempt) && mDoMemCd_getStatus(0) == 8,
                 "OS replacement denial was classified as progress refusal or missing gamefile");
            sFailedWriteAttempt = o.save_attempt; sReadsAfterWriteFault = o.completed_reads;
            unlock(); unchangedGci(); enter(Step::WriteFailurePublished);
        }
        break;
    case Step::WriteFailurePublished:
        need(v.native_saves == sBaseline.native_saves && v.unsaved_remote && v.remote_applies == 1,
             "failed native UI write cleared pending receipt or committed a save");
        if (sAge >= 2) enter(Step::RetryWrite);
        break;
    case Step::RetryWrite:
        acknowledgeError(o);
        if (o.proc == dMenu_save_c::PROC_MEMCARD_DATA_LOAD_WAIT) sWriteReadWaitSeen = true;
        if (o.completed_reads > sReadsAfterWriteFault && o.last_read_result == 1) sWriteReadSuccessSeen = true;
        if (o.save_attempt && o.save_attempt != sFailedWriteAttempt && o.last_save_result == 1) {
            need(sWriteReadWaitSeen && sWriteReadSuccessSeen && o.buffer_valid &&
                 !g_mDoMemCd_control.pcWasSaveRefused(o.save_attempt),
                 "native UI write retry skipped required real controller read");
            finalGci(o); sSaved = true; enter(Step::SavePublished);
        }
        break;
    case Step::SavePublished:
        if (sAge >= 2) {
            need(sSaved && v.native_saves == sBaseline.native_saves + 1 &&
                 v.progress.identity.save_generation == sBaseline.progress.identity.save_generation + 1 &&
                 !v.unsaved_remote && v.remote_applies == 1 && v.local_awards == 0 && v.authorized,
                 "successful real UI SaveSync was not matched by exact native commit/generation/pending state");
            need(!o.recollection_exchanged, "successful UI save left recollection swapped");
            enter(Step::CloseSuccess);
        }
        break;
    case Step::CloseSuccess:
        if (o.save_present && o.save_status == 2 && o.proc == dMenu_save_c::PROC_SAVE_END_WAIT) pulse(PAD_BUTTON_A);
        else if (o.save_present && o.save_status == 2 && o.proc == dMenu_save_c::PROC_GAME_CONTINUE2) {
            if (o.choice != 0) pulse(0, -80, 0);
            else pulse(PAD_BUTTON_A);
        }
        else if (o.collection_present && o.collection_mode == 0) pulse(PAD_BUTTON_B);
        if (!paused() && v.progress.safe_to_apply) {
            need(v.progress.telescope_owned && v.remote_applies == 1 && !v.unsaved_remote &&
                 v.native_saves == sBaseline.native_saves + 1 && v.session.room_identity == sRoomIdentity,
                 "post-UI state lost receipt/save/room identity");
            writef(STDERR_FILENO, "[cos] network-save-ui: PASS genuine-refusal=1 read-denial-retry=1 replacement-denial-retry=1 "
                   "actual-native-UI-save=1 both-GCI-copies=1 local-awards=0 remote-applies=1 native-loads-unchanged=1 "
                   "XYZ/resources/position-unchanged=1 blank-other-slot-bytes-preserved=1\n");
            enter(Step::Done); unlock(); setDrivenPad(false, 0, 0, 0); pc_exit(PC_EXIT_REACHED);
        }
        break;
    case Step::Off: case Step::Done: break;
    }
}
} // namespace pc
