#include "pc/pc_network_body_policy.h"
#include "pc/pc_remote_body_visual_policy.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1); } ++checks; } while (0)
static unsigned checks;
using namespace pc_network_body;
static void id(char out[33], char c) { std::memset(out,c,32);out[32]=0; }
static PcNetworkPose pose(float x) {
    PcNetworkPose p{};p.active=1;std::strcpy(p.stage,"sea");p.room=44;p.layer=9;p.quest_generation=17;
    p.base_scale[0]=p.base_scale[1]=p.base_scale[2]=1;p.root[0]=p.root[5]=p.root[10]=1;p.root[3]=x;
    for(unsigned i=0;i<42;++i){p.joints[i][0]=p.joints[i][5]=p.joints[i][10]=1;p.joints[i][3]=x+float(i);p.joint_scale[i]=uint8_t(i%2);}
    return p;
}
struct Fixture {
    Local local{};PcNetworkSnapshot room{};PcNetworkPresenceSnapshot presence{};
    Fixture(){local.generation=2;local.scene_epoch=23;local.quest_generation=5;id(local.player_id,'a');std::strcpy(local.stage,"sea");local.room=44;local.layer=9;
        room.status=PC_NETWORK_CONNECTED;room.generation=2;room.peer_count=2;id(room.peers[0].player_id,'a');room.peers[0].connection_epoch=10;room.peers[0].online=true;
        id(room.peers[1].player_id,'b');room.peers[1].connection_epoch=11;room.peers[1].online=true;
        presence.generation=2;presence.count=1;auto&r=presence.peers[0];id(r.player_id,'b');r.connection_epoch=11;r.sequence=8;r.pose=pose(1234);}
};
static void nativeVisualPolicy() {
    PcRemoteBodyPose p{}; p.version=PC_REMOTE_BODY_POSE_VERSION; p.joint_count=42;
    p.scene_epoch=7; p.sample_frame=100; std::strcpy(p.stage,"sea"); p.room=44; p.layer=9;
    PcRemoteBodyVisual v{}; v.version=PC_REMOTE_VISUAL_VERSION; v.scene_epoch=7;
    v.sample_frame=100; std::strcpy(v.stage,"sea"); v.room=44; v.layer=9; v.open_hands=3;
    CHECK(pc_remote_visual::valid(v)); CHECK(pc_remote_visual::matches(v,p,100));
    CHECK(pc_remote_visual::matches(v,p,101)); CHECK(!pc_remote_visual::matches(v,p,102));
    CHECK(!pc_remote_visual::matches(v,p,99)); // Future samples cannot authorize a draw.
    for (unsigned mask=0; mask<4; ++mask) {
        v.open_hands=static_cast<uint8_t>(mask);
        CHECK(pc_remote_visual::hand_mask(true,true,v,p,100)==mask);
        CHECK(pc_remote_visual::hand_mask(false,true,v,p,100)==0); // Network/default pose.
        CHECK(pc_remote_visual::hand_mask(true,false,v,p,100)==0); // Unknown native selection.
    }
    v.open_hands=3; auto bad=v; bad.open_hands=4; CHECK(!pc_remote_visual::valid(bad));
    bad=v; bad.reserved=1; CHECK(!pc_remote_visual::valid(bad));
    bad=v; bad.version=0; CHECK(!pc_remote_visual::valid(bad));
    bad=v; bad.scene_epoch=0; CHECK(!pc_remote_visual::valid(bad));
    bad=v; ++bad.scene_epoch; CHECK(!pc_remote_visual::matches(bad,p,100));
    bad=v; --bad.sample_frame; CHECK(!pc_remote_visual::matches(bad,p,100)); // Pair must be SAME native draw.
    bad=v; ++bad.room; CHECK(!pc_remote_visual::matches(bad,p,100));
    bad=v; ++bad.layer; CHECK(!pc_remote_visual::matches(bad,p,100));
    bad=v; bad.room=64; CHECK(!pc_remote_visual::valid(bad));
    bad=v; bad.layer=16; CHECK(!pc_remote_visual::valid(bad));
    bad=v; std::strcpy(bad.stage,"RoomA"); CHECK(!pc_remote_visual::matches(bad,p,100));
    bad=v; bad.stage[0]=0; CHECK(!pc_remote_visual::valid(bad));
    bad=v; bad.stage[0]='!'; CHECK(!pc_remote_visual::valid(bad));
    bad=v; bad.stage[8]='x'; CHECK(!pc_remote_visual::valid(bad));
    bad=v; bad.stage[4]='x'; CHECK(!pc_remote_visual::valid(bad)); // Hidden trailing bytes after NUL.
    auto wrong=p; wrong.version=0; CHECK(!pc_remote_visual::matches(v,wrong,100));
    wrong=p; wrong.joint_count=41; CHECK(!pc_remote_visual::matches(v,wrong,100));
    wrong=p; wrong.reserved=1; CHECK(!pc_remote_visual::matches(v,wrong,100));
    p.sample_frame=v.sample_frame=UINT32_MAX;
    CHECK(pc_remote_visual::matches(v,p,0)); // Actual unsigned frame wrap, age1.
    CHECK(!pc_remote_visual::matches(v,p,1));
}
int main(){
    nativeVisualPolicy();
    Fixture f;Slot slot;PcNetworkRemotePose chosen{};unsigned extra=99;
    CHECK(slot.select(f.local,f.room,f.presence,chosen,&extra)==Result::changed);CHECK(extra==0&&chosen.sequence==8);
    CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::held);
    ++f.presence.peers[0].sequence;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::newer);
    f.presence.peers[0].sequence=8;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::invalid);CHECK(chosen.sequence==0);
    CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::invalid); // high water survives rejection
    f.presence.peers[0].sequence=9;f.presence.peers[0].pose.root[3]+=1;
    CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::invalid); // equal sequence cannot change values
    ++f.presence.peers[0].sequence;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::newer);
    ++f.presence.peers[0].pose.quest_generation;f.presence.peers[0].sequence=1;
    CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::invalid); // quest cannot reset the connection sequence
    f.presence.peers[0].sequence=11;
    CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::changed);
    --f.presence.peers[0].pose.quest_generation;f.presence.peers[0].sequence=10;
    CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::invalid); // old quest plus old sequence remains rejected
    ++f.presence.peers[0].pose.quest_generation;f.presence.peers[0].sequence=1;
    ++f.presence.peers[0].connection_epoch;++f.room.peers[1].connection_epoch;
    CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::changed);
    ++f.local.scene_epoch;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::changed);
    ++f.local.quest_generation;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::changed);
    ++f.local.generation;++f.room.generation;++f.presence.generation;
    CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::changed);
    f.presence.peers[0].age_ms=PC_NETWORK_POSE_EXPIRY_MS;
    CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::hidden);CHECK(chosen.sequence==0);
    f.presence.peers[0].age_ms=0;++f.presence.peers[0].sequence;
    CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::newer);
    PcRemoteBodyPose body{};CHECK(to_body(f.local,f.presence.peers[0],900,body));
    CHECK(body.base[0][3]==1235&&body.joints[41][0][3]==1275); // no local-offset injection
    CHECK(body.sample_frame==900&&body.scene_epoch==f.local.scene_epoch&&body.scale_flags[41]==1);
    CHECK(std::memcmp(body.joints,f.presence.peers[0].pose.joints,sizeof body.joints)==0);
    f.presence.peers[0].pose.joints[41][0]=std::numeric_limits<float>::quiet_NaN();
    CHECK(!to_body(f.local,f.presence.peers[0],901,body));CHECK(body.joint_count==0);
    f=Fixture{};f.room.status=PC_NETWORK_RECONNECTING;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::hidden);CHECK(!slot.bound);
    f=Fixture{};++f.presence.generation;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::hidden);
    f=Fixture{};f.room.peers[1].online=false;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::hidden);
    f=Fixture{};++f.room.peers[1].connection_epoch;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::hidden);
    f=Fixture{};id(f.presence.peers[0].player_id,'a');CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::hidden);
    f=Fixture{};std::strcpy(f.presence.peers[0].pose.stage,"RoomA");CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::hidden);
    f=Fixture{};std::strcpy(f.presence.peers[0].pose.stage,"sea!");CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::hidden);
    f=Fixture{};++f.presence.peers[0].pose.room;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::hidden);
    f=Fixture{};++f.presence.peers[0].pose.layer;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::hidden);
    f=Fixture{};f.presence.count=2;f.presence.peers[1]=f.presence.peers[0];CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::invalid);
    f=Fixture{};slot.clear();f.room.peer_count=3;id(f.room.peers[2].player_id,'c');f.room.peers[2].online=true;f.room.peers[2].connection_epoch=12;
    f.presence.count=2;f.presence.peers[1]=f.presence.peers[0];id(f.presence.peers[1].player_id,'c');f.presence.peers[1].connection_epoch=12;
    CHECK(slot.select(f.local,f.room,f.presence,chosen,&extra)==Result::changed);CHECK(chosen.player_id[0]=='b'&&extra==1);
    // Input order changes cannot steal an already-bound eligible peer's slot.
    auto tmp=f.presence.peers[0];f.presence.peers[0]=f.presence.peers[1];f.presence.peers[1]=tmp;
    CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::held);CHECK(chosen.player_id[0]=='b');
    f.room.peer_count=PC_NETWORK_PEERS+1;CHECK(slot.select(f.local,f.room,f.presence,chosen)==Result::hidden);
    std::printf("PASS network-body copied policy %u checks; synthetic only, no network/render proof\n",checks);
}
