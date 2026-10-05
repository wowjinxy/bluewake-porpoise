// SPDX-License-Identifier: GPL-3.0-or-later
// Startup-only Windows adapter. No module load or guest/native helper calls.
#include "loaded_code_admission.h"
#include "loaded_code_image.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
namespace {
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    ~Handle(){ if(value!=INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Identity { FILE_ID_INFO id{};std::uint64_t size=0; };
bool hex(const char* text,std::array<std::uint8_t,32>& out) {
    if(!text || std::strlen(text)!=64) return false;
    for(unsigned i=0;i<32;++i) {
        const auto digit=[](char c)->unsigned { return c>='0'&&c<='9'?unsigned(c-'0'):c>='a'&&c<='f'?unsigned(c-'a'+10):16; };
        const auto a=digit(text[i*2]),b=digit(text[i*2+1]);if(a>15||b>15) return false;out[i]=std::uint8_t(a*16+b);
    }
    return true;
}
bool wide(const char* path,std::wstring& out) {
    if(!path || !*path || std::strlen(path)>131068) return false;
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,nullptr,0);
    if(n<2 || n>32768) return false;
    std::vector<wchar_t> bytes(std::size_t(n),0);
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,bytes.data(),n)!=n) return false;
    out.assign(bytes.data());return true;
}
bool utf8(const std::wstring& path,std::string& out) {
    const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,path.c_str(),-1,nullptr,0,nullptr,nullptr);
    if(n<2 || n>131072) return false;
    std::vector<char> bytes(std::size_t(n),0);
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,path.c_str(),-1,bytes.data(),n,nullptr,nullptr)!=n) return false;
    out.assign(bytes.data());return true;
}
bool identity(HANDLE handle,Identity& out) {
    FILE_STANDARD_INFO info{};FILE_ATTRIBUTE_TAG_INFO attr{};
    if(GetFileType(handle)!=FILE_TYPE_DISK ||
       !GetFileInformationByHandleEx(handle,FileIdInfo,&out.id,sizeof out.id) ||
       !GetFileInformationByHandleEx(handle,FileStandardInfo,&info,sizeof info) ||
       !GetFileInformationByHandleEx(handle,FileAttributeTagInfo,&attr,sizeof attr) ||
       info.Directory || info.EndOfFile.QuadPart<=0 || (attr.FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    out.size=std::uint64_t(info.EndOfFile.QuadPart);return out.size<=bw_ic_code::kMaxFile;
}
bool same(const Identity& a,const Identity& b) {
    return a.size==b.size && a.id.VolumeSerialNumber==b.id.VolumeSerialNumber &&
        !std::memcmp(a.id.FileId.Identifier,b.id.FileId.Identifier,sizeof a.id.FileId.Identifier);
}
bool open_read(const std::wstring& path,Handle& out,Identity& id) {
    out.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    return out.value!=INVALID_HANDLE_VALUE && identity(out.value,id);
}
bool read_all(HANDLE handle,std::uint64_t size,std::vector<std::uint8_t>& out) {
    if(!size || size>bw_ic_code::kMaxFile) return false;
    LARGE_INTEGER zero{};if(!SetFilePointerEx(handle,zero,nullptr,FILE_BEGIN)) return false;
    out.resize(std::size_t(size));std::size_t off=0;
    while(off<out.size()) {
        DWORD got=0;const DWORD want=DWORD(std::min<std::size_t>(1024*1024,out.size()-off));
        if(!ReadFile(handle,out.data()+off,want,&got,nullptr) || got!=want) return false;off+=got;
    }
    std::uint8_t extra;DWORD got=0;return ReadFile(handle,&extra,1,&got,nullptr) && !got;
}
bool digest(const std::uint8_t* bytes,std::size_t size,std::array<std::uint8_t,32>& out) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    struct Cleanup {
        BCRYPT_ALG_HANDLE& algorithm;BCRYPT_HASH_HANDLE& hash;
        ~Cleanup(){if(hash) BCryptDestroyHash(hash);if(algorithm) BCryptCloseAlgorithmProvider(algorithm,0);}
    } cleanup{algorithm,hash};
    DWORD object_size=0,hash_size=0,got=0;
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if(ok) ok=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_size),sizeof object_size,&got,0)>=0 && object_size<=65536 &&
        BCryptGetProperty(algorithm,BCRYPT_HASH_LENGTH,reinterpret_cast<PUCHAR>(&hash_size),sizeof hash_size,&got,0)>=0 && hash_size==out.size();
    std::vector<std::uint8_t> object(object_size);
    if(ok) ok=BCryptCreateHash(algorithm,&hash,object.data(),object_size,nullptr,0,0)>=0;
    for(std::size_t off=0;ok&&off<size;) {
        const ULONG count=ULONG(std::min<std::size_t>(1024*1024,size-off));
        ok=BCryptHashData(hash,const_cast<PUCHAR>(bytes+off),count,0)>=0;off+=count;
    }
    if(ok) ok=BCryptFinishHash(hash,out.data(),DWORD(out.size()),0)>=0;
    return ok;
}
bool decimal(const std::string& text,std::uint64_t& out) {
    if(text.empty()) return false;out=0;
    for(char c:text) { if(c<'0'||c>'9'||out>(std::numeric_limits<std::uint64_t>::max()-unsigned(c-'0'))/10) return false;out=out*10+unsigned(c-'0'); }
    return out>0 && out<=bw_ic_code::kMaxFile;
}
bool policy(const std::vector<std::uint8_t>& bytes,std::array<std::uint8_t,32>& artifact,std::uint64_t& size) {
    const std::string value(bytes.begin(),bytes.end()),prefix="BLUEWAKE_IC_MODULE_POLICY_V1\nartifact_sha256=";
    if(value.compare(0,prefix.size(),prefix)) return false;
    const auto mid=prefix.size()+64;
    if(value.size()<mid+17 || value.compare(mid,15,"\nartifact_size=") || value.back()!='\n') return false;
    return hex(value.substr(prefix.size(),64).c_str(),artifact) && decimal(value.substr(mid+15,value.size()-mid-16),size);
}
bool final_path(HANDLE handle,std::wstring& out) {
    const DWORD n=GetFinalPathNameByHandleW(handle,nullptr,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!n||n>=32768) return false;std::vector<wchar_t> bytes(std::size_t(n)+1,0);
    const DWORD got=GetFinalPathNameByHandleW(handle,bytes.data(),DWORD(bytes.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!got||got>=bytes.size()) return false;out.assign(bytes.data(),got);return true;
}
bool module_path(HMODULE module,std::wstring& out) {
    std::vector<wchar_t> bytes(32768,0);const DWORD n=GetModuleFileNameW(module,bytes.data(),DWORD(bytes.size()));
    if(!n||n>=bytes.size()) return false;out.assign(bytes.data(),n);return true;
}
bool image_range(HMODULE module,std::uint32_t image_size,const void* pointer,std::size_t size,bool execute) {
    const auto base=reinterpret_cast<std::uintptr_t>(module),address=reinterpret_cast<std::uintptr_t>(pointer);
    if(!size || address<base || address-base>image_size || size>image_size-(address-base)) return false;
    std::size_t done=0;
    while(done<size) {
        MEMORY_BASIC_INFORMATION info{};
        if(VirtualQuery(reinterpret_cast<const void*>(address+done),&info,sizeof info)!=sizeof info || info.State!=MEM_COMMIT ||
           info.Type!=MEM_IMAGE || info.AllocationBase!=module || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return false;
        const DWORD access=info.Protect&0xff;
        const bool readable=access==PAGE_READONLY||access==PAGE_READWRITE||access==PAGE_WRITECOPY||access==PAGE_EXECUTE_READ||access==PAGE_EXECUTE_READWRITE||access==PAGE_EXECUTE_WRITECOPY;
        if(!readable || (execute && access!=PAGE_EXECUTE_READ)) return false;
        const auto region=reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if(region>address+done || info.RegionSize<=address+done-region) return false;
        const auto available=info.RegionSize-(address+done-region);done+=std::min<std::size_t>(available,size-done);
    }
    return true;
}
bool copied(const void* source,void* target,std::size_t size) {
    SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),source,target,size,&got) && got==size;
}
} // namespace
struct BwIcLoadedCode {
    const std::thread::id thread=std::this_thread::get_id();
    std::uint64_t generation=0;bool alive=true,bound=false;
    Handle file;Identity file_id;std::vector<std::uint8_t> file_bytes;std::string load_path;
    HMODULE actual=nullptr,reference=nullptr;const void* getter=nullptr;
    const StaticRecompModuleDesc* descriptor=nullptr;bw_ic_code::Image image;
    ~BwIcLoadedCode(){ if(reference) FreeLibrary(reference); }
};
namespace {
std::mutex mutex;BwIcLoadedCode* active=nullptr;std::uint64_t issued=0;
bool owned(const BwIcLoadedCode* lease) { return lease && active==lease && lease->thread==std::this_thread::get_id(); }
bool owned_generation(const BwIcLoadedCode* lease,std::uint64_t generation) { return generation && owned(lease) && lease->generation==generation; }
bool code_pointer(const BwIcLoadedCode& lease,const void* address) {
    const auto base=reinterpret_cast<std::uintptr_t>(lease.actual),p=reinterpret_cast<std::uintptr_t>(address);
    if(p<base || p-base>std::numeric_limits<std::uint32_t>::max()) return false;
    return bw_ic_code::executable_contains(lease.image,std::uint32_t(p-base),1) && image_range(lease.actual,lease.image.image_size,address,1,true);
}
bool verify_image(BwIcLoadedCode& lease) {
    if(!bw_ic_code::build_expected(lease.file_bytes.data(),lease.file_bytes.size(),reinterpret_cast<std::uintptr_t>(lease.actual),lease.image)) return false;
    std::vector<std::uint8_t> copy(1024*1024);
    for(const auto& code:lease.image.executable) {
        const auto* start=reinterpret_cast<const std::uint8_t*>(lease.actual)+code.rva;
        if(!image_range(lease.actual,lease.image.image_size,start,code.bytes.size(),true)) return false;
        for(std::size_t off=0;off<code.bytes.size();) {
            const auto count=std::min(copy.size(),code.bytes.size()-off);
            if(!copied(start+off,copy.data(),count) || std::memcmp(copy.data(),code.bytes.data()+off,count)) return false;off+=count;
        }
    }
    return true;
}
template<class T> bool table(const BwIcLoadedCode& lease,const T* pointer,std::uint32_t count,std::uint32_t limit) {
    return count<=limit && (!count || (pointer&&image_range(lease.actual,lease.image.image_size,pointer,std::size_t(count)*sizeof(T),false)));
}
} // namespace
extern "C" BwIcLoadedCode* bw_ic_code_prepare(const char* module,const char* policy_file,const char* approved,uint64_t* generation) noexcept {
    if(!generation) return nullptr;*generation=0;
    try {
        std::lock_guard<std::mutex> lock(mutex);if(active||issued==std::numeric_limits<std::uint64_t>::max()) return nullptr;
        std::array<std::uint8_t,32> approved_hash{},actual_hash{},artifact_hash{};
        if(!hex(approved,approved_hash)) return nullptr;
        std::wstring path;Handle policy_handle;Identity policy_id;
        if(!wide(policy_file,path)||!open_read(path,policy_handle,policy_id)||policy_id.size>1024) return nullptr;
        std::vector<std::uint8_t> policy_bytes;std::uint64_t artifact_size=0;
        if(!read_all(policy_handle.value,policy_id.size,policy_bytes)||!digest(policy_bytes.data(),policy_bytes.size(),actual_hash)||
            actual_hash!=approved_hash || !policy(policy_bytes,artifact_hash,artifact_size)) return nullptr;
        auto next=std::make_unique<BwIcLoadedCode>();
        if(!wide(module,path)||!open_read(path,next->file,next->file_id)||next->file_id.size!=artifact_size ||
            !read_all(next->file.value,artifact_size,next->file_bytes)||!digest(next->file_bytes.data(),next->file_bytes.size(),actual_hash)||actual_hash!=artifact_hash ||
            !final_path(next->file.value,path)||!utf8(path,next->load_path)) return nullptr;
        Identity after;if(!identity(next->file.value,after)||!same(after,next->file_id)) return nullptr;
        next->generation=++issued;active=next.release();*generation=active->generation;return active;
    } catch(...){ return nullptr; }
}
extern "C" const char* bw_ic_code_load_path(const BwIcLoadedCode* lease,uint64_t generation) noexcept {
    try { std::lock_guard<std::mutex> lock(mutex);return owned_generation(lease,generation)&&lease->alive&&!lease->bound?lease->load_path.c_str():nullptr; } catch(...){ return nullptr; }
}
extern "C" bool bw_ic_code_bind(BwIcLoadedCode* lease,uint64_t generation,void* lib,const void* getter) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex);if(!owned_generation(lease,generation)||!lease->alive||lease->bound||!lib||!getter) return false;
        lease->actual=static_cast<HMODULE>(lib);std::wstring path;Handle actual_file;Identity id;
        if(!module_path(lease->actual,path)||!open_read(path,actual_file,id)||!same(id,lease->file_id)) { lease->alive=false;return false; }
        HMODULE reference=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(getter),&reference)) { lease->alive=false;return false; }
        lease->reference=reference;lease->getter=getter;
        if(reference!=lease->actual||!verify_image(*lease)||!code_pointer(*lease,getter)) { lease->alive=false;return false; }
        lease->bound=true;return true;
    } catch(...){ bw_ic_code_revoke(lease,generation);return false; }
}
extern "C" bool bw_ic_code_bind_descriptor(BwIcLoadedCode* lease,uint64_t generation,const StaticRecompModuleDesc* descriptor) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex);
        if(!owned_generation(lease,generation)||!lease->alive||!lease->bound||lease->descriptor || !image_range(lease->actual,lease->image.image_size,descriptor,sizeof(*descriptor),false)) return false;
        StaticRecompModuleDesc d{};
        bool ok=copied(descriptor,&d,sizeof d)&&d.dispatch&&code_pointer(*lease,reinterpret_cast<const void*>(d.dispatch)) &&
            (!d.on_state_loaded||code_pointer(*lease,reinterpret_cast<const void*>(d.on_state_loaded))) &&
            table(*lease,d.code_ranges,d.num_code_ranges,1048576)&&table(*lease,d.smc_ranges,d.num_smc_ranges,1048576)&&
            table(*lease,d.chunk_ranges,d.num_chunk_ranges,1048576)&&table(*lease,d.chunk_hashes,d.num_chunk_ranges,1048576)&&
            table(*lease,d.rel_modules,d.num_rel_modules,4096);
        for(std::uint32_t i=0;ok&&i<d.num_rel_modules;++i) {
            StaticRecompRelModule rel{};ok=copied(d.rel_modules+i,&rel,sizeof rel)&&table(*lease,rel.sections,rel.num_sections,64);
        }
        if(!ok) { lease->alive=false;return false; }
        lease->descriptor=descriptor;
        // Approved file handle and immutable identity survive; large startup
        // parser/copy backing is released. Callback gates only use this lease.
        lease->file_bytes.clear();lease->file_bytes.shrink_to_fit();lease->image.executable.clear();
        return true;
    } catch(...){ bw_ic_code_revoke(lease,generation);return false; }
}
extern "C" uint64_t bw_ic_code_generation(const BwIcLoadedCode* lease) noexcept {
    try { std::lock_guard<std::mutex> lock(mutex);return owned(lease)&&lease->alive&&lease->descriptor?lease->generation:0; } catch(...){ return 0; }
}
extern "C" bool bw_ic_code_is_live(const BwIcLoadedCode* lease,uint64_t generation,const StaticRecompModuleDesc* descriptor) noexcept {
    try { std::lock_guard<std::mutex> lock(mutex);return owned(lease)&&lease->alive&&lease->bound&&generation&&lease->generation==generation&&descriptor&&lease->descriptor==descriptor; } catch(...){ return false; }
}
extern "C" void bw_ic_code_revoke(BwIcLoadedCode* lease,uint64_t generation) noexcept {
    try { std::lock_guard<std::mutex> lock(mutex);if(generation && active==lease && lease && lease->generation==generation) lease->alive=false; } catch(...){}
}
extern "C" void bw_ic_code_destroy(BwIcLoadedCode* lease,uint64_t generation) noexcept {
    try { std::lock_guard<std::mutex> lock(mutex);if(!owned_generation(lease,generation)) return;lease->alive=false;active=nullptr;delete lease; } catch(...){}
}
#else
extern "C" BwIcLoadedCode* bw_ic_code_prepare(const char*,const char*,const char*,uint64_t* g) noexcept { if(g)*g=0;return nullptr; }
extern "C" const char* bw_ic_code_load_path(const BwIcLoadedCode*,uint64_t) noexcept { return nullptr; }
extern "C" bool bw_ic_code_bind(BwIcLoadedCode*,uint64_t,void*,const void*) noexcept { return false; }
extern "C" bool bw_ic_code_bind_descriptor(BwIcLoadedCode*,uint64_t,const StaticRecompModuleDesc*) noexcept { return false; }
extern "C" uint64_t bw_ic_code_generation(const BwIcLoadedCode*) noexcept { return 0; }
extern "C" bool bw_ic_code_is_live(const BwIcLoadedCode*,uint64_t,const StaticRecompModuleDesc*) noexcept { return false; }
extern "C" void bw_ic_code_revoke(BwIcLoadedCode*,uint64_t) noexcept {}
extern "C" void bw_ic_code_destroy(BwIcLoadedCode*,uint64_t) noexcept {}
#endif
