// Explicit real-room diagnostic, off by default. No fake peers/poses, authored
// effects, direct game memory changes, PAD/keyboard injection or production toggle.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_network_body.h"
#include "pc/pc_network_body_policy.h"
#include "pc/pc_network_game.h"
#include "pc_internal.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "m_Do/m_Do_ext.h"
#include <cstdio>
#include <cstring>

namespace {
pc_network_body::Slot sSlot;
PcRemoteBodyPose sCaptured{};
bool sHaveCapture, sStartAttempted, sWantVisual, sModelReady;
bool sModelAttempted, sBudgetRefused;
unsigned sExtra = ~0u;
uint64_t sLocalAccepted, sRemoteStaged;
const char* sHiddenReason;

bool first_model_budget() {
    // V4's helper allocates from Zelda, not the tiny remainder of JKR System.
    // This is one diagnostic slot, never a promise that seven graphs fit there.
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRHeap* system = JKRHeap::getSystemHeap();
    JKRHeap* zelda = mDoExt_getZeldaHeap();
    if (!root || !system || !zelda) return false;
    const u32 largest = zelda->getMaxAllocatableSize(32);
    const s32 total = zelda->getTotalFreeSize();
    std::fprintf(stderr, "[cos] network-body: budget root_total=%d root_largest=%u system_total=%d system_largest=%u zelda_total=%d zelda_largest=%u; one=4MiB scratch=12MiB seven=PENDING\n",
        root->getTotalFreeSize(), root->getMaxAllocatableSize(32), system->getTotalFreeSize(), system->getMaxAllocatableSize(32), total, largest);
    // Fixed heap headers/alignment plus a small untouched-parent margin. This
    // guard avoids an obviously impossible initial allocation; loader-internal
    // forced OOM and concurrent heap changes still need native qualification.
    constexpr u32 demand = 16 * 1024 * 1024 + 64 * 1024;
    return total >= 0 && u32(total) >= demand + 1024 * 1024 && largest >= demand;
}

void hidden(const char* reason, bool publish = false, bool reset_binding = true) {
    if (reset_binding) sSlot.clear();
    sWantVisual = false; sModelReady = false;
    pc_remote_body::hide();
    if (publish) {
        PcNetworkPose invisible{};
        pc_network_game_publish_pose(&invisible);
    }
    if (!sHiddenReason || std::strcmp(sHiddenReason, reason))
        std::fprintf(stderr, "[cos] network-body: hidden %s (render history forgotten; no visual PASS)\n", reason);
    sHiddenReason = reason;
}

bool configured(PcNetworkGameSnapshot& state, PcNetworkProfile& profile) {
    pc_network_game_snapshot(&state);
    return state.prepared && state.room_mode && !state.restart_pending && !state.failed &&
        pc_network_profile_mounted(&profile) && profile.enabled;
}

bool active_quest(const PcRemoteBodyPose& context, const PcNetworkGameSnapshot& state, PcProgressSnapshot& progress) {
    pc_progress_capture(&progress);
    return state.authorized && progress.facts_valid && progress.identity.quest_generation &&
        progress.identity.quest_slot >= 0 && progress.identity.quest_slot < 3 &&
        progress.identity.quest_generation == state.progress.identity.quest_generation &&
        progress.identity.session_id == state.progress.identity.session_id &&
        !std::strcmp(progress.identity.profile_id, pc_network_profile_mounted_identity()) &&
        context.scene_epoch == ((uint64_t(progress.identity.scene_id) << 32) | progress.identity.player_id);
}

bool same_context(const PcRemoteBodyPose& a, const PcRemoteBodyPose& b) {
    return a.scene_epoch == b.scene_epoch && a.room == b.room && a.layer == b.layer && !std::memcmp(a.stage, b.stage, 9);
}

PcNetworkPose network_pose(const PcRemoteBodyPose& pose, uint64_t quest) {
    PcNetworkPose result{};
    result.active = 1;
    result.quest_generation = quest;
    std::memcpy(result.stage, pose.stage, 9);
    result.room = uint16_t(pose.room); result.layer = uint16_t(pose.layer);
    static_assert(sizeof result.root == sizeof pose.base, "native world root layout");
    static_assert(sizeof result.joints == sizeof pose.joints, "native CL42 layout");
    std::memcpy(result.root, pose.base, sizeof result.root);
    std::memcpy(result.base_scale, pose.base_scale, sizeof result.base_scale);
    std::memcpy(result.joints, pose.joints, sizeof result.joints);
    std::memcpy(result.joint_scale, pose.scale_flags, sizeof result.joint_scale);
    return result;
}

bool publish_local(const PcRemoteBodyPose& context, const PcNetworkGameSnapshot& state, const PcProgressSnapshot& progress) {
    if (!sHaveCapture || !same_context(context, sCaptured) || pc_frame_count() - sCaptured.sample_frame > 1 ||
        state.session.status != PC_NETWORK_CONNECTED) return false;
    const PcNetworkPose pose = network_pose(sCaptured, progress.identity.quest_generation);
    if (!pc_network_game_publish_pose(&pose)) return false;
    ++sLocalAccepted;
    if (sLocalAccepted == 1 || sLocalAccepted % 300 == 0)
        std::fprintf(stderr, "[cos] network-body: real local pose accepted=%llu quest=%llu generation=%llu frame=%u\n",
            static_cast<unsigned long long>(sLocalAccepted), static_cast<unsigned long long>(pose.quest_generation),
            static_cast<unsigned long long>(state.session.generation), pc_frame_count());
    return true;
}
} // namespace

