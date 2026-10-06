// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic-DLL test. The test owner must pin the DLL built
// solely from loaded_code_fixture_dll.cpp and its generated external policy.
#include "loaded_code_admission.h"
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
namespace {
unsigned checks=0;
#define CHECK(v) do { ++checks; if(!(v)) { std::fprintf(stderr,"line%d\n",__LINE__); std::abort(); } } while(0)
std::wstring wide(const char* text){
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,nullptr,0);CHECK(n>0);
    std::vector<wchar_t> b(std::size_t(n),0);CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,b.data(),n)==n);return b.data();
}
std::array<std::uint8_t,32> policy_artifact_hash(const char* path){
    // The fixture runner independently hashes the authored DLL and admits this
    // exact external policy. Parse its expected artifact field, not the getter.
    FILE* file=_wfopen(wide(path).c_str(),L"rb");CHECK(file);
    char bytes[1025]{};const auto count=std::fread(bytes,1,1024,file);
    CHECK(!std::ferror(file)&&std::fgetc(file)==EOF);CHECK(std::fclose(file)==0);
    const std::string text(bytes,count),prefix="BLUEWAKE_IC_MODULE_POLICY_V1\nartifact_sha256=";
    CHECK(text.compare(0,prefix.size(),prefix)==0);
    CHECK(text.size()>prefix.size()+64&&text[prefix.size()+64]=='\n');
    std::array<std::uint8_t,32> hash{};
    const auto digit=[](char c){return c>='0'&&c<='9'?unsigned(c-'0'):c>='a'&&c<='f'?unsigned(c-'a'+10):16u;};
    for(unsigned i=0;i<32;++i){
        const auto a=digit(text[prefix.size()+i*2]),b=digit(text[prefix.size()+i*2+1]);
        CHECK(a<16&&b<16);hash[i]=std::uint8_t(a*16+b);
    }
    return hash;
}
void hash_rejected(const BwIcLoadedCode* lease,std::uint64_t generation,const StaticRecompModuleDesc* descriptor){
    std::array<std::uint8_t,32> hash;hash.fill(0xA5);
    CHECK(!bw_ic_code_artifact_sha256(lease,generation,descriptor,hash.data()));
    for(const auto byte:hash)CHECK(byte==0);
}
void hash_matches(const BwIcLoadedCode* lease,std::uint64_t generation,const StaticRecompModuleDesc* descriptor,
                  const std::array<std::uint8_t,32>& expected){
    std::array<std::uint8_t,32> hash;hash.fill(0xA5);
    CHECK(bw_ic_code_artifact_sha256(lease,generation,descriptor,hash.data()));
    CHECK(hash==expected);
}
}
int main(int argc,char** argv){
    CHECK(argc==4); // synthetic DLL, external policy, root-approved policy hash
    const auto expected_hash=policy_artifact_hash(argv[2]);
    hash_rejected(nullptr,0,nullptr);
    hash_rejected(nullptr,1,reinterpret_cast<const StaticRecompModuleDesc*>(1));
    CHECK(!bw_ic_code_artifact_sha256(nullptr,0,nullptr,nullptr));
    std::uint64_t generation=123;
    CHECK(!bw_ic_code_prepare(nullptr,nullptr,nullptr,&generation)&&generation==0);
    CHECK(!bw_ic_code_prepare(argv[1],argv[2],argv[3],nullptr));
    std::string bad=argv[3];CHECK(bad.size()==64);bad[0]=bad[0]=='0'?'1':'0';
    CHECK(!bw_ic_code_prepare(argv[1],argv[2],bad.c_str(),&generation)&&generation==0);
    const auto path=wide(argv[1]);
    HANDLE writer=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    CHECK(writer!=INVALID_HANDLE_VALUE);
    CHECK(!bw_ic_code_prepare(argv[1],argv[2],argv[3],&generation)&&generation==0);
    CHECK(CloseHandle(writer));
    auto* first=bw_ic_code_prepare(argv[1],argv[2],argv[3],&generation);
    CHECK(first&&generation);const auto first_generation=generation;
    hash_rejected(first,generation,nullptr);
    hash_rejected(first,generation,reinterpret_cast<const StaticRecompModuleDesc*>(1));
    CHECK(!bw_ic_code_prepare(argv[1],argv[2],argv[3],&generation)&&generation==0);
    generation=first_generation;
    CHECK(bw_ic_code_load_path(first,generation));
    CHECK(!bw_ic_code_load_path(first,generation+1));
    writer=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    CHECK(writer==INVALID_HANDLE_VALUE); // retained module handle forbids write sharing
    const auto actual_path=wide(bw_ic_code_load_path(first,generation));
    HMODULE lib=LoadLibraryExW(actual_path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    CHECK(lib);auto getter=reinterpret_cast<StaticRecompGetModuleFn>(GetProcAddress(lib,STATICRECOMP_GET_MODULE_SYMBOL));CHECK(getter);
    CHECK(!bw_ic_code_bind(first,generation+1,lib,reinterpret_cast<const void*>(getter)));
    CHECK(bw_ic_code_bind(first,generation,lib,reinterpret_cast<const void*>(getter)));
    hash_rejected(first,generation,reinterpret_cast<const StaticRecompModuleDesc*>(1)); // no descriptor yet
    CHECK(!bw_ic_code_is_live(first,generation,nullptr));
    CHECK(!bw_ic_code_bind(first,generation,lib,reinterpret_cast<const void*>(getter)));
    const auto* descriptor=getter();CHECK(descriptor&&!std::memcmp(descriptor->game_id,"SYNTH\0\0\0",8));
    CHECK(!bw_ic_code_bind_descriptor(first,generation,reinterpret_cast<const StaticRecompModuleDesc*>(1)));
    CHECK(bw_ic_code_bind_descriptor(first,generation,descriptor));
    CHECK(bw_ic_code_generation(first)==generation);
    CHECK(bw_ic_code_is_live(first,generation,descriptor));
    hash_matches(first,generation,descriptor,expected_hash);
    // A caller owns its copy: editing it cannot mutate the cached artifact ID.
    std::array<std::uint8_t,32> caller_copy{};
    CHECK(bw_ic_code_artifact_sha256(first,generation,descriptor,caller_copy.data()));
    caller_copy.fill(0);hash_matches(first,generation,descriptor,expected_hash);
    CHECK(!bw_ic_code_artifact_sha256(first,generation,descriptor,nullptr));
    hash_rejected(first,0,descriptor);
    hash_rejected(first,generation+1,descriptor);
    hash_rejected(first,generation,nullptr);
    hash_rejected(first,generation,reinterpret_cast<const StaticRecompModuleDesc*>(1));
    CHECK(!bw_ic_code_is_live(first,generation+1,descriptor));
    CHECK(!bw_ic_code_is_live(first,generation,reinterpret_cast<const StaticRecompModuleDesc*>(1)));
    bool foreign=true;std::thread thread([&]{foreign=bw_ic_code_is_live(first,generation,descriptor);
        hash_rejected(first,generation,descriptor);});thread.join();CHECK(!foreign);
    bw_ic_code_revoke(first,generation+1);CHECK(bw_ic_code_is_live(first,generation,descriptor));
    bw_ic_code_destroy(first,generation+1);CHECK(bw_ic_code_is_live(first,generation,descriptor));
    bw_ic_code_revoke(first,generation);CHECK(!bw_ic_code_is_live(first,generation,descriptor));
    hash_rejected(first,generation,descriptor);
    bw_ic_code_destroy(first,generation);CHECK(!bw_ic_code_is_live(first,generation,descriptor));
    hash_rejected(first,generation,descriptor);
    auto* second=bw_ic_code_prepare(argv[1],argv[2],argv[3],&generation);CHECK(second&&generation>first_generation);
    CHECK(bw_ic_code_bind(second,generation,lib,reinterpret_cast<const void*>(getter)));
    CHECK(bw_ic_code_bind_descriptor(second,generation,descriptor));
    hash_matches(second,generation,descriptor,expected_hash);
    hash_rejected(second,first_generation,descriptor);
    hash_rejected(first,first_generation,descriptor); // allocator may reuse the old lease address
    CHECK(!bw_ic_code_is_live(second,first_generation,descriptor));
    bw_ic_code_revoke(second,first_generation);CHECK(bw_ic_code_is_live(second,generation,descriptor));
    bw_ic_code_destroy(second,generation);
    hash_rejected(second,generation,descriptor);
    auto* wrong=bw_ic_code_prepare(argv[1],argv[2],argv[3],&generation);CHECK(wrong);
    HMODULE system=GetModuleHandleW(L"kernel32.dll");CHECK(system);
    CHECK(!bw_ic_code_bind(wrong,generation,system,reinterpret_cast<const void*>(GetProcAddress(system,"GetCurrentProcess"))));
    bw_ic_code_destroy(wrong,generation);
    auto* forwarded=bw_ic_code_prepare(argv[1],argv[2],argv[3],&generation);CHECK(forwarded);
    CHECK(!bw_ic_code_bind(forwarded,generation,lib,reinterpret_cast<const void*>(GetProcAddress(system,"GetCurrentProcess"))));
    bw_ic_code_destroy(forwarded,generation);
    CHECK(FreeLibrary(lib));
    // Actual unload/reload can reuse the image and lease addresses. Issued
    // generation still prevents a stale ID query from regaining admission.
    auto* reloaded=bw_ic_code_prepare(argv[1],argv[2],argv[3],&generation);CHECK(reloaded&&generation>first_generation);
    const auto reload_path=wide(bw_ic_code_load_path(reloaded,generation));
    HMODULE reload_lib=LoadLibraryExW(reload_path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    CHECK(reload_lib);auto reload_getter=reinterpret_cast<StaticRecompGetModuleFn>(GetProcAddress(reload_lib,STATICRECOMP_GET_MODULE_SYMBOL));CHECK(reload_getter);
    CHECK(bw_ic_code_bind(reloaded,generation,reload_lib,reinterpret_cast<const void*>(reload_getter)));
    const auto* reload_descriptor=reload_getter();CHECK(reload_descriptor);
    CHECK(bw_ic_code_bind_descriptor(reloaded,generation,reload_descriptor));
    hash_matches(reloaded,generation,reload_descriptor,expected_hash);
    hash_rejected(reloaded,first_generation,reload_descriptor);
    hash_rejected(first,first_generation,descriptor);
    bw_ic_code_destroy(reloaded,generation);hash_rejected(reloaded,generation,reload_descriptor);
    CHECK(FreeLibrary(reload_lib));
    std::printf("SYNTHETIC_WINDOWS_LOADED_CODE checks=%u; no game/native collector proof\n",checks);
}
