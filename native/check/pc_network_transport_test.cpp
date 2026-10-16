// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted loopback/replay cases from BlueWake tests/network_session_test.cpp.
#include "pc/network/network_session.h"
#include "network_wire.h"
#include "network_store.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <array>
#include <thread>
using namespace std::chrono_literals;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using Session=std::unique_ptr<PcNetworkSession,decltype(&pc_network_destroy)>;
static Session session(){return Session(pc_network_create(),pc_network_destroy);}
static bool until(const std::function<bool()>& predicate,int milliseconds=7000){
    const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
    do { if(predicate())return true; std::this_thread::sleep_for(5ms); } while(std::chrono::steady_clock::now()<end);
    return predicate();
}
static PcNetworkSnapshot status(PcNetworkSession* s){PcNetworkSnapshot out{};pc_network_status(s,&out);return out;}
static PcNetworkConfig config(unsigned port,char player,bool create=false){
    PcNetworkConfig c{};pc_network_native_compatibility(&c.compatibility);std::strcpy(c.server,"127.0.0.1");
    c.port=uint16_t(port);std::strcpy(c.room,"native");std::memset(c.player_id,player,32);
    std::snprintf(c.player_name,sizeof c.player_name,"Link%c",player);std::strcpy(c.room_password,"fixture-only");c.create_room=create;return c;
}
static void connected(PcNetworkSession* s){CHECK(until([&]{return status(s).status==PC_NETWORK_CONNECTED;}));}
static unsigned drain(PcNetworkSession* s,bool snapshot){unsigned count=0;PcNetworkUpdate update{};
    while(pc_network_poll(s,&update)){CHECK(update.delta.key==1&&update.delta.value==1);CHECK(update.snapshot==snapshot);++count;}return count;}
static pc_net::Bytes hello(const PcNetworkConfig& c){pc_net::Bytes b;pc_net::identity(b,c.compatibility);pc_net::string(b,c.room);
    pc_net::string(b,c.player_id);pc_net::string(b,c.player_name);pc_net::string(b,c.room_password);b.push_back(c.create_room?1:0);return b;}
static pc_net::Frame receive(pc_net::Pipe& pipe,pc_net::Type type){pc_net::Frame found{};
    CHECK(until([&]{CHECK(pipe.pump());for(;;){pc_net::Frame frame;int result=pipe.next(frame);CHECK(result>=0);if(!result)return false;
        if(frame.type==type){found=std::move(frame);return true;}}}));return found;}
