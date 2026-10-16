// SPDX-License-Identifier: GPL-3.0-or-later
// Copied-value transient companion to network_wire.h; see PROVENANCE.md.
#ifndef PC_NATIVE_NETWORK_PRESENCE_H
#define PC_NATIVE_NETWORK_PRESENCE_H
#include "network_wire.h"
#include <cmath>
#include <limits>

namespace pc_net {
constexpr uint16_t pose_version = 1;
constexpr float pose_world_limit = 10000000.0f;
constexpr float pose_basis_limit = 1024.0f;
constexpr float pose_scale_limit = 64.0f;
constexpr size_t pose_active_bytes = 1+2+2+8+2+8+2+2+48+12+42*48+42;
static_assert(pose_active_bytes+34+8 < max_body, "One complete Link pose fits one bounded frame");
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,
              "Presence wire format requires IEEE754 binary32");

inline bool scene_equal(const PcNetworkPose& a, const PcNetworkPose& b) {
    return a.active && b.active && a.room==b.room && a.layer==b.layer &&
        std::strcmp(a.stage,b.stage)==0;
}
inline bool context_equal(const PcNetworkPose& a, const PcNetworkPose& b) {
    return scene_equal(a,b) && a.quest_generation==b.quest_generation;
}
inline bool matrix_valid(const float* matrix) {
    for(unsigned row=0;row<3;++row)for(unsigned column=0;column<4;++column) {
        const float value=matrix[row*4+column];
        if(!std::isfinite(value) || std::fabs(value)>(column==3?pose_world_limit:pose_basis_limit))return false;
    }
    return true;
}
inline bool pose_valid(const PcNetworkPose& pose) {
    if(pose.active==0)return true; // hidden must work even with stale/NaN fields
    if(pose.active!=1 || !std::memchr(pose.stage,0,sizeof pose.stage) || !pose.stage[0] ||
       std::strspn(pose.stage,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=std::strlen(pose.stage) ||
       pose.room>63 || pose.layer>15 || !pose.quest_generation || !matrix_valid(pose.root))return false;
    for(float scale:pose.base_scale)if(!std::isfinite(scale)||scale<=0||scale>pose_scale_limit)return false;
    for(unsigned i=0;i<PC_NETWORK_POSE_JOINTS;++i)
        if(pose.joint_scale[i]>1||!matrix_valid(pose.joints[i]))return false;
    return true;
}
inline void put_float(Bytes& bytes,float value) {
    uint32_t bits=0;std::memcpy(&bits,&value,sizeof bits);put32(bytes,bits);
}
inline float get_float(Reader& reader) {
    const uint32_t bits=reader.dword();float value=0;std::memcpy(&value,&bits,sizeof value);return value;
}
inline void pose_write(Bytes& bytes,const PcNetworkPose& pose) {
    bytes.push_back(pose.active?1:0);
    if(!pose.active)return;
    put16(bytes,pose_version);put16(bytes,PC_NETWORK_POSE_JOINTS);
    put64(bytes,pose.quest_generation);string(bytes,pose.stage);
    put16(bytes,pose.room);put16(bytes,pose.layer);
    for(float value:pose.root)put_float(bytes,value);
    for(float value:pose.base_scale)put_float(bytes,value);
    for(const auto& joint:pose.joints)for(float value:joint)put_float(bytes,value);
    for(uint8_t scale:pose.joint_scale)bytes.push_back(scale);
}
inline bool pose_read(Reader& reader,PcNetworkPose& pose) {
    pose={};pose.active=reader.byte();
    if(pose.active==0)return reader.ok;
    if(pose.active!=1 || reader.word()!=pose_version || reader.word()!=PC_NETWORK_POSE_JOINTS)return false;
    pose.quest_generation=reader.qword();
    if(!reader.text(pose.stage,sizeof pose.stage))return false;
    pose.room=reader.word();pose.layer=reader.word();
    for(float& value:pose.root)value=get_float(reader);
    for(float& value:pose.base_scale)value=get_float(reader);
    for(auto& joint:pose.joints)for(float& value:joint)value=get_float(reader);
    for(uint8_t& scale:pose.joint_scale)scale=reader.byte();
    return reader.ok&&pose_valid(pose);
}
} // namespace pc_net
#endif
