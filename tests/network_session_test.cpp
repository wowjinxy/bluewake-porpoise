// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "network_session.h"
#include "network_wire.h"
#include "network_digest.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <thread>
using namespace std::chrono_literals;
using Session=std::unique_ptr<BwNetworkSession,decltype(&bw_network_destroy)>;
static Session session(){return Session(bw_network_create(),bw_network_destroy);}
static BwNetworkConfig config(unsigned port,char player,bool host=false){
    BwNetworkConfig c{};std::strcpy(c.server,"127.0.0.1");c.port=uint16_t(port);std::strcpy(c.room,"qualification");
    std::memset(c.player_id,player,32);std::snprintf(c.player_name,33,"Link%c",player);std::strcpy(c.room_password,"private-test");
    c.create_room=host;std::strcpy(c.compatibility.game_id,"GZLE01");c.compatibility.progression_schema=BW_PROGRESSION_SCHEMA;
    for(char* field:{c.compatibility.build_id,c.compatibility.module_digest,c.compatibility.options_digest})std::memset(field,'1',64);
    return c;
}
static bool until(const std::function<bool()>& predicate,int milliseconds=5000){const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
    do{if(predicate())return true;std::this_thread::sleep_for(5ms);}while(std::chrono::steady_clock::now()<end);return predicate();}
static BwNetworkSnapshot status(BwNetworkSession* s){BwNetworkSnapshot out{};bw_network_status(s,&out);return out;}
static void connected(BwNetworkSession* s){assert(until([&]{return status(s).status==BW_NETWORK_CONNECTED;}));}
static void drain(BwNetworkSession* s,BwProgressionState& state,unsigned* live=nullptr,unsigned* snapshots=nullptr){BwNetworkUpdate u;
    while(bw_network_poll(s,&u)){assert(bw_progression_valid(u.delta));bw_progression_merge(&state,u.delta);if(u.snapshot){if(snapshots)++*snapshots;}else if(live)++*live;}}
