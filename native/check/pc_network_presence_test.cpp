// SPDX-License-Identifier: GPL-3.0-or-later
// Real loopback copied-value transport checks; no game/render/CARD proof.
#include "pc/network/network_session.h"
#include "pc/network/pc_network_client_store.h"
#include "pc/pc_network_profile.h"
#include "network_presence.h"
#include "network_store.h"
#include "network_digest.h"
#include "pc_network_presence_v3_fixture.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <thread>
#include <utility>

using namespace pc_net;
using namespace std::chrono_literals;
namespace fs=std::filesystem;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using Session=std::unique_ptr<PcNetworkSession,decltype(&pc_network_destroy)>;
using Server=std::unique_ptr<PcNetworkServer,decltype(&pc_network_server_stop)>;
static Session session(){Session result(pc_network_create(),pc_network_destroy);CHECK(result);return result;}
static bool until(const std::function<bool()>& predicate,int milliseconds=7000,const std::function<void()>& tick={}){
    const auto end=Clock::now()+std::chrono::milliseconds(milliseconds);
    do{if(tick)tick();if(predicate())return true;std::this_thread::sleep_for(5ms);}while(Clock::now()<end);
    if(tick)tick();return predicate();
}
static PcNetworkSnapshot status(PcNetworkSession* s){PcNetworkSnapshot value{};pc_network_status(s,&value);return value;}
static PcNetworkPresenceSnapshot view(PcNetworkSession* s){PcNetworkPresenceSnapshot value{};CHECK(pc_network_copy_presence(s,&value));CHECK(value.count<=PC_NETWORK_PEERS&&value.pending<=1);return value;}
static PcNetworkConfig config(uint16_t port,char id,const char* room,bool create=false){
    PcNetworkConfig result{};pc_network_native_compatibility(&result.compatibility);
    std::strcpy(result.server,"127.0.0.1");result.port=port;std::strcpy(result.room,room);
    std::memset(result.player_id,id,32);std::snprintf(result.player_name,33,"Fixture%c",id);
    std::strcpy(result.room_password,"fixture-only");result.create_room=create;return result;
}
static void join(PcNetworkSession* s,const PcNetworkConfig& c){CHECK(pc_network_join(s,&c));CHECK(until([&]{return status(s).status==PC_NETWORK_CONNECTED;}));}
static PcNetworkPose pose(float x=1){
    PcNetworkPose value{};value.active=1;std::strcpy(value.stage,"sea");value.room=44;value.quest_generation=1;
    value.base_scale[0]=value.base_scale[1]=value.base_scale[2]=1;
    value.root[0]=value.root[5]=value.root[10]=1;value.root[3]=x;
    for(unsigned i=0;i<PC_NETWORK_POSE_JOINTS;++i){
        value.joints[i][0]=value.joints[i][5]=value.joints[i][10]=1;
        value.joints[i][3]=x+float(i);value.joints[i][7]=float(i)*0.5f;
        value.joint_scale[i]=uint8_t(i%2);
    }
    return value;
}
static Bytes pose_body(const PcNetworkPose& value){Bytes bytes;pose_write(bytes,value);return bytes;}
static Bytes hello(const PcNetworkConfig& c){Bytes body;identity(body,c.compatibility);string(body,c.room);string(body,c.player_id);
    string(body,c.player_name);string(body,c.room_password);body.push_back(c.create_room?1:0);return body;}
static Frame receive(Pipe& pipe,Type wanted){Frame result{};bool found=false;
    CHECK(until([&]{const bool live=pipe.pump();for(;;){Frame current{};const int read=pipe.next(current);CHECK(read>=0);if(!read)break;
        if(current.type==wanted){result=std::move(current);found=true;return true;}}
        CHECK(live&&!pipe.eof);return false;}));CHECK(found);return result;
}
static void send(Pipe& pipe,Type type,uint64_t sequence,const Bytes& body){CHECK(pipe.queue(type,sequence,body));
    CHECK(until([&]{const bool live=pipe.pump();CHECK(live||pipe.output.empty());return pipe.output.empty();}));}
static void raw_join(Pipe& pipe,const PcNetworkConfig& c){pipe.socket=connect(c.server,c.port);CHECK(pipe.socket!=invalid);
    send(pipe,Hello,0,hello(c));receive(pipe,Welcome);}
