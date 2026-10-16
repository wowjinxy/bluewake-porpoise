// SPDX-License-Identifier: GPL-3.0-or-later
// COS_NETWORK_RECOVERY=card-retry|host-load; off unless explicitly selected.
// card-retry accompanies network-progress/receive or a real save-load. Native
// CARD requests/results are never fabricated: Windows sharing denies one read
// and one atomic replacement, then the original save sweep verifies the retry.
// host-load starts an owned listener before actual file-select quest loading.
// Bind127.0.0.2/server127.0.0.1 distinguishes local-host rejoin from public IP.
// Requires COS_NETWORK_RECOVERY_PRIVATE_ROOT beneath build/, containing the
// exact marker network-recovery.allow: "BlueWake.Native.Recovery.Private.v1\n".
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc_network_recovery_smoke.h"
#include "pc_internal.h"
#include "pc/pc_network_game.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "../../network/network_digest.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_MemCard.h"
#include <dolphin/pad.h>
#include "../../network/atomic_file.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <unistd.h>

namespace pc {
namespace {
namespace fs = std::filesystem;
enum class Case { Off, CardRetry, HostLoad };
enum class Step { Waiting, ReadLocked, ReadFailurePublished, ReadRetry,
                  ReadRecovered, WriteLocked, WriteFailurePublished,
                  WriteControllerRead, WriteControllerRecovered,
                  WriteRetryReady, WriteRetry, SavePublished, HostLoaded, Done };
Case sCase = Case::Off;
Step sStep = Step::Waiting;
bool sChecked = false, sReceive = false, sLoaded = false, sHostStarted = false;
bool sInitialConnected = false, sSaveCompared = false;
bool sBeginOwningSave = false;
bool sReadIoErrorObserved = false, sWriteIoErrorObserved = false;
// getStatus maps the controller's terminal IOERROR state12 to UI status8.
// Observe it before LoadSync rewrites that state as part of consuming failure.
constexpr u32 kNativeIoErrorStatus = 8;
unsigned sFrame = 0, sSince = 0, sStartFrame = 0, sStable = 0;
uint64_t sStartNs = 0, sSavesBeforeWrite = 0, sGenerationBeforeWrite = 0;
uint64_t sInitialGeneration = 0, sInitialRevision = 0;
PcNetworkGameSnapshot sBefore{};
std::string sGci, sReadHash, sWriteHash, sRoomIdentity;
bool sUnsavedBefore = false;
u8 sSlots[3]{}, sItems[3]{};
int sResources[5]{};
// This CARD read repairs controller readiness only. Never unpack its older
// disk snapshot over the current unsaved quest or reuse it as the retry payload.
alignas(32) u8 sControllerRead[sizeof(card_gamedata) * 3];
#ifdef _WIN32
HANDLE sLock = INVALID_HANDLE_VALUE;
#endif

const char* caseName() { return sCase == Case::HostLoad ? "host-load" : "card-retry"; }
void unlock() {
#ifdef _WIN32
    if (sLock != INVALID_HANDLE_VALUE) { CloseHandle(sLock); sLock = INVALID_HANDLE_VALUE; }
#endif
}
[[noreturn]] void fail(const char* reason) {
    unlock();
    writef(STDERR_FILENO, "[cos] network-recovery: FAIL case=%s step=%d frame=%u: %s\n",
           caseName(), static_cast<int>(sStep), sFrame, reason);
    pc_exit(PC_EXIT_CHECK_FAILED);
}
[[noreturn]] void usage(const char* reason) {
    unlock();
    writef(STDERR_FILENO, "[cos] network-recovery: usage: %s\n", reason);
    pc_exit(PC_EXIT_USAGE);
}
void need(bool condition, const char* reason) { if (!condition) fail(reason); }
std::string utf8(const fs::path& path) { const auto s = path.u8string(); return {s.begin(), s.end()}; }
fs::path absolute(const char* value) {
    if (!value || !*value || std::strlen(value) >= 1024) usage("explicit bounded private paths required");
    return fs::absolute(fs::u8path(value)).lexically_normal();
}
bool inside(const fs::path& path, const fs::path& parent) {
    auto a = path.begin();
    for (auto b = parent.begin(); b != parent.end(); ++a, ++b) {
        if (a == path.end()) return false;
#ifdef _WIN32
        if (_wcsicmp(a->c_str(), b->c_str())) return false;
#else
        if (*a != *b) return false;
#endif
    }
    return path != parent;
}
std::string readSmall(const fs::path& path, size_t expected) {
    FILE* file = pc_net_atomic_open(utf8(path).c_str(), "rb");
    need(file != nullptr, "cannot read an explicitly private fixture file");
    std::string bytes(expected, '\0');
    const bool read = std::fread(bytes.data(), 1, expected, file) == expected &&
                      std::fgetc(file) == EOF && !std::ferror(file);
    const bool closed = std::fclose(file) == 0;
    need(read && closed, "private fixture file size/read mismatch");
    return bytes;
}
std::string gciHash() { return pc_net::digest(readSmall(fs::u8path(sGci), 98368)); }
PcNetworkGameSnapshot snapshot() { PcNetworkGameSnapshot v{}; pc_network_game_snapshot(&v); return v; }
void enter(Step step) { sStep = step; sSince = 0; }

void validatePrivate() {
    const char* privateText = std::getenv("COS_NETWORK_RECOVERY_PRIVATE_ROOT");
    const auto root = absolute(privateText);
    bool belowBuild = false;
    for (const auto& part : root.parent_path()) if (part == "build") belowBuild = true;
    if (!belowBuild || !fs::is_directory(root) || fs::is_symlink(fs::symlink_status(root)))
        usage("private recovery root must be an existing directory beneath build/");
    const std::string marker = "BlueWake.Native.Recovery.Private.v1\n";
    const auto markerPath = root / "network-recovery.allow";
    if (!fs::is_regular_file(fs::symlink_status(markerPath)) || fs::hard_link_count(markerPath) != 1)
        usage("private recovery opt-in marker must be one regular file");
    if (readSmall(markerPath, marker.size()) != marker)
        usage("private recovery opt-in marker mismatch");
    const auto run = absolute(gConfig.runDir);
    const auto storage = absolute(std::getenv("COS_NETWORK_ROOT"));
    const auto prefs = absolute(std::getenv("COS_NETWORK_PROFILE"));
    if (!inside(run, root) || !inside(storage, root) || !inside(prefs, root))
        usage("run/profile/network storage must all be beneath the explicit private root");
    const char* card = std::getenv("COS_CARD_DIR");
    if (card && *card) usage("room diagnostics refuse a personal-card override");
    const auto mounted = absolute(pc_network_game_card_base());
    if (!inside(mounted, storage)) usage("mounted room card escaped private storage");
    sGci = utf8(mounted / "USA" / "Card A" / "01-GZLE-gczelda.gci");
    const char* actual = runCardGciPath();
    if (!actual || absolute(actual) != absolute(sGci.c_str()))
        usage("harness GCI differs from the validated mounted room Card A");
}

bool selected() {
    JKRPcHostAllocScope host;
    if (sChecked) return sCase != Case::Off;
    sChecked = true;
    const char* mode = std::getenv("COS_NETWORK_RECOVERY");
    if (!mode || !*mode) return false;
    if (!std::strcmp(mode, "card-retry")) sCase = Case::CardRetry;
    else if (!std::strcmp(mode, "host-load")) sCase = Case::HostLoad;
    else usage("COS_NETWORK_RECOVERY must be card-retry|host-load");
#ifndef _WIN32
    usage("OS sharing-denial recovery diagnostics are Windows-only");
#endif
    if (!gConfig.smoke || gConfig.frames || gConfig.milestone || gConfig.uncapped)
        usage("use a native smoke without frame/milestone exits or uncapped pacing");
    const bool load = !std::strcmp(gConfig.smoke, "save-load");
    const char* networkCase = std::getenv("COS_NETWORK_CASE");
    sReceive = !std::strcmp(gConfig.smoke, "network-progress") && networkCase &&
               !std::strcmp(networkCase, "receive");
    if (sCase == Case::HostLoad ? !load : !load && !sReceive)
        usage("host-load requires save-load; card-retry requires save-load or network-progress/receive");
    if (load && (pc_boot_stage() || bootPreset())) usage("fresh loads cannot debug-boot or use a preset");
    const auto view = snapshot();
    if (!view.prepared || !view.room_mode) usage("validated isolated room mode required");
    validatePrivate();
    sStartNs = monotonicNs(); sStartFrame = sFrame;
    writef(STDERR_FILENO, "[cos] network-recovery: armed case=%s receiver=%d private-route=1 OS-faults=%d\n",
           caseName(), sReceive, sCase == Case::CardRetry);
    return true;
}

void capture(const PcNetworkGameSnapshot& view) {
    need(view.authorized && view.progress.telescope_owned && view.progress.safe_to_apply,
         "active owned Telescope quest must be authorized and idle before retry probe");
    need(!sReceive || (view.unsaved_remote && view.remote_applies == 1 && view.local_awards == 0),
         "receiver must reach actual one-time grant pending a native save");
    sBefore = view; sUnsavedBefore = view.unsaved_remote;
    for (int i = 0; i != 3; ++i) { sSlots[i] = dComIfGs_getSelectItem(i); sItems[i] = dComIfGp_getSelectItem(i); }
    sResources[0] = dComIfGs_getRupee(); sResources[1] = dComIfGs_getLife();
    sResources[2] = dComIfGs_getMagic(); sResources[3] = dComIfGs_getArrowNum(); sResources[4] = dComIfGs_getBombNum();
}
void unchanged(const PcNetworkGameSnapshot& view) {
    const auto& a = view.progress.identity;
    const auto& b = sBefore.progress.identity;
    need(a.session_id == b.session_id && a.epoch == b.epoch &&
         a.quest_generation == b.quest_generation && a.quest_slot == b.quest_slot &&
         a.card_serial == b.card_serial && a.scene_id == b.scene_id && a.player_id == b.player_id &&
         !std::strcmp(a.profile_id, b.profile_id) && !std::memcmp(a.player_name, b.player_name, sizeof(a.player_name)) &&
         view.native_loads == sBefore.native_loads, "quest/player/profile changed during native retry");
    need(view.local_awards == sBefore.local_awards && view.remote_applies == sBefore.remote_applies &&
         view.progress.telescope_owned == sBefore.progress.telescope_owned, "retry duplicated or changed permanent reward");
    need(view.progress.facts_valid == sBefore.progress.facts_valid &&
         (!view.progress.facts_valid || !std::memcmp(view.progress.facts, sBefore.progress.facts,
                                                     sizeof(view.progress.facts))),
         "controller retry changed the current native permanent facts");
    for (int i = 0; i != 3; ++i)
        need(sSlots[i] == dComIfGs_getSelectItem(i) && sItems[i] == dComIfGp_getSelectItem(i), "retry changed XYZ selectors");
    need(sResources[0] == dComIfGs_getRupee() && sResources[1] == dComIfGs_getLife() &&
         sResources[2] == dComIfGs_getMagic() && sResources[3] == dComIfGs_getArrowNum() &&
         sResources[4] == dComIfGs_getBombNum(), "retry changed local resource counts");
}
void lockGci(bool readFault) {
#ifdef _WIN32
    need(sLock == INVALID_HANDLE_VALUE, "a controlled GCI lock is already held");
    std::unique_ptr<wchar_t, decltype(&std::free)> path(pc_net_atomic_wide_path(sGci.c_str()), std::free);
    need(path != nullptr, "cannot convert private GCI path");
    // Read: deny every second opener. Write: pin with no desired access because
    // SDL opens r+b streams with share0; a retained read handle would block its
    // readiness probe before atomic publication. Omit FILE_SHARE_DELETE so only
    // the real replacement fails, then verify bytes through a short-lived reader.
    sLock = CreateFileW(path.get(), readFault ? GENERIC_READ : 0,
                        readFault ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    need(sLock != INVALID_HANDLE_VALUE, "cannot acquire controlled private GCI sharing denial");
    BY_HANDLE_FILE_INFORMATION info{};
    need(GetFileInformationByHandle(sLock, &info) && info.nNumberOfLinks == 1 &&
         !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
         info.nFileSizeHigh == 0 && info.nFileSizeLow == 98368, "GCI lock target is not one regular native save");
    HANDLE reader = sLock;
    if (!readFault) {
        reader = CreateFileW(path.get(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        need(reader != INVALID_HANDLE_VALUE, "cannot read through the replacement-only GCI pin");
        BY_HANDLE_FILE_INFORMATION readInfo{};
        const bool sameFile = GetFileInformationByHandle(reader, &readInfo) &&
            readInfo.dwVolumeSerialNumber == info.dwVolumeSerialNumber &&
            readInfo.nFileIndexHigh == info.nFileIndexHigh && readInfo.nFileIndexLow == info.nFileIndexLow &&
            readInfo.nNumberOfLinks == 1 &&
            !(readInfo.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
            readInfo.nFileSizeHigh == 0 && readInfo.nFileSizeLow == 98368;
        if (!sameFile) CloseHandle(reader);
        need(sameFile, "verification reader differs from the pinned regular native GCI");
    }
    std::string bytes(98368, '\0'); DWORD count = 0;
    const bool read = ReadFile(reader, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) &&
                      count == bytes.size();
    const bool closed = readFault || CloseHandle(reader);
    need(read && closed,
         "cannot verify the locked private GCI bytes");
    need(pc_net::digest(bytes) == (readFault ? sReadHash : sWriteHash), "GCI changed while acquiring controlled lock");
#else
    (void)readFault;
    usage("controlled Windows GCI lock unavailable");
#endif
}
} // namespace

void networkRecoveryFrame(unsigned frames) {
    sFrame = frames;
    if (!selected()) return;
    JKRPcHostAllocScope host;
    ++sSince;
    const auto view = snapshot();
    need(!view.failed && view.session.status != PC_NETWORK_FAILED && view.session.status != PC_NETWORK_REJECTED,
         "native networking failed during recovery");
    need(monotonicNs() - sStartNs < 360000000000ULL && frames - sStartFrame < 16000,
         "bounded native recovery milestones expired");
    if (sStep != Step::Waiting && sSince > 900) fail("recovery step exceeded 900 normally paced frames");
    if (sCase == Case::HostLoad) {
        if (!sHostStarted) {
            PcNetworkProfile profile{};
            need(pc_network_game_profile(&profile) && !std::strcmp(profile.bind, "127.0.0.2") &&
                 !std::strcmp(profile.config.server, "127.0.0.1"), "host-load requires differing owned loopback bind and public endpoint");
            sReadHash = gciHash();
            need(view.native_loads == 0 && pc_network_game_start(true), "owned Host must start before actual quest loading");
            sHostStarted = true;
        }
        if (!sInitialConnected && view.session.status == PC_NETWORK_CONNECTED) {
            need(view.native_loads == 0 && view.local_host && view.session.room_identity[0],
                 "owned host did not connect before actual file-select load");
            sInitialConnected = true; sInitialGeneration = view.session.generation;
            sInitialRevision = view.session.server_revision; sRoomIdentity = view.session.room_identity;
            writef(STDERR_FILENO, "[cos] network-recovery: initial owned Host connected generation=%llu revision=%llu\n",
                   static_cast<unsigned long long>(sInitialGeneration), static_cast<unsigned long long>(sInitialRevision));
        }
        if (!sLoaded) return;
        setDrivenPad(true, 0, 0, 0);
        need(view.native_loads != 0 && view.local_awards == 0 && view.remote_applies == 0 && view.progress.telescope_owned,
             "fresh owning quest load lost ownership or emitted an award/apply");
        if (view.session.status == PC_NETWORK_CONNECTED && view.local_host && view.authorized &&
            view.session.room_identity == sRoomIdentity && view.session.server_revision >= sInitialRevision &&
            view.session.pending_outgoing == 0 && !view.pending_local) {
            if (++sStable >= 90) {
                need(gciHash() == sReadHash, "post-load owned Host changed the original GCI");
                writef(STDERR_FILENO, "[cos] network-recovery: QUALIFIED case=host-load native_loads=%llu initial-generation=%llu current-generation=%llu owned-bind=127.0.0.2 public-endpoint=127.0.0.1 same-room=1 GCI-unchanged=1 local_awards=0 remote_applies=0\n",
                       static_cast<unsigned long long>(view.native_loads), static_cast<unsigned long long>(sInitialGeneration),
                       static_cast<unsigned long long>(view.session.generation));
                pc_exit(PC_EXIT_REACHED);
            }
        } else sStable = 0;
        return;
    }
    if (sStep == Step::Waiting) {
        if (sBeginOwningSave) {
            // Finish an ordinary arrival message with the same paced A input
            // as save-load; do not start saving during a cutscene/transition.
            setDrivenPad(true, dComIfGp_event_runCheck() && sSince % 40 < 2 ? PAD_BUTTON_A : 0, 0, 0);
            if (!view.progress.safe_to_apply) return;
            sBeginOwningSave = false;
            saveSweepStart("network-recovery");
        }
        return;
    }
    unchanged(view);
    if (sStep == Step::ReadFailurePublished) {
        if (sSince < 2) return; // game integration consumes the actual result on its next frame
        need(!view.authorized && view.unsaved_remote == sUnsavedBefore, "failed native read did not suspend authorization/preserve pending save");
        need(gciHash() == sReadHash, "failed native read changed original GCI");
        writef(STDERR_FILENO, "[cos] network-recovery: actual LoadSync2 published; unauthorized=1 GCI-unchanged=1; retrying native Load\n");
        mDoMemCd_Load();
        enter(Step::ReadRetry);
    } else if (sStep == Step::ReadRecovered) {
        if (sSince < 1) return;
        need(view.authorized && view.unsaved_remote == sUnsavedBefore, "successful native read retry did not restore only the same quest authorization");
        need(gciHash() == sReadHash, "successful read retry changed original GCI");
    } else if (sStep == Step::WriteFailurePublished) {
        if (sSince < 2) return;
        need(view.authorized && view.native_saves == sSavesBeforeWrite &&
             view.progress.identity.save_generation == sGenerationBeforeWrite && view.unsaved_remote == sUnsavedBefore,
             "failed native write claimed commit or cleared received unsaved progress");
        need(gciHash() == sWriteHash, "failed native atomic publication changed original GCI");
        writef(STDERR_FILENO, "[cos] network-recovery: actual SaveSync2 published; commit-unchanged=1 received-pending=%d GCI-unchanged=1; restoring controller through native Load\n", sUnsavedBefore);
        // IOERROR leaves the real controller in an error state. A direct STORE
        // cannot recover it: use the ordinary accepted read/result path first.
        // This does not call card_to_memory, QUEST_LOADED or replace sDataBuf.
        std::memset(sControllerRead, 0, sizeof(sControllerRead));
        mDoMemCd_Load();
        enter(Step::WriteControllerRead);
    } else if (sStep == Step::WriteControllerRead) {
        if (sSince < 2) return; // observe the actual request suspend authorization
        need(!view.authorized && view.unsaved_remote == sUnsavedBefore &&
             view.native_saves == sSavesBeforeWrite &&
             view.progress.identity.save_generation == sGenerationBeforeWrite,
             "controller recovery read changed save state or did not suspend sharing");
        const u32 result = mDoMemCd_LoadSync(sControllerRead, sizeof(sControllerRead), 0);
        if (result == 0) return;
        need(result == 1, "controller recovery native LoadSync failed");
        for (unsigned slot = 0; slot != 3; ++slot)
            need(mDoMemCdRWm_TestCheckSumGameData(sControllerRead + slot * sizeof(card_gamedata)),
                 "controller recovery read returned a damaged native game slot");
        need(gciHash() == sWriteHash, "controller recovery read changed the original GCI");
        unchanged(snapshot());
        enter(Step::WriteControllerRecovered);
    } else if (sStep == Step::WriteControllerRecovered) {
        if (sSince < 2) return; // require the matching real read result publication
        need(view.authorized && view.unsaved_remote == sUnsavedBefore &&
             view.native_saves == sSavesBeforeWrite &&
             view.progress.identity.save_generation == sGenerationBeforeWrite,
             "controller read did not restore only the prior quest authorization");
        need(gciHash() == sWriteHash, "controller recovery changed persisted native data");
        writef(STDERR_FILENO, "[cos] network-recovery: actual controller LoadSync1 published; separate-read-buffer=1 active-quest-unchanged=1 authorization-restored=1 GCI-unchanged=1; native retry permitted\n");
        enter(Step::WriteRetryReady);
    } else if (sStep == Step::SavePublished) {
        if (sSince < 2) return;
        need(sSaveCompared && view.authorized && view.native_saves == sSavesBeforeWrite + 1 &&
             view.progress.identity.save_generation > sGenerationBeforeWrite && !view.unsaved_remote,
             "successful native write retry lacks matching committed/cleared save proof");
        const auto savedHash = gciHash();
        need(savedHash != sWriteHash, "successful native save did not advance persisted GCI");
        writef(STDERR_FILENO, "[cos] network-recovery: QUALIFIED case=card-retry receiver=%d actual-read-failures=1 actual-write-failures=1 authorization-restored=1 native-commit=1 original-GCI-preserved-on-failure=1 both-copies-compared=1 pending-cleared=1 duplicate-award-apply=0 GCI-sha256=%s\n",
               sReceive, savedHash.c_str());
        enter(Step::Done);
        pc_exit(PC_EXIT_REACHED);
    }
}

bool networkRecoveryLoadComplete(unsigned frames) {
    sFrame = frames;
    if (!selected()) return false;
    JKRPcHostAllocScope host;
    need(!sLoaded && !sReceive, "unexpected repeated native load completion");
    sLoaded = true;
    if (sCase == Case::HostLoad) {
        need(sInitialConnected, "owning host connection was not observed before native quest activation");
        need(gciHash() == sReadHash, "actual file-select load changed the private original GCI");
        enter(Step::HostLoaded);
    } else {
        setDrivenPad(true, 0, 0, 0);
        sBeginOwningSave = true; // caller first retires the completed load state
    }
    return true;
}

void networkRecoveryBeforeCardRead() {
    if (!selected() || sCase != Case::CardRetry) return;
    JKRPcHostAllocScope host;
    need(sStep == Step::Waiting && (sReceive || sLoaded), "controlled read outside selected native save flow");
    capture(snapshot());
    sReadHash = gciHash(); lockGci(true); enter(Step::ReadLocked);
    writef(STDERR_FILENO, "[cos] network-recovery: controlled private read denial armed; original-GCI-sha256=%s\n", sReadHash.c_str());
}
bool networkRecoveryReadMayPoll() {
    if (!selected() || sCase != Case::CardRetry) return true;
    if (sStep == Step::ReadFailurePublished) return false;
    if (sStep == Step::ReadLocked) {
        const u32 status = mDoMemCd_getStatus(0);
        if (status == 14) return false; // request queued or worker still holds its mutex
        need(status == kNativeIoErrorStatus,
             "existing GCI read denial was not reported as native I/O error");
        sReadIoErrorObserved = true;
        writef(STDERR_FILENO, "[cos] network-recovery: existing GCI read denial terminal-status=8 before LoadSync; missing-file=0\n");
    }
    return true;
}
bool networkRecoveryCardReadResult(int result) {
    if (!selected() || sCase != Case::CardRetry || result == 0) return false;
    JKRPcHostAllocScope host;
    if (sStep == Step::ReadLocked) {
        need(result == 2 && sReadIoErrorObserved,
             "controlled OS read denial lacks terminal I/O status and actual LoadSync2");
        unlock(); need(gciHash() == sReadHash, "failed read altered GCI");
        enter(Step::ReadFailurePublished); return true;
    }
    need(sStep == Step::ReadRetry && result == 1, "native read retry failed or was uncorrelated");
    enter(Step::ReadRecovered); return false;
}
void networkRecoveryBeforeCardWrite() {
    if (!selected() || sCase != Case::CardRetry) return;
    JKRPcHostAllocScope host;
    need(sStep == Step::ReadRecovered, "controlled write must follow successful read retry");
    const auto view = snapshot(); unchanged(view);
    need(view.authorized && view.unsaved_remote == sUnsavedBefore, "read retry did not recover sharing before native write");
    sSavesBeforeWrite = view.native_saves; sGenerationBeforeWrite = view.progress.identity.save_generation;
    sWriteHash = gciHash(); need(sWriteHash == sReadHash, "read retry mutated persisted GCI");
    lockGci(false); enter(Step::WriteLocked);
}
bool networkRecoveryWriteMayRetry() {
    if (!selected() || sCase != Case::CardRetry) return false;
    if (sStep != Step::WriteRetryReady) return false;
    const u32 status = mDoMemCd_getStatus(0);
    if (status == 14) return false; // an actual controller reader holds its mutex
    need(status == mDoMemCd_Ctrl_c::CARD_STAT_CREATE,
         "quest retry requires a successful native read to restore controller readiness");
    enter(Step::WriteRetry); return true;
}
bool networkRecoveryWriteMayPoll() {
    if (!selected() || sCase != Case::CardRetry || sStep != Step::WriteLocked) return true;
    const u32 status = mDoMemCd_getStatus(0);
    if (status == 14) return false;
    need(status == kNativeIoErrorStatus,
         "existing GCI replacement denial was not reported as native I/O error");
    sWriteIoErrorObserved = true;
    writef(STDERR_FILENO, "[cos] network-recovery: existing GCI replacement denial terminal-status=8 before SaveSync; missing-file=0\n");
    return true;
}
bool networkRecoveryCardWriteResult(int result) {
    if (!selected() || sCase != Case::CardRetry || result == 0) return false;
    JKRPcHostAllocScope host;
    if (sStep == Step::WriteLocked) {
        need(result == 2 && sWriteIoErrorObserved,
             "controlled replacement denial lacks terminal I/O status and actual SaveSync2");
        unlock(); need(gciHash() == sWriteHash, "failed atomic replacement altered original GCI");
        enter(Step::WriteFailurePublished); return true;
    }
    need(sStep == Step::WriteRetry && result == 1, "native write retry failed or was uncorrelated");
    enter(Step::SavePublished); return false;
}
bool networkRecoverySaveComplete(unsigned frames) {
    sFrame = frames;
    if (!selected() || sCase != Case::CardRetry) return false;
    need(sStep == Step::SavePublished, "save comparison completed without actual controlled retry");
    sSaveCompared = true;
    return true;
}
} // namespace pc