static void send_rejected_frame(pc_net::Pipe& pipe,pc_net::Type type,const pc_net::Bytes& body){
    CHECK(pipe.queue(type,0,body));bool transportClosed=false;
    CHECK(until([&]{
        if(!pipe.pump())transportClosed=true;
        return pipe.output.empty()||transportClosed||pipe.eof;
    }));
    // pump() sends first, then receives. Winsock may report the expected reset
    // in that SAME call after every byte was sent. A partial send still fails;
    // never clear/reset the pipe before checking its undelivered output.
    CHECK(pipe.output.empty());
    // Rejection must beat the client's five-second handshake timeout.
    CHECK(transportClosed||until([&]{return !pipe.pump()||pipe.eof;},2000));
}
static void stored_history(const std::filesystem::path& path,const PcNetworkConfig& config){
    // Called only after our server/client workers have stopped. Reading an
    // acknowledged file during replacement could itself interfere on Windows.
    pc_net::RoomStore store;std::string error;pc_net::StoredRoom room;bool found=false;
    if(!store.open(path.u8string().c_str(),error)||!store.load(config.compatibility,config.room,room,found,error)||!found){
        std::fprintf(stderr,"retained store diagnostic: %s found=%u\n",error.c_str(),unsigned(found));return;}
    std::fprintf(stderr,"retained store revision=%llu facts=%u\n",(unsigned long long)room.revision,room.progress.values[1]);
    for(const auto& item:room.clients)std::fprintf(stderr,"retained client=%s sequence=%llu receipts=%zu first=%llu last=%llu\n",
        item.first.c_str(),(unsigned long long)item.second.sequence,item.second.recent.size(),
        (unsigned long long)(item.second.recent.empty()?0:item.second.recent.front().sequence),
        (unsigned long long)(item.second.recent.empty()?0:item.second.recent.back().sequence));
}
static void replay_and_rejection(unsigned port){using namespace pc_net;auto c=config(port,'d',true);std::strcpy(c.room,"replay");
    Pipe pipe;pipe.socket=connect(c.server,c.port);CHECK(pipe.socket!=invalid);CHECK(pipe.queue(Hello,0,hello(c)));receive(pipe,Welcome);
    Bytes body;delta(body,{1,1});CHECK(pipe.queue(Delta,1,body));auto first=receive(pipe,Commit);
    CHECK(pipe.queue(Delta,1,body));auto replay=receive(pipe,Commit);CHECK(first.body==replay.body);
    body.clear();delta(body,{1,0});CHECK(pipe.queue(Delta,1,body));receive(pipe,Reject);
    Pipe gap;gap.socket=connect(c.server,c.port);CHECK(gap.socket!=invalid);CHECK(gap.queue(Hello,0,hello(c)));receive(gap,Welcome);
    body.clear();delta(body,{1,1});CHECK(gap.queue(Delta,3,body));receive(gap,Reject);
    Pipe legacy;legacy.socket=connect(c.server,c.port);CHECK(legacy.socket!=invalid);
    auto old=frame(Hello,0,hello(c));std::memcpy(old.data(),"BWPN",4);legacy.output.push_back(old);receive(legacy,Reject);
    Pipe incompatible;incompatible.socket=connect(c.server,c.port);CHECK(incompatible.socket!=invalid);
    c.compatibility.rules_digest[0]^=1;CHECK(incompatible.queue(Hello,0,hello(c)));receive(incompatible,Reject);
    Pipe oversized;oversized.socket=connect("127.0.0.1",uint16_t(port));CHECK(oversized.socket!=invalid);
    oversized.output.push_back(frame(Hello,0,Bytes(max_body+1,0)));receive(oversized,Reject);
}
static unsigned discard(PcNetworkSession* s){PcNetworkUpdate update{};unsigned n=0;while(pc_network_poll(s,&update)){
    CHECK(pc_network_fact_valid(update.delta));if(update.reward.reward_id)CHECK(pc_network_reward_valid(update.reward));++n;}return n;}