static const PcNetworkRemotePose* find(const PcNetworkPresenceSnapshot& snapshot,char id){
    for(unsigned i=0;i<snapshot.count;++i)if(snapshot.peers[i].player_id[0]==id)return &snapshot.peers[i];return nullptr;
}
static uint64_t epoch(PcNetworkSession* s,char id){const auto snapshot=status(s);
    for(unsigned i=0;i<snapshot.peer_count;++i)if(snapshot.peers[i].player_id[0]==id)return snapshot.peers[i].connection_epoch;return 0;
}
static Bytes read(const fs::path& path){std::ifstream input(path,std::ios::binary);CHECK(input);return Bytes(std::istreambuf_iterator<char>(input),{});}
static void write(const fs::path& path,const Bytes& bytes){CHECK(!fs::exists(path));std::ofstream output(path,std::ios::binary);
    CHECK(output);output.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));output.close();CHECK(output);}

static void codec_and_bounds(){
    static_assert(PC_NETWORK_POSE_JOINTS==42,"Native GZLE01 Link hierarchy");
    static_assert(PC_NETWORK_PROTOCOL==4&&PC_NETWORK_STORAGE_PROTOCOL==3&&PC_NETWORK_SCHEMA==2,"Intentional split");
    auto original=pose(32768.5f);CHECK(pc_network_pose_valid(&original));auto body=pose_body(original);
    CHECK(body.size()<2300&&body.size()+42<max_body);Reader reader{body};PcNetworkPose decoded{};
    CHECK(pose_read(reader,decoded)&&reader.done());CHECK(decoded.active==1&&!std::strcmp(decoded.stage,"sea")&&decoded.room==44);
    CHECK(decoded.quest_generation==1&&decoded.root[3]==32768.5f&&decoded.base_scale[2]==1);
    for(unsigned i=0;i<PC_NETWORK_POSE_JOINTS;++i){CHECK(decoded.joints[i][3]==original.joints[i][3]);CHECK(decoded.joint_scale[i]==i%2);}
    auto invalid=original;invalid.root[0]=std::numeric_limits<float>::quiet_NaN();CHECK(!pc_network_pose_valid(&invalid));
    invalid=original;invalid.joints[41][11]=std::numeric_limits<float>::infinity();CHECK(!pc_network_pose_valid(&invalid));
    invalid=original;invalid.root[1]=pose_basis_limit+1;CHECK(!pc_network_pose_valid(&invalid));
    invalid=original;invalid.root[3]=pose_world_limit+1;CHECK(!pc_network_pose_valid(&invalid));
    invalid=original;invalid.base_scale[1]=0;CHECK(!pc_network_pose_valid(&invalid));
    invalid=original;invalid.joint_scale[41]=2;CHECK(!pc_network_pose_valid(&invalid));
    invalid=original;invalid.room=64;CHECK(!pc_network_pose_valid(&invalid));
    invalid=original;invalid.layer=16;CHECK(!pc_network_pose_valid(&invalid));
    invalid=original;invalid.quest_generation=0;CHECK(!pc_network_pose_valid(&invalid));
    invalid=original;std::memset(invalid.stage,'a',sizeof invalid.stage);CHECK(!pc_network_pose_valid(&invalid));
    invalid=original;std::strcpy(invalid.stage,"../sea");CHECK(!pc_network_pose_valid(&invalid));
    invalid=original;invalid.active=2;CHECK(!pc_network_pose_valid(&invalid));
    for(unsigned mutation=0;mutation<4;++mutation){auto bytes=body;
        if(mutation==0)bytes[0]=2;
        if(mutation==1)bytes[2]=2; //unsupported matrix contract version
        if(mutation==2)bytes[4]=41; //wrong native joint count
        if(mutation==3)bytes.pop_back();
        Reader damaged{bytes};CHECK(!pose_read(damaged,decoded)||!damaged.done());
    }
    invalid.active=0;invalid.root[0]=std::numeric_limits<float>::quiet_NaN();invalid.room=65535;
    CHECK(pc_network_pose_valid(&invalid));body=pose_body(invalid);CHECK(body==Bytes{0});
    Reader hidden{body};CHECK(pose_read(hidden,decoded)&&hidden.done()&&!decoded.active&&!decoded.quest_generation&&decoded.root[0]==0);
    body.push_back(0);Reader trailing{body};CHECK(pose_read(trailing,decoded)&&!trailing.done());
    Pipe bounded;for(unsigned i=0;i<10000;++i)CHECK(bounded.queue_latest(0,Pose,i+1,pose_body(original)));
    CHECK(bounded.output.empty()&&bounded.latest_count()==1);
    CHECK(!bounded.queue_latest(PC_NETWORK_PEERS,Pose,1,{}));CHECK(!bounded.queue_latest(0,Delta,1,{}));
    CHECK(!bounded.queue_latest(0,Pose,1,Bytes(max_body+1,0)));
}

