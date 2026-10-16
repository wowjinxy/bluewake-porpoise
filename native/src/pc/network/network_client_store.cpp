// SPDX-License-Identifier: GPL-3.0-or-later
// Native client journal. Atomic/path principles adapted from network_store.cpp
// and pc_network_profile.cpp; donor attribution remains in PROVENANCE.md.
#include "pc/network/pc_network_client_store.h"
#include "network_wire.h"
#include "network_digest.h"
#include "atomic_file.h"
#include <filesystem>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <system_error>
#include <fcntl.h>
#ifndef _WIN32
#include <sys/file.h>
#endif
namespace {
namespace fs=std::filesystem;
using namespace pc_net;
constexpr unsigned kVersion=1;
constexpr size_t kGciBytes=0x18040;
void message(char* out,unsigned capacity,const char* text){if(out&&capacity)std::snprintf(out,capacity,"%s",text);}
bool fail(char* out,unsigned capacity,const char* text){message(out,capacity,text);return false;}
bool terminated(const char* s,size_t n){return s&&std::memchr(s,0,n);}
bool hex(const char* s,size_t n){if(!s)return false;for(size_t i=0;i<n;++i)if(!s[i]||!std::strchr("0123456789abcdef",s[i]))return false;return s[n]==0;}
std::string utf8(const fs::path& path){const auto s=path.u8string();return {s.begin(),s.end()};}
fs::path io(const fs::path& path){
#ifdef _WIN32
    std::unique_ptr<wchar_t,decltype(&std::free)> wide(pc_net_atomic_wide_path(utf8(path).c_str()),std::free);
    if(!wide)throw std::runtime_error("Invalid native client path");return fs::path(wide.get());
#else
    return path;
#endif
}
bool path_value(const char* text,fs::path& out){
    if(!text||!*text||std::strlen(text)>1024)return false;out=fs::u8path(text);
    for(const auto& part:out.relative_path()){const auto s=utf8(part);if(s.empty()||s==".")continue;
        if(s==".."||s.back()==' '||s.back()=='.'||s.find_first_of(":\r\n")!=std::string::npos)return false;}
    out=fs::absolute(out).lexically_normal();return !out.empty()&&utf8(out).size()<1024;
}
bool node(const fs::path& path,bool directory){
    std::error_code ec;const auto status=fs::symlink_status(io(path),ec);if(ec||fs::is_symlink(status))return false;
#ifdef _WIN32
    const DWORD attributes=GetFileAttributesW(io(path).c_str());if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
#endif
    return directory?fs::is_directory(status):(fs::is_regular_file(status)&&fs::hard_link_count(io(path),ec)==1&&!ec);
}
bool absent(const fs::path& path){std::error_code ec;const auto s=fs::symlink_status(io(path),ec);
    return (!ec||ec==std::errc::no_such_file_or_directory)&&!fs::exists(s);}
struct Pins {
#ifdef _WIN32
    std::vector<HANDLE> handles;
    ~Pins(){for(HANDLE h:handles)CloseHandle(h);}
    bool add(const fs::path& path){HANDLE h=CreateFileW(io(path).c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,
        nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(h==INVALID_HANDLE_VALUE)return false;BY_HANDLE_FILE_INFORMATION info{};
        if(!GetFileInformationByHandle(h,&info)||!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)){CloseHandle(h);return false;}
        handles.push_back(h);return true;}
#else
    struct Pin{int fd;fs::path path;};std::vector<Pin> handles;
    ~Pins(){for(const auto& h:handles)::close(h.fd);}
    bool add(const fs::path& path){int fd=::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return false;handles.push_back({fd,path});return true;}
#endif
};
bool directories(const fs::path& path,bool create,Pins* pins=nullptr){
    fs::path at=path.root_path();if(!node(at,true))return false;
    for(const auto& part:path.relative_path()){at/=part;std::error_code ec;const auto s=fs::symlink_status(io(at),ec);
        if(ec&&ec!=std::errc::no_such_file_or_directory)return false;
        if(!fs::exists(s)&&(!create||!fs::create_directory(io(at),ec)||ec))return false;
        if(!node(at,true)||(pins&&!pins->add(at)))return false;}
    return true;
}
bool sync_dir(const fs::path& path){
#ifdef _WIN32
    (void)path;return true; // publication uses MoveFileEx WRITE_THROUGH
#else
    int fd=::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return false;
    const bool ok=fsync(fd)==0;return ::close(fd)==0&&ok;
#endif
}
bool sync_parents(fs::path path){for(;;){if(!sync_dir(path))return false;auto parent=path.parent_path();if(parent.empty()||parent==path)return true;path=parent;}}
std::string nonce(){std::random_device r;const char* chars="0123456789abcdef";std::string text(32,'0');for(char& c:text)c=chars[r()&15];return text;}
bool facts_valid(const PcNetworkFacts& facts){for(uint16_t key=0;key<PC_NETWORK_FACTS;++key)if(facts.values[key]&&!pc_network_fact_valid({key,facts.values[key]}))return false;return true;}
uint32_t normalized(uint16_t key,uint32_t value){return (key==PC_PERMANENT_ARROW_MAX||key==PC_PERMANENT_BOMB_MAX)&&!value?30:value;}
bool covers(const PcNetworkFacts& actual,const PcNetworkFacts& needed){
    if(!facts_valid(actual)||!facts_valid(needed))return false;
    for(uint16_t key=0;key<PC_NETWORK_FACTS;++key){
        if(pc_permanent_server_authoritative(key)){
            const auto a=normalized(key,actual.values[key]);
            const auto b=normalized(key,needed.values[key]);
            if(a!=b)return false;
        }
        else if(actual.values[key]<needed.values[key])return false;}
    return true;
}
const PcNetworkReward* find(const PcNetworkClientState& state,uint64_t id){
    const auto begin=state.rewards,end=begin+state.reward_count;
    const auto found=std::lower_bound(begin,end,id,[](const PcNetworkReward& a,uint64_t value){return a.reward_id<value;});
    return found!=end&&found->reward_id==id?found:nullptr;
}
bool canonical_valid(const PcNetworkClientState& state){
    if(!facts_valid(state.facts)||state.reward_count>PC_NETWORK_REWARDS||state.revision==UINT64_MAX)return false;
    bool any=false;for(auto value:state.facts.values)any|=value!=0;
    if(!state.canonical_present)return !state.room_identity[0]&&!state.revision&&!state.reward_count&&!any;
    if(!hex(state.room_identity,32)||((state.revision==0)!=(!any))||state.revision<state.reward_count)return false;
    PcNetworkFacts steps{};uint64_t previous=0;
    for(unsigned i=0;i<state.reward_count;++i){const auto& claim=state.rewards[i];
        if(!pc_network_reward_valid(claim)||claim.reward_id<=previous)return false;previous=claim.reward_id;
        if(pc_permanent_server_authoritative(claim.key))steps.values[claim.key]=pc_permanent_operation_result(claim.key,steps.values[claim.key],claim.operation,claim.value);
        else if(state.facts.values[claim.key]<claim.value)return false;}
    for(uint16_t key=0;key<PC_NETWORK_FACTS;++key)if(pc_permanent_server_authoritative(key)&&steps.values[key]!=state.facts.values[key])return false;
    return true;
}
bool retained_pending(const PcNetworkClientState& state,const PcNetworkReward* claims,unsigned count){
    if(count>PC_NETWORK_QUEUE)return false;
    for(unsigned i=0;i<count;++i){const auto& claim=claims[i];if(!pc_network_reward_valid(claim))return false;
        for(unsigned j=0;j<i;++j)if(claims[j].reward_id==claim.reward_id)return false;
        if(const auto* known=find(state,claim.reward_id)){if(!same_reward(*known,claim))return false;continue;}
        bool pending=false;for(unsigned j=0;j<state.outgoing_count;++j)if(same_reward(state.outgoing[j],claim))pending=true;
        if(!pending)return false;}
    return true;
}
bool valid(const PcNetworkClientState& state){
    PcNetworkCompatibility expected{};pc_network_native_compatibility(&expected);
    if(!terminated(state.compatibility.game_id,7)||!terminated(state.compatibility.build_id,65)||!terminated(state.compatibility.semantic_digest,65)||
        !terminated(state.compatibility.rules_digest,65)||!same(expected,state.compatibility)||!terminated(state.profile_identity,65)||
        !hex(state.profile_identity,64)||!terminated(state.room_identity,33)||state.serial==UINT64_MAX||!canonical_valid(state)||state.outgoing_count>PC_NETWORK_QUEUE)return false;
    for(unsigned i=0;i<state.outgoing_count;++i){const auto& claim=state.outgoing[i];if(!pc_network_reward_valid(claim)||find(state,claim.reward_id))return false;
        for(unsigned j=0;j<i;++j)if(state.outgoing[j].reward_id==claim.reward_id)return false;}
    for(unsigned slot=0;slot<PC_NETWORK_CLIENT_SLOTS;++slot){const auto& intent=state.intents[slot];const auto& receipt=state.receipts[slot];
        if(intent.active&&(!state.canonical_present||!intent.token||intent.token>state.serial||intent.revision>state.revision||!facts_valid(intent.facts)||
            (!intent.provisional&&intent.pending_count)||!retained_pending(state,intent.pending,intent.pending_count)))return false;
        if(receipt.valid&&(!state.canonical_present||receipt.quest_slot!=slot||!receipt.token||receipt.token>state.serial||receipt.revision>state.revision||
            !terminated(receipt.gci_sha256,65)||!hex(receipt.gci_sha256,64)||!facts_valid(receipt.facts)||
            (!receipt.provisional&&receipt.pending_count)||(receipt.provisional&&receipt.reconciled)||!retained_pending(state,receipt.pending,receipt.pending_count)))return false;}
    return true;
}
bool projected_facts(const PcNetworkClientState& state,PcNetworkFacts& facts){
    if(!state.canonical_present||!valid(state))return false;facts=state.facts;
    for(unsigned i=0;i<state.outgoing_count;++i){const auto& claim=state.outgoing[i];
        facts.values[claim.key]=pc_permanent_operation_result(claim.key,facts.values[claim.key],claim.operation,claim.value);}
    facts.values[PC_PERMANENT_ARROW_MAX]=normalized(PC_PERMANENT_ARROW_MAX,facts.values[PC_PERMANENT_ARROW_MAX]);
    facts.values[PC_PERMANENT_BOMB_MAX]=normalized(PC_PERMANENT_BOMB_MAX,facts.values[PC_PERMANENT_BOMB_MAX]);return true;
}
void put_facts(Bytes& b,const PcNetworkFacts& facts){unsigned count=0;for(auto value:facts.values)if(value)++count;put16(b,uint16_t(count));
    for(uint16_t key=0;key<PC_NETWORK_FACTS;++key)if(facts.values[key])delta(b,{key,facts.values[key]});}
bool get_facts(Reader& r,PcNetworkFacts& facts){const unsigned count=r.word();if(count>PC_NETWORK_FACTS)return false;int previous=-1;
    for(unsigned i=0;i<count;++i){auto d=delta(r);if(!r.ok||!pc_network_fact_valid(d)||d.key<=previous)return false;facts.values[d.key]=d.value;previous=d.key;}return true;}
Bytes encode(const PcNetworkClientState& state){
    Bytes b;identity(b,state.compatibility);string(b,state.profile_identity);string(b,state.room_identity);put64(b,state.serial);put64(b,state.revision);b.push_back(state.canonical_present?1:0);put_facts(b,state.facts);
    put16(b,uint16_t(state.reward_count));for(unsigned i=0;i<state.reward_count;++i)reward(b,state.rewards[i]);
    put16(b,uint16_t(state.outgoing_count));for(unsigned i=0;i<state.outgoing_count;++i)reward(b,state.outgoing[i]);
    for(const auto& intent:state.intents){b.push_back(intent.active?1:0);if(intent.active){b.push_back(intent.provisional?1:0);put64(b,intent.token);put64(b,intent.revision);put_facts(b,intent.facts);
        put16(b,uint16_t(intent.pending_count));for(unsigned i=0;i<intent.pending_count;++i)reward(b,intent.pending[i]);}}
    for(const auto& receipt:state.receipts){b.push_back(receipt.valid?1:0);if(receipt.valid){b.push_back(receipt.recovered?1:0);b.push_back(receipt.provisional?1:0);b.push_back(receipt.reconciled?1:0);
        put16(b,uint16_t(receipt.quest_slot));put64(b,receipt.token);put64(b,receipt.revision);string(b,receipt.gci_sha256);put_facts(b,receipt.facts);
        put16(b,uint16_t(receipt.pending_count));for(unsigned i=0;i<receipt.pending_count;++i)reward(b,receipt.pending[i]);}}
    Bytes file={'B','N','C','J'};put32(file,kVersion);put32(file,PC_NETWORK_PROTOCOL);put32(file,PC_NETWORK_SCHEMA);put32(file,uint32_t(b.size()));file.insert(file.end(),b.begin(),b.end());
    const auto hash=digest(std::string(file.begin(),file.end()));file.insert(file.end(),hash.begin(),hash.end());return file;
}
bool decode(const Bytes& bytes,PcNetworkClientState& state){
    if(bytes.size()<84||bytes.size()>PC_NETWORK_CLIENT_MAX_FILE||std::memcmp(bytes.data(),"BNCJ",4))return false;
    Bytes header(bytes.begin()+4,bytes.begin()+20);Reader h{header};if(h.dword()!=kVersion||h.dword()!=PC_NETWORK_PROTOCOL||h.dword()!=PC_NETWORK_SCHEMA||h.dword()!=bytes.size()-84||!h.done())return false;
    auto hash=digest(std::string(bytes.begin(),bytes.end()-64));if(!std::equal(hash.begin(),hash.end(),bytes.end()-64))return false;
    Bytes body(bytes.begin()+20,bytes.end()-64);Reader r{body};PcNetworkClientState next{};
    if(!identity(r,next.compatibility)||!r.text(next.profile_identity,65)||!r.text(next.room_identity,33))return false;
    next.serial=r.qword();next.revision=r.qword();auto present=r.byte();if(present>1)return false;next.canonical_present=present!=0;
    if(!get_facts(r,next.facts))return false;next.reward_count=r.word();if(next.reward_count>PC_NETWORK_REWARDS)return false;
    for(unsigned i=0;i<next.reward_count;++i)next.rewards[i]=reward(r);
    next.outgoing_count=r.word();if(next.outgoing_count>PC_NETWORK_QUEUE)return false;for(unsigned i=0;i<next.outgoing_count;++i)next.outgoing[i]=reward(r);
    for(auto& intent:next.intents){auto active=r.byte();if(active>1)return false;intent.active=active!=0;
        if(intent.active){auto provisional=r.byte();if(provisional>1)return false;intent.provisional=provisional!=0;intent.token=r.qword();intent.revision=r.qword();if(!get_facts(r,intent.facts))return false;
            intent.pending_count=r.word();if(intent.pending_count>PC_NETWORK_QUEUE)return false;for(unsigned i=0;i<intent.pending_count;++i)intent.pending[i]=reward(r);}}
    for(auto& receipt:next.receipts){auto active=r.byte();if(active>1)return false;receipt.valid=active!=0;
        if(receipt.valid){auto recovered=r.byte(),provisional=r.byte(),reconciled=r.byte();if(recovered>1||provisional>1||reconciled>1)return false;
            receipt.recovered=recovered!=0;receipt.provisional=provisional!=0;receipt.reconciled=reconciled!=0;receipt.quest_slot=r.word();receipt.token=r.qword();receipt.revision=r.qword();
            if(!r.text(receipt.gci_sha256,65)||!get_facts(r,receipt.facts))return false;receipt.pending_count=r.word();if(receipt.pending_count>PC_NETWORK_QUEUE)return false;
            for(unsigned i=0;i<receipt.pending_count;++i)receipt.pending[i]=reward(r);}}
    if(!r.done()||!next.serial||!valid(next))return false;state=next;return true;
}
bool read_journal(const fs::path& path,Bytes& bytes){
    if(!node(path,false))return false;std::error_code ec;const auto n=fs::file_size(io(path),ec);if(ec||n>PC_NETWORK_CLIENT_MAX_FILE)return false;
    FILE* file=pc_net_atomic_open(utf8(path).c_str(),"rb");if(!file)return false;bytes.resize(size_t(n));
    const bool ok=std::fread(bytes.data(),1,bytes.size(),file)==bytes.size()&&std::fgetc(file)==EOF&&!std::ferror(file);return std::fclose(file)==0&&ok;
}
FILE* exclusive_file(const fs::path& path){
#ifdef _WIN32
    int fd=_wopen(io(path).c_str(),_O_CREAT|_O_EXCL|_O_WRONLY|_O_BINARY,_S_IREAD|_S_IWRITE);if(fd<0)return nullptr;FILE* f=_fdopen(fd,"wb");if(!f)_close(fd);return f;
#else
    int fd=::open(path.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return nullptr;FILE* f=fdopen(fd,"wb");if(!f)::close(fd);return f;
#endif
}
bool publish(const fs::path& pending,const fs::path& target,bool exists){
#ifdef _WIN32
    return pc_net_atomic_move(utf8(pending).c_str(),utf8(target).c_str(),exists)==0;
#else
    if(exists)return ::rename(pending.c_str(),target.c_str())==0;
    if(::link(pending.c_str(),target.c_str()))return false;return ::unlink(pending.c_str())==0;
#endif
}
}
struct PcNetworkClientStore {
    fs::path root;Pins pins;PcNetworkClientState state{};std::string checksum;bool exists=false,healthy=true;
#ifdef _WIN32
    HANDLE lease=INVALID_HANDLE_VALUE;
    ~PcNetworkClientStore(){if(lease!=INVALID_HANDLE_VALUE)CloseHandle(lease);}
#else
    int lease=-1;
    ~PcNetworkClientStore(){if(lease>=0)::close(lease);}
#endif
    bool persist(PcNetworkClientState& next,char* error,unsigned capacity){
        if(!healthy)return fail(error,capacity,"Native client journal unavailable");
        if(state.serial>=UINT64_MAX-1)return fail(error,capacity,"Native client journal serial exhausted");
        next.serial=state.serial+1;if(!valid(next))return fail(error,capacity,"Invalid native client journal transaction");
        try {
            const auto target=root/"client.bwjournal";if(!directories(root,false)){healthy=false;return fail(error,capacity,"Native client directory changed");}
            if(exists){Bytes before;if(!read_journal(target,before)||digest(std::string(before.begin(),before.end()))!=checksum){healthy=false;return fail(error,capacity,"Native client journal changed outside its owner");}}
            else if(!absent(target)){healthy=false;return fail(error,capacity,"Native client journal unexpectedly appeared");}
            const auto bytes=encode(next);if(bytes.size()>PC_NETWORK_CLIENT_MAX_FILE)return fail(error,capacity,"Native client journal bound exceeded");
            const auto pending=root/("client.bwjournal.tmp-"+nonce());FILE* f=exclusive_file(pending);
            if(!f){healthy=false;return fail(error,capacity,"Cannot create native client transaction");}
            bool ok=std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size()&&pc_net_atomic_flush(f);if(std::fclose(f))ok=false;
            if(ok)ok=publish(pending,target,exists);if(ok)ok=sync_dir(root);
            // Preserve failed private pending evidence; never discard the last
            // acknowledged outbox/file or enable an in-memory fallback.
            if(!ok){healthy=false;return fail(error,capacity,"Cannot durably publish native client transaction");}
            state=next;checksum=digest(std::string(bytes.begin(),bytes.end()));exists=true;message(error,capacity,"");return true;
        }catch(...){healthy=false;return fail(error,capacity,"Cannot persist native client transaction");}
    }
};
extern "C" PcNetworkClientStore* pc_network_client_open(const char* directory,const PcNetworkCompatibility* compatibility,const char* profile,char* error,unsigned capacity){
    try {
        auto store=std::make_unique<PcNetworkClientStore>();if(!compatibility||!hex(profile,64)||!path_value(directory,store->root)){
            fail(error,capacity,"Invalid native client journal identity/path");return nullptr;}
        store->state.compatibility=*compatibility;std::strcpy(store->state.profile_identity,profile);if(!valid(store->state)){fail(error,capacity,"Unsupported native client compatibility");return nullptr;}
        if(!directories(store->root,true,&store->pins)||!sync_parents(store->root)){fail(error,capacity,"Unsafe native client directory");return nullptr;}
        const auto lock=store->root/"client.lock";
#ifdef _WIN32
        store->lease=CreateFileW(io(lock).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(store->lease==INVALID_HANDLE_VALUE||!node(lock,false)){fail(error,capacity,"Native client journal already owned or unsafe");return nullptr;}
#else
        store->lease=::open(lock.c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);struct stat info{};
        if(store->lease<0||fstat(store->lease,&info)||!S_ISREG(info.st_mode)||info.st_nlink!=1||flock(store->lease,LOCK_EX|LOCK_NB)){
            fail(error,capacity,"Native client journal already owned or unsafe");return nullptr;}
#endif
        unsigned entries=0;bool found=false;
        for(const auto& entry:fs::directory_iterator(io(store->root))){if(++entries>18||!node(entry.path(),false)){fail(error,capacity,"Unsafe or excessive native client files");return nullptr;}
            const auto name=utf8(entry.path().filename());if(name=="client.lock")continue;
            if(name=="client.bwjournal"){found=true;continue;}
            const std::string prefix="client.bwjournal.tmp-";
            if(name.size()!=prefix.size()+32||name.compare(0,prefix.size(),prefix)||!hex(name.c_str()+prefix.size(),32)||fs::file_size(io(entry.path()))>PC_NETWORK_CLIENT_MAX_FILE){
                fail(error,capacity,"Unknown native client journal file");return nullptr;}}
        if(found){Bytes bytes;PcNetworkClientState loaded{};
            if(!read_journal(store->root/"client.bwjournal",bytes)||!decode(bytes,loaded)||!same(loaded.compatibility,*compatibility)||std::strcmp(profile,loaded.profile_identity)){
                fail(error,capacity,"Malformed or incompatible native client journal");return nullptr;}
            store->state=loaded;store->checksum=digest(std::string(bytes.begin(),bytes.end()));store->exists=true;
        }else {auto first=store->state;if(!store->persist(first,error,capacity))return nullptr;}
        message(error,capacity,"");return store.release();
    }catch(...){fail(error,capacity,"Cannot open native client journal");return nullptr;}
}
extern "C" void pc_network_client_close(PcNetworkClientStore* store){delete store;}
extern "C" bool pc_network_client_copy(PcNetworkClientStore* store,PcNetworkClientState* out){if(!store||!out||!store->healthy)return false;*out=store->state;return true;}
extern "C" bool pc_network_client_projection(PcNetworkClientStore* store,PcNetworkClientProjection* out,char* error,unsigned capacity){
    if(!store||!store->healthy||!out)return fail(error,capacity,"Native replay requires a healthy durable client journal");
    PcNetworkClientProjection next{};if(!projected_facts(store->state,next.facts))return fail(error,capacity,"Native replay requires a bound canonical campaign");
    next.serial=store->state.serial;next.revision=store->state.revision;next.provisional=store->state.outgoing_count!=0;
    std::memcpy(next.room_identity,store->state.room_identity,33);*out=next;message(error,capacity,"");return true;
}
extern "C" bool pc_network_client_enqueue(PcNetworkClientStore* store,PcNetworkReward claim,char* error,unsigned capacity){
    if(!store||!store->healthy||!pc_network_reward_valid(claim))return fail(error,capacity,"Invalid completed native reward");
    if(const auto* known=find(store->state,claim.reward_id)){if(!same_reward(*known,claim))return fail(error,capacity,"Conflicting confirmed native reward");message(error,capacity,"");return true;}
    for(unsigned i=0;i<store->state.outgoing_count;++i)if(store->state.outgoing[i].reward_id==claim.reward_id){
        if(!same_reward(store->state.outgoing[i],claim))return fail(error,capacity,"Conflicting pending native reward");message(error,capacity,"");return true;}
    if(store->state.outgoing_count>=PC_NETWORK_QUEUE)return fail(error,capacity,"Native completed reward outbox full");
    auto next=store->state;next.outgoing[next.outgoing_count++]=claim;return store->persist(next,error,capacity);
}
extern "C" bool pc_network_client_stage(PcNetworkClientStore* store,const char* room,const PcNetworkFacts* facts,const PcNetworkReward* claims,unsigned count,uint64_t revision,char* error,unsigned capacity){
    if(!store||!store->healthy||!room||!hex(room,32)||!facts||count>PC_NETWORK_REWARDS||(count&&!claims))return fail(error,capacity,"Invalid received canonical state");
    const auto& old=store->state;if(old.room_identity[0]&&std::strcmp(old.room_identity,room))return fail(error,capacity,"Server room identity changed; campaign reconciliation required");
    auto next=old;next.canonical_present=true;std::strcpy(next.room_identity,room);next.facts=*facts;next.revision=revision;next.reward_count=count;
    if(count)std::copy(claims,claims+count,next.rewards);if(!canonical_valid(next))return fail(error,capacity,"Malformed received facts or authored ledger");
    if(old.canonical_present){
        if(revision<old.revision)return fail(error,capacity,"Received canonical revision regressed");
        for(uint16_t key=0;key<PC_NETWORK_FACTS;++key)if(next.facts.values[key]<old.facts.values[key])return fail(error,capacity,"Received permanent facts regressed");
        for(unsigned i=0;i<old.reward_count;++i){const auto* current=find(next,old.rewards[i].reward_id);if(!current||!same_reward(*current,old.rewards[i]))return fail(error,capacity,"Received authored ledger changed");}
        if(revision==old.revision&&(count!=old.reward_count||std::memcmp(&next.facts,&old.facts,sizeof next.facts)))return fail(error,capacity,"Received state changed without a revision");
    }
    next.outgoing_count=0;for(unsigned i=0;i<old.outgoing_count;++i){const auto& pending=old.outgoing[i];const auto* known=find(next,pending.reward_id);
        if(known){if(!same_reward(*known,pending))return fail(error,capacity,"Server receipt conflicts with durable local reward");}
        else next.outgoing[next.outgoing_count++]=pending;}
    return store->persist(next,error,capacity);
}
namespace {
bool prepare_save(PcNetworkClientStore* store,unsigned slot,const PcNetworkFacts* frozen,uint64_t revision,uint64_t* token,bool provisional,char* error,unsigned capacity){
    if(!store||!store->healthy||!frozen||!token||slot>=PC_NETWORK_CLIENT_SLOTS||!store->state.canonical_present||
        (!provisional&&store->state.outgoing_count)||revision!=store->state.revision)return fail(error,capacity,"Native save must reconcile durable room progress first");
    for(unsigned i=0;i<PC_NETWORK_CLIENT_SLOTS;++i)if(store->state.intents[i].active){
        if(error&&capacity)std::snprintf(error,capacity,"Native save slot%u has an intent requiring completion or qualified recovery",i+1);return false;}
    auto projected=store->state.facts;
    if(provisional&&!projected_facts(store->state,projected))return fail(error,capacity,"Invalid durable offline claim projection");
    if(!covers(*frozen,projected))return fail(error,capacity,"Native frozen facts do not match the durable claim projection");
    auto next=store->state;auto& intent=next.intents[slot];intent.active=true;intent.provisional=provisional;
    intent.token=next.serial+1;intent.revision=revision;intent.facts=*frozen;
    if(provisional){intent.pending_count=next.outgoing_count;std::copy(next.outgoing,next.outgoing+next.outgoing_count,intent.pending);}
    if(!store->persist(next,error,capacity))return false;*token=intent.token;return true;
}
}
extern "C" bool pc_network_client_prepare_save(PcNetworkClientStore* store,unsigned slot,const PcNetworkFacts* frozen,uint64_t revision,uint64_t* token,char* error,unsigned capacity){return prepare_save(store,slot,frozen,revision,token,false,error,capacity);}
extern "C" bool pc_network_client_prepare_offline_save(PcNetworkClientStore* store,unsigned slot,const PcNetworkFacts* frozen,uint64_t revision,uint64_t* token,char* error,unsigned capacity){return prepare_save(store,slot,frozen,revision,token,true,error,capacity);}
extern "C" bool pc_network_client_cancel_save(PcNetworkClientStore* store,uint64_t token,char* error,unsigned capacity){
    if(!store||!store->healthy||!token)return fail(error,capacity,"Invalid native save cancellation");auto next=store->state;
    for(auto& intent:next.intents)if(intent.active&&intent.token==token){intent={};return store->persist(next,error,capacity);}return fail(error,capacity,"Native save intent not found");
}
extern "C" bool pc_network_client_hash_gci(const char* text,char hash[65],char* error,unsigned capacity){
    if(!hash)return fail(error,capacity,"Missing native GCI hash output");
    try {
        fs::path path;if(!path_value(text,path)||!directories(path.parent_path(),false)||!node(path,false))return fail(error,capacity,"Unsafe native GCI hash source");
        Bytes bytes(kGciBytes);bool ok=false;
#ifdef _WIN32
        HANDLE file=CreateFileW(io(path).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(file==INVALID_HANDLE_VALUE)return fail(error,capacity,"Cannot lock native GCI hash source");BY_HANDLE_FILE_INFORMATION info{};LARGE_INTEGER size{};DWORD got=0;
        ok=GetFileInformationByHandle(file,&info)&&!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&info.nNumberOfLinks==1&&
            GetFileSizeEx(file,&size)&&size.QuadPart==kGciBytes&&ReadFile(file,bytes.data(),DWORD(bytes.size()),&got,nullptr)&&got==bytes.size();
        if(!CloseHandle(file))ok=false;
#else
        int fd=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return fail(error,capacity,"Cannot lock native GCI hash source");struct stat before{},after{};
        ok=fstat(fd,&before)==0&&S_ISREG(before.st_mode)&&before.st_nlink==1&&before.st_size==kGciBytes;size_t at=0;
        while(ok&&at<bytes.size()){ssize_t n=::read(fd,bytes.data()+at,bytes.size()-at);if(n<=0)ok=false;else at+=size_t(n);}
        ok=ok&&fstat(fd,&after)==0&&before.st_dev==after.st_dev&&before.st_ino==after.st_ino&&before.st_size==after.st_size&&before.st_mtime==after.st_mtime;
        if(::close(fd))ok=false;
#endif
        if(!ok||std::memcmp(bytes.data(),"GZLE01",6)||std::memcmp(bytes.data()+8,"gczelda\0",8)||bytes[0x38]!=0||bytes[0x39]!=12)return fail(error,capacity,"Native GCI hash source has incompatible structure or changed during read");
        const auto value=digest(std::string(bytes.begin(),bytes.end()));std::memcpy(hash,value.c_str(),65);message(error,capacity,"");return true;
    }catch(...){return fail(error,capacity,"Cannot hash native GCI source");}
}
namespace {
bool record(PcNetworkClientStore* store,unsigned slot,const char* path,bool recovered,const char* expected,char* error,unsigned capacity){
    char hash[65]{};if(!pc_network_client_hash_gci(path,hash,error,capacity))return false;
    if(expected&&std::strcmp(hash,expected))return fail(error,capacity,"Native GCI changed since the qualified load source");
    auto next=store->state;const auto intent=next.intents[slot];auto& receipt=next.receipts[slot];receipt={};receipt.valid=true;receipt.recovered=recovered;
    receipt.quest_slot=slot;receipt.token=intent.token;receipt.revision=intent.revision;receipt.facts=intent.facts;receipt.provisional=intent.provisional;
    receipt.pending_count=intent.pending_count;std::copy(intent.pending,intent.pending+intent.pending_count,receipt.pending);
    std::memcpy(receipt.gci_sha256,hash,65);next.intents[slot]={};
    return store->persist(next,error,capacity);
}
}
extern "C" bool pc_network_client_commit_save(PcNetworkClientStore* store,uint64_t token,const char* path,char* error,unsigned capacity){
    if(!store||!store->healthy||!token)return fail(error,capacity,"Invalid native save receipt");
    for(unsigned slot=0;slot<PC_NETWORK_CLIENT_SLOTS;++slot)if(store->state.intents[slot].active&&store->state.intents[slot].token==token)return record(store,slot,path,false,nullptr,error,capacity);
    return fail(error,capacity,"Native save receipt has no matching prepared intent");
}
extern "C" bool pc_network_client_recover_loaded(PcNetworkClientStore* store,unsigned slot,const char* path,const char* source,const PcNetworkFacts* loaded,bool* recovered,char* error,unsigned capacity){
    if(recovered)*recovered=false;if(!store||!store->healthy||slot>=PC_NETWORK_CLIENT_SLOTS||!source||!hex(source,64)||!loaded||!recovered)return fail(error,capacity,"Invalid fresh native load recovery evidence");
    const auto& intent=store->state.intents[slot];if(!intent.active){message(error,capacity,"");return true;}
    if(!covers(*loaded,intent.facts))return fail(error,capacity,"Fresh native load does not contain the prepared permanent facts");
    if(!record(store,slot,path,true,source,error,capacity))return false;*recovered=true;return true;
}
extern "C" bool pc_network_client_reconcile_loaded(PcNetworkClientStore* store,unsigned slot,const char* path,const char* source,const PcNetworkFacts* loaded,PcNetworkClientLoadDisposition* disposition,char* error,unsigned capacity){
    if(disposition)*disposition=PC_NETWORK_CLIENT_LOAD_NONE;
    if(!store||!store->healthy||slot>=PC_NETWORK_CLIENT_SLOTS||!source||!hex(source,64)||!loaded||!facts_valid(*loaded)||!disposition)
        return fail(error,capacity,"Invalid fresh native load disposition evidence");
    const auto& intent=store->state.intents[slot];if(!intent.active){message(error,capacity,"");return true;}
    if(covers(*loaded,intent.facts)){
        if(!record(store,slot,path,true,source,error,capacity))return false;*disposition=PC_NETWORK_CLIENT_LOAD_PERSISTED;return true;
    }
    // A higher STEP rank is not proof that the prepared rank was absent; it
    // may reflect later progress. Keep this ambiguous case for reconciliation.
    bool deficient=false;
    for(uint16_t key=0;key<PC_NETWORK_FACTS;++key){const auto actual=normalized(key,loaded->values[key]),needed=normalized(key,intent.facts.values[key]);
        if(pc_permanent_server_authoritative(key)&&actual>needed)return fail(error,capacity,"Fresh native STEP rank needs campaign reconciliation; intent retained");
        if(actual<needed)deficient=true;}
    if(!deficient)return fail(error,capacity,"Fresh native snapshot does not establish intent disposition");
    char hash[65]{};if(!pc_network_client_hash_gci(path,hash,error,capacity))return false;
    if(std::strcmp(hash,source))return fail(error,capacity,"Native GCI changed since the qualified load source");
    auto next=store->state;next.intents[slot]={};if(!store->persist(next,error,capacity))return false;
    *disposition=PC_NETWORK_CLIENT_LOAD_NOT_PRESENT;return true;
}
extern "C" bool pc_network_client_confirm_loaded(PcNetworkClientStore* store,unsigned slot,const char* path,const char* source,const PcNetworkFacts* loaded,bool* confirmed,char* error,unsigned capacity){
    if(confirmed)*confirmed=false;if(!store||!store->healthy||slot>=PC_NETWORK_CLIENT_SLOTS||!source||!hex(source,64)||!loaded||!confirmed)return fail(error,capacity,"Invalid provisional receipt reconciliation evidence");
    const auto& old=store->state.receipts[slot];if(!old.valid||!old.provisional){message(error,capacity,"");return true;}
    if(store->state.outgoing_count)return fail(error,capacity,"Completed native rewards still await room confirmation");
    if(store->state.intents[slot].active)return fail(error,capacity,"A newer native save intent still requires recovery");
    for(unsigned i=0;i<old.pending_count;++i){const auto* known=find(store->state,old.pending[i].reward_id);
        if(!known||!same_reward(*known,old.pending[i]))return fail(error,capacity,"Provisional native save still has unconfirmed authored rewards");}
    if(!covers(*loaded,store->state.facts))return fail(error,capacity,"Fresh native load does not contain reconciled canonical progress");
    char hash[65]{};if(!pc_network_client_hash_gci(path,hash,error,capacity))return false;
    if(std::strcmp(hash,source))return fail(error,capacity,"Native GCI changed since the qualified load source");
    auto next=store->state;auto& receipt=next.receipts[slot];receipt.provisional=false;receipt.reconciled=true;
    receipt.pending_count=0;receipt.revision=next.revision;receipt.facts=*loaded;std::memcpy(receipt.gci_sha256,hash,65);
    if(!store->persist(next,error,capacity))return false;*confirmed=true;return true;
}
