// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BLUEWAKE_NETWORK_WIRE_H
#define BLUEWAKE_NETWORK_WIRE_H
#include "network_session.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif
namespace bw_net {
#ifdef _WIN32
using Socket=SOCKET; constexpr Socket invalid=INVALID_SOCKET;
inline void close(Socket s){if(s!=invalid)closesocket(s);}
inline bool init(){static const bool ready=[](){WSADATA d;return WSAStartup(MAKEWORD(2,2),&d)==0;}();return ready;}
inline bool again(){const int e=WSAGetLastError();return e==WSAEWOULDBLOCK||e==WSAEINPROGRESS||e==WSAEALREADY;}
inline bool nonblocking(Socket s){u_long mode=1;return ioctlsocket(s,FIONBIO,&mode)==0;}
#else
using Socket=int; constexpr Socket invalid=-1;
inline void close(Socket s){if(s!=invalid)::close(s);}
inline bool init(){return true;}
inline bool again(){return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINPROGRESS||errno==EALREADY;}
inline bool nonblocking(Socket s){
#ifdef SO_NOSIGPIPE
    const int one=1;if(setsockopt(s,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one)!=0)return false;
#endif
    const int f=fcntl(s,F_GETFL,0);return f>=0&&fcntl(s,F_SETFL,f|O_NONBLOCK)==0;}
#endif
using Bytes=std::vector<uint8_t>;
using Clock=std::chrono::steady_clock;
enum Type:uint16_t{Hello=1,Welcome,Reject,Delta,Commit,Snapshot,Roster,Ping,Pong,Leave,Presence};
constexpr size_t max_body=4096,max_buffer=65536,header_size=20;
inline void put16(Bytes& b,uint16_t n){b.push_back(uint8_t(n>>8));b.push_back(uint8_t(n));}
inline void put32(Bytes& b,uint32_t n){put16(b,uint16_t(n>>16));put16(b,uint16_t(n));}
inline void put64(Bytes& b,uint64_t n){put32(b,uint32_t(n>>32));put32(b,uint32_t(n));}
inline void string(Bytes& b,const char* s){const size_t n=std::strlen(s);put16(b,uint16_t(n));b.insert(b.end(),s,s+n);}
struct Reader{
    const Bytes& data;size_t at=0;bool ok=true;
    uint8_t byte(){if(at>=data.size()){ok=false;return 0;}return data[at++];}
    uint16_t word(){uint16_t a=byte();return uint16_t((a<<8)|byte());}
    uint32_t dword(){uint32_t a=word();return (a<<16)|word();}
    uint64_t qword(){uint64_t a=dword();return (a<<32)|dword();}
    bool text(char* out,size_t capacity){const size_t n=word();if(!ok||n>=capacity||n>data.size()-at){ok=false;return false;}
        if(std::find(data.begin()+at,data.begin()+at+n,0)!=data.begin()+at+n){ok=false;return false;}
        std::memcpy(out,data.data()+at,n);out[n]=0;at+=n;return true;}
    bool done()const{return ok&&at==data.size();}
};
struct Frame{Type type;uint64_t sequence=0;Bytes body;};
inline Bytes frame(Type type,uint64_t sequence,const Bytes& body){Bytes b={'B','W','P','N'};put16(b,BW_NETWORK_PROTOCOL);put16(b,type);put32(b,uint32_t(body.size()));put64(b,sequence);b.insert(b.end(),body.begin(),body.end());return b;}
inline void identity(Bytes& b,const BwNetworkCompatibility& c){string(b,BW_NETWORK_NAMESPACE);put32(b,c.progression_schema);string(b,c.game_id);string(b,c.build_id);string(b,c.module_digest);string(b,c.options_digest);}
inline bool identity(Reader& r,BwNetworkCompatibility& c){char ns[64]={};return r.text(ns,sizeof ns)&&std::strcmp(ns,BW_NETWORK_NAMESPACE)==0&&
    (c.progression_schema=r.dword(),r.text(c.game_id,sizeof c.game_id))&&r.text(c.build_id,sizeof c.build_id)&&r.text(c.module_digest,sizeof c.module_digest)&&r.text(c.options_digest,sizeof c.options_digest);}
inline bool same(const BwNetworkCompatibility& a,const BwNetworkCompatibility& b){return a.progression_schema==b.progression_schema&&
    std::strcmp(a.game_id,b.game_id)==0&&std::strcmp(a.build_id,b.build_id)==0&&std::strcmp(a.module_digest,b.module_digest)==0&&std::strcmp(a.options_digest,b.options_digest)==0;}
inline void delta(Bytes& b,BwProgressionDelta d){put16(b,d.key);put32(b,d.value);}
inline BwProgressionDelta delta(Reader& r){const uint16_t k=r.word();return {k,r.dword()};}
struct Pipe{
    Socket socket=invalid;Bytes input;std::deque<Bytes> output;size_t sent=0;bool eof=false;Clock::time_point activity=Clock::now();
    Pipe()=default;Pipe(const Pipe&)=delete;Pipe& operator=(const Pipe&)=delete;
    Pipe(Pipe&& p)noexcept:socket(p.socket),input(std::move(p.input)),output(std::move(p.output)),sent(p.sent),eof(p.eof),activity(p.activity){p.socket=invalid;}
    ~Pipe(){close(socket);}
    void reset(){close(socket);socket=invalid;input.clear();output.clear();sent=0;eof=false;activity=Clock::now();}
    bool queue(Type t,uint64_t n,const Bytes& b){if(b.size()>max_body||output.size()>=256)return false;output.push_back(frame(t,n,b));return true;}
    bool pump(){
        if(socket==invalid)return false;
        while(!output.empty()){
            const Bytes& b=output.front();
#ifdef _WIN32
            const int n=::send(socket,reinterpret_cast<const char*>(b.data()+sent),int(b.size()-sent),0);
#else
#ifdef MSG_NOSIGNAL
            const int flags=MSG_NOSIGNAL;
#else
            const int flags=0;
#endif
            const int n=int(::send(socket,b.data()+sent,b.size()-sent,flags));
#endif
            if(n<0){if(again())break;return false;}if(n==0)return false;sent+=size_t(n);activity=Clock::now();
            if(sent==b.size()){output.pop_front();sent=0;}
        }
        for(unsigned tries=0;tries<16;++tries){uint8_t bytes[4096];
            const int n=int(::recv(socket,reinterpret_cast<char*>(bytes),sizeof bytes,0));
            if(n<0){if(again())break;return false;}if(n==0){eof=true;break;}
            if(input.size()+size_t(n)>max_buffer)return false;input.insert(input.end(),bytes,bytes+n);activity=Clock::now();
        }return true;
    }
    int next(Frame& f){
        if(input.size()<header_size)return 0;
        if(std::memcmp(input.data(),"BWPN",4)!=0)return -1;
        Bytes header(input.begin()+4,input.begin()+header_size);Reader r{header};
        const unsigned version=r.word(),type=r.word(),size=r.dword();f.sequence=r.qword();
        if(version!=BW_NETWORK_PROTOCOL||type<Hello||type>Presence||size>max_body)return -1;
        if(input.size()<header_size+size)return 0;
        f.type=Type(type);f.body.assign(input.begin()+header_size,input.begin()+header_size+size);
        input.erase(input.begin(),input.begin()+header_size+size);return 1;
    }
};
inline Socket connect(const char* host,uint16_t port){
    if(!init())return invalid;addrinfo hints{},*list=nullptr;hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_flags=AI_NUMERICHOST;
    if(std::strcmp(host,"localhost")==0)host="127.0.0.1";
    const std::string service=std::to_string(port);if(getaddrinfo(host,service.c_str(),&hints,&list)!=0)return invalid;
    Socket result=invalid;
    for(addrinfo* p=list;p;p=p->ai_next){Socket s=::socket(p->ai_family,p->ai_socktype,p->ai_protocol);if(s==invalid)continue;
        if(!nonblocking(s)){close(s);continue;}const int n=::connect(s,p->ai_addr,int(p->ai_addrlen));
        if(n==0){result=s;break;}if(again()){
            fd_set writable;FD_ZERO(&writable);FD_SET(s,&writable);timeval wait{0,250000};
            if(select(int(s)+1,nullptr,&writable,nullptr,&wait)>0){int error=0;
#ifdef _WIN32
                int win_length=sizeof error;if(getsockopt(s,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&win_length)==0&&error==0){result=s;break;}
#else
                socklen_t length=sizeof error;
                if(getsockopt(s,SOL_SOCKET,SO_ERROR,&error,&length)==0&&error==0){result=s;break;}
#endif
            }
        }close(s);
    }freeaddrinfo(list);return result;
}
} // namespace bw_net
#endif
