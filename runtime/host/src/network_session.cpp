// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_wire.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
using namespace bw_net;
namespace {
std::string nonce(){std::random_device random;std::string out(32,'0');const char* hex="0123456789abcdef";for(char& c:out)c=hex[random()&15];return out;}
void copy(char* target,size_t n,const std::string& s){std::snprintf(target,n,"%s",s.c_str());}
struct Receipt{uint64_t sequence;BwProgressionDelta delta;};
struct ClientHistory{uint64_t sequence=0;std::deque<Receipt> recent;};
struct Room{BwNetworkCompatibility compatibility{};std::string id,password;BwProgressionState progress{};uint64_t revision=0;std::map<std::string,ClientHistory> clients;};
struct ServerPeer{Pipe pipe;std::string room;BwNetworkPeer info{};bool joined=false,closing=false;Clock::time_point accepted=Clock::now();};
void presence(Bytes& b,const BwNetworkPeer& p){string(b,p.player_id);string(b,p.player_name);string(b,p.stage);b.push_back(uint8_t(p.room));
    for(float value:p.position){uint32_t bits;std::memcpy(&bits,&value,4);put32(b,bits);}b.push_back(p.online?1:0);}
bool presence(Reader& r,BwNetworkPeer& p){if(!r.text(p.player_id,sizeof p.player_id)||!r.text(p.player_name,sizeof p.player_name)||!r.text(p.stage,sizeof p.stage))return false;
    for(const char* s=p.player_id;*s;++s)if(!((*s>='0'&&*s<='9')||(*s>='a'&&*s<='f')))r.ok=false;
    if(std::strlen(p.player_id)!=32||!p.player_name[0])r.ok=false;
    for(const char* s=p.player_name;*s;++s)if(uint8_t(*s)<32||uint8_t(*s)>126)r.ok=false;
    for(const char* s=p.stage;*s;++s)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='_'||*s=='-'))r.ok=false;
    p.room=int8_t(r.byte());for(float& value:p.position){uint32_t bits=r.dword();std::memcpy(&value,&bits,4);if(!std::isfinite(value)||std::fabs(value)>1e9f)r.ok=false;}
    const uint8_t online=r.byte();if(online>1)r.ok=false;p.online=online!=0;return r.ok;}
}
struct BwNetworkServer{
    Socket listener=invalid;uint16_t port=0;std::atomic<bool> stop{false};std::thread worker;
    std::map<std::string,Room> rooms;std::vector<std::unique_ptr<ServerPeer>> peers;
    ~BwNetworkServer(){stop=true;if(worker.joinable())worker.join();close(listener);}
    void reject(ServerPeer& p,const char* reason){Bytes b;string(b,reason);if(!p.pipe.queue(Reject,0,b))p.pipe.reset();p.closing=true;}
    void roster(const std::string& room){Bytes b;unsigned count=0;for(const auto& p:peers)if(p->joined&&!p->closing&&p->room==room)++count;
        put16(b,uint16_t(count));for(const auto& p:peers)if(p->joined&&!p->closing&&p->room==room)presence(b,p->info);
        for(auto& p:peers)if(p->joined&&!p->closing&&p->room==room&&!p->pipe.queue(Roster,0,b))p->closing=true;}
    void hello(ServerPeer& p,const Frame& f){
        Reader r{f.body};BwNetworkConfig c{};std::strcpy(c.server,"127.0.0.1");c.port=port;
        if(!identity(r,c.compatibility)||!r.text(c.room,sizeof c.room)||!r.text(c.player_id,sizeof c.player_id)||
            !r.text(c.player_name,sizeof c.player_name)||!r.text(c.room_password,sizeof c.room_password)){reject(p,"Malformed handshake");return;}
        const uint8_t create=r.byte();c.create_room=create!=0;
        if(create>1||!r.done()||f.sequence||!bw_network_config_valid(&c,nullptr,0)){reject(p,"Invalid Wind Waker handshake");return;}
        auto found=rooms.find(c.room);
        if(found==rooms.end()){
            if(!c.create_room){reject(p,"Room does not exist; host it first");return;}
            if(rooms.size()>=16){reject(p,"Server room limit reached");return;}
            Room room;room.compatibility=c.compatibility;room.id=nonce();room.password=c.room_password;
            found=rooms.emplace(c.room,std::move(room)).first;
        }
        Room& room=found->second;
        if(!same(room.compatibility,c.compatibility)){reject(p,"Game/build/module/options compatibility mismatch");return;}
        if(room.password!=c.room_password){reject(p,"Room password mismatch");return;}
        if(!room.clients.count(c.player_id)&&room.clients.size()>=128){reject(p,"Room player identity limit reached");return;}
        /* A reconnect with the same stable identity replaces its prior socket,
         * preventing one identity from generating two concurrent sequences. */
        for(auto& other:peers)if(other.get()!=&p&&other->joined&&other->room==c.room&&std::strcmp(other->info.player_id,c.player_id)==0){other->closing=true;other->pipe.reset();}
        p.room=c.room;copy(p.info.player_id,33,c.player_id);copy(p.info.player_name,33,c.player_name);p.info.online=true;p.joined=true;
        Bytes b;identity(b,room.compatibility);string(b,room.id.c_str());put64(b,room.revision);put64(b,room.clients[c.player_id].sequence);
        unsigned count=0;for(uint32_t v:room.progress.values)if(v)++count;put16(b,uint16_t(count));
        for(uint16_t key=0;key<BW_PROGRESSION_KEYS;++key)if(room.progress.values[key])delta(b,{key,room.progress.values[key]});
        if(!p.pipe.queue(Welcome,0,b)){p.closing=true;return;}roster(p.room);
    }
    void handle(ServerPeer& p,const Frame& f){
        if(!p.joined){if(f.type==Hello)hello(p,f);else reject(p,"Handshake required");return;}
        Room& room=rooms.at(p.room);
        if(f.type==Delta){Reader r{f.body};const auto d=delta(r);
            if(!r.done()||!bw_progression_valid(d)||!f.sequence){reject(p,"Invalid permanent progression delta");return;}
            auto& history=room.clients.at(p.info.player_id);uint64_t& last=history.sequence;
            if(last==UINT64_MAX||f.sequence>last+1){reject(p,"Client sequence gap or exhaustion");return;}
            const bool fresh=f.sequence==last+1;
            if(!fresh){const auto old=std::find_if(history.recent.begin(),history.recent.end(),[&](const Receipt& x){return x.sequence==f.sequence;});
                if(old==history.recent.end()||old->delta.key!=d.key||old->delta.value!=d.value){reject(p,"Replay payload mismatch or expired sequence");return;}}
            const bool changed=fresh&&bw_progression_merge(&room.progress,d);
            if(fresh){last=f.sequence;history.recent.push_back({f.sequence,d});if(history.recent.size()>BW_NETWORK_QUEUE)history.recent.pop_front();}
            if(changed)++room.revision;
            Bytes b;string(b,p.info.player_id);put64(b,f.sequence);put64(b,room.revision);delta(b,{d.key,room.progress.values[d.key]});
            if(changed){for(auto& other:peers)if(other->joined&&!other->closing&&other->room==p.room&&!other->pipe.queue(Commit,0,b))other->closing=true;}
            else if(!p.pipe.queue(Commit,0,b))p.closing=true;
        }else if(f.type==Presence){Reader r{f.body};BwNetworkPeer info{};
            if(!presence(r,info)||!r.done()||std::strcmp(info.player_id,p.info.player_id)||std::strcmp(info.player_name,p.info.player_name)){
                reject(p,"Invalid player presence");return;}p.info=info;p.info.online=true;roster(p.room);
        }else if(f.type==Ping){if(!f.body.empty()||!p.pipe.queue(Pong,f.sequence,{}))p.closing=true;}
        else if(f.type==Leave){p.closing=true;}else reject(p,"Unexpected client packet");
    }
    void run(){
        while(!stop){
            for(unsigned tries=0;tries<8;++tries){Socket s=::accept(listener,nullptr,nullptr);if(s==invalid)break;
                if(peers.size()>=BW_NETWORK_PEERS||!nonblocking(s)){close(s);continue;}
                auto p=std::make_unique<ServerPeer>();p->pipe.socket=s;peers.push_back(std::move(p));}
            for(auto& p:peers){
                if(p->pipe.socket==invalid)continue;
                if(!p->pipe.pump()){p->closing=true;p->pipe.reset();continue;}
                if(!p->closing)for(unsigned n=0;n<128;++n){Frame f;int result=p->pipe.next(f);if(result==0)break;
                    if(result<0){reject(*p,"Malformed or oversized protocol frame");break;}handle(*p,f);if(p->closing)break;}
                const auto now=Clock::now();
                if(p->pipe.eof||(!p->joined&&now-p->accepted>std::chrono::seconds(5))||now-p->pipe.activity>std::chrono::seconds(30)){p->closing=true;p->pipe.reset();}
                if(p->closing&&p->pipe.output.empty())p->pipe.reset();
            }
            std::vector<std::string> changed;
            peers.erase(std::remove_if(peers.begin(),peers.end(),[&](auto& p){if(p->pipe.socket!=invalid)return false;if(p->joined)changed.push_back(p->room);return true;}),peers.end());
            for(const auto& room:changed)roster(room);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
};
struct Pending{BwProgressionDelta delta;uint64_t sequence;bool sent=false;};
struct BwNetworkSession{
    std::mutex mutex;std::atomic<bool> stop{false};std::thread worker;BwNetworkConfig config{};
    BwNetworkSnapshot status{};std::deque<Pending> outgoing;std::deque<BwNetworkUpdate> incoming;
    uint64_t next_sequence=1;BwNetworkPeer local{};bool presence_pending=false;
    ~BwNetworkSession(){leave();}
    void leave(){stop=true;if(worker.joinable())worker.join();std::lock_guard<std::mutex> lock(mutex);
        outgoing.clear();incoming.clear();status.status=BW_NETWORK_OFFLINE;status.peer_count=0;copy(status.message,128,"Offline");next_sequence=1;}
    void state(BwNetworkStatus value,const char* message){std::lock_guard<std::mutex> lock(mutex);status.status=value;copy(status.message,128,message);}
    bool received(const Frame& f,bool& welcome,bool& rejected){
        Reader r{f.body};std::lock_guard<std::mutex> lock(mutex);
        if(f.type==Reject){char message[128];if(!r.text(message,sizeof message)||!r.done())return false;
            status.status=BW_NETWORK_REJECTED;copy(status.message,128,message);rejected=true;return true;}
        if(f.type==Welcome){BwNetworkCompatibility c{};char room_id[33];
            if(welcome||!identity(r,c)||!same(c,config.compatibility)||!r.text(room_id,sizeof room_id))return false;
            const uint64_t revision=r.qword(),ack=r.qword();const unsigned count=r.word();if(count>BW_NETWORK_QUEUE)return false;
            if(ack==UINT64_MAX||std::strlen(room_id)!=32||std::strspn(room_id,"0123456789abcdef")!=32)return false;
            std::deque<BwNetworkUpdate> fresh;std::array<bool,BW_PROGRESSION_KEYS> keys{};
            for(unsigned i=0;i<count;++i){const auto d=delta(r);if(!bw_progression_valid(d)||keys[d.key])return false;keys[d.key]=true;
                fresh.push_back({d,revision,status.generation+1,true});}if(!r.done())return false;
            const bool changed=status.room_identity[0]&&std::strcmp(status.room_identity,room_id);
            if(changed){uint64_t seq=ack+1;for(auto& p:outgoing){p.sequence=seq++;p.sent=false;}next_sequence=seq;}
            else {while(!outgoing.empty()&&outgoing.front().sequence<=ack)outgoing.pop_front();next_sequence=std::max(next_sequence,ack+1);for(auto& p:outgoing)p.sent=false;}
            incoming.swap(fresh);copy(status.room_identity,33,room_id);status.server_revision=revision;++status.generation;
            status.status=BW_NETWORK_CONNECTED;copy(status.message,128,"Connected; isolated room progression");welcome=true;presence_pending=true;return true;
        }
        if(!welcome)return false;
        if(f.type==Commit){char origin[33];if(!r.text(origin,sizeof origin))return false;const uint64_t sequence=r.qword(),revision=r.qword();const auto d=delta(r);
            if(!r.done()||!sequence||std::strlen(origin)!=32||std::strspn(origin,"0123456789abcdef")!=32||!bw_progression_valid(d))return false;
            if(std::strcmp(origin,config.player_id)==0){while(!outgoing.empty()&&outgoing.front().sequence<=sequence)outgoing.pop_front();}
            if(revision<=status.server_revision)return true;
            if(revision!=status.server_revision+1||incoming.size()>=BW_NETWORK_QUEUE)return false;
            incoming.push_back({d,revision,status.generation,false});status.server_revision=revision;++status.received;return true;
        }
        if(f.type==Roster){const unsigned count=r.word();if(count>BW_NETWORK_PEERS)return false;std::array<BwNetworkPeer,BW_NETWORK_PEERS> peers{};
            for(unsigned i=0;i<count;++i){if(!presence(r,peers[i]))return false;
                for(unsigned j=0;j<i;++j)if(!std::strcmp(peers[i].player_id,peers[j].player_id))return false;}if(!r.done())return false;
            status.peer_count=count;for(unsigned i=0;i<count;++i)status.peers[i]=peers[i];return true;}
        if(f.type==Pong)return f.body.empty();return false;
    }
    void run(){
        bool ever=false;
        while(!stop){state(ever?BW_NETWORK_RECONNECTING:BW_NETWORK_CONNECTING,ever?"Reconnecting; queued progress retained":"Connecting");
            Pipe pipe;pipe.socket=bw_net::connect(config.server,config.port);
            if(pipe.socket==invalid){for(unsigned i=0;i<40&&!stop;++i)std::this_thread::sleep_for(std::chrono::milliseconds(25));continue;}
            Bytes hello;identity(hello,config.compatibility);string(hello,config.room);string(hello,config.player_id);string(hello,config.player_name);string(hello,config.room_password);hello.push_back(config.create_room?1:0);
            pipe.queue(Hello,0,hello);bool welcome=false,rejected=false;auto ping=Clock::now(),began=Clock::now();
            while(!stop){
                if(!pipe.pump())break;bool valid=true;
                for(unsigned i=0;i<128;++i){Frame f;const int result=pipe.next(f);if(result==0)break;if(result<0||!received(f,welcome,rejected)){valid=false;break;}if(rejected)break;}
                if(!valid||rejected||pipe.eof)break;
                if(welcome){
                    std::lock_guard<std::mutex> lock(mutex);
                    for(auto& p:outgoing)if(!p.sent){Bytes b;delta(b,p.delta);if(!pipe.queue(Delta,p.sequence,b)){valid=false;break;}p.sent=true;++status.sent;}
                    if(presence_pending){Bytes b;presence(b,local);if(!pipe.queue(Presence,0,b))valid=false;else presence_pending=false;}
                }
                if(!valid)break;
                const auto now=Clock::now();if(!welcome&&now-began>std::chrono::seconds(5))break;
                if(now-pipe.activity>std::chrono::seconds(15))break;
                if(now-ping>std::chrono::seconds(2)){pipe.queue(Ping,0,{});ping=now;}
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if(stop){pipe.queue(Leave,0,{});pipe.pump();break;}
            if(rejected)return;
            if(welcome){ever=true;std::lock_guard<std::mutex> lock(mutex);++status.reconnects;for(auto& p:outgoing)p.sent=false;}
            for(unsigned i=0;i<20&&!stop;++i)std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }
};
extern "C" BwNetworkServer* bw_network_server_start(const char* bind,uint16_t port){
    if(!bind||!init())return nullptr;
    auto server=std::make_unique<BwNetworkServer>();server->listener=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(server->listener==invalid||!nonblocking(server->listener))return nullptr;
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port);
    if(inet_pton(AF_INET,bind,&address.sin_addr)!=1||::bind(server->listener,reinterpret_cast<sockaddr*>(&address),sizeof address)!=0||listen(server->listener,8)!=0)return nullptr;
#ifdef _WIN32
    int size=sizeof address;
#else
    socklen_t size=sizeof address;
#endif
    if(getsockname(server->listener,reinterpret_cast<sockaddr*>(&address),&size)!=0)return nullptr;
    server->port=ntohs(address.sin_port);server->worker=std::thread([pointer=server.get()]{pointer->run();});return server.release();
}
extern "C" uint16_t bw_network_server_port(BwNetworkServer* s){return s?s->port:0;}
extern "C" void bw_network_server_stop(BwNetworkServer* s){delete s;}
extern "C" BwNetworkSession* bw_network_create(){try{return new BwNetworkSession;}catch(...){return nullptr;}}
extern "C" void bw_network_destroy(BwNetworkSession* s){delete s;}
extern "C" void bw_network_leave(BwNetworkSession* s){if(s)s->leave();}
extern "C" bool bw_network_join(BwNetworkSession* s,const BwNetworkConfig* c){
    char message[128];if(!s||!bw_network_config_valid(c,message,sizeof message))return false;s->leave();
    {std::lock_guard<std::mutex> lock(s->mutex);s->config=*c;s->status={};s->status.status=BW_NETWORK_CONNECTING;
        s->local={};copy(s->local.player_id,33,c->player_id);copy(s->local.player_name,33,c->player_name);s->local.online=true;s->stop=false;}
    try{s->worker=std::thread([s]{s->run();});return true;}catch(...){s->state(BW_NETWORK_FAILED,"Cannot start connection worker");return false;}
}
extern "C" bool bw_network_submit(BwNetworkSession* s,BwProgressionDelta d){
    if(!s||!bw_progression_valid(d))return false;std::lock_guard<std::mutex> lock(s->mutex);
    if((s->status.status!=BW_NETWORK_CONNECTED&&s->status.status!=BW_NETWORK_RECONNECTING)||s->outgoing.size()>=BW_NETWORK_QUEUE||s->next_sequence==UINT64_MAX)return false;
    s->outgoing.push_back({d,s->next_sequence++,false});return true;
}
extern "C" bool bw_network_presence(BwNetworkSession* s,const BwGameScene* scene){
    if(!s||!scene||!std::memchr(scene->stage,0,sizeof scene->stage))return false;
    for(const char* p=scene->stage;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'))return false;
    for(float value:scene->position)if(!std::isfinite(value)||std::fabs(value)>1e9f)return false;
    std::lock_guard<std::mutex> lock(s->mutex);copy(s->local.stage,9,scene->stage);s->local.room=scene->stay_room;
    std::memcpy(s->local.position,scene->position,sizeof scene->position);s->presence_pending=true;return true;
}
extern "C" bool bw_network_poll(BwNetworkSession* s,BwNetworkUpdate* out){if(!s||!out)return false;std::lock_guard<std::mutex> lock(s->mutex);
    if(s->incoming.empty())return false;*out=s->incoming.front();s->incoming.pop_front();return true;}
extern "C" void bw_network_status(BwNetworkSession* s,BwNetworkSnapshot* out){if(!out)return;if(!s){*out={};return;}
    std::lock_guard<std::mutex> lock(s->mutex);*out=s->status;out->pending_outgoing=unsigned(s->outgoing.size());out->pending_incoming=unsigned(s->incoming.size());}
