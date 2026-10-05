// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic-DLL test. The test owner must pin the DLL built
// solely from loaded_code_fixture_dll.cpp and its generated external policy.
#include "loaded_code_admission.h"
#include <windows.h>
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
}
int main(int argc,char** argv){
    CHECK(argc==4); // synthetic DLL, external policy, root-approved policy hash
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
    CHECK(!bw_ic_code_is_live(first,generation,nullptr));
    CHECK(!bw_ic_code_bind(first,generation,lib,reinterpret_cast<const void*>(getter)));
    const auto* descriptor=getter();CHECK(descriptor&&!std::memcmp(descriptor->game_id,"SYNTH\0\0\0",8));
    CHECK(!bw_ic_code_bind_descriptor(first,generation,reinterpret_cast<const StaticRecompModuleDesc*>(1)));
    CHECK(bw_ic_code_bind_descriptor(first,generation,descriptor));
    CHECK(bw_ic_code_generation(first)==generation);
    CHECK(bw_ic_code_is_live(first,generation,descriptor));
    CHECK(!bw_ic_code_is_live(first,generation+1,descriptor));
    CHECK(!bw_ic_code_is_live(first,generation,reinterpret_cast<const StaticRecompModuleDesc*>(1)));
    bool foreign=true;std::thread thread([&]{foreign=bw_ic_code_is_live(first,generation,descriptor);});thread.join();CHECK(!foreign);
    bw_ic_code_revoke(first,generation+1);CHECK(bw_ic_code_is_live(first,generation,descriptor));
    bw_ic_code_destroy(first,generation+1);CHECK(bw_ic_code_is_live(first,generation,descriptor));
    bw_ic_code_revoke(first,generation);CHECK(!bw_ic_code_is_live(first,generation,descriptor));
    bw_ic_code_destroy(first,generation);CHECK(!bw_ic_code_is_live(first,generation,descriptor));
    auto* second=bw_ic_code_prepare(argv[1],argv[2],argv[3],&generation);CHECK(second&&generation>first_generation);
    CHECK(bw_ic_code_bind(second,generation,lib,reinterpret_cast<const void*>(getter)));
    CHECK(bw_ic_code_bind_descriptor(second,generation,descriptor));
    CHECK(!bw_ic_code_is_live(second,first_generation,descriptor));
    bw_ic_code_revoke(second,first_generation);CHECK(bw_ic_code_is_live(second,generation,descriptor));
    bw_ic_code_destroy(second,generation);
    auto* wrong=bw_ic_code_prepare(argv[1],argv[2],argv[3],&generation);CHECK(wrong);
    HMODULE system=GetModuleHandleW(L"kernel32.dll");CHECK(system);
    CHECK(!bw_ic_code_bind(wrong,generation,system,reinterpret_cast<const void*>(GetProcAddress(system,"GetCurrentProcess"))));
    bw_ic_code_destroy(wrong,generation);
    auto* forwarded=bw_ic_code_prepare(argv[1],argv[2],argv[3],&generation);CHECK(forwarded);
    CHECK(!bw_ic_code_bind(forwarded,generation,lib,reinterpret_cast<const void*>(GetProcAddress(system,"GetCurrentProcess"))));
    bw_ic_code_destroy(forwarded,generation);
    CHECK(FreeLibrary(lib));
    std::printf("SYNTHETIC_WINDOWS_LOADED_CODE checks=%u; no game/native collector proof\n",checks);
}