static void operations_and_ledger(){
    auto server=pc_network_server_start("127.0.0.1",0);CHECK(server);const auto port=pc_network_server_port(server);
    auto host=session(),guest=session();auto a=config(port,'a',true),b=config(port,'b');std::strcpy(a.room,"operations");std::strcpy(b.room,"operations");
    CHECK(pc_network_join(host.get(),&a));connected(host.get());CHECK(pc_network_join(guest.get(),&b));connected(guest.get());
    CHECK(!pc_network_submit(host.get(),{48,1})&&!pc_network_submit(host.get(),{50,99})&&!pc_network_submit(host.get(),{51,60}));
    const PcNetworkReward first{UINT64_C(3)<<56|1,50,PC_PERMANENT_STEP,1};
    CHECK(pc_network_reward_known(guest.get(),first.reward_id,nullptr)==0);
    CHECK(pc_network_submit_reward(host.get(),first));CHECK(until([&]{return status(host.get()).pending_outgoing==0&&status(guest.get()).server_revision==1;}));
    PcNetworkFacts facts{};std::array<PcNetworkReward,8> claims{};unsigned count=0;PcNetworkSnapshot snapshot{};
    CHECK(pc_network_copy_state(guest.get(),&facts,claims.data(),unsigned(claims.size()),&count,&snapshot));
    CHECK(count==1&&facts.values[50]==60&&snapshot.server_revision==1&&pc_net::same_reward(claims[0],first));
    PcNetworkFacts unchanged{};unchanged.values[0]=123;
    CHECK(!pc_network_copy_state(guest.get(),&unchanged,nullptr,0,&count,&snapshot)&&count==1&&unchanged.values[0]==123);
    CHECK(pc_network_submit_reward(guest.get(),first));CHECK(until([&]{return status(guest.get()).pending_outgoing==0;}));
    CHECK(status(guest.get()).server_revision==1&&pc_network_reward_known(guest.get(),first.reward_id,&claims[0])==1);
    CHECK(pc_network_submit_reward(guest.get(),{UINT64_C(3)<<56|2,50,PC_PERMANENT_STEP,1}));
    CHECK(until([&]{return status(host.get()).server_revision==2&&status(guest.get()).pending_outgoing==0;}));
    CHECK(pc_network_copy_state(host.get(),&facts,claims.data(),unsigned(claims.size()),&count,&snapshot)&&count==2&&facts.values[50]==99);
    CHECK(pc_network_submit_reward(host.get(),first));CHECK(until([&]{return status(host.get()).pending_outgoing==0;}));CHECK(status(host.get()).server_revision==2);
    CHECK(pc_network_submit_reward(host.get(),{UINT64_C(2)<<56|1,9,PC_PERMANENT_MAX,1}));
    CHECK(pc_network_submit_reward(host.get(),{UINT64_C(2)<<56|2,9,PC_PERMANENT_MAX,2}));
    CHECK(until([&]{return status(guest.get()).server_revision==4&&status(host.get()).pending_outgoing==0;}));
    CHECK(pc_network_copy_state(guest.get(),&facts,claims.data(),unsigned(claims.size()),&count,&snapshot)&&count==4&&facts.values[9]==2);
    // Same authored producer with another valid payload is explicitly refused.
    CHECK(pc_network_submit_reward(guest.get(),{UINT64_C(2)<<56|1,9,PC_PERMANENT_MAX,2}));
    CHECK(until([&]{return status(guest.get()).status==PC_NETWORK_REJECTED;}));CHECK(status(host.get()).server_revision==4);
    pc_network_leave(guest.get());pc_network_leave(host.get());pc_network_server_stop(server);
}
static pc_net::Socket fixture_listener(uint16_t& port){using namespace pc_net;
    Socket listener=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);CHECK(listener!=invalid&&nonblocking(listener));
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    CHECK(::bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof address)==0&&listen(listener,1)==0);
#ifdef _WIN32
    int length=sizeof address;
#else
    socklen_t length=sizeof address;