static void v3_reopen(const fs::path& base){
    // Literal arrays were serialized independently with protocol3 before this
    // proposal. No encode(new constant)->decode(new constant) tautology.
    CHECK(digest(std::string(reinterpret_cast<const char*>(v3_fixture::server),sizeof v3_fixture::server))==
        "3eaa45e87e430c0a4d29591361af84db4366150fbf40f271a9f7030c22a4d50a");
    CHECK(digest(std::string(reinterpret_cast<const char*>(v3_fixture::client),sizeof v3_fixture::client))==
        "ab2f327f3bfd4d2842582a39b7dd011c597cf90bbf01558e177db186b5362785");
    const auto folder=base/"v3";fs::create_directories(folder);const auto ini=folder/"network.ini";
    write(ini,Bytes(v3_fixture::ini,v3_fixture::ini+std::strlen(v3_fixture::ini)));
    PcNetworkProfile profile{};char error[256]{};
    CHECK(pc_network_profile_load(ini.u8string().c_str(),true,&profile,error,sizeof error));
    CHECK(profile.enabled&&!std::strcmp(profile.config.server,"127.0.0.1"));
    char selected[1100]{},identityText[65]{};const auto root=folder/"rooms",personal=folder/"personal";
    CHECK(pc_network_profile_route(&profile,root.u8string().c_str(),personal.u8string().c_str(),selected,sizeof selected,
        identityText,sizeof identityText,error,sizeof error));
    CHECK(std::strcmp(identityText,v3_fixture::route)==0);CHECK(fs::u8path(selected).parent_path()==root/v3_fixture::route);
    const auto host=folder/"server",journal=folder/"journal";fs::create_directories(host);fs::create_directories(journal);
    const auto roomFile=host/(std::string(v3_fixture::key)+".bwroom"),clientFile=journal/"client.bwjournal";
    write(roomFile,Bytes(std::begin(v3_fixture::server),std::end(v3_fixture::server)));
    write(clientFile,Bytes(std::begin(v3_fixture::client),std::end(v3_fixture::client)));
    const auto beforeRoom=read(roomFile),beforeClient=read(clientFile);std::string detail;
    {RoomStore store;CHECK(store.open(host.u8string().c_str(),detail));StoredRoom room;bool found=false;
        CHECK(store.load(profile.config.compatibility,"presence-compat",room,found,detail)&&found);
        CHECK(room.revision==1&&room.progress.values[48]==1&&room.rewards.size()==1);
        CHECK(room.clients.at(std::string(32,'a')).sequence==1&&room.rewards.begin()->first==(UINT64_C(3)<<56|1));
        CHECK(room_credential_matches(room,"fixture-only"));}
    auto store=std::unique_ptr<PcNetworkClientStore,decltype(&pc_network_client_close)>(
        pc_network_client_open(journal.u8string().c_str(),&profile.config.compatibility,v3_fixture::route,error,sizeof error),pc_network_client_close);
    CHECK(store);auto state=std::make_unique<PcNetworkClientState>();CHECK(pc_network_client_copy(store.get(),state.get()));
    CHECK(state->serial==3&&state->revision==1&&state->facts.values[48]==1&&state->reward_count==1&&state->outgoing_count==1);
    CHECK(state->outgoing[0].reward_id==(UINT64_C(3)<<56|2));PcNetworkClientProjection projected{};
    CHECK(pc_network_client_projection(store.get(),&projected,error,sizeof error)&&projected.provisional&&projected.facts.values[48]==2);
    CHECK(projected.revision==1&&state->outgoing_count==1);store.reset();
    CHECK(read(roomFile)==beforeRoom&&read(clientFile)==beforeClient);
    // Sockets deliberately reject the old wire even though old disk files stay valid.
    Pipe wire;wire.input=frame(Ping,0,{});wire.input[5]=3;Frame parsed{};CHECK(wire.next(parsed)==-1);
}

