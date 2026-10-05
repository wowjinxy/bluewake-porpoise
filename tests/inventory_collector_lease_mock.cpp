// SYNTHETIC lease contract mock. Provides no file/image/native proof.
// Linked exclusively into the adapted bridge fixture, never the actual host
// or the independent Windows loaded-code test.
#include "inventory_collector_lease_mock.h"
#include <limits>
#include <mutex>
#include <thread>
struct BwIcLoadedCode {
    const StaticRecompModuleDesc* descriptor=nullptr;
    uint64_t generation=0;bool active=false,alive=false;
    std::thread::id thread;
};
namespace { BwIcLoadedCode slot;std::mutex mutex;uint64_t issued=0;
bool same(const BwIcLoadedCode* code,uint64_t generation){
    return code==&slot&&slot.active&&generation&&slot.generation==generation;
}
bool owned(const BwIcLoadedCode* code,uint64_t generation){return same(code,generation)&&slot.thread==std::this_thread::get_id();}
}
extern "C" BwIcLoadedCode* bw_ic_fixture_lease(const StaticRecompModuleDesc* descriptor,uint64_t* generation){
    if(!generation)return nullptr;*generation=0;std::lock_guard<std::mutex> lock(mutex);
    if(slot.active||!descriptor||issued==std::numeric_limits<uint64_t>::max())return nullptr;
    slot={descriptor,++issued,true,true,std::this_thread::get_id()};*generation=issued;return &slot;
}
extern "C" BwIcLoadedCode* bw_ic_code_prepare(const char*,const char*,const char*,uint64_t* generation)noexcept{if(generation)*generation=0;return nullptr;}
extern "C" const char* bw_ic_code_load_path(const BwIcLoadedCode*,uint64_t)noexcept{return nullptr;}
extern "C" bool bw_ic_code_bind(BwIcLoadedCode*,uint64_t,void*,const void*)noexcept{return false;}
extern "C" bool bw_ic_code_bind_descriptor(BwIcLoadedCode*,uint64_t,const StaticRecompModuleDesc*)noexcept{return false;}
extern "C" uint64_t bw_ic_code_generation(const BwIcLoadedCode* code)noexcept{
    try{std::lock_guard<std::mutex> lock(mutex);return code==&slot&&owned(code,slot.generation)&&slot.alive?slot.generation:0;}catch(...){return 0;}
}
extern "C" bool bw_ic_code_is_live(const BwIcLoadedCode* code,uint64_t generation,const StaticRecompModuleDesc* descriptor)noexcept{
    try{std::lock_guard<std::mutex> lock(mutex);return owned(code,generation)&&slot.alive&&descriptor&&slot.descriptor==descriptor;}catch(...){return false;}
}
extern "C" void bw_ic_code_revoke(BwIcLoadedCode* code,uint64_t generation)noexcept{
    try{std::lock_guard<std::mutex> lock(mutex);if(same(code,generation))slot.alive=false;}catch(...){}
}
extern "C" void bw_ic_code_destroy(BwIcLoadedCode* code,uint64_t generation)noexcept{
    try{std::lock_guard<std::mutex> lock(mutex);if(owned(code,generation)){slot.active=false;slot.alive=false;slot.descriptor=nullptr;}}catch(...){}
}
