// SPDX-License-Identifier: GPL-3.0-or-later
#include "randomizer_store.h"
#include "network_digest.h"
#include "atomic_file.h"
#ifdef rename
#undef rename
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <thread>
#include <fcntl.h>
#ifndef _WIN32
#include <sys/file.h>
#endif

namespace bluewake::randomizer::storage {
namespace {
namespace fs = std::filesystem;
constexpr std::size_t HeaderBytes = 4 + 4 + 8 + 64 + 64 + 4 + 4;
constexpr std::size_t MaxFileBytes = HeaderBytes + Store::MaxCardBytes + Store::MaxLedgerBytes + 64;
bool hex(const std::string& s) { return s.size()==64 && s.find_first_not_of("0123456789abcdef")==std::string::npos; }
std::string digest(const Bytes& bytes) { return bw_net::digest(std::string(bytes.begin(),bytes.end())); }
void put(Bytes& bytes,std::uint64_t value,unsigned count) {
    for(unsigned i=count;i;--i)bytes.push_back(static_cast<std::uint8_t>(value>>(8*(i-1))));
}
std::uint64_t get(const Bytes& bytes,std::size_t at,unsigned count) {
    std::uint64_t value=0;for(unsigned i=0;i<count;++i)value=(value<<8)|bytes[at+i];return value;
}
Bytes encode(const Generation& g) {
    Bytes out={'B','W','S','G'};put(out,1,4);put(out,g.number,8);
    out.insert(out.end(),g.profile.begin(),g.profile.end());out.insert(out.end(),g.origin_card.begin(),g.origin_card.end());
    put(out,g.card.size(),4);put(out,g.ledger.size(),4);
    out.insert(out.end(),g.card.begin(),g.card.end());out.insert(out.end(),g.ledger.begin(),g.ledger.end());
    const auto checksum=digest(out);out.insert(out.end(),checksum.begin(),checksum.end());return out;
}
bool decode(const Bytes& bytes,Generation& out) {
    if(bytes.size()<HeaderBytes+65 || bytes.size()>MaxFileBytes ||
       !std::equal(bytes.begin(),bytes.begin()+4,"BWSG") || get(bytes,4,4)!=1) return false;
    const auto card=get(bytes,144,4),ledger=get(bytes,148,4);
    if(!card || card>Store::MaxCardBytes || ledger>Store::MaxLedgerBytes ||
       bytes.size()!=HeaderBytes+card+ledger+64) return false;
    const std::string checksum(bytes.end()-64,bytes.end());
    if(!hex(checksum) || bw_net::digest(std::string(bytes.begin(),bytes.end()-64))!=checksum) return false;
    Generation next;next.number=get(bytes,8,8);
    next.profile.assign(bytes.begin()+16,bytes.begin()+80);next.origin_card.assign(bytes.begin()+80,bytes.begin()+144);
    if(!next.number || !hex(next.profile) || !hex(next.origin_card)) return false;
    next.card.assign(bytes.begin()+HeaderBytes,bytes.begin()+HeaderBytes+card);
    next.ledger.assign(bytes.begin()+HeaderBytes+card,bytes.end()-64);
    next.card_digest=digest(next.card);next.ledger_digest=digest(next.ledger);next.record_digest=digest(bytes);
    out=std::move(next);return true;
}
bool safe(const fs::path& path,bool directory) {
    std::error_code ec;const auto status=fs::symlink_status(path,ec);
    if(ec || fs::is_symlink(status)) return false;
#ifdef _WIN32
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) return false;
#endif
    return directory ? fs::is_directory(status) : fs::is_regular_file(status)&&fs::hard_link_count(path,ec)==1&&!ec;
}
bool present(const fs::path& path,bool& exists) {
    std::error_code ec;const auto status=fs::symlink_status(path,ec);
    if(ec==std::errc::no_such_file_or_directory || (!ec && status.type()==fs::file_type::not_found)) { exists=false;return true; }
    exists=true;return !ec && safe(path,false);
}
bool parents(const fs::path& path) {
    auto at=path.root_path();for(const auto& piece:path.relative_path()) {
        at/=piece;std::error_code ec;const auto status=fs::symlink_status(at,ec);
        if(ec==std::errc::no_such_file_or_directory || (!ec && status.type()==fs::file_type::not_found)) continue;
        if(ec || !safe(at,true)) return false;
    }return true;
}
bool sync_directory(const fs::path& directory) {
#ifdef _WIN32
    (void)directory;return true; // Atomic MoveFileExW publication uses WRITE_THROUGH.
#else
    const int fd=::open(directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(fd<0)return false;
    const bool synced=fsync(fd)==0;return ::close(fd)==0&&synced;
#endif
}
bool sync_parents(fs::path path) {
    for(;;){if(!sync_directory(path))return false;const auto parent=path.parent_path();
        if(parent.empty()||parent==path)return true;path=parent;}
}
bool read(const fs::path& path,Bytes& bytes,std::size_t limit=MaxFileBytes) {
    if(!safe(path,false))return false;std::error_code ec;const auto count=fs::file_size(path,ec);
    if(ec||count>limit)return false;FILE* file=bw_atomic_open(path.u8string().c_str(),"rb");if(!file)return false;
    bytes.resize(static_cast<std::size_t>(count));
    const bool ok=std::fread(bytes.data(),1,bytes.size(),file)==bytes.size() && std::fgetc(file)==EOF && !std::ferror(file);
    return std::fclose(file)==0&&ok;
}
FILE* exclusive(const fs::path& path) {
#ifdef _WIN32
    const int fd=_wopen(path.c_str(),_O_CREAT|_O_EXCL|_O_WRONLY|_O_BINARY,_S_IREAD|_S_IWRITE);
    if(fd<0)return nullptr;FILE* file=_fdopen(fd,"wb");if(!file)_close(fd);return file;
#else
    const int fd=::open(path.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0)return nullptr;FILE* file=fdopen(fd,"wb");if(!file)::close(fd);return file;
#endif
}
bool publish(const fs::path& path,const Bytes& bytes) {
    bool exists;if(!present(path,exists))return false;
    static std::atomic<std::uint64_t> sequence{0};
    const auto pending=fs::u8path(path.u8string()+".pending-"+std::to_string(getpid())+"-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++sequence));
    FILE* file=exclusive(pending);if(!file)return false;
    bool ok=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size() && bw_atomic_flush(file);
    if(std::fclose(file)!=0)ok=false;
    if(!ok)return false; // Preserve the unacknowledged candidate for inspection.
#ifdef _WIN32
    if(bw_atomic_move(pending.u8string().c_str(),path.u8string().c_str(),exists)!=0)return false;
#else
    if(exists){if(std::rename(pending.c_str(),path.c_str())!=0)return false;}
    else {if(::link(pending.c_str(),path.c_str())!=0)return false;if(::unlink(pending.c_str())!=0)return false;}
#endif
    return sync_directory(path.parent_path());
}
#ifdef BLUEWAKE_RANDOMIZER_STORE_TEST
Fault fault=Fault::None;
bool fail(Fault phase){if(fault!=phase)return false;fault=Fault::None;return true;}
#endif
}
struct Store::Impl {
    fs::path root;std::string profile,previous_digest;Generation current;bool found=false,healthy=true;
    const std::thread::id owner=std::this_thread::get_id();
#ifdef _WIN32
    HANDLE lease=INVALID_HANDLE_VALUE;
    ~Impl(){if(lease!=INVALID_HANDLE_VALUE)CloseHandle(lease);}
#else
    int lease=-1;
    ~Impl(){if(lease>=0)::close(lease);}
#endif
    bool unchanged() const {
        if(owner!=std::this_thread::get_id())return false;
        if(!safe(root,true)||!parents(root))return false;
        bool exists;if(!present(root/"current.bwseed",exists)||exists!=found)return false;
        Bytes bytes;if(exists&&(!read(root/"current.bwseed",bytes)||digest(bytes)!=current.record_digest))return false;
        if(!present(root/"previous.bwseed",exists)||exists!=!previous_digest.empty())return false;
        return !exists||(read(root/"previous.bwseed",bytes)&&digest(bytes)==previous_digest);
    }
};
Store::Store()=default;
Store::~Store()=default;
bool Store::open(const char* directory,const std::string& profile,std::string& error) {
    if(impl_||!directory||!*directory||std::strlen(directory)>2048||!hex(profile)) { error="Invalid seed store or profile identity";return false; }
    try {
        auto next=std::make_unique<Impl>();next->root=fs::absolute(fs::u8path(directory)).lexically_normal();next->profile=profile;
        if(!parents(next->root)){error="Seed store contains an unsafe link";return false;}
        std::error_code ec;fs::create_directories(next->root,ec);
        if(ec||!safe(next->root,true)||!parents(next->root)||!sync_parents(next->root)){error="Cannot prepare seed store";return false;}
        const auto lock=next->root/"seed.lock";
#ifdef _WIN32
        next->lease=CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(next->lease==INVALID_HANDLE_VALUE||!safe(lock,false)){error="Seed store is locked or unsafe";return false;}
#else
        next->lease=::open(lock.c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);struct stat status{};
        if(next->lease<0||fstat(next->lease,&status)||!S_ISREG(status.st_mode)||status.st_nlink!=1||flock(next->lease,LOCK_EX|LOCK_NB)){
            error="Seed store is locked or unsafe";return false;}
#endif
        unsigned entries=0;for(const auto& entry:fs::directory_iterator(next->root)) {
            const auto name=entry.path().filename().u8string();
            const auto pending=[&](const char* stem){const std::string prefix=std::string(stem)+".pending-";
                return name.size()>prefix.size()&&name.size()<prefix.size()+80&&name.rfind(prefix,0)==0&&
                    name.substr(prefix.size()).find_first_not_of("0123456789-")==std::string::npos;};
            if(++entries>32||!safe(entry.path(),false)||
               (name!="seed.lock"&&name!="current.bwseed"&&name!="previous.bwseed"&&name!="working.card"&&
                !pending("current.bwseed")&&!pending("previous.bwseed")&&!pending("working.card"))){
                error="Unknown or unsafe files in seed store";return false;}
            if(fs::file_size(entry.path())>MaxFileBytes){error="Oversized seed store file";return false;}
        }
        if(!present(next->root/"current.bwseed",next->found)){error="Unsafe complete seed generation";return false;}
        if(next->found){Bytes bytes;if(!read(next->root/"current.bwseed",bytes)||!decode(bytes,next->current)||next->current.profile!=profile){
            error="Damaged generation or seed identity mismatch";return false;}}
        bool previous;if(!present(next->root/"previous.bwseed",previous)){error="Unsafe retained generation";return false;}
        if(previous){Bytes bytes;Generation retained;if(!next->found||!read(next->root/"previous.bwseed",bytes)||!decode(bytes,retained)||
            retained.profile!=profile||retained.origin_card!=next->current.origin_card||
            (retained.number!=next->current.number&&retained.number!=next->current.number-1)||
            (retained.number==next->current.number&&retained.record_digest!=next->current.record_digest)){
                error="Retained generation does not match the complete seed pair";return false;}
            next->previous_digest=digest(bytes);}
        impl_=std::move(next);error.clear();return true;
    } catch(...){error="Cannot open seed store";return false;}
}
bool Store::load(Generation& out,bool& found,std::string& error,CommitReceipt* receipt) const {
    out={};found=false;if(receipt)*receipt={};
    try {
        if(!impl_||!impl_->healthy||!impl_->unchanged()){error="Seed store changed or is unavailable";return false;}
        if(impl_->found){out=impl_->current;if(receipt)*receipt=CommitReceipt(out);found=true;}error.clear();return true;
    }catch(...){out={};if(receipt)*receipt={};error="Cannot read complete seed pair";return false;}
}
bool Store::commit(std::uint64_t expected,const Bytes& card,const Bytes& ledger,CommitReceipt& out,std::string& error) {
    out={};if(!impl_||!impl_->healthy||!impl_->unchanged()){error="Seed store changed or is unavailable";return false;}
    if(expected!=(impl_->found?impl_->current.number:0)||expected==std::numeric_limits<std::uint64_t>::max()||
       card.empty()||card.size()>MaxCardBytes||ledger.size()>MaxLedgerBytes){error="Stale or invalid seed generation";return false;}
    try {
        Generation next;next.profile=impl_->profile;next.number=expected+1;next.card=card;next.ledger=ledger;
        next.card_digest=digest(card);next.ledger_digest=digest(ledger);
        next.origin_card=impl_->found?impl_->current.origin_card:next.card_digest;
        const auto bytes=encode(next);next.record_digest=digest(bytes);
        if(impl_->found){Bytes before;if(!read(impl_->root/"current.bwseed",before)||!publish(impl_->root/"previous.bwseed",before)){
            impl_->healthy=false;error="Cannot retain the previous complete seed pair";return false;}
            impl_->previous_digest=digest(before);}
#ifdef BLUEWAKE_RANDOMIZER_STORE_TEST
        if(fail(Fault::BeforePublish)){error="Injected before complete pair publication";return false;}
#endif
        if(!publish(impl_->root/"current.bwseed",bytes)){impl_->healthy=false;error="Seed pair publication failed; keep recovery files";return false;}
#ifdef BLUEWAKE_RANDOMIZER_STORE_TEST
        if(fail(Fault::AfterPublish)){impl_->healthy=false;error="Injected unacknowledged publication";return false;}
#endif
        impl_->current=std::move(next);impl_->found=true;
        out=CommitReceipt(impl_->current);
        error.clear();return true;
    }catch(...){impl_->healthy=false;error="Seed pair publication failed; keep recovery files";return false;}
}
bool Store::restore_working_card(std::string& error) {
    if(!impl_||!impl_->healthy||!impl_->found||!impl_->unchanged()){error="No complete seed pair is available for recovery";return false;}
    try {if(!publish(impl_->root/"working.card",impl_->current.card)){impl_->healthy=false;error="Cannot recover the isolated working card";return false;}
        error.clear();return true;
    }catch(...){impl_->healthy=false;error="Cannot recover the isolated working card";return false;}
}
std::string Store::working_card_path() const { return impl_?(impl_->root/"working.card").u8string():std::string(); }
#ifdef BLUEWAKE_RANDOMIZER_STORE_TEST
void fail_next_commit(Fault value){fault=value;}
#endif
} // namespace bluewake::randomizer::storage