static void sessions_and_durable_fairness(const fs::path& folder){
    char error[256]{};Server server(pc_network_server_start_persistent("127.0.0.1",0,folder.u8string().c_str(),error,sizeof error),pc_network_server_stop);CHECK(server);
    const auto port=pc_network_server_port(server.get());auto a=session(),b=session(),isolated=session();
    const auto ca=config(port,'a',"presence",true),cb=config(port,'b',"presence"),cc=config(port,'c',"isolated",true);
    join(a.get(),ca);join(b.get(),cb);join(isolated.get(),cc);
    CHECK(until([&]{return status(a.get()).peer_count==2&&status(isolated.get()).peer_count==1;}));
    auto pa=pose(),pb=pose(200),pc=pose(300);CHECK(pc_network_publish_pose(b.get(),&pb));CHECK(pc_network_publish_pose(isolated.get(),&pc));
    const auto before=status(a.get());const auto poseBefore=view(a.get());
    const auto burstBegan=Clock::now();
    for(unsigned i=0;i<1000;++i){pa=pose(float(i));CHECK(pc_network_publish_pose(a.get(),&pa));}
    auto refresh=[&]{CHECK(pc_network_publish_pose(b.get(),&pb));CHECK(pc_network_publish_pose(isolated.get(),&pc));};
    CHECK(until([&]{const auto current=view(b.get());const auto* row=find(current,'a');return row&&row->pose.root[3]==999;},3000,refresh));
    const auto received=view(b.get());CHECK(received.count==1&&received.peers[0].connection_epoch==epoch(b.get(),'a'));
    CHECK(received.peers[0].pose.joints[41][3]==1040&&received.peers[0].pose.joint_scale[41]==1&&received.peers[0].age_ms<1000);
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-burstBegan).count();
    CHECK(view(a.get()).coalesced-poseBefore.coalesced>=900);
    CHECK(view(a.get()).sent-poseBefore.sent<=2+uint64_t(elapsed/PC_NETWORK_POSE_PERIOD_MS));
    CHECK(view(isolated.get()).count==0);const auto after=status(a.get());
    CHECK(after.server_revision==before.server_revision&&after.pending_outgoing==0&&after.pending_incoming==0&&after.sent==before.sent);
    PcNetworkFacts facts{};unsigned count=99;CHECK(pc_network_copy_state(a.get(),&facts,nullptr,0,&count,nullptr)&&count==0);

    std::strcpy(pb.stage,"Other");CHECK(pc_network_publish_pose(b.get(),&pb));CHECK(view(b.get()).count==0);
    std::strcpy(pb.stage,"sea");++pb.quest_generation;CHECK(pc_network_publish_pose(b.get(),&pb));CHECK(view(b.get()).count==0);
    pa=pose(333);pa.quest_generation=2;CHECK(pc_network_publish_pose(a.get(),&pa));
    CHECK(until([&]{const auto current=view(b.get());const auto* row=find(current,'a');return row&&row->pose.quest_generation==2;},3000,refresh));
    auto hidden=pa;hidden.active=0;hidden.root[0]=std::numeric_limits<float>::quiet_NaN();
    CHECK(pc_network_publish_pose(a.get(),&hidden));CHECK(until([&]{return view(b.get()).count==0;},3000,refresh));

    // Flood the API while real flushed reliable receipts are being accepted.
    // Pose coalescing must neither fill reliable queues nor cause reconnects.
    std::atomic<bool> running{true};std::thread publisher([&]{while(running){
        pc_network_publish_pose(a.get(),&pa);pc_network_publish_pose(b.get(),&pb);std::this_thread::sleep_for(1ms);}});
    const auto reconnectsA=status(a.get()).reconnects,reconnectsB=status(b.get()).reconnects;
    for(unsigned i=0;i<16;++i)CHECK(pc_network_submit(a.get(),{1,1}));
    CHECK(until([&]{const auto one=status(a.get()),two=status(b.get());
        CHECK(one.status==PC_NETWORK_CONNECTED&&two.status==PC_NETWORK_CONNECTED);
        CHECK(one.reconnects==reconnectsA&&two.reconnects==reconnectsB);CHECK(view(b.get()).pending<=1);
        return !one.pending_outgoing&&one.server_revision==1&&two.server_revision==1;},30000));
    CHECK(until([&]{return view(b.get()).count==1;},3000));running=false;publisher.join();
    CHECK(status(a.get()).sent-before.sent==16&&view(a.get()).coalesced>100);
    pc_network_leave(a.get());CHECK(until([&]{return status(b.get()).peer_count==1&&view(b.get()).count==0;}));
    PcNetworkPresenceSnapshot empty{};CHECK(!pc_network_copy_presence(a.get(),&empty)&&empty.count==0);
    pc_network_leave(b.get());pc_network_leave(isolated.get());server.reset();
    RoomStore reopened;std::string detail;CHECK(reopened.open(folder.u8string().c_str(),detail));StoredRoom room;bool found=false;
    CHECK(reopened.load(ca.compatibility,"presence",room,found,detail)&&found);
    CHECK(room.revision==1&&room.progress.values[1]==1&&room.rewards.empty());
    CHECK(room.clients.at(std::string(32,'a')).sequence==16&&room.clients.at(std::string(32,'b')).sequence==0);
}