bool pc_network_body_diagnostic_selected() {
    return pc::gConfig.smoke && !std::strcmp(pc::gConfig.smoke, "network-body");
}

void pc_network_body_diagnostic_capture(J3DModel* model, daPy_lk_c* player) {
    if (!pc_network_body_diagnostic_selected()) return;
    PcRemoteBodyPose pose{};
    if (!pc_remote_body_capture_native(model, player, &pose)) {
        sHaveCapture = false; hidden("unsafe local native context", true); return;
    }
    sCaptured = pose; sHaveCapture = true;
    PcNetworkGameSnapshot state{}; PcNetworkProfile profile{}; PcProgressSnapshot progress{};
    if (!configured(state, profile) || !active_quest(pose, state, progress)) {
        hidden("mounted room or qualified native quest unavailable", true); return;
    }
    publish_local(pose, state, progress);
    // Construct/isolate a single private CL graph only after a real eligible peer
    // has appeared. No local pointer survives this immediate ownership check.
    if (sWantVisual) {
        if (!sModelAttempted) {
            sModelAttempted = true;
            sBudgetRefused = !first_model_budget();
        }
        // A refused first preflight stays refused for this diagnostic process.
        // A later render/capture tick cannot bypass the one-shot budget guard.
        if (sBudgetRefused) { hidden("one-slot heap budget unavailable"); return; }
        sModelReady = pc_remote_body::prepare_private(model);
        if (!sModelReady) hidden("private model preparation refused");
    }
}

