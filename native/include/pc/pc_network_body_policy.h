#pragma once

#include "pc/network/network_session.h"
#include "pc/pc_remote_body.h"
#include <cmath>
#include <cstring>

// Copied-value display policy. This never admits a peer, invents a pose, calls
// native gameplay, interpolates matrices, or changes durable progress.
namespace pc_network_body {
struct Local {
    uint64_t generation = 0, scene_epoch = 0, quest_generation = 0;
    char player_id[33]{};
    char stage[9]{};
    uint16_t room = 0, layer = 0;
};
enum class Result { hidden, changed, newer, held, invalid };

inline bool player_id(const char value[33]) {
    if (value[32]) return false;
    for (unsigned i = 0; i < 32; ++i) {
        const char c = value[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}
inline bool same_pose(const PcNetworkPose& a, const PcNetworkPose& b) {
    // Field comparison excludes C struct padding from identity decisions.
    return a.active == b.active && a.quest_generation == b.quest_generation && a.room == b.room && a.layer == b.layer &&
        !std::memcmp(a.stage, b.stage, sizeof a.stage) && !std::memcmp(a.root, b.root, sizeof a.root) &&
        !std::memcmp(a.base_scale, b.base_scale, sizeof a.base_scale) &&
        !std::memcmp(a.joints, b.joints, sizeof a.joints) && !std::memcmp(a.joint_scale, b.joint_scale, sizeof a.joint_scale);
}
inline bool geometry_safe(const PcNetworkPose& pose) {
    if (pose.active != 1 || !pose.quest_generation || pose.room > 63 || pose.layer > 15 || !pose.stage[0] || pose.stage[8]) return false;
    for (unsigned i = 0; i < 8 && pose.stage[i]; ++i) {
        const char c = pose.stage[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) return false;
    }
    for (float scale : pose.base_scale) if (!std::isfinite(scale) || scale <= 0 || scale > 128) return false;
    for (unsigned i = 0; i <= PC_NETWORK_POSE_JOINTS; ++i) {
        const float* matrix = i == PC_NETWORK_POSE_JOINTS ? pose.root : pose.joints[i];
        if (i < PC_NETWORK_POSE_JOINTS && pose.joint_scale[i] > 1) return false;
        for (unsigned j = 0; j < 12; ++j)
            if (!std::isfinite(matrix[j]) || std::fabs(matrix[j]) > (j % 4 == 3 ? 10000000.0f : 128.0f)) return false;
    }
    return true;
}
inline bool visible(const Local& local, const PcNetworkSnapshot& room, const PcNetworkRemotePose& remote) {
    if (!player_id(remote.player_id) || !remote.connection_epoch || !remote.sequence ||
        !std::memcmp(remote.player_id, local.player_id, sizeof local.player_id) ||
        remote.age_ms >= PC_NETWORK_POSE_EXPIRY_MS || !geometry_safe(remote.pose) ||
        remote.pose.room != local.room || remote.pose.layer != local.layer ||
        std::memcmp(remote.pose.stage, local.stage, sizeof local.stage)) return false;
    unsigned matches = 0;
    for (unsigned i = 0; i < room.peer_count; ++i) {
        const auto& peer = room.peers[i];
        if (!std::memcmp(peer.player_id, remote.player_id, sizeof peer.player_id) && peer.online &&
            peer.connection_epoch == remote.connection_epoch) ++matches;
    }
    return matches == 1;
}
struct Slot {
    bool bound = false;
    Local local{};
    PcNetworkRemotePose last{};
    void clear() { bound = false; local = {}; last = {}; }

    Result select(const Local& nextLocal, const PcNetworkSnapshot& room,
                  const PcNetworkPresenceSnapshot& presence, PcNetworkRemotePose& selected,
                  unsigned* other_visible = nullptr) {
        selected = {};
        if (other_visible) *other_visible = 0;
        if (!nextLocal.generation || !nextLocal.scene_epoch || !nextLocal.quest_generation || !player_id(nextLocal.player_id) ||
            room.status != PC_NETWORK_CONNECTED || room.generation != nextLocal.generation ||
            presence.generation != nextLocal.generation || room.peer_count > PC_NETWORK_PEERS || presence.count > PC_NETWORK_PEERS) {
            clear(); return Result::hidden;
        }
        const PcNetworkRemotePose* chosen = nullptr;
        const PcNetworkRemotePose* preferred = nullptr;
        unsigned visibleCount = 0;
        for (unsigned i = 0; i < presence.count; ++i) {
            const auto& candidate = presence.peers[i];
            for (unsigned j = 0; j < i; ++j) if (!std::memcmp(candidate.player_id, presence.peers[j].player_id, 33)) {
                return Result::invalid;
            }
            if (!visible(nextLocal, room, candidate)) continue;
            ++visibleCount;
            if (bound && !std::memcmp(candidate.player_id, last.player_id, 33)) preferred = &candidate;
            if (!chosen || std::strcmp(candidate.player_id, chosen->player_id) < 0) chosen = &candidate;
        }
        if (other_visible) *other_visible = visibleCount ? visibleCount - 1 : 0;
        if (preferred) chosen = preferred;
        if (!chosen) return Result::hidden; // Keep this live connection's high-water identity.
        // Connection sequence is independent of a quest/scene display binding.
        // A local scene or remote quest change cannot authorize an old sample.
        const bool sameConnection = bound && local.generation == nextLocal.generation &&
            !std::memcmp(last.player_id, chosen->player_id, 33) && last.connection_epoch == chosen->connection_epoch;
        if (sameConnection && (chosen->sequence < last.sequence ||
            (chosen->sequence == last.sequence && !same_pose(chosen->pose, last.pose)))) return Result::invalid;
        const bool sameStream = sameConnection && local.scene_epoch == nextLocal.scene_epoch &&
            local.quest_generation == nextLocal.quest_generation && !std::memcmp(local.stage, nextLocal.stage, 9) &&
            local.room == nextLocal.room && local.layer == nextLocal.layer &&
            last.pose.quest_generation == chosen->pose.quest_generation;
        const Result result = !sameStream ? Result::changed : chosen->sequence == last.sequence ? Result::held : Result::newer;
        bound = true; local = nextLocal; last = *chosen; selected = *chosen;
        return result;
    }
};

inline bool to_body(const Local& local, const PcNetworkRemotePose& remote, uint32_t receive_frame, PcRemoteBodyPose& body) {
    body = {};
    if (!local.scene_epoch || !geometry_safe(remote.pose) || remote.pose.room != local.room || remote.pose.layer != local.layer ||
        std::memcmp(remote.pose.stage, local.stage, 9) || remote.age_ms >= PC_NETWORK_POSE_EXPIRY_MS) return false;
    static_assert(PC_NETWORK_POSE_JOINTS == PC_REMOTE_BODY_JOINTS, "CL42 copied contract");
    static_assert(sizeof body.joints == sizeof remote.pose.joints, "world matrix layout");
    body.version = PC_REMOTE_BODY_POSE_VERSION; body.joint_count = PC_REMOTE_BODY_JOINTS;
    body.scene_epoch = local.scene_epoch; body.sample_frame = receive_frame;
    std::memcpy(body.stage, remote.pose.stage, sizeof body.stage);
    body.room = int8_t(remote.pose.room); body.layer = int8_t(remote.pose.layer);
    std::memcpy(body.base, remote.pose.root, sizeof body.base);
    std::memcpy(body.base_scale, remote.pose.base_scale, sizeof body.base_scale);
    std::memcpy(body.joints, remote.pose.joints, sizeof body.joints);
    std::memcpy(body.scale_flags, remote.pose.joint_scale, sizeof body.scale_flags);
    return true; // No +150 offset and no base/world multiplication.
}
} // namespace pc_network_body