static void rejected(Pipe& pipe,const Bytes& body,uint64_t sequence=1){
    if(body.size()>max_body)pipe.output.push_back(frame(Pose,sequence,body));
    else CHECK(pipe.queue(Pose,sequence,body));
    bool closed=false;
    CHECK(until([&]{if(!pipe.pump())closed=true;return pipe.output.empty()||closed||pipe.eof;}));
    CHECK(pipe.output.empty());
    CHECK(closed||until([&]{const bool live=pipe.pump();for(;;){Frame frame{};const int result=pipe.next(frame);CHECK(result>=0);
        if(!result)break;if(frame.type==Reject)return true;}return !live||pipe.eof;},2000));
}
static void raw_authority_stale_expiry(){
    Server server(pc_network_server_start("127.0.0.1",0),pc_network_server_stop);CHECK(server);const auto port=pc_network_server_port(server.get());
    auto receiver=session();const auto cb=config(port,'b',"raw",true);join(receiver.get(),cb);
    auto local=pose(200);auto refresh=[&]{CHECK(pc_network_publish_pose(receiver.get(),&local));};refresh();
    Pipe raw;const auto ca=config(port,'a',"raw");raw_join(raw,ca);
    CHECK(until([&]{return epoch(receiver.get(),'a')!=0;}));const auto firstEpoch=epoch(receiver.get(),'a');
    send(raw,Pose,7,pose_body(pose(7)));
    CHECK(until([&]{const auto current=view(receiver.get());const auto* row=find(current,'a');return row&&row->sequence==7;},3000,refresh));
    send(raw,Pose,6,pose_body(pose(600)));send(raw,Pose,7,pose_body(pose(700)));
    const auto deadline=Clock::now()+250ms;
    while(Clock::now()<deadline){refresh();const auto snapshot=view(receiver.get());const auto* row=find(snapshot,'a');
        CHECK(row&&row->sequence==7&&row->pose.root[3]==7);CHECK(raw.pump());std::this_thread::sleep_for(5ms);}
    send(raw,Pose,8,pose_body(pose(8)));CHECK(until([&]{const auto current=view(receiver.get());const auto* row=find(current,'a');return row&&row->sequence==8;},3000,refresh));
    CHECK(until([&]{return view(receiver.get()).count==0;},1800,refresh));CHECK(view(receiver.get()).expired>=1);
    send(raw,Pose,9,pose_body(pose(9)));CHECK(until([&]{return view(receiver.get()).count==1;},3000,refresh));
    raw.reset();CHECK(until([&]{return status(receiver.get()).peer_count==1&&view(receiver.get()).count==0;},3000,refresh));
    raw_join(raw,ca);CHECK(until([&]{return epoch(receiver.get(),'a')&&epoch(receiver.get(),'a')!=firstEpoch;}));
    send(raw,Pose,1,pose_body(pose(100)));CHECK(until([&]{const auto snapshot=view(receiver.get());const auto* row=find(snapshot,'a');
        return row&&row->sequence==1&&row->connection_epoch!=firstEpoch&&row->pose.root[3]==100;},3000,refresh));
    // Forwarded envelopes are server-only: a client cannot name another origin.
    Bytes spoof;string(spoof,cb.player_id);put64(spoof,epoch(receiver.get(),'b'));pose_write(spoof,pose(999));rejected(raw,spoof,2);
    raw.reset();CHECK(until([&]{return status(receiver.get()).peer_count==1&&view(receiver.get()).count==0;},3000,refresh));
    for(unsigned test=0;test<7;++test){Pipe malformed;auto c=config(port,"cdef012"[test],"raw");raw_join(malformed,c);Bytes body=pose_body(pose());uint64_t sequence=1;
        if(test==0)body[0]=2;
        if(test==1)body[4]=41;
        if(test==2){auto value=pose();value.joints[41][3]=std::numeric_limits<float>::quiet_NaN();body=pose_body(value);}
        if(test==3)body.push_back(0);
        if(test==4)sequence=0;
        if(test==5)sequence=UINT64_MAX;
        if(test==6)body.resize(max_body+1);
        rejected(malformed,body,sequence);malformed.reset();
        CHECK(until([&]{return status(receiver.get()).peer_count==1;},3000,refresh));CHECK(view(receiver.get()).count==0);
    }
    CHECK(status(receiver.get()).server_revision==0&&status(receiver.get()).pending_incoming==0);pc_network_leave(receiver.get());
}

