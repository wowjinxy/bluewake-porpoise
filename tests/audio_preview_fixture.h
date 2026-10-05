#ifndef BW_PREVIEW_FIXTURE_H
#define BW_PREVIEW_FIXTURE_H
#ifdef NDEBUG
#undef NDEBUG
#endif
#ifdef rename
#undef rename
#endif
#include "audio_preview.h"
#include "audio_preview_test_api.h"
#include "audio_customization.h"
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
namespace fixture {
namespace fs=std::filesystem;
extern thread_local bool audio_call;
extern std::atomic<unsigned> audio_allocations,audio_frees;
inline void put16(std::vector<uint8_t>& b,uint16_t n){b.push_back(uint8_t(n));b.push_back(uint8_t(n>>8));}
inline void put32(std::vector<uint8_t>& b,uint32_t n){for(unsigned i=0;i<4;++i)b.push_back(uint8_t(n>>(i*8)));}
inline void tag(std::vector<uint8_t>& b,const char* s){b.insert(b.end(),s,s+4);}
inline void set16(std::vector<uint8_t>& b,size_t at,uint16_t n){b.at(at)=uint8_t(n);b.at(at+1)=uint8_t(n>>8);}
inline void set32(std::vector<uint8_t>& b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(n>>(i*8));}
inline std::vector<uint8_t> wav(const std::vector<int16_t>& samples,unsigned rate=32000){
    assert(samples.size()%2==0);std::vector<uint8_t> b;
    tag(b,"RIFF");put32(b,36u+static_cast<uint32_t>(samples.size()*2));tag(b,"WAVE");
    tag(b,"fmt ");put32(b,16);put16(b,1);put16(b,2);put32(b,rate);put32(b,rate*4);put16(b,4);put16(b,16);
    tag(b,"data");put32(b,static_cast<uint32_t>(samples.size()*2));for(auto v:samples)put16(b,static_cast<uint16_t>(v));return b;
}
inline void save(const fs::path& p,const std::vector<uint8_t>& b){std::ofstream out(p,std::ios::binary|std::ios::trunc);assert(out);out.write(reinterpret_cast<const char*>(b.data()),b.size());out.close();assert(out);}
inline std::vector<uint8_t> be(const std::vector<int16_t>& samples){std::vector<uint8_t> b;for(auto v:samples){auto n=static_cast<uint16_t>(v);b.push_back(uint8_t(n>>8));b.push_back(uint8_t(n));}return b;}
inline BwAudioPreviewStatus status(BwAudioPreview* p){BwAudioPreviewStatus s{};assert(bluewake_audio_preview_status(p,&s));return s;}
inline void wait(const std::function<bool()>& ready){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!ready()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
inline void ready(BwAudioPreview* p){wait([&]{auto s=status(p);return s.state==BW_PREVIEW_READY;});}
inline void failed(BwAudioPreview* p){wait([&]{auto s=status(p);return s.state==BW_PREVIEW_FAILED;});assert(status(p).error[0]);}
struct Destroy {void operator()(BwAudioPreview* p) const {bluewake_audio_preview_destroy(p);}};
using Handle=std::unique_ptr<BwAudioPreview,Destroy>;
inline Handle create(){Handle p(bluewake_audio_preview_create());assert(p);return p;}
inline bool play(BwAudioPreview* p,const fs::path& path){auto text=path.u8string();return bluewake_audio_preview_play_utf8(p,text.c_str());}
inline bool play_loop(BwAudioPreview* p,const fs::path& path,uint64_t a,uint64_t b){auto text=path.u8string();return bluewake_audio_preview_play_loop_utf8(p,text.c_str(),a,b);}
inline bool mix(BwAudioPreview* p,uint8_t* data,size_t bytes,unsigned rate=32000){
    const auto before_alloc=audio_allocations.load(),before_free=audio_frees.load();
    audio_call=true;const bool result=bluewake_audio_preview_mix_be16(p,data,bytes,rate);audio_call=false;
    assert(audio_allocations.load()==before_alloc&&audio_frees.load()==before_free);return result;
}
inline bool mix(BwAudioPreview* p,std::vector<uint8_t>& b,unsigned rate=32000){return mix(p,b.data(),b.size(),rate);}
inline void native_config(unsigned master=100,unsigned music=100,unsigned sfx=100,bool muted=false){assert(bluewake_audio_configure(master,music,sfx,muted));}
}
int run_copy_seam(const std::filesystem::path&);
#endif
