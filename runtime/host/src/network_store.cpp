// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_store.h"
#include "network_wire.h"
#include "network_digest.h"
#include "atomic_file.h"
#include <filesystem>
#include <limits>
#include <random>
#include <system_error>
#include <fcntl.h>
#ifndef _WIN32
#include <sys/file.h>
#endif
namespace bw_net {
namespace {
namespace fs=std::filesystem;
bool hex(const std::string& s,size_t size){return s.size()==size&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
bool slug(const std::string& s){return !s.empty()&&s.size()<=32&&s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos;}
std::string random_id(){std::random_device random;const char* digits="0123456789abcdef";std::string out(32,'0');for(char& c:out)c=digits[random()&15];return out;}
std::string credential(const StoredRoom& room,const char* password){
    Bytes data;string(data,"BlueWake.RoomCredential.v1");string(data,room.id.c_str());string(data,room.salt.c_str());string(data,password);
    return digest(std::string(data.begin(),data.end()));
}
bool compatibility(const BwNetworkCompatibility& manifest,const std::string& name){
    BwNetworkConfig c{};c.compatibility=manifest;std::strcpy(c.server,"127.0.0.1");c.port=1;
    if(!slug(name))return false;std::memcpy(c.room,name.c_str(),name.size()+1);std::memset(c.player_id,'0',32);std::strcpy(c.player_name,"Storage");
    return bw_network_config_valid(&c,nullptr,0);
}
bool valid(const StoredRoom& room){
    if(!compatibility(room.compatibility,room.name)||!hex(room.id,32)||!hex(room.salt,32)||!hex(room.credential,64)||
       room.clients.size()>RoomStore::max_clients||room.revision==UINT64_MAX)return false;
    bool facts=false;unsigned keys=0;
    for(uint16_t key=0;key<BW_PROGRESSION_KEYS;++key)if(room.progress.values[key]){
        if(!bw_progression_valid({key,room.progress.values[key]}))return false;facts=true;++keys;}
    if(keys>BW_NETWORK_QUEUE||(room.revision==0&&facts)||(room.revision!=0&&!facts))return false;
    uint64_t submissions=0;
    for(const auto& client:room.clients){
        const auto& history=client.second;
        if(!hex(client.first,32)||history.sequence==UINT64_MAX||history.recent.size()!=std::min<uint64_t>(history.sequence,BW_NETWORK_QUEUE))return false;
        submissions=history.sequence>UINT64_MAX-submissions?UINT64_MAX:submissions+history.sequence;
        uint64_t expected=history.sequence-uint64_t(history.recent.size());
        for(const auto& receipt:history.recent){
            if(receipt.sequence!=++expected||!bw_progression_valid(receipt.delta))return false;
            auto canonical=room.progress;if(bw_progression_merge(&canonical,receipt.delta))return false;
        }
    }
    return room.revision<=submissions;
}
Bytes encode(const StoredRoom& room){
    Bytes body;identity(body,room.compatibility);string(body,room.name.c_str());string(body,room.id.c_str());string(body,room.salt.c_str());string(body,room.credential.c_str());put64(body,room.revision);
    unsigned count=0;for(uint32_t value:room.progress.values)if(value)++count;put16(body,uint16_t(count));
    for(uint16_t key=0;key<BW_PROGRESSION_KEYS;++key)if(room.progress.values[key])delta(body,{key,room.progress.values[key]});
    put16(body,uint16_t(room.clients.size()));
    for(const auto& client:room.clients){string(body,client.first.c_str());put64(body,client.second.sequence);put16(body,uint16_t(client.second.recent.size()));
        for(const auto& receipt:client.second.recent){put64(body,receipt.sequence);delta(body,receipt.delta);}}
    Bytes file={'B','W','R','S'};put32(file,RoomStore::version);put32(file,BW_NETWORK_PROTOCOL);put32(file,BW_PROGRESSION_SCHEMA);put32(file,uint32_t(body.size()));file.insert(file.end(),body.begin(),body.end());
    const auto checksum=digest(std::string(file.begin(),file.end()));file.insert(file.end(),checksum.begin(),checksum.end());return file;
}
bool decode(const Bytes& file,StoredRoom& out){
    if(file.size()<84||file.size()>RoomStore::max_file_bytes||std::memcmp(file.data(),"BWRS",4))return false;
    Bytes header(file.begin()+4,file.begin()+20);Reader hr{header};
    if(hr.dword()!=RoomStore::version||hr.dword()!=BW_NETWORK_PROTOCOL||hr.dword()!=BW_PROGRESSION_SCHEMA||hr.dword()!=file.size()-84||!hr.done())return false;
    const auto checksum=digest(std::string(file.begin(),file.end()-64));
    if(!std::equal(checksum.begin(),checksum.end(),file.end()-64))return false;
    Bytes body(file.begin()+20,file.end()-64);Reader r{body};StoredRoom fresh;char name[33],id[33],salt[33],proof[65];
    if(!identity(r,fresh.compatibility)||!r.text(name,sizeof name)||!r.text(id,sizeof id)||!r.text(salt,sizeof salt)||!r.text(proof,sizeof proof))return false;
    fresh.name=name;fresh.id=id;fresh.salt=salt;fresh.credential=proof;fresh.revision=r.qword();
    const unsigned keys=r.word();if(keys>BW_NETWORK_QUEUE)return false;int previous=-1;
    for(unsigned i=0;i<keys;++i){const auto d=delta(r);if(!r.ok||d.key<=previous||!d.value||!bw_progression_valid(d))return false;previous=d.key;fresh.progress.values[d.key]=d.value;}
    const unsigned clients=r.word();if(clients>RoomStore::max_clients)return false;std::string previous_id;
    for(unsigned i=0;i<clients;++i){char player[33];if(!r.text(player,sizeof player)||!hex(player,32)||(!previous_id.empty()&&previous_id>=player))return false;
        previous_id=player;RoomClientHistory history;history.sequence=r.qword();const unsigned recent=r.word();if(recent>BW_NETWORK_QUEUE)return false;
        for(unsigned j=0;j<recent;++j){const auto sequence=r.qword();const auto d=delta(r);history.recent.push_back({sequence,d});}
        fresh.clients.emplace(player,std::move(history));}
    if(!r.done()||!valid(fresh))return false;out=std::move(fresh);return true;
}
bool safe_path(const fs::path& path,bool directory){
    std::error_code ec;const auto status=fs::symlink_status(path,ec);
    if(ec||fs::is_symlink(status))return false;
#ifdef _WIN32
    const DWORD attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
#endif
    if(directory)return fs::is_directory(status);
    return fs::is_regular_file(status)&&fs::hard_link_count(path,ec)==1&&!ec;
}
bool safe_parents(const fs::path& path){
    auto at=path.root_path();for(const auto& piece:path.relative_path()){at/=piece;std::error_code ec;
        if(fs::exists(at,ec)){if(ec||!safe_path(at,true))return false;}else if(ec)return false;}
    return true;
}
bool read_file(const fs::path& path,Bytes& bytes){
    if(!safe_path(path,false))return false;std::error_code ec;const auto size=fs::file_size(path,ec);
    if(ec||size>RoomStore::max_file_bytes)return false;
    FILE* file=bw_atomic_open(path.u8string().c_str(),"rb");if(!file)return false;bytes.resize(size);
    const bool read=std::fread(bytes.data(),1,bytes.size(),file)==bytes.size()&&std::fgetc(file)==EOF&&!std::ferror(file);
    return std::fclose(file)==0&&read;
}
bool sync_directory(const fs::path& directory){
#ifdef _WIN32
    (void)directory;return true; // MoveFileExW(MOVEFILE_WRITE_THROUGH) owns publication durability.
#else
    const int descriptor=::open(directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(descriptor<0)return false;
    const bool ok=fsync(descriptor)==0;const bool closed=::close(descriptor)==0;return ok&&closed;
#endif
}
bool sync_parents(fs::path directory){
    /* A first host can create Network/Server and several parents at once.
     * Flush each directory, including each newly created parent's entry. */
    for(;;){if(!sync_directory(directory))return false;const auto parent=directory.parent_path();if(parent.empty()||parent==directory)return true;directory=parent;}
}
FILE* exclusive_file(const fs::path& path){
#ifdef _WIN32
    const int descriptor=_wopen(path.c_str(),_O_CREAT|_O_EXCL|_O_WRONLY|_O_BINARY,_S_IREAD|_S_IWRITE);
    if(descriptor<0)return nullptr;FILE* file=_fdopen(descriptor,"wb");if(!file)_close(descriptor);return file;
#else
    const int descriptor=::open(path.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600);
    if(descriptor<0)return nullptr;FILE* file=fdopen(descriptor,"wb");if(!file)::close(descriptor);return file;
#endif
}
bool publish(const fs::path& pending,const fs::path& target,bool exists){
#ifdef _WIN32
    return bw_atomic_move(pending.u8string().c_str(),target.u8string().c_str(),exists)==0;
#else
    if(exists)return ::rename(pending.c_str(),target.c_str())==0;
    if(::link(pending.c_str(),target.c_str())!=0)return false;
    return ::unlink(pending.c_str())==0;
#endif
}
}
std::string room_storage_key(const BwNetworkCompatibility& manifest,const char* room){Bytes bytes;identity(bytes,manifest);string(bytes,room);return digest(std::string(bytes.begin(),bytes.end()));}
void room_set_credential(StoredRoom& room,const char* password){room.salt=random_id();room.credential=credential(room,password);}
bool room_credential_matches(const StoredRoom& room,const char* password){return room.credential==credential(room,password);}
struct RoomStore::Impl {
    fs::path root;
    struct Entry {StoredRoom room;std::string checksum;};
    std::map<std::string,Entry> entries;
    bool healthy=true;
#ifdef _WIN32
    HANDLE lease=INVALID_HANDLE_VALUE;
    ~Impl(){if(lease!=INVALID_HANDLE_VALUE)CloseHandle(lease);}
#else
    int lease=-1;
    ~Impl(){if(lease>=0)::close(lease);}
#endif
};
RoomStore::RoomStore()=default;
RoomStore::~RoomStore()=default;
bool RoomStore::open(const char* directory,std::string& error){
    if(impl||!directory||!*directory||std::strlen(directory)>1024){error="Invalid durable server directory";return false;}
    try{
        auto fresh=std::make_unique<Impl>();fresh->root=fs::absolute(fs::u8path(directory)).lexically_normal();
        if(!safe_parents(fresh->root)){error="Durable server directory contains an unsafe link";return false;}
        std::error_code ec;fs::create_directories(fresh->root,ec);
        if(ec||!safe_path(fresh->root,true)||!safe_parents(fresh->root)||!sync_parents(fresh->root)){error="Cannot create durable server directory";return false;}
        const auto lock=fresh->root/"server.lock";
#ifdef _WIN32
        fresh->lease=CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(fresh->lease==INVALID_HANDLE_VALUE||!safe_path(lock,false)){error="Durable server directory is locked or unsafe";return false;}
#else
        fresh->lease=::open(lock.c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);struct stat status{};
        if(fresh->lease<0||fstat(fresh->lease,&status)||!S_ISREG(status.st_mode)||status.st_nlink!=1||flock(fresh->lease,LOCK_EX|LOCK_NB)){error="Durable server directory is locked or unsafe";return false;}
#endif
        unsigned entries=0;size_t loaded_bytes=0;
        for(const auto& file:fs::directory_iterator(fresh->root)){
            if(++entries>64||!safe_path(file.path(),false)){error="Unsafe or excessive durable server files";return false;}
            const std::string name=file.path().filename().u8string();if(name=="server.lock")continue;
            const bool room_file=name.size()==71&&hex(name.substr(0,64),64)&&name.substr(64)==".bwroom";
            const bool pending=name.size()==108&&hex(name.substr(0,64),64)&&name.substr(64,12)==".bwroom.tmp-"&&hex(name.substr(76),32);
            if(pending){if(fs::file_size(file.path())>max_file_bytes){error="Oversized abandoned room transaction";return false;}continue;}
            if(!room_file||fresh->entries.size()>=max_rooms){error="Unknown or excessive durable server room files";return false;}
            Bytes bytes;StoredRoom room;if(!read_file(file.path(),bytes)||!decode(bytes,room)||room_storage_key(room.compatibility,room.name.c_str())!=name.substr(0,64)){error="Malformed or incompatible durable room file";return false;}
            loaded_bytes+=bytes.size();if(loaded_bytes>max_rooms*max_file_bytes){error="Durable room memory bound exceeded";return false;}
            fresh->entries.emplace(name.substr(0,64),Impl::Entry{std::move(room),digest(std::string(bytes.begin(),bytes.end()))});
        }
        impl=std::move(fresh);error.clear();return true;
    }catch(...){error="Cannot open durable server state";return false;}
}
bool RoomStore::load(const BwNetworkCompatibility& manifest,const char* name,StoredRoom& out,bool& found,std::string& error)const{
    found=false;if(!impl||!impl->healthy||!name||!compatibility(manifest,name)){error="Durable room store unavailable";return false;}
    const auto at=impl->entries.find(room_storage_key(manifest,name));if(at==impl->entries.end()){error.clear();return true;}
    Bytes bytes;if(!read_file(impl->root/(at->first+".bwroom"),bytes)||digest(std::string(bytes.begin(),bytes.end()))!=at->second.checksum){error="Durable room changed outside its server transaction";return false;}
    out=at->second.room;found=true;error.clear();return true;
}
bool RoomStore::save(const StoredRoom& room,std::string& error){
    if(!impl||!impl->healthy||!valid(room)){error="Invalid durable room transaction";return false;}
    try{
        const auto key=room_storage_key(room.compatibility,room.name.c_str());const auto prior=impl->entries.find(key);
        if(prior==impl->entries.end()&&impl->entries.size()>=max_rooms){error="Durable room limit reached";return false;}
        const auto target=impl->root/(key+".bwroom");std::error_code ec;const bool exists=fs::exists(target,ec);
        if(ec||exists!=(prior!=impl->entries.end())){impl->healthy=false;error="Durable room path changed outside its server transaction";return false;}
        if(exists){Bytes before;if(!read_file(target,before)||digest(std::string(before.begin(),before.end()))!=prior->second.checksum){impl->healthy=false;error="Stale or changed durable room transaction";return false;}}
        if(prior!=impl->entries.end()){
            if(room.id!=prior->second.room.id||room.salt!=prior->second.room.salt||room.credential!=prior->second.room.credential||room.revision<prior->second.room.revision){error="Stale durable room identity or revision";return false;}
            for(const auto& client:prior->second.room.clients){const auto current=room.clients.find(client.first);if(current==room.clients.end()||current->second.sequence<client.second.sequence){error="Stale durable replay history";return false;}}
            for(uint16_t key=0;key<BW_PROGRESSION_KEYS;++key)if(prior->second.room.progress.values[key]){auto current=room.progress;if(bw_progression_merge(&current,{key,prior->second.room.progress.values[key]})){error="Durable permanent progress cannot regress";return false;}}
        }
        /* Never rewrite an acknowledged receipt, including a lower but valid
         * payload. New canonical facts/revisions must follow new receipts.
         * Production publishes one client's ordered transaction at a time. */
        BwProgressionState explained=prior==impl->entries.end()?BwProgressionState{}:prior->second.room.progress;
        uint64_t explained_revision=prior==impl->entries.end()?0:prior->second.room.revision;
        for(const auto& client:room.clients){
            const RoomClientHistory* old=nullptr;
            if(prior!=impl->entries.end()){const auto at=prior->second.room.clients.find(client.first);if(at!=prior->second.room.clients.end())old=&at->second;}
            for(const auto& receipt:client.second.recent){
                if(old&&receipt.sequence<=old->sequence){
                    const auto at=std::find_if(old->recent.begin(),old->recent.end(),[&](const RoomReceipt& r){return r.sequence==receipt.sequence;});
                    if(at==old->recent.end()||at->delta.key!=receipt.delta.key||at->delta.value!=receipt.delta.value){error="Acknowledged durable replay receipt changed";return false;}
                }else if(bw_progression_merge(&explained,receipt.delta))++explained_revision;
            }
        }
        if(explained_revision!=room.revision||std::memcmp(&explained,&room.progress,sizeof explained)){
            error="Durable progress or revision is not explained by new receipts";return false;
        }
        const Bytes bytes=encode(room);if(bytes.size()>max_file_bytes){error="Durable room transaction too large";return false;}
        const auto pending=impl->root/(key+".bwroom.tmp-"+random_id());FILE* file=exclusive_file(pending);
        if(!file){impl->healthy=false;error="Cannot create durable room transaction";return false;}
        bool ok=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size()&&bw_atomic_flush(file);
        if(std::fclose(file)!=0)ok=false;
        if(ok)ok=publish(pending,target,exists);
        if(ok)ok=sync_directory(impl->root);
        if(!ok){bw_atomic_remove(pending.u8string().c_str());impl->healthy=false;error="Cannot durably publish room progression";return false;}
        impl->entries[key]={room,digest(std::string(bytes.begin(),bytes.end()))};error.clear();return true;
    }catch(...){if(impl)impl->healthy=false;error="Cannot persist durable room transaction";return false;}
}
unsigned RoomStore::rooms()const{return impl?unsigned(impl->entries.size()):0;}
}