static Socket listener(uint16_t& port){Socket socket=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);CHECK(socket!=invalid&&nonblocking(socket));
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    CHECK(::bind(socket,reinterpret_cast<sockaddr*>(&address),sizeof address)==0&&listen(socket,1)==0);
#ifdef _WIN32
    int size=sizeof address;
#else
    socklen_t size=sizeof address;
#endif
    CHECK(getsockname(socket,reinterpret_cast<sockaddr*>(&address),&size)==0);port=ntohs(address.sin_port);return socket;
}
static Bytes forwarded(const char* player,uint64_t epochValue,const PcNetworkPose& value){
    Bytes bytes;string(bytes,player);put64(bytes,epochValue);pose_write(bytes,value);return bytes;
}
static void late_join_replays_fresh_pose(){
    Server server(pc_network_server_start("127.0.0.1",0),pc_network_server_stop);CHECK(server);
    const auto port=pc_network_server_port(server.get());auto observer=session();
    join(observer.get(),config(port,'c',"late-join",true));
    const auto observing=pose(300);auto refreshObserver=[&]{CHECK(pc_network_publish_pose(observer.get(),&observing));};
    refreshObserver();Pipe origin;raw_join(origin,config(port,'a',"late-join"));
    CHECK(until([&]{return epoch(observer.get(),'a')!=0;}));
    // Publish exactly once. First prove it was relayed, then let the relay's
    //100ms period elapse while the source remains inside its1000ms lifetime.
    send(origin,Pose,7,pose_body(pose(707)));
    CHECK(until([&]{const auto current=view(observer.get());const auto* row=find(current,'a');
        return row&&row->sequence==7&&row->pose.root[3]==707;},3000,refreshObserver));
    const auto settled=Clock::now()+std::chrono::milliseconds(PC_NETWORK_POSE_PERIOD_MS+30);
    while(Clock::now()<settled){refreshObserver();CHECK(origin.pump());std::this_thread::sleep_for(5ms);}

    auto newcomer=session();join(newcomer.get(),config(port,'b',"late-join"));
    const auto local=pose(200);auto refreshNewcomer=[&]{CHECK(pc_network_publish_pose(newcomer.get(),&local));};
    CHECK(until([&]{const auto current=view(newcomer.get());const auto* row=find(current,'a');
        return row&&row->sequence==7&&row->pose.root[3]==707&&row->connection_epoch==epoch(newcomer.get(),'a');
    },600,refreshNewcomer));
    // Initial Welcome/roster must complete before the replay. No source
    // republication, durable sequence/ledger/revision, or new pose sequence.
    const auto snapshot=status(newcomer.get());
    CHECK(snapshot.status==PC_NETWORK_CONNECTED&&snapshot.generation==1&&snapshot.server_revision==0);
    CHECK(snapshot.pending_incoming==0&&snapshot.pending_outgoing==0&&snapshot.sent==0);
    CHECK(view(newcomer.get()).peers[0].age_ms<PC_NETWORK_POSE_EXPIRY_MS);
    pc_network_leave(newcomer.get());pc_network_leave(observer.get());
}

