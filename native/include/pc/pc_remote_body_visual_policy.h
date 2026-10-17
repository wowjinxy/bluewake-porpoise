// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pc/pc_remote_body.h"
#include <cstring>

// Value-only draw/pose correlation. No actor, shape, texture or wire pointers.
namespace pc_remote_visual {
inline bool stage_name(const char name[9]) {
    if (!name[0] || name[8]) return false;
    bool ended = false;
    for (unsigned i = 0; i < 9; ++i) {
        const char c = name[i];
        if (!c) { ended = true; continue; }
        if (ended || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}
inline bool valid(const PcRemoteBodyVisual& visual) {
    return visual.version == PC_REMOTE_VISUAL_VERSION && visual.scene_epoch &&
        visual.room >= 0 && visual.room <= 63 && visual.layer >= 0 && visual.layer <= 15 &&
        !visual.reserved && !(visual.open_hands & ~3u) && stage_name(visual.stage);
}
inline bool matches(const PcRemoteBodyVisual& visual, const PcRemoteBodyPose& pose, uint32_t now) {
    return valid(visual) && pose.version == PC_REMOTE_BODY_POSE_VERSION &&
        pose.joint_count == PC_REMOTE_BODY_JOINTS && !pose.reserved &&
        visual.scene_epoch == pose.scene_epoch && visual.sample_frame == pose.sample_frame &&
        now - pose.sample_frame <= 1 && visual.room == pose.room && visual.layer == pose.layer &&
        !std::memcmp(visual.stage, pose.stage, sizeof visual.stage);
}
inline uint8_t hand_mask(bool local_native_pair, bool have_visual,
                         const PcRemoteBodyVisual& visual, const PcRemoteBodyPose& pose, uint32_t now) {
    return local_native_pair && have_visual && matches(visual, pose, now) ? visual.open_hands : 0;
}
} // namespace pc_remote_visual