void pc_network_body_diagnostic_before_p1() {
    if (!pc_network_body_diagnostic_selected()) return;
    PcRemoteBodyPose context{};
    PcNetworkGameSnapshot state{}; PcNetworkProfile profile{}; PcProgressSnapshot progress{};
    if (!pc_remote_body_context_native(&context) || !configured(state, profile) || !active_quest(context, state, progress)) {
        hidden("unsafe receiver context", true); return;
    }
    if (state.session.status != PC_NETWORK_CONNECTED) { hidden("session not connected"); return; }
    // Publish actual final native values before asking for same-area visibility.
    // Holding an unchanged valid pose is allowed only while its native capture is
    // fresh; remote receive-age remains checked independently every frame.
    if (!publish_local(context, state, progress)) { hidden("no fresh local pose", true); return; }
    PcNetworkPresenceSnapshot presence{};
    if (!pc_network_game_copy_presence(&presence)) { hidden("presence generation unavailable"); return; }
    pc_network_game_snapshot(&state);
    pc_network_body::Local local{};
    local.generation = state.session.generation;
    local.scene_epoch = context.scene_epoch;
    local.quest_generation = progress.identity.quest_generation;
    std::memcpy(local.player_id, profile.config.player_id, 33);
    std::memcpy(local.stage, context.stage, 9);
    local.room = uint16_t(context.room); local.layer = uint16_t(context.layer);
    PcNetworkRemotePose remote{};
    unsigned extra = 0;
    const auto result = sSlot.select(local, state.session, presence, remote, &extra);
    if (result == pc_network_body::Result::hidden || result == pc_network_body::Result::invalid) {
        hidden(result == pc_network_body::Result::invalid ? "remote stream rejected" : "no eligible same-area remote", false, false); return;
    }
    if (!pc_network_pose_valid(&remote.pose)) { hidden("remote value rejected"); return; }
    if (result == pc_network_body::Result::changed) {
        pc_remote_body::hide(); // Reset private interpolation history before another stream.
        std::fprintf(stderr, "[cos] network-body: binding %.8s connection=%llu quest=%llu generation=%llu scene=%llu\n",
            remote.player_id, static_cast<unsigned long long>(remote.connection_epoch),
            static_cast<unsigned long long>(remote.pose.quest_generation), static_cast<unsigned long long>(presence.generation),
            static_cast<unsigned long long>(context.scene_epoch));
    }
    sWantVisual = true;
    if (extra != sExtra) {
        sExtra = extra;
        if (extra) std::fprintf(stderr, "[cos] network-body: %u additional eligible peers hidden by ONE-SLOT diagnostic; full seven remain pending\n", extra);
    }
    if (!sModelReady) { pc_remote_body::hide(); return; }
    PcRemoteBodyPose body{};
    if (!pc_network_body::to_body(local, remote, pc_frame_count(), body) || !pc_remote_body::stage(body)) {
        pc_remote_body::hide(); return;
    }
    pc_remote_body::before_p1(context);
    sHiddenReason = nullptr;
    if (++sRemoteStaged == 1 || sRemoteStaged % 300 == 0)
        std::fprintf(stderr, "[cos] network-body: staged real remote=%llu seq=%llu age_ms=%u frame=%u; actual visual inspection required\n",
            static_cast<unsigned long long>(sRemoteStaged), static_cast<unsigned long long>(remote.sequence), remote.age_ms, pc_frame_count());
}

void pc_network_body_diagnostic_frame() {
    if (!pc_network_body_diagnostic_selected()) return;
    PcRemoteBodyPose context{};
    PcNetworkGameSnapshot state{}; PcNetworkProfile profile{}; PcProgressSnapshot progress{};
    if (!configured(state, profile)) { sHaveCapture = false; hidden("explicit mounted room required", true); return; }
    if (!pc_remote_body_context_native(&context) || !active_quest(context, state, progress)) {
        sHaveCapture = false; hidden("native quest not active", true); return;
    }
    // One explicit owned start, using only the immutable mounted role/endpoint.
    // No personal-mode opt-in, duplicate listener, or auto-rejoin after user Leave.
    if (!sStartAttempted) {
        sStartAttempted = true;
        if (state.session.status == PC_NETWORK_OFFLINE) {
            if (!pc_network_game_start(profile.host)) { hidden("configured Host/Join refused"); return; }
            std::fprintf(stderr, "[cos] network-body: configured %s requested; no visual/multiplayer PASS\n", profile.host ? "host" : "client");
        } else if (state.session.status == PC_NETWORK_CONNECTED || state.session.status == PC_NETWORK_CONNECTING ||
                   state.session.status == PC_NETWORK_RECONNECTING) {
            std::fprintf(stderr, "[cos] network-body: retaining existing configured connection; no listener restart\n");
        } else { hidden("configured session unavailable"); return; }
    }
    if (state.session.status == PC_NETWORK_REJECTED || state.session.status == PC_NETWORK_FAILED || state.failed) {
        hidden("configured connection rejected", true); return;
    }
    if (!publish_local(context, state, progress)) hidden("no fresh active source", true);
}
