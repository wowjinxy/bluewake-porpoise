// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from BlueWake runtime/host/src; see PROVENANCE.md.
#include "network_presence.h"
#include "network_store.h"
#include <atomic>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
using namespace pc_net;
namespace {
std::string nonce(){std::random_device random;std::string out(32,'0');const char* hex="0123456789abcdef";for(char& c:out)c=hex[random()&15];return out;}
void copy(char* target,size_t n,const std::string& s){std::snprintf(target,n,"%s",s.c_str());}
using Receipt=RoomReceipt;
using Room=StoredRoom;
uint64_t epoch_seed(){std::random_device random;const uint64_t value=(uint64_t(random())<<32)^uint64_t(random());
    return value&&value!=UINT64_MAX?value:1;}
struct ServerPose {
    PcNetworkPose value{};
    uint64_t sequence=0;
    bool dirty=false;
    Clock::time_point updated{},relayed{};
};
struct ServerPeer {
    Pipe pipe;
    std::string room;
    PcNetworkPeer info{};
    unsigned slot=0;
    bool joined=false,closing=false,ready_for_poses=false;
    Clock::time_point accepted=Clock::now();
    ServerPose pose;
};
void presence(Bytes& b,const PcNetworkPeer& p){string(b,p.player_id);string(b,p.player_name);b.push_back(p.online?1:0);put64(b,p.connection_epoch);}
bool presence(Reader& r,PcNetworkPeer& p){if(!r.text(p.player_id,sizeof p.player_id)||!r.text(p.player_name,sizeof p.player_name))return false;
    for(const char* s=p.player_id;*s;++s)if(!((*s>='0'&&*s<='9')||(*s>='a'&&*s<='f')))r.ok=false;
    if(std::strlen(p.player_id)!=32||!p.player_name[0])r.ok=false;
    for(const char* s=p.player_name;*s;++s)if(uint8_t(*s)<32||uint8_t(*s)>126)r.ok=false;
    const uint8_t online=r.byte();if(online!=1)r.ok=false;p.online=online!=0;
    p.connection_epoch=r.qword();if(!p.connection_epoch)r.ok=false;return r.ok;}
}
struct PcNetworkServer{
    Socket listener=invalid;uint16_t port=0;std::atomic<bool> stop{false};std::thread worker;
    std::map<std::string,Room> rooms;std::vector<std::unique_ptr<ServerPeer>> peers;
    std::unique_ptr<RoomStore> store;bool storage_failed=false;
    uint64_t next_epoch=epoch_seed();
    ~PcNetworkServer(){stop=true;if(worker.joinable())worker.join();pc_net::close(listener);}
    void reject(ServerPeer& p,const char* reason){Bytes b;string(b,reason);if(!p.pipe.queue(Reject,0,b))p.pipe.reset();p.closing=true;}
    void roster(const std::string& room){
        // Drop old unsent per-origin frames before a changed roster. A partial
        // frame finishes before the durable roster on the same ordered stream.
        for(auto& peer:peers)if(peer->joined&&peer->room==room){peer->pipe.drop_latest();peer->pose.dirty=peer->pose.sequence!=0;}
        Bytes b;unsigned count=0;for(const auto& p:peers)if(p->joined&&!p->closing&&p->room==room)++count;
        put16(b,uint16_t(count));for(const auto& p:peers)if(p->joined&&!p->closing&&p->room==room)presence(b,p->info);
        for(auto& p:peers)if(p->joined&&!p->closing&&p->room==room&&!p->pipe.queue(Roster,0,b))p->closing=true;}
    void hello(ServerPeer& p,const Frame& f){
        if(storage_failed){reject(p,"Durable room storage unavailable");return;}
        Reader r{f.body};PcNetworkConfig c{};std::strcpy(c.server,"127.0.0.1");c.port=port;
        if(!identity(r,c.compatibility)||!r.text(c.room,sizeof c.room)||!r.text(c.player_id,sizeof c.player_id)||
            !r.text(c.player_name,sizeof c.player_name)||!r.text(c.room_password,sizeof c.room_password)){reject(p,"Malformed handshake");return;}
        const uint8_t create=r.byte();c.create_room=create!=0;
        if(create>1||!r.done()||f.sequence||!pc_network_config_valid(&c,nullptr,0)){reject(p,"Invalid Wind Waker handshake");return;}
        auto found=rooms.find(c.room);
        if(found==rooms.end()){
            if(rooms.size()>=16){reject(p,"Server room limit reached");return;}
            Room room;bool saved=false;std::string error;
            if(store&&!store->load(c.compatibility,c.room,room,saved,error)){storage_failed=true;reject(p,"Durable room storage unavailable");return;}
            if(!saved){
                if(!c.create_room){reject(p,"Room does not exist; host it first");return;}
                room.name=c.room;room.compatibility=c.compatibility;room.id=nonce();room_set_credential(room,c.room_password);
            }
            found=rooms.emplace(c.room,std::move(room)).first;
        }
        Room& room=found->second;
        if(!same(room.compatibility,c.compatibility)){reject(p,"Native game/schema/rules compatibility mismatch");return;}
        if(!room_credential_matches(room,c.room_password)){reject(p,"Room password mismatch");return;}
        if(!room.clients.count(c.player_id)&&room.clients.size()>=128){reject(p,"Room player identity limit reached");return;}
        /* Joining admits a stable replay identity. Persist it before Welcome,
         * including initial room creation; never ACK an unsaved identity. */
        if(!room.clients.count(c.player_id)){
            Room next=room;next.clients.emplace(c.player_id,RoomClientHistory{});std::string error;
            if(store&&!store->save(next,error)){storage_failed=true;reject(p,"Cannot persist room identity");return;}
            room=std::move(next);
        }
        if(store){StoredRoom persisted;bool saved=false;std::string error;
            if(!store->load(room.compatibility,room.name.c_str(),persisted,saved,error)||!saved||persisted.id!=room.id||persisted.revision!=room.revision){
                storage_failed=true;reject(p,"Durable room storage unavailable");return;}}
        /* A reconnect with the same stable identity replaces its prior socket,
         * preventing one identity from generating two concurrent sequences. */
        for(auto& other:peers)if(other.get()!=&p&&other->joined&&other->room==c.room&&std::strcmp(other->info.player_id,c.player_id)==0){other->closing=true;other->pipe.reset();}
        if(next_epoch==UINT64_MAX){reject(p,"Connection epoch exhaustion");return;}
        p.info.connection_epoch=next_epoch++;
        p.room=c.room;copy(p.info.player_id,33,c.player_id);copy(p.info.player_name,33,c.player_name);p.info.online=true;p.joined=true;
        Bytes b;identity(b,room.compatibility);string(b,room.id.c_str());put64(b,room.revision);put64(b,room.clients[c.player_id].sequence);
        unsigned count=0;for(uint32_t v:room.progress.values)if(v)++count;put16(b,uint16_t(count));
        for(uint16_t key=0;key<PC_NETWORK_FACTS;++key)if(room.progress.values[key])delta(b,{key,room.progress.values[key]});
        put16(b,uint16_t(room.rewards.size()));
        if(!p.pipe.queue(Welcome,0,b)){p.closing=true;return;}
        // Bounded pages keep each frame below4KiB. The receiver publishes no
        // Connected state until the entire ledger has passed validation.
        unsigned offset=0;auto claim=room.rewards.begin();
        while(claim!=room.rewards.end()){
            Bytes page;put64(page,room.revision);put16(page,uint16_t(offset));put16(page,uint16_t(room.rewards.size()));
            const unsigned n=std::min<unsigned>(240,unsigned(room.rewards.size())-offset);put16(page,uint16_t(n));
            for(unsigned i=0;i<n;++i,++claim)reward(page,claim->second);
            if(!p.pipe.queue(Snapshot,0,page)){p.closing=true;return;}offset+=n;
        }
        roster(p.room);
    }
    void accept_pose(ServerPeer& peer,const Frame& frame){
        Reader reader{frame.body};PcNetworkPose next{};
        if(!frame.sequence||frame.sequence==UINT64_MAX||!pose_read(reader,next)||!reader.done()){
            reject(peer,"Malformed transient pose");return;
        }
        // This sequence belongs only to this live socket. It never touches the
        // permanent client history, ACK, room revision, store or reward ledger.
        if(frame.sequence<=peer.pose.sequence)return;
        if(!context_equal(peer.pose.value,next))for(auto& recipient:peers)recipient->pipe.drop_latest(peer.slot);
        peer.pose.value=next;peer.pose.sequence=frame.sequence;
        peer.pose.updated=Clock::now();peer.pose.dirty=true;
    }
    void relay_poses(){
        const auto now=Clock::now();
        for(auto& source:peers){
            if(!source->joined||source->closing||!source->pose.sequence)continue;
            auto& pose=source->pose;
            if(now-pose.updated>=std::chrono::milliseconds(PC_NETWORK_POSE_EXPIRY_MS)){
                pose.value={};pose.dirty=false;
                for(auto& recipient:peers)recipient->pipe.drop_latest(source->slot);
                continue; // receivers independently expire without a newer sequence
            }
            if(!pose.dirty||now-pose.relayed<std::chrono::milliseconds(PC_NETWORK_POSE_PERIOD_MS))continue;
            Bytes body;string(body,source->info.player_id);put64(body,source->info.connection_epoch);pose_write(body,pose.value);
            for(auto& recipient:peers)if(recipient.get()!=source.get()&&recipient->joined&&recipient->ready_for_poses&&!recipient->closing&&recipient->room==source->room)
                recipient->pipe.queue_latest(source->slot,Pose,pose.sequence,body);
            pose.dirty=false;pose.relayed=now;
        }
    }
    void handle(ServerPeer& p,const Frame& f){
        if(!p.joined){if(f.type==Hello)hello(p,f);else reject(p,"Handshake required");return;}
        Room& room=rooms.at(p.room);
        if(f.type==Pose){accept_pose(p,f);return;}
        if(f.type==Delta||f.type==Award){Reader r{f.body};PcNetworkReward claim{};PcNetworkFact d{};
            if(f.type==Award){claim=reward(r);d={claim.key,claim.value};}else d=delta(r);
            if(storage_failed){reject(p,"Durable room storage unavailable");return;}
            if(!r.done()||!f.sequence||(f.type==Award?!pc_network_reward_valid(claim):
                (!pc_network_fact_valid(d)||pc_permanent_server_authoritative(d.key)))){reject(p,"Invalid permanent progression operation");return;}
            if(claim.reward_id){const auto known=room.rewards.find(claim.reward_id);
                if(known!=room.rewards.end()&&!same_reward(known->second,claim)){reject(p,"Authored reward payload mismatch");return;}
                if(known==room.rewards.end()&&room.rewards.size()>=PC_NETWORK_REWARDS){reject(p,"Authored reward ledger limit reached");return;}}
            auto& history=room.clients.at(p.info.player_id);uint64_t& last=history.sequence;
            if(last>=UINT64_MAX-1||f.sequence>last+1){reject(p,"Client sequence gap or exhaustion");return;}
            const bool fresh=f.sequence==last+1;
            if(!fresh){const auto old=std::find_if(history.recent.begin(),history.recent.end(),[&](const Receipt& x){return x.sequence==f.sequence;});
                if(old==history.recent.end()||!same_reward(old->reward,claim)||
                    (!claim.reward_id&&(old->delta.key!=d.key||old->delta.value!=d.value))){reject(p,"Replay payload mismatch or expired sequence");return;}
                if(store){StoredRoom persisted;bool saved=false;std::string error;
                    if(!store->load(room.compatibility,room.name.c_str(),persisted,saved,error)||!saved){storage_failed=true;reject(p,"Durable room storage unavailable");return;}}}
            bool changed=false;
            if(fresh){
                Room next=room;
                if(claim.reward_id){if(!next.rewards.count(claim.reward_id)){
                    next.progress.values[claim.key]=pc_permanent_operation_result(claim.key,next.progress.values[claim.key],claim.operation,claim.value);
                    next.rewards.emplace(claim.reward_id,claim);changed=true;
                }}else changed=pc_network_fact_merge(&next.progress,d);
                if(changed&&next.revision==UINT64_MAX-1){reject(p,"Room revision exhaustion");return;}
                auto& next_history=next.clients.at(p.info.player_id);next_history.sequence=f.sequence;
                const PcNetworkFact receipt=claim.reward_id?PcNetworkFact{claim.key,next.progress.values[claim.key]}:d;
                next_history.recent.push_back({f.sequence,receipt,claim});if(next_history.recent.size()>PC_NETWORK_QUEUE)next_history.recent.pop_front();
                if(changed)++next.revision;
                std::string error;if(store&&!store->save(next,error)){storage_failed=true;reject(p,"Cannot persist room progression");return;}
                room=std::move(next);
            }
            Bytes b;string(b,p.info.player_id);put64(b,f.sequence);put64(b,room.revision);delta(b,{d.key,room.progress.values[d.key]});reward(b,claim);
            if(changed){for(auto& other:peers)if(other->joined&&!other->closing&&other->room==p.room&&!other->pipe.queue(Commit,0,b))other->closing=true;}
            else if(!p.pipe.queue(Commit,0,b))p.closing=true;
        }else if(f.type==Ping){if(!f.body.empty()||!p.pipe.queue(Pong,f.sequence,{}))p.closing=true;}
        else if(f.type==Leave){p.closing=true;}else reject(p,"Unexpected client packet");
    }
    void run(){
        while(!stop){
            for(unsigned tries=0;tries<8;++tries){Socket s=::accept(listener,nullptr,nullptr);if(s==invalid)break;
                if(peers.size()>=PC_NETWORK_PEERS||!nonblocking(s)){pc_net::close(s);continue;}
                auto p=std::make_unique<ServerPeer>();p->pipe.socket=s;
                std::array<bool,PC_NETWORK_PEERS> occupied{};for(const auto& peer:peers)occupied[peer->slot]=true;
                while(p->slot<occupied.size()&&occupied[p->slot])++p->slot;
                peers.push_back(std::move(p));}
            for(auto& p:peers){
                if(p->pipe.socket==invalid)continue;
                if(!p->pipe.pump()){p->closing=true;p->pipe.reset();continue;}
                // A pose must never interrupt Welcome's paged atomic ledger.
                // TCP ordering then guarantees all initial pages and roster
                // arrive before the first transient frame, without a new ACK.
                if(p->joined&&!p->closing&&!p->ready_for_poses&&p->pipe.output.empty()){
                    p->ready_for_poses=true;
                    // Hello's roster may have consumed a one-shot fresh pose
                    // before this peer drained its initial ledger. Reconsider
                    // current same-room sources once; relay keeps its100ms cap.
                    const auto ready=Clock::now();
                    for(auto& source:peers)if(source.get()!=p.get()&&source->joined&&!source->closing&&
                        source->room==p->room&&source->pose.sequence&&
                        ready-source->pose.updated<std::chrono::milliseconds(PC_NETWORK_POSE_EXPIRY_MS))
                        source->pose.dirty=true;
                }
                if(!p->closing)for(unsigned n=0;n<8;++n){Frame f;int result=p->pipe.next(f);if(result==0)break;
                    if(result<0){reject(*p,"Malformed or oversized protocol frame");break;}handle(*p,f);if(p->closing)break;
                    // Each durable Delta can block on a real flush. Yield to
                    // other peers, then pump its ACK before the next receipt;
                    // a full replay backlog must not hide ACKs/heartbeats.
                    if(store&&(f.type==Delta||f.type==Award))break;}
                const auto now=Clock::now();
                if(p->pipe.eof||(!p->joined&&now-p->accepted>std::chrono::seconds(5))||now-p->pipe.activity>std::chrono::seconds(30)){p->closing=true;p->pipe.reset();}
                if(p->closing&&p->pipe.output.empty())p->pipe.reset();
            }
            std::vector<std::string> changed;
            peers.erase(std::remove_if(peers.begin(),peers.end(),[&](auto& p){if(p->pipe.socket!=invalid)return false;if(p->joined)changed.push_back(p->room);return true;}),peers.end());
            for(const auto& room:changed)roster(room);
            relay_poses();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
};
struct Pending{PcNetworkFact delta;uint64_t sequence;bool sent=false;PcNetworkReward reward{};};
struct PcNetworkSession{
    std::mutex mutex;std::atomic<bool> stop{false};std::thread worker;PcNetworkConfig config{};
    PcNetworkSnapshot status{};std::deque<Pending> outgoing;std::deque<PcNetworkUpdate> incoming;
    PcNetworkFacts canonical{};std::map<uint64_t,PcNetworkReward> rewards;
    struct StagedSnapshot {bool active=false;PcNetworkFacts facts{};std::map<uint64_t,PcNetworkReward> rewards;
        char room_id[33]{};uint64_t revision=0,ack=0;unsigned expected=0;} staged;
    uint64_t next_sequence=1;
    struct RemotePose {PcNetworkRemotePose value{};Clock::time_point received{};};
    std::array<RemotePose,PC_NETWORK_PEERS> remote_poses{};
    PcNetworkPose local_pose{};Clock::time_point local_pose_time{};
    bool pose_pending=false;uint64_t next_pose_sequence=1;
    PcNetworkPresenceSnapshot pose_counters{};
    void clear_visible_poses(){for(auto& remote:remote_poses)remote.value.pose={};}
    void reset_connection_poses(){remote_poses={};local_pose={};pose_pending=false;next_pose_sequence=1;}
    void expire_poses(Clock::time_point now){for(auto& remote:remote_poses)
        if(remote.value.pose.active&&now-remote.received>=std::chrono::milliseconds(PC_NETWORK_POSE_EXPIRY_MS)){
            remote.value.pose={};++pose_counters.expired;
        }}
    ~PcNetworkSession(){leave();}
    void leave(){stop=true;if(worker.joinable())worker.join();std::lock_guard<std::mutex> lock(mutex);
        outgoing.clear();incoming.clear();canonical={};rewards.clear();staged={};reset_connection_poses();pose_counters={};status.status=PC_NETWORK_OFFLINE;status.peer_count=0;copy(status.message,128,"Offline");next_sequence=1;}
    void state(PcNetworkStatus value,const char* message){std::lock_guard<std::mutex> lock(mutex);status.status=value;
        if(value!=PC_NETWORK_CONNECTED)reset_connection_poses();copy(status.message,128,message);}
    bool publish_snapshot(bool& welcome){
        if(staged.rewards.size()!=staged.expected||status.generation==UINT64_MAX)return false;
        PcNetworkFacts steps{};
        for(const auto& entry:staged.rewards){const auto& claim=entry.second;
            if(pc_permanent_server_authoritative(claim.key))steps.values[claim.key]=pc_permanent_operation_result(claim.key,steps.values[claim.key],claim.operation,claim.value);
            else if(staged.facts.values[claim.key]<claim.value)return false;}
        for(uint16_t key=0;key<PC_NETWORK_FACTS;++key)if(pc_permanent_server_authoritative(key)&&steps.values[key]!=staged.facts.values[key])return false;
        const bool changed=status.room_identity[0]&&std::strcmp(status.room_identity,staged.room_id);
        if(changed){if(outgoing.size()>=UINT64_MAX-staged.ack)return false;uint64_t seq=staged.ack+1;
            for(auto& p:outgoing){p.sequence=seq++;p.sent=false;}next_sequence=seq;}
        else {while(!outgoing.empty()&&outgoing.front().sequence<=staged.ack)outgoing.pop_front();next_sequence=std::max(next_sequence,staged.ack+1);for(auto& p:outgoing)p.sent=false;}
        std::deque<PcNetworkUpdate> fresh;
        for(uint16_t key=0;key<PC_NETWORK_FACTS;++key)if(staged.facts.values[key])fresh.push_back({{key,staged.facts.values[key]},staged.revision,status.generation+1,true,{}});
        canonical=staged.facts;rewards=std::move(staged.rewards);incoming.swap(fresh);
        copy(status.room_identity,33,staged.room_id);status.server_revision=staged.revision;++status.generation;
        status.status=PC_NETWORK_CONNECTED;copy(status.message,128,"Connected; shared native permanent progress");staged={};welcome=true;return true;
    }
    bool received(const Frame& f,bool& welcome,bool& rejected){
        Reader r{f.body};std::lock_guard<std::mutex> lock(mutex);
        if(f.type==Reject){char message[128];if(!r.text(message,sizeof message)||!r.done())return false;
            status.status=PC_NETWORK_REJECTED;reset_connection_poses();copy(status.message,128,message);rejected=true;return true;}
        if(f.type==Welcome){PcNetworkCompatibility c{};char room_id[33];
            if(welcome||staged.active||f.sequence||!identity(r,c)||!same(c,config.compatibility)||!r.text(room_id,sizeof room_id))return false;
            const uint64_t revision=r.qword(),ack=r.qword();const unsigned count=r.word();if(count>PC_NETWORK_QUEUE)return false;
            if(ack==UINT64_MAX||revision==UINT64_MAX||((revision==0)!=(count==0))||
                std::strlen(room_id)!=32||std::strspn(room_id,"0123456789abcdef")!=32)return false;
            StagedSnapshot next;next.active=true;next.revision=revision;next.ack=ack;copy(next.room_id,33,room_id);std::array<bool,PC_NETWORK_FACTS> keys{};
            for(unsigned i=0;i<count;++i){const auto d=delta(r);if(!pc_network_fact_valid(d)||keys[d.key])return false;keys[d.key]=true;
                next.facts.values[d.key]=d.value;}
            next.expected=r.word();if(!r.done()||next.expected>PC_NETWORK_REWARDS||revision<next.expected)return false;
            staged=std::move(next);return staged.expected?true:publish_snapshot(welcome);
        }
        if(f.type==Snapshot){if(welcome||!staged.active||f.sequence)return false;
            const auto revision=r.qword();const unsigned offset=r.word(),total=r.word(),count=r.word();
            if(revision!=staged.revision||total!=staged.expected||offset!=staged.rewards.size()||!count||count>240||count>total-offset)return false;
            uint64_t previous=staged.rewards.empty()?0:staged.rewards.rbegin()->first;
            for(unsigned i=0;i<count;++i){const auto claim=reward(r);if(!pc_network_reward_valid(claim)||claim.reward_id<=previous)return false;
                previous=claim.reward_id;staged.rewards.emplace(claim.reward_id,claim);}
            if(!r.done())return false;return staged.rewards.size()==staged.expected?publish_snapshot(welcome):true;
        }
        if(!welcome)return false;
        if(f.type==Pose){
            char origin[33]{};if(!r.text(origin,sizeof origin))return false;
            const uint64_t epoch=r.qword();PcNetworkPose pose{};
            if(!epoch||!f.sequence||f.sequence==UINT64_MAX||!pose_read(r,pose)||!r.done())return false;
            unsigned index=0;for(;index<status.peer_count;++index)
                if(!std::strcmp(status.peers[index].player_id,origin)&&status.peers[index].connection_epoch==epoch)break;
            // Roster assigns the identity/epoch; stale sockets never regain a row.
            if(index==status.peer_count||!std::strcmp(origin,config.player_id))return true;
            auto& remote=remote_poses[index];
            if(f.sequence<=remote.value.sequence){++pose_counters.stale;return true;}
            remote.value={};copy(remote.value.player_id,33,origin);remote.value.connection_epoch=epoch;
            remote.value.sequence=f.sequence;remote.value.pose=pose;remote.received=Clock::now();
            ++pose_counters.received;return true;
        }
        if(f.type==Commit){char origin[33];if(!r.text(origin,sizeof origin))return false;const uint64_t sequence=r.qword(),revision=r.qword();const auto d=delta(r);const auto claim=reward(r);
            if(!r.done()||f.sequence||!sequence||std::strlen(origin)!=32||std::strspn(origin,"0123456789abcdef")!=32||!pc_network_fact_valid(d)||
                (claim.reward_id?(!pc_network_reward_valid(claim)||claim.key!=d.key):(claim.key||claim.operation||claim.value||pc_permanent_server_authoritative(d.key))))return false;
            const auto known=claim.reward_id?rewards.find(claim.reward_id):rewards.end();
            if(known!=rewards.end()&&!same_reward(known->second,claim))return false;
            if(std::strcmp(origin,config.player_id)==0){while(!outgoing.empty()&&outgoing.front().sequence<=sequence)outgoing.pop_front();}
            if(revision<=status.server_revision)return true;
            if(revision!=status.server_revision+1||incoming.size()>=PC_NETWORK_QUEUE)return false;
            if(claim.reward_id){if(known!=rewards.end()||rewards.size()>=PC_NETWORK_REWARDS||
                    pc_permanent_operation_result(claim.key,canonical.values[claim.key],claim.operation,claim.value)!=d.value)return false;
                rewards.emplace(claim.reward_id,claim);
            }else if(canonical.values[d.key]>=d.value)return false;
            canonical.values[d.key]=d.value;incoming.push_back({d,revision,status.generation,false,claim});status.server_revision=revision;++status.received;return true;
        }
        if(f.type==Roster){const unsigned count=r.word();if(count>PC_NETWORK_PEERS)return false;std::array<PcNetworkPeer,PC_NETWORK_PEERS> peers{};
            for(unsigned i=0;i<count;++i){if(!presence(r,peers[i]))return false;
                for(unsigned j=0;j<i;++j)if(!std::strcmp(peers[i].player_id,peers[j].player_id))return false;}if(!r.done())return false;
            std::array<RemotePose,PC_NETWORK_PEERS> retained{};
            for(unsigned i=0;i<count;++i)for(unsigned j=0;j<status.peer_count;++j)
                if(!std::strcmp(peers[i].player_id,status.peers[j].player_id)&&peers[i].connection_epoch==status.peers[j].connection_epoch)
                    retained[i]=remote_poses[j];
            remote_poses=retained;status.peer_count=count;
            for(unsigned i=0;i<PC_NETWORK_PEERS;++i)status.peers[i]=peers[i];return true;}
        if(f.type==Pong)return f.body.empty();return false;
    }
    void run(){
        bool ever=false;
        while(!stop){state(ever?PC_NETWORK_RECONNECTING:PC_NETWORK_CONNECTING,ever?"Reconnecting; queued progress retained":"Connecting");
            {std::lock_guard<std::mutex> lock(mutex);staged={};}
            Pipe pipe;pipe.socket=pc_net::connect(config.server,config.port);
            if(pipe.socket==invalid){for(unsigned i=0;i<40&&!stop;++i)std::this_thread::sleep_for(std::chrono::milliseconds(25));continue;}
            Bytes hello;identity(hello,config.compatibility);string(hello,config.room);string(hello,config.player_id);string(hello,config.player_name);string(hello,config.room_password);hello.push_back(config.create_room?1:0);
            pipe.queue(Hello,0,hello);bool welcome=false,rejected=false;auto ping=Clock::now(),began=Clock::now();
            auto pose_tick=Clock::now()-std::chrono::milliseconds(PC_NETWORK_POSE_PERIOD_MS);
            while(!stop){
                if(!pipe.pump())break;bool valid=true;
                for(unsigned i=0;i<128;++i){Frame f;const int result=pipe.next(f);if(result==0)break;if(result<0||!received(f,welcome,rejected)){valid=false;break;}if(rejected)break;}
                if(!valid||rejected||pipe.eof)break;
                if(welcome){
                    std::lock_guard<std::mutex> lock(mutex);
                    for(auto& p:outgoing)if(!p.sent){Bytes b;if(p.reward.reward_id)reward(b,p.reward);else delta(b,p.delta);
                        if(!pipe.queue(p.reward.reward_id?Award:Delta,p.sequence,b)){valid=false;break;}p.sent=true;++status.sent;}
                    const auto now=Clock::now();expire_poses(now);
                    if(local_pose.active&&now-local_pose_time>=std::chrono::milliseconds(PC_NETWORK_POSE_EXPIRY_MS)){
                        local_pose={};pose_pending=true;clear_visible_poses();
                    }
                    if(pose_pending&&now-pose_tick>=std::chrono::milliseconds(PC_NETWORK_POSE_PERIOD_MS)){
                        if(next_pose_sequence==UINT64_MAX){valid=false;break;}
                        Bytes body;pose_write(body,local_pose);
                        pipe.queue_latest(0,Pose,next_pose_sequence++,body);
                        pose_pending=false;pose_tick=now;++pose_counters.sent;
                    }
                }
                if(!valid)break;
                const auto now=Clock::now();if(!welcome&&now-began>std::chrono::seconds(5))break;
                if(now-pipe.activity>std::chrono::seconds(15))break;
                if(now-ping>std::chrono::seconds(2)){pipe.queue(Ping,0,{});ping=now;}
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if(stop){pipe.queue(Leave,0,{});pipe.pump();break;}
            if(rejected)return;
            if(welcome){ever=true;std::lock_guard<std::mutex> lock(mutex);++status.reconnects;for(auto& p:outgoing)p.sent=false;
                reset_connection_poses();status.status=PC_NETWORK_RECONNECTING;copy(status.message,128,"Reconnecting; queued progress retained");}
            for(unsigned i=0;i<20&&!stop;++i)std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }
};
namespace {
PcNetworkServer* start_server(const char* bind,uint16_t port,const char* directory,char* error,unsigned error_size){
    const auto failure=[&](const char* text){if(error&&error_size)std::snprintf(error,error_size,"%s",text);return static_cast<PcNetworkServer*>(nullptr);};
    if(!bind||!init())return nullptr;
    try {
    auto server=std::make_unique<PcNetworkServer>();server->listener=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(server->listener==invalid||!nonblocking(server->listener))return failure("Cannot initialize dedicated server socket");
#ifndef _WIN32
    /* Rebind an owned listener after server-initiated disconnects leave TCP
     * TIME_WAIT records. This does not permit concurrent listening servers;
     * persistent writers additionally require the exclusive store lease. */
    const int reuse=1;
    if(setsockopt(server->listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof reuse)!=0)return failure("Cannot configure dedicated server socket");
#endif
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port);
    if(inet_pton(AF_INET,bind,&address.sin_addr)!=1||::bind(server->listener,reinterpret_cast<sockaddr*>(&address),sizeof address)!=0||listen(server->listener,8)!=0)return failure("Cannot bind dedicated server");
#ifdef _WIN32
    int size=sizeof address;
#else
    socklen_t size=sizeof address;
#endif
    if(getsockname(server->listener,reinterpret_cast<sockaddr*>(&address),&size)!=0)return failure("Cannot determine dedicated server port");
    if(directory){server->store=std::make_unique<RoomStore>();std::string detail;
        if(!server->store->open(directory,detail))return failure(detail.c_str());}
    server->port=ntohs(address.sin_port);server->worker=std::thread([pointer=server.get()]{pointer->run();});return server.release();
    }catch(...){return failure("Cannot start dedicated server worker");}
}
}
extern "C" PcNetworkServer* pc_network_server_start(const char* bind,uint16_t port){return start_server(bind,port,nullptr,nullptr,0);}
extern "C" PcNetworkServer* pc_network_server_start_persistent(const char* bind,uint16_t port,const char* directory,char* error,unsigned error_size){
    if(!directory||!*directory){if(error&&error_size)std::snprintf(error,error_size,"%s","Missing durable server directory");return nullptr;}
    if(error&&error_size)*error=0;return start_server(bind,port,directory,error,error_size);
}
extern "C" uint16_t pc_network_server_port(PcNetworkServer* s){return s?s->port:0;}
extern "C" void pc_network_server_stop(PcNetworkServer* s){delete s;}
extern "C" PcNetworkSession* pc_network_create(){try{return new PcNetworkSession;}catch(...){return nullptr;}}
extern "C" void pc_network_destroy(PcNetworkSession* s){delete s;}
extern "C" void pc_network_leave(PcNetworkSession* s){if(s)s->leave();}
extern "C" bool pc_network_join(PcNetworkSession* s,const PcNetworkConfig* c){
    char message[128];if(!s||!pc_network_config_valid(c,message,sizeof message))return false;s->leave();
    {std::lock_guard<std::mutex> lock(s->mutex);s->config=*c;s->status={};s->status.status=PC_NETWORK_CONNECTING;
        s->stop=false;}
    try{s->worker=std::thread([s]{s->run();});return true;}catch(...){s->state(PC_NETWORK_FAILED,"Cannot start connection worker");return false;}
}
extern "C" bool pc_network_submit(PcNetworkSession* s,PcNetworkFact d){
    if(!s||!pc_network_fact_valid(d)||pc_permanent_server_authoritative(d.key))return false;std::lock_guard<std::mutex> lock(s->mutex);
    if((s->status.status!=PC_NETWORK_CONNECTED&&s->status.status!=PC_NETWORK_RECONNECTING)||s->outgoing.size()>=PC_NETWORK_QUEUE||s->next_sequence==UINT64_MAX)return false;
    s->outgoing.push_back({d,s->next_sequence++,false});return true;
}
extern "C" bool pc_network_submit_reward(PcNetworkSession* s,PcNetworkReward claim){
    if(!s||!pc_network_reward_valid(claim))return false;std::lock_guard<std::mutex> lock(s->mutex);
    if((s->status.status!=PC_NETWORK_CONNECTED&&s->status.status!=PC_NETWORK_RECONNECTING)||s->outgoing.size()>=PC_NETWORK_QUEUE||s->next_sequence==UINT64_MAX)return false;
    s->outgoing.push_back({{claim.key,claim.value},s->next_sequence++,false,claim});return true;
}
extern "C" bool pc_network_poll(PcNetworkSession* s,PcNetworkUpdate* out){if(!s||!out)return false;std::lock_guard<std::mutex> lock(s->mutex);
    if(s->incoming.empty())return false;*out=s->incoming.front();s->incoming.pop_front();return true;}
extern "C" void pc_network_status(PcNetworkSession* s,PcNetworkSnapshot* out){if(!out)return;if(!s){*out={};return;}
    std::lock_guard<std::mutex> lock(s->mutex);*out=s->status;out->pending_outgoing=unsigned(s->outgoing.size());out->pending_incoming=unsigned(s->incoming.size());}
extern "C" bool pc_network_copy_state(PcNetworkSession* s,PcNetworkFacts* facts,PcNetworkReward* claims,
    unsigned capacity,unsigned* count,PcNetworkSnapshot* snapshot){
    if(!s||!facts||!count)return false;std::lock_guard<std::mutex> lock(s->mutex);
    if(snapshot){*snapshot=s->status;snapshot->pending_outgoing=unsigned(s->outgoing.size());snapshot->pending_incoming=unsigned(s->incoming.size());}
    *count=unsigned(s->rewards.size());
    if(s->status.status!=PC_NETWORK_CONNECTED||s->staged.active||capacity<*count||(*count&&!claims))return false;
    *facts=s->canonical;unsigned i=0;for(const auto& entry:s->rewards)claims[i++]=entry.second;return true;
}
extern "C" int pc_network_reward_known(PcNetworkSession* s,uint64_t id,PcNetworkReward* out){
    if(!s||!pc_permanent_producer_valid(id))return -1;std::lock_guard<std::mutex> lock(s->mutex);
    if(s->status.status!=PC_NETWORK_CONNECTED||s->staged.active)return -1;
    const auto at=s->rewards.find(id);if(at==s->rewards.end())return 0;if(out)*out=at->second;return 1;
}

extern "C" bool pc_network_pose_valid(const PcNetworkPose* pose){return pose&&pose_valid(*pose);}
extern "C" bool pc_network_publish_pose(PcNetworkSession* session,const PcNetworkPose* pose){
    if(!session||!pose||!pose_valid(*pose))return false;
    std::lock_guard<std::mutex> lock(session->mutex);
    if(session->status.status!=PC_NETWORK_CONNECTED)return false;
    PcNetworkPose canonical{};if(pose->active)canonical=*pose;
    if(!context_equal(session->local_pose,canonical))session->clear_visible_poses();
    if(session->pose_pending)++session->pose_counters.coalesced;
    session->local_pose=canonical;session->local_pose_time=Clock::now();session->pose_pending=true;
    return true;
}
extern "C" bool pc_network_copy_presence(PcNetworkSession* session,PcNetworkPresenceSnapshot* out){
    if(!out)return false;*out={};if(!session)return false;
    std::lock_guard<std::mutex> lock(session->mutex);
    const auto now=Clock::now();session->expire_poses(now);
    *out=session->pose_counters;out->generation=session->status.generation;out->pending=session->pose_pending?1:0;
    if(session->status.status!=PC_NETWORK_CONNECTED)return false;
    if(!session->local_pose.active)return true;
    for(const auto& remote:session->remote_poses)if(scene_equal(session->local_pose,remote.value.pose)){
        auto& copy=out->peers[out->count++];copy=remote.value;
        copy.age_ms=unsigned(std::chrono::duration_cast<std::chrono::milliseconds>(now-remote.received).count());
    }
    return true;
}