#endif
    CHECK(getsockname(listener,reinterpret_cast<sockaddr*>(&address),&length)==0);
    port=ntohs(address.sin_port);return listener;
}
static void malformed_paged_snapshot(){using namespace pc_net;
    // Exercise the real client decoder: exhausted revisions and an impossible
    // empty/nonempty canonical state must fail before any Welcome publication.
    for(unsigned fixture=0;fixture<3;++fixture){
        // Rejection triggers automatic reconnect. Give each case a listener of
        // its own so a stopped case's queued reconnect cannot impersonate the
        // next case on a shared accept backlog.
        uint16_t port=0;const Socket listener=fixture_listener(port);
        auto invalidClient=session();auto invalidConfig=config(port,char('b'+fixture));CHECK(pc_network_join(invalidClient.get(),&invalidConfig));
        Pipe invalidPipe;CHECK(until([&]{invalidPipe.socket=::accept(listener,nullptr,nullptr);return invalidPipe.socket!=invalid;}));
        CHECK(nonblocking(invalidPipe.socket));const auto greeting=receive(invalidPipe,Hello);
        CHECK(greeting.sequence==0&&greeting.body==hello(invalidConfig));
        const uint64_t revision=fixture==0?UINT64_MAX:(fixture==1?0:1);const unsigned keys=fixture==2?0:1;
        Bytes invalidWelcome;identity(invalidWelcome,invalidConfig.compatibility);string(invalidWelcome,std::string(32,'a').c_str());
        put64(invalidWelcome,revision);put64(invalidWelcome,0);put16(invalidWelcome,uint16_t(keys));if(keys)delta(invalidWelcome,{1,1});put16(invalidWelcome,0);
        send_rejected_frame(invalidPipe,Welcome,invalidWelcome);
        CHECK(status(invalidClient.get()).generation==0&&status(invalidClient.get()).status!=PC_NETWORK_CONNECTED);
        PcNetworkUpdate unpublished{};CHECK(!pc_network_poll(invalidClient.get(),&unpublished));pc_network_leave(invalidClient.get());close(listener);
    }
    uint16_t port=0;const Socket listener=fixture_listener(port);
    auto client=session();auto c=config(port,'a');CHECK(pc_network_join(client.get(),&c));
    Pipe pipe;CHECK(until([&]{pipe.socket=::accept(listener,nullptr,nullptr);return pipe.socket!=invalid;}));CHECK(nonblocking(pipe.socket));
    const auto greeting=receive(pipe,Hello);CHECK(greeting.sequence==0&&greeting.body==hello(c));
    Bytes b;identity(b,c.compatibility);string(b,std::string(32,'a').c_str());put64(b,1);put64(b,0);put16(b,1);delta(b,{1,1});put16(b,1);
    CHECK(pipe.queue(Welcome,0,b));CHECK(until([&]{return pipe.pump()&&pipe.output.empty();}));
    // A ping proves the worker received and waited on the incomplete snapshot.
    // Facts must remain private even though Welcome itself contains a tool.
    receive(pipe,Ping);PcNetworkFacts facts{};facts.values[0]=123;PcNetworkSnapshot s{};unsigned count=0;PcNetworkReward out{};
    CHECK(!pc_network_copy_state(client.get(),&facts,&out,1,&count,&s)&&facts.values[0]==123&&s.status!=PC_NETWORK_CONNECTED&&s.generation==0);
    b.clear();put64(b,1);put16(b,1);put16(b,1);put16(b,1);reward(b,{UINT64_C(2)<<56|1,1,PC_PERMANENT_MAX,1});
    send_rejected_frame(pipe,Snapshot,b);
    CHECK(status(client.get()).generation==0&&status(client.get()).status!=PC_NETWORK_CONNECTED);
    PcNetworkUpdate update{};CHECK(!pc_network_poll(client.get(),&update));pc_network_leave(client.get());close(listener);
}
static void eight_peers_and_paged_bound(){
    auto server=pc_network_server_start("127.0.0.1",0);CHECK(server);const auto port=pc_network_server_port(server);
    std::vector<Session> peers;std::vector<PcNetworkConfig> configs;
    const char peerIds[]="abcdef01";
    static_assert(sizeof peerIds-1==PC_NETWORK_PEERS,"One valid hexadecimal fixture identity per peer");
    for(unsigned i=0;i<PC_NETWORK_PEERS;++i){peers.push_back(session());auto c=config(port,peerIds[i],i==0);std::strcpy(c.room,"bounded");configs.push_back(c);
        CHECK(pc_network_join(peers.back().get(),&configs.back()));connected(peers.back().get());}
    CHECK(until([&]{return status(peers[0].get()).peer_count==PC_NETWORK_PEERS;}));
    for(unsigned base=0;base<PC_NETWORK_REWARDS;base+=64){
        for(unsigned i=base;i<base+64;++i)CHECK(pc_network_submit_reward(peers[i%PC_NETWORK_PEERS].get(),{UINT64_C(2)<<56|(i+1),1,PC_PERMANENT_MAX,1}));
        CHECK(until([&]{bool complete=true;for(auto& peer:peers){discard(peer.get());const auto s=status(peer.get());
            if(s.status!=PC_NETWORK_CONNECTED||s.server_revision!=base+64||s.pending_outgoing)complete=false;}return complete;}));
    }
    std::vector<PcNetworkReward> claims(PC_NETWORK_REWARDS);PcNetworkFacts facts{};PcNetworkSnapshot snapshot{};unsigned count=0;
    CHECK(pc_network_copy_state(peers[0].get(),&facts,claims.data(),unsigned(claims.size()),&count,&snapshot));
    CHECK(count==PC_NETWORK_REWARDS&&facts.values[1]==1&&snapshot.server_revision==PC_NETWORK_REWARDS);
    const auto first=claims.front(),last=claims.back();CHECK(first.reward_id==(UINT64_C(2)<<56|1)&&last.reward_id==(UINT64_C(2)<<56|PC_NETWORK_REWARDS));
    // Full state spans more than64KiB of pages. Rejoin publishes all claims
    // atomically together with one canonical fact and one generation.
    pc_network_leave(peers.back().get());CHECK(until([&]{return status(peers[0].get()).peer_count==PC_NETWORK_PEERS-1;}));
    CHECK(pc_network_join(peers.back().get(),&configs.back()));connected(peers.back().get());
    CHECK(pc_network_copy_state(peers.back().get(),&facts,claims.data(),unsigned(claims.size()),&count,&snapshot)&&count==PC_NETWORK_REWARDS);
    CHECK(snapshot.generation==1&&snapshot.server_revision==PC_NETWORK_REWARDS&&pc_net::same_reward(claims.front(),first)&&pc_net::same_reward(claims.back(),last));
    CHECK(pc_network_submit_reward(peers.back().get(),first));CHECK(until([&]{return status(peers.back().get()).pending_outgoing==0;}));
    CHECK(status(peers.back().get()).server_revision==PC_NETWORK_REWARDS); // exact duplicate still allowed at capacity
    CHECK(pc_network_submit_reward(peers.back().get(),{UINT64_C(2)<<56|(PC_NETWORK_REWARDS+1),1,PC_PERMANENT_MAX,1}));
    CHECK(until([&]{return status(peers.back().get()).status==PC_NETWORK_REJECTED;}));
    for(unsigned i=0;i<PC_NETWORK_PEERS-1;++i)CHECK(status(peers[i].get()).status==PC_NETWORK_CONNECTED);
    for(auto& peer:peers)pc_network_leave(peer.get());pc_network_server_stop(server);
}
int main(){
    const auto dir=std::filesystem::temp_directory_path()/("BlueWake-native-transport-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(!std::filesystem::exists(dir));char error[128]{};
    auto server=pc_network_server_start_persistent("127.0.0.1",0,dir.u8string().c_str(),error,sizeof error);CHECK(server);
    const auto port=pc_network_server_port(server);auto host=session(),guest=session();CHECK(host&&guest);
    auto a=config(port,'a',true),b=config(port,'b');CHECK(!pc_network_submit(host.get(),{1,1}));
    CHECK(pc_network_join(host.get(),&a));connected(host.get());CHECK(pc_network_join(guest.get(),&b));connected(guest.get());
    CHECK(until([&]{return status(host.get()).peer_count==2&&status(guest.get()).peer_count==2;}));
    CHECK(drain(host.get(),true)==0&&drain(guest.get(),true)==0);CHECK(!pc_network_submit(host.get(),{0,1}));
    CHECK(pc_network_submit(host.get(),{1,1}));CHECK(until([&]{return status(guest.get()).pending_incoming==1&&status(host.get()).pending_outgoing==0;}));
    CHECK(drain(guest.get(),false)==1&&drain(host.get(),false)==1);CHECK(status(host.get()).server_revision==1);
    CHECK(pc_network_submit(host.get(),{1,1}));CHECK(until([&]{return status(host.get()).pending_outgoing==0;}));
    CHECK(drain(guest.get(),false)==0&&status(guest.get()).server_revision==1);
    pc_network_leave(guest.get());CHECK(pc_network_join(guest.get(),&b));connected(guest.get());CHECK(drain(guest.get(),true)==1);
    auto bad=session();auto wrong=config(port,'c');std::strcpy(wrong.room_password,"incorrect");CHECK(pc_network_join(bad.get(),&wrong));
    CHECK(until([&]{return status(bad.get()).status==PC_NETWORK_REJECTED;}));
    wrong=config(port,'c',true);std::strcpy(wrong.room,"separate");CHECK(pc_network_join(bad.get(),&wrong));connected(bad.get());CHECK(drain(bad.get(),true)==0);
    pc_network_leave(bad.get());pc_network_leave(guest.get());replay_and_rejection(port);
    const auto before=status(host.get());pc_network_server_stop(server);
    CHECK(until([&]{return status(host.get()).status==PC_NETWORK_RECONNECTING;}));
    for(unsigned i=0;i<PC_NETWORK_QUEUE;++i)CHECK(pc_network_submit(host.get(),{1,1}));
    CHECK(!pc_network_submit(host.get(),{1,1}));
    server=pc_network_server_start_persistent("127.0.0.1",port,dir.u8string().c_str(),error,sizeof error);CHECK(server);
    CHECK(pc_network_join(guest.get(),&b));connected(guest.get());CHECK(drain(guest.get(),true)==1);
    const auto guestBefore=status(guest.get());
    // Windows durable flushes measured ~185 ms each in the preserved first
    // stress run. Keep the complete 128-receipt contract with a finite60s
    // completion budget, require ACK progress within8s after Welcome, and
    // assert another peer stays connected throughout the backlog.
    const auto began=std::chrono::steady_clock::now();auto lastProgress=began;
    unsigned lastPending=PC_NETWORK_QUEUE+1;uint64_t lastGeneration=0;int lastState=-1;
    bool complete=false;const char* failure="completion budget exhausted";
    while(std::chrono::steady_clock::now()-began<60s){
        const auto current=status(host.get()),other=status(guest.get());const auto now=std::chrono::steady_clock::now();
        if(current.pending_outgoing!=lastPending||current.generation!=lastGeneration||int(current.status)!=lastState){
            std::printf("reconnect progress ms=%lld status=%d generation=%llu pending=%u reconnects=%llu message=%s\n",
                (long long)std::chrono::duration_cast<std::chrono::milliseconds>(now-began).count(),int(current.status),
                (unsigned long long)current.generation,current.pending_outgoing,(unsigned long long)current.reconnects,current.message);std::fflush(stdout);
            if(current.pending_outgoing<lastPending||current.generation!=lastGeneration)lastProgress=now;
            lastPending=current.pending_outgoing;lastGeneration=current.generation;lastState=int(current.status);
        }
        if(other.status!=PC_NETWORK_CONNECTED||other.reconnects!=guestBefore.reconnects||other.generation!=guestBefore.generation){failure="other peer disconnected during durable backlog";break;}
        if(current.reconnects>before.reconnects+1){failure="extra reconnect during durable backlog";break;}
        if(current.status==PC_NETWORK_CONNECTED&&current.generation>before.generation&&current.pending_outgoing==0){complete=true;break;}
        if(current.status==PC_NETWORK_CONNECTED&&now-lastProgress>8s){failure="no durable ACK progress within8s";break;}
        std::this_thread::sleep_for(5ms);
    }
    if(!complete){const auto last=status(host.get());std::fprintf(stderr,"reconnect failed: %s status=%d generation=%llu pending=%u reconnects=%llu message=%s\n",
        failure,int(last.status),(unsigned long long)last.generation,last.pending_outgoing,(unsigned long long)last.reconnects,last.message);
        pc_network_leave(host.get());pc_network_leave(guest.get());pc_network_server_stop(server);stored_history(dir,a);CHECK(complete);}
    const auto after=status(host.get());CHECK(!std::strcmp(before.room_identity,after.room_identity));
    CHECK(after.server_revision==1&&after.reconnects==before.reconnects+1&&drain(host.get(),true)==1);
    pc_network_leave(host.get());pc_network_leave(guest.get());pc_network_server_stop(server);
    operations_and_ledger();malformed_paged_snapshot();eight_peers_and_paged_bound();
    std::printf("native network transport/replay/durable-reconnect: PASS checks=%u\n",checks);
    return 0;
}