static void prefs_test(){
    assert(bw_net::digest("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    assert(bw_net::digest("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const auto dir=std::filesystem::temp_directory_path()/std::filesystem::path("BlueWake-network-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);const auto path=dir.u8string();char error[128],route[1024],other[1024];
    BwNetworkPreferences p{};assert(bw_network_preferences_load(path.c_str(),&p,error,sizeof error));assert(!p.room_mode);
    assert(!std::filesystem::exists(dir/"network.ini"));p.room_mode=true;p.config=config(49383,'a',true);
    std::ofstream(dir/"GZLE01.card",std::ios::binary)<<"personal untouched";
    assert(bw_network_preferences_save(path.c_str(),&p,error,sizeof error));BwNetworkPreferences read{};
    assert(bw_network_preferences_load(path.c_str(),&read,error,sizeof error));assert(read.room_mode&&bw_network_same_room(&p.config,&read.config));
    assert(bw_network_room_card_path(path.c_str(),&p.config,route,sizeof route,error,sizeof error));
    assert(!std::filesystem::exists(std::filesystem::u8path(route)));assert(std::strlen(route)<260);
    auto changed=p.config;changed.player_id[0]='b';assert(bw_network_room_card_path(path.c_str(),&changed,other,sizeof other,error,sizeof error));assert(std::strcmp(route,other));
    changed=p.config;changed.compatibility.options_digest[0]='2';assert(bw_network_room_card_path(path.c_str(),&changed,other,sizeof other,error,sizeof error));assert(std::strcmp(route,other));
    changed=p.config;std::strcpy(changed.room,"another");assert(bw_network_room_card_path(path.c_str(),&changed,other,sizeof other,error,sizeof error));assert(std::strcmp(route,other));
    changed=p.config;std::strcpy(changed.server,"localhost");assert(bw_network_room_card_path(path.c_str(),&changed,other,sizeof other,error,sizeof error));assert(std::strcmp(route,other));
    changed=p.config;std::strcpy(changed.room,"../personal");assert(!bw_network_room_card_path(path.c_str(),&changed,other,sizeof other,error,sizeof error));
    changed=p.config;std::memset(changed.server,'x',sizeof changed.server);assert(!bw_network_config_valid(&changed,error,sizeof error));
    assert(!bw_network_room_card_path(path.c_str(),&p.config,other,4,error,sizeof error));
    std::ofstream(dir/"network.ini",std::ios::app)<<"room_mode=0\n";assert(!bw_network_preferences_load(path.c_str(),&read,error,sizeof error));
    std::ifstream personal(dir/"GZLE01.card",std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(personal)),{});assert(bytes=="personal untouched");personal.close();
    std::filesystem::remove_all(dir);
}
static bw_net::Bytes hello(const BwNetworkConfig& c){bw_net::Bytes b;bw_net::identity(b,c.compatibility);bw_net::string(b,c.room);bw_net::string(b,c.player_id);
    bw_net::string(b,c.player_name);bw_net::string(b,c.room_password);b.push_back(c.create_room?1:0);return b;}
static bw_net::Frame receive(bw_net::Pipe& p,bw_net::Type type){bw_net::Frame found{};assert(until([&]{assert(p.pump());
    for(;;){bw_net::Frame f;int n=p.next(f);assert(n>=0);if(!n)return false;if(f.type==type){found=std::move(f);return true;}}}));return found;}
static void replay_test(unsigned port){
    using namespace bw_net;auto c=config(port,'d',true);std::strcpy(c.room,"replay");Pipe pipe;pipe.socket=bw_net::connect(c.server,c.port);assert(pipe.socket!=invalid);
    assert(pipe.queue(Hello,0,hello(c)));receive(pipe,Welcome);Bytes b;delta(b,{2,0x22});assert(pipe.queue(Delta,1,b));auto first=receive(pipe,Commit);
    Reader r{first.body};char origin[33];assert(r.text(origin,33));assert(r.qword()==1&&r.qword()==1);assert(delta(r).value==0x22&&r.done());
    assert(pipe.queue(Delta,1,b));auto second=receive(pipe,Commit);assert(first.body==second.body);
    b.clear();delta(b,{3,0x25});assert(pipe.queue(Delta,1,b));receive(pipe,Reject); // same sequence with a changed payload never applies
    Pipe reconnect;reconnect.socket=bw_net::connect(c.server,c.port);assert(reconnect.socket!=invalid);assert(reconnect.queue(Hello,0,hello(c)));
    auto welcome=receive(reconnect,Welcome);Reader wr{welcome.body};BwNetworkCompatibility identity{};assert(bw_net::identity(wr,identity));char room[33];assert(wr.text(room,33));
    assert(wr.qword()==1&&wr.qword()==1&&wr.word()==1);auto d=delta(wr);assert(d.key==2&&d.value==0x22&&wr.done());
    b.clear();delta(b,{3,0x25});assert(reconnect.queue(Delta,3,b));receive(reconnect,Reject); // sequence gap
}
static void framing_test(){using namespace bw_net;Bytes body;delta(body,{2,0x22});const auto bytes=frame(Delta,9,body);Pipe pipe;Frame parsed{};
    for(size_t i=0;i<bytes.size();++i){pipe.input.push_back(bytes[i]);assert(pipe.next(parsed)==(i+1==bytes.size()?1:0));}
    assert(parsed.type==Delta&&parsed.sequence==9&&parsed.body==body&&pipe.input.empty());
    pipe.input=bytes;pipe.input[4]=0xFF;assert(pipe.next(parsed)==-1);pipe.input=bytes;pipe.input[8]=1;assert(pipe.next(parsed)==-1);
    pipe.input=bytes;pipe.input[0]='X';assert(pipe.next(parsed)==-1);
}
static void automatic_reconnect_test(){
    auto first=bw_network_server_start("127.0.0.1",0);assert(first);const auto port=bw_network_server_port(first);auto c=config(port,'e',true);std::strcpy(c.room,"restart");
    auto client=session();assert(bw_network_join(client.get(),&c));connected(client.get());const auto before=status(client.get());
    assert(bw_network_submit(client.get(),{2,0x22}));assert(until([&]{return status(client.get()).pending_outgoing==0;}));
    bw_network_server_stop(first);assert(until([&]{return status(client.get()).status==BW_NETWORK_RECONNECTING;}));
    assert(bw_network_submit(client.get(),{34,60}));auto second=bw_network_server_start("127.0.0.1",port);assert(second);connected(client.get());
    assert(until([&]{return status(client.get()).generation>before.generation&&status(client.get()).pending_outgoing==0;}));
    const auto after=status(client.get());assert(after.reconnects>0&&std::strcmp(before.room_identity,after.room_identity));
    BwProgressionState received{};drain(client.get(),received);assert(received.values[34]==60); // retained unsent fact rebased after fresh room identity
    bw_network_leave(client.get());bw_network_server_stop(second);
}
static void core_test(){
    prefs_test();framing_test();automatic_reconnect_test();std::unique_ptr<BwNetworkServer,decltype(&bw_network_server_stop)> server(bw_network_server_start("127.0.0.1",0),bw_network_server_stop);assert(server);
    unsigned port=bw_network_server_port(server.get());auto host=session(),guest=session();assert(host&&guest);auto a=config(port,'a',true),b=config(port,'b');
    assert(bw_network_join(host.get(),&a));connected(host.get());assert(bw_network_join(guest.get(),&b));connected(guest.get());
    assert(until([&]{return status(host.get()).peer_count==2&&status(guest.get()).peer_count==2;}));
    BwGameScene one{},two{};std::strcpy(one.stage,"LinkUG");std::strcpy(two.stage,"sea");one.stay_room=11;two.stay_room=44;one.position[0]=12;two.position[0]=99;
    assert(bw_network_presence(host.get(),&one)&&bw_network_presence(guest.get(),&two));assert(until([&]{auto s=status(host.get());
        bool x=false,y=false;for(unsigned i=0;i<s.peer_count;++i){x|=!std::strcmp(s.peers[i].stage,"LinkUG");y|=!std::strcmp(s.peers[i].stage,"sea");}return x&&y;}));
    assert(!bw_network_submit(host.get(),{14,0x50}));assert(bw_network_submit(host.get(),{2,0x22})&&bw_network_submit(host.get(),{34,60}));
    BwProgressionState received{};unsigned live=0;assert(until([&]{drain(guest.get(),received,&live);return received.values[2]==0x22&&received.values[34]==60;}));assert(live==2);
    assert(bw_network_submit(host.get(),{2,0x22}));assert(until([&]{return status(host.get()).pending_outgoing==0;}));std::this_thread::sleep_for(30ms);drain(guest.get(),received,&live);assert(live==2);
    auto bad=session();auto mismatch=config(port,'c');mismatch.compatibility.options_digest[0]='2';assert(bw_network_join(bad.get(),&mismatch));assert(until([&]{return status(bad.get()).status==BW_NETWORK_REJECTED;}));
    std::strcpy(mismatch.room_password,"wrong");mismatch.compatibility=a.compatibility;assert(bw_network_join(bad.get(),&mismatch));assert(until([&]{return status(bad.get()).status==BW_NETWORK_REJECTED;}));
    mismatch=config(port,'c',true);std::strcpy(mismatch.room,"isolated");assert(bw_network_join(bad.get(),&mismatch));connected(bad.get());BwProgressionState separate{};drain(bad.get(),separate);assert(!separate.values[2]);
    bw_network_leave(guest.get());assert(status(guest.get()).status==BW_NETWORK_OFFLINE);assert(bw_network_join(guest.get(),&b));connected(guest.get());received={};unsigned snapshots=0;drain(guest.get(),received,nullptr,&snapshots);assert(snapshots==2&&received.values[2]==0x22&&received.values[34]==60);
    replay_test(port);bw_network_leave(host.get());bw_network_leave(guest.get());bw_network_leave(bad.get());
    std::puts("network core, replay, compatibility, presence, preferences and isolation tests passed");
}
static int client_role(unsigned port,const char* role){
    const bool publisher=!std::strcmp(role,"publisher");assert(publisher||!std::strcmp(role,"receiver"));auto client=session();auto c=config(port,publisher?'a':'b',publisher);
    assert(bw_network_join(client.get(),&c));connected(client.get());std::printf("{\"ready\":true,\"role\":\"%s\"}\n",role);std::fflush(stdout);
    BwGameScene scene{};std::strcpy(scene.stage,publisher?"LinkUG":"sea");scene.stay_room=publisher?11:44;assert(bw_network_presence(client.get(),&scene));
    if(publisher){assert(until([&]{return status(client.get()).peer_count==2;},10000));assert(bw_network_submit(client.get(),{2,0x22})&&bw_network_submit(client.get(),{34,60}));
        assert(until([&]{return status(client.get()).pending_outgoing==0;}));std::this_thread::sleep_for(1500ms);
        std::printf("{\"passed\":true,\"role\":\"publisher\",\"revision\":%llu}\n",(unsigned long long)status(client.get()).server_revision);
    }else{BwProgressionState state{};unsigned live=0;assert(until([&]{drain(client.get(),state,&live);return state.values[2]==0x22&&state.values[34]==60;},10000));assert(live==2);
        bw_network_leave(client.get());assert(bw_network_join(client.get(),&c));connected(client.get());state={};unsigned snapshots=0;drain(client.get(),state,nullptr,&snapshots);
        assert(snapshots==2&&state.values[2]==0x22&&state.values[34]==60);
        std::puts("{\"passed\":true,\"role\":\"receiver\",\"live\":2,\"reconnect_snapshot\":2}");}
    return 0;
}
int main(int argc,char** argv){if(argc==5&&!std::strcmp(argv[1],"--port")&&!std::strcmp(argv[3],"--role"))return client_role(unsigned(std::strtoul(argv[2],nullptr,10)),argv[4]);
    assert(argc==1);core_test();return 0;}