static void incomplete_welcome_no_publication(){
    uint16_t port=0;const Socket listenSocket=listener(port);auto client=session();const auto c=config(port,'c',"incomplete");
    CHECK(pc_network_join(client.get(),&c));Pipe server;
    CHECK(until([&]{server.socket=::accept(listenSocket,nullptr,nullptr);return server.socket!=invalid;}));
    CHECK(nonblocking(server.socket));pc_net::close(listenSocket);receive(server,Hello);
    Bytes welcome;identity(welcome,c.compatibility);string(welcome,std::string(32,'b').c_str());
    put64(welcome,1);put64(welcome,0);put16(welcome,1);delta(welcome,{1,1});put16(welcome,1); //ledger page still missing
    send(server,Welcome,0,welcome);
    send(server,Pose,1,forwarded(std::string(32,'a').c_str(),22,pose()));
    CHECK(until([&]{return !server.pump()||server.eof;},2000));
    CHECK(status(client.get()).generation==0&&status(client.get()).status!=PC_NETWORK_CONNECTED);
    PcNetworkFacts facts{};facts.values[0]=123;unsigned count=0;
    CHECK(!pc_network_copy_state(client.get(),&facts,nullptr,0,&count,nullptr)&&facts.values[0]==123);
    PcNetworkPresenceSnapshot poses{};CHECK(!pc_network_copy_presence(client.get(),&poses)&&poses.count==0);
    PcNetworkUpdate update{};CHECK(!pc_network_poll(client.get(),&update));pc_network_leave(client.get());
}
static void client_decoder_stale_and_clear(){
    uint16_t port=0;const Socket listenSocket=listener(port);auto client=session();const auto c=config(port,'b',"decoder");
    CHECK(pc_network_join(client.get(),&c));Pipe server;
    CHECK(until([&]{server.socket=::accept(listenSocket,nullptr,nullptr);return server.socket!=invalid;}));
    CHECK(nonblocking(server.socket));pc_net::close(listenSocket);const auto greeting=receive(server,Hello);CHECK(greeting.body==hello(c));
    Bytes welcome;identity(welcome,c.compatibility);string(welcome,std::string(32,'c').c_str());
    put64(welcome,0);put64(welcome,0);put16(welcome,0);put16(welcome,0);send(server,Welcome,0,welcome);
    Bytes roster;put16(roster,2);
    for(const auto entry:{std::make_pair('a',UINT64_C(22)),std::make_pair('b',UINT64_C(11))}){
        string(roster,std::string(32,entry.first).c_str());string(roster,"Fixture");roster.push_back(1);put64(roster,entry.second);
    }
    send(server,Roster,0,roster);CHECK(until([&]{return status(client.get()).status==PC_NETWORK_CONNECTED&&status(client.get()).peer_count==2;}));
    const auto local=pose(200);auto refresh=[&]{CHECK(pc_network_publish_pose(client.get(),&local));CHECK(server.pump());};refresh();
    const auto origin=std::string(32,'a');send(server,Pose,5,forwarded(origin.c_str(),22,pose(5)));
    CHECK(until([&]{const auto current=view(client.get());const auto* row=find(current,'a');return row&&row->sequence==5;},3000,refresh));
    send(server,Pose,4,forwarded(origin.c_str(),22,pose(4)));
    CHECK(until([&]{return view(client.get()).stale==1;},3000,refresh));
    CHECK(view(client.get()).peers[0].pose.root[3]==5);
    // Unknown roster identity and retired epochs cannot create or replace rows.
    send(server,Pose,99,forwarded(std::string(32,'d').c_str(),22,pose(99)));
    send(server,Pose,99,forwarded(origin.c_str(),21,pose(99)));
    auto hidden=pose();hidden.active=0;hidden.root[0]=std::numeric_limits<float>::quiet_NaN();
    send(server,Pose,6,forwarded(origin.c_str(),22,hidden));
    CHECK(until([&]{return view(client.get()).count==0;},3000,refresh));
    send(server,Pose,5,forwarded(origin.c_str(),22,pose(500)));
    CHECK(until([&]{return view(client.get()).stale==2;},3000,refresh));CHECK(view(client.get()).count==0);
    send(server,Pose,7,forwarded(origin.c_str(),22,pose(7)));
    CHECK(until([&]{return view(client.get()).count==1;},3000,refresh));
    // Replacing the server-assigned epoch retires the old sequence immediately.
    roster.clear();put16(roster,2);
    for(const auto entry:{std::make_pair('a',UINT64_C(23)),std::make_pair('b',UINT64_C(11))}){
        string(roster,std::string(32,entry.first).c_str());string(roster,"Fixture");roster.push_back(1);put64(roster,entry.second);
    }
    send(server,Roster,0,roster);CHECK(until([&]{return epoch(client.get(),'a')==23&&view(client.get()).count==0;},3000,refresh));
    send(server,Pose,1,forwarded(origin.c_str(),23,pose(1)));
    CHECK(until([&]{const auto current=view(client.get());const auto* row=find(current,'a');return row&&row->sequence==1&&row->connection_epoch==23;},3000,refresh));
    server.reset();CHECK(until([&]{return status(client.get()).status==PC_NETWORK_RECONNECTING;}));
    PcNetworkPresenceSnapshot cleared{};CHECK(!pc_network_copy_presence(client.get(),&cleared)&&cleared.count==0&&cleared.pending==0);
    CHECK(!pc_network_publish_pose(client.get(),&local));pc_network_leave(client.get());
}
static void socket_backpressure(){
    uint16_t port=0;const Socket listenSocket=listener(port);Pipe sender,receiver;sender.socket=connect("127.0.0.1",port);CHECK(sender.socket!=invalid);
    CHECK(until([&]{receiver.socket=::accept(listenSocket,nullptr,nullptr);return receiver.socket!=invalid;}));CHECK(nonblocking(receiver.socket));pc_net::close(listenSocket);
    const int small=4096;CHECK(setsockopt(sender.socket,SOL_SOCKET,SO_SNDBUF,reinterpret_cast<const char*>(&small),sizeof small)==0);
    CHECK(setsockopt(receiver.socket,SOL_SOCKET,SO_RCVBUF,reinterpret_cast<const char*>(&small),sizeof small)==0);
    bool blocked=false;for(unsigned i=0;i<10000&&!blocked;++i){CHECK(sender.queue_latest(0,Pose,i+1,pose_body(pose(float(i)))));CHECK(sender.pump());
        blocked=sender.latest_count()!=0;CHECK(sender.output.empty()&&sender.latest_count()<=2);}
    CHECK(blocked);
    // No reads on the other socket: replacing ten thousand poses retains only
    // one unsent latest value plus at most one already-partial bounded frame.
    for(unsigned i=0;i<10000;++i)CHECK(sender.queue_latest(0,Pose,i+20000,pose_body(pose(float(i)))));
    CHECK(sender.latest_count()<=2&&sender.output.empty());
    CHECK(sender.queue(Ping,4321,{}));CHECK(sender.queue_latest(0,Pose,40000,pose_body(pose(123456))));
    bool ping=false,finalPose=false;
    CHECK(until([&]{CHECK(sender.pump());CHECK(receiver.pump());for(;;){Frame frame{};const int result=receiver.next(frame);CHECK(result>=0);if(!result)break;
        CHECK(frame.body.size()<=max_body);if(frame.type==Ping){CHECK(frame.sequence==4321&&frame.body.empty());ping=true;}
        else{CHECK(frame.type==Pose);Reader reader{frame.body};PcNetworkPose value{};CHECK(pose_read(reader,value)&&reader.done());
            if(frame.sequence==40000){CHECK(value.root[3]==123456);finalPose=true;}}
        }return ping&&finalPose&&sender.output.empty()&&!sender.latest_count();},10000));
}

int main(){
    const auto base=fs::absolute(fs::temp_directory_path())/("BlueWake-presence-"+std::to_string(Clock::now().time_since_epoch().count()));
    CHECK(!fs::exists(base));fs::create_directories(base);std::printf("synthetic presence fixtures: %s\n",base.u8string().c_str());std::fflush(stdout);
    CHECK(init());codec_and_bounds();v3_reopen(base);sessions_and_durable_fairness(base/"durable");raw_authority_stale_expiry();late_join_replays_fresh_pose();incomplete_welcome_no_publication();client_decoder_stale_and_clear();socket_backpressure();
    std::printf("native transient presence transport: PASS checks=%u (no game/render qualification)\n",checks);return 0;
}
