#include "audio_preview_fixture.h"
#include <cstdio>
#include <cstdlib>
#include <new>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
namespace fixture {
thread_local bool audio_call=false;
std::atomic<unsigned> audio_allocations{0},audio_frees{0};
}
#if __has_feature(address_sanitizer)
#include <sanitizer/allocator_interface.h>
static void allocator_observed(const volatile void*,size_t){if(fixture::audio_call)++fixture::audio_allocations;}
static void free_observed(const volatile void*){if(fixture::audio_call)++fixture::audio_frees;}
#else
void* operator new(std::size_t size){if(fixture::audio_call)++fixture::audio_allocations;void* p=std::malloc(size?size:1);if(!p)throw std::bad_alloc{};return p;}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p) noexcept {if(fixture::audio_call&&p)++fixture::audio_frees;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
#endif
using namespace fixture;

static int core(const fs::path& dir) {
    auto p=create();native_config();unsigned checks=0;
    auto off=be({1111,-2222,32767,-32768}),off_before=off;
    assert(!mix(p.get(),off)&&off==off_before&&status(p.get()).state==BW_PREVIEW_IDLE);++checks;
    assert(!bluewake_audio_preview_status(nullptr,nullptr));assert(!mix(nullptr,off));
    const auto a=dir/fs::u8path(u8"synthetic-☃.wav");save(a,wav({3000,-3000,4000,-4000}));
    assert(play(p.get(),a));ready(p.get());assert(status(p.get()).total_frames==2&&status(p.get()).sample_rate==32000);
    auto out=be({0,0,0,0,123,-456});assert(mix(p.get(),out));assert(out==be({3000,-3000,4000,-4000,123,-456}));
    auto s=status(p.get());assert(s.state==BW_PREVIEW_FINISHED&&s.played_frames==2&&s.cursor_frame==2);
    out=off_before;assert(!mix(p.get(),out)&&out==off_before);++checks;
    assert(play(p.get(),a));ready(p.get());native_config(50,50,0);
    out=be({1000,-1000});assert(mix(p.get(),out));assert(out==be({2500,-2500}));
    assert(bluewake_audio_customize_output_be16(out.data(),out.size()));assert(out==be({1250,-1250}));++checks;
    assert(play(p.get(),a));ready(p.get());native_config(100,100,0);
    out=be({32000,-32000});assert(mix(p.get(),out));assert(out==be({32767,-32768}));++checks;
    assert(play(p.get(),a));ready(p.get());native_config(100,0,100);
    out=off_before;assert(!mix(p.get(),out)&&out==off_before&&status(p.get()).played_frames==2);++checks;
    assert(play(p.get(),a));ready(p.get());native_config(100,100,100,true);
    out=be({1000,-1000});assert(mix(p.get(),out));assert(bluewake_audio_customize_output_be16(out.data(),out.size()));assert(out==be({0,0}));++checks;
    native_config();const auto b=dir/"48000.wav";save(b,wav({100,-200},48000));
    assert(play(p.get(),b));ready(p.get());out=off_before;assert(!mix(p.get(),out,32000)&&out==off_before);
    s=status(p.get());assert(s.state==BW_PREVIEW_RATE_MISMATCH&&s.played_frames==0&&s.observed_output_rate==32000);
    assert(mix(p.get(),out,48000)&&out==be({1211,-2422,32767,-32768}));++checks;
    assert(play(p.get(),a));ready(p.get());out=off_before;
    assert(!mix(p.get(),nullptr,4));assert(!mix(p.get(),out.data(),3));assert(!mix(p.get(),out.data(),0));
    assert(!mix(p.get(),out.data(),BW_PREVIEW_MAX_OUTPUT_BYTES+4));assert(out==off_before&&status(p.get()).played_frames==0);
    out=be({0,0});assert(mix(p.get(),out)&&out==be({3000,-3000}));++checks;
    const auto loop_file=dir/"loop.wav";save(loop_file,wav({10,-10,20,-20,30,-30,40,-40,50,-50,60,-60}));
    assert(play_loop(p.get(),loop_file,2,5));ready(p.get());out=be(std::vector<int16_t>(22));assert(mix(p.get(),out));
    assert(out==be({10,-10,20,-20,30,-30,40,-40,50,-50,30,-30,40,-40,50,-50,30,-30,40,-40,50,-50}));
    s=status(p.get());assert(s.state==BW_PREVIEW_PLAYING&&s.looping&&s.played_frames==11&&s.cursor_frame==2);++checks;
    assert(play_loop(p.get(),loop_file,1,2));ready(p.get());out=be(std::vector<int16_t>(8));assert(mix(p.get(),out));
    assert(out==be({10,-10,20,-20,20,-20,20,-20}));++checks;
    auto chunks=create();assert(play_loop(chunks.get(),loop_file,2,5));ready(chunks.get());
    auto first=be(std::vector<int16_t>(6)),second=be(std::vector<int16_t>(16));assert(mix(chunks.get(),first));assert(mix(chunks.get(),second));
    first.insert(first.end(),second.begin(),second.end());
    assert(play_loop(p.get(),loop_file,2,5));ready(p.get());out=be(std::vector<int16_t>(22));assert(mix(p.get(),out)&&out==first);++checks;
    assert(!play_loop(p.get(),loop_file,3,3));assert(status(p.get()).state==BW_PREVIEW_FAILED);
    assert(play_loop(p.get(),loop_file,0,7));failed(p.get());out=off_before;assert(!mix(p.get(),out)&&out==off_before);++checks;
    save(a,wav({3000,-3000}));assert(play(p.get(),a));ready(p.get());save(a,wav({700,-800}));
    out=be({0,0});assert(mix(p.get(),out)&&out==be({3000,-3000}));assert(play(p.get(),a));ready(p.get());
    out=be({0,0});assert(mix(p.get(),out)&&out==be({700,-800}));++checks;
    bluewake_audio_preview_stop(p.get());s=status(p.get());assert(s.state==BW_PREVIEW_STOPPED);out=off_before;assert(!mix(p.get(),out)&&out==off_before);++checks;
    const auto invalid=dir/"invalid.wav";const auto good=wav({111,-222});
    std::vector<std::vector<uint8_t>> malformed;
    auto value=good;value.pop_back();malformed.push_back(value);
    value=good;set32(value,4,UINT32_MAX);malformed.push_back(value);
    value=good;set32(value,40,UINT32_MAX);malformed.push_back(value);
    value=good;set16(value,20,3);malformed.push_back(value);
    value=good;set16(value,22,1);malformed.push_back(value);
    value=good;set16(value,34,24);malformed.push_back(value);
    value=good;set16(value,32,8);malformed.push_back(value);
    value=good;set32(value,24,44100);malformed.push_back(value);
    value=good;set32(value,28,0);malformed.push_back(value);
    value=good;value.insert(value.end(),good.begin()+36,good.end());set32(value,4,value.size()-8);malformed.push_back(value);
    for(const auto& raw:malformed){save(invalid,raw);assert(play(p.get(),invalid));failed(p.get());out=off_before;assert(!mix(p.get(),out)&&out==off_before);}++checks;
    value=good;value.insert(value.begin()+12,{'J','U','N','K',1,0,0,0,'x',0});set32(value,4,value.size()-8);
    save(invalid,value);assert(play(p.get(),invalid));ready(p.get());out=be({0,0});assert(mix(p.get(),out)&&out==be({111,-222}));++checks;
    value=good;value.insert(value.begin()+36,{0,0});set32(value,4,value.size()-8);set32(value,16,18);
    save(invalid,value);assert(play(p.get(),invalid));ready(p.get());out=be({0,0});assert(mix(p.get(),out)&&out==be({111,-222}));++checks;
    assert(play(p.get(),dir/"missing.wav"));failed(p.get());assert(play(p.get(),dir));failed(p.get());++checks;
    assert(!bluewake_audio_preview_play_utf8(p.get(),""));assert(!bluewake_audio_preview_play_utf8(p.get(),"\xc0\x80"));
    std::string too_long(4096,'x');assert(!bluewake_audio_preview_play_utf8(p.get(),too_long.c_str()));++checks;
    save(invalid,good);fs::resize_file(invalid,64u*1024u*1024u+1u);assert(play(p.get(),invalid));failed(p.get());fs::remove(invalid);++checks;
    bluewake_audio_preview_test_hold_decode(p.get(),true);assert(play(p.get(),a));
    wait([&]{return bluewake_audio_preview_test_decode_waiting(p.get());});const auto pending=status(p.get()).generation;
    bluewake_audio_preview_cancel(p.get());assert(status(p.get()).generation>pending&&status(p.get()).state==BW_PREVIEW_CANCELLED);
    bluewake_audio_preview_test_hold_decode(p.get(),false);out=off_before;assert(!mix(p.get(),out)&&out==off_before);++checks;
    bluewake_audio_preview_test_hold_decode(p.get(),true);assert(play(p.get(),a));wait([&]{return bluewake_audio_preview_test_decode_waiting(p.get());});
    bluewake_audio_preview_reset(p.get());assert(status(p.get()).state==BW_PREVIEW_RESET);bluewake_audio_preview_test_hold_decode(p.get(),false);
    out=off_before;assert(!mix(p.get(),out)&&out==off_before);++checks;
    bluewake_audio_preview_test_hold_decode(p.get(),true);assert(play(p.get(),a));wait([&]{return bluewake_audio_preview_test_decode_waiting(p.get());});
    bluewake_audio_preview_shutdown(p.get());s=status(p.get());assert(s.state==BW_PREVIEW_SHUTDOWN&&!s.accepting_requests);
    assert(!play(p.get(),a));out=off_before;assert(!mix(p.get(),out)&&out==off_before);bluewake_audio_preview_shutdown(p.get());++checks;
    std::printf("Preview core: %u contract groups PASS; no device/native assets.\n",checks);return 0;
}

static int threads(const fs::path& dir) {
    native_config();const auto a=dir/"a.wav",b=dir/"b.wav";save(a,wav(std::vector<int16_t>(256,1000)));save(b,wav(std::vector<int16_t>(256,2000)));
    auto p=create();assert(play_loop(p.get(),a,0,128));ready(p.get());
    bluewake_audio_preview_test_hold_mix(p.get(),true);auto old=be(std::vector<int16_t>(16)),old_before=old;
    std::thread audio([&]{assert(!mix(p.get(),old));});wait([&]{return bluewake_audio_preview_test_mix_waiting(p.get());});
    assert(play_loop(p.get(),b,0,128));ready(p.get()); // Worker retires hazardous old asset, cannot free it here.
    bluewake_audio_preview_test_hold_mix(p.get(),false);audio.join();assert(old==old_before);
    auto out=be(std::vector<int16_t>(16));assert(mix(p.get(),out)&&out==be(std::vector<int16_t>(16,2000)));
    bluewake_audio_preview_test_hold_decode(p.get(),true);assert(play(p.get(),a));wait([&]{return bluewake_audio_preview_test_decode_waiting(p.get());});
    const auto first=status(p.get()).generation;assert(play(p.get(),b));assert(status(p.get()).generation>first);
    bluewake_audio_preview_test_hold_decode(p.get(),false);ready(p.get());out=be({0,0});assert(mix(p.get(),out)&&out==be({2000,2000}));
    std::atomic<bool> done{false};std::atomic<unsigned> calls{0};
    std::atomic<bool> monitoring{true};
    std::thread observer([&]{uint64_t previous=0;while(monitoring.load()){
        const auto s=status(p.get());assert(s.generation>=previous);previous=s.generation;
        if(s.state==BW_PREVIEW_READY||s.state==BW_PREVIEW_PLAYING){assert(s.sample_rate==32000&&s.total_frames==128);}
    }});
    std::thread consumer([&]{std::array<uint8_t,32> chunk{};while(!done.load()){
        chunk.fill(0);mix(p.get(),chunk.data(),chunk.size());for(size_t i=0;i<chunk.size();i+=2){const int n=(chunk[i]<<8)|chunk[i+1];assert(n==0||n==1000||n==2000);}++calls;
    }});
    for(unsigned i=0;i<300;++i){if(i%5==0)bluewake_audio_preview_stop(p.get());else if(i%7==0)bluewake_audio_preview_reset(p.get());else assert(play_loop(p.get(),i%2?a:b,0,128));
        if(i%17==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(play_loop(p.get(),a,0,128));wait([&]{auto s=status(p.get());return s.state==BW_PREVIEW_READY||s.state==BW_PREVIEW_PLAYING;});
    done.store(true);consumer.join();monitoring.store(false);observer.join();assert(calls.load()>0&&audio_allocations.load()==0&&audio_frees.load()==0);
    bluewake_audio_preview_test_hold_mix(p.get(),true);out=be({0,0});const auto before=out;
    std::thread held([&]{assert(!mix(p.get(),out));});wait([&]{return bluewake_audio_preview_test_mix_waiting(p.get());});
    std::thread closer([&]{bluewake_audio_preview_shutdown(p.get());});wait([&]{return status(p.get()).state==BW_PREVIEW_SHUTDOWN;});
    bluewake_audio_preview_test_hold_mix(p.get(),false);held.join();closer.join();assert(out==before);assert(!play(p.get(),a));
    std::puts("Preview threads: hazard retirement, last request, 300 concurrent generations, reset and shutdown PASS; audio allocations/frees=0.");return 0;
}

int main(int argc,char** argv) {
#if __has_feature(address_sanitizer)
    assert(__sanitizer_install_malloc_and_free_hooks(allocator_observed,free_observed)>0);
#endif
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    assert(argc==3);const auto dir=fs::u8path(argv[2]);fs::create_directories(dir);
    if(std::strcmp(argv[1],"core")==0)return core(dir);
    if(std::strcmp(argv[1],"threads")==0)return threads(dir);
    if(std::strcmp(argv[1],"seam")==0)return run_copy_seam(dir);
    assert(false);return 1;
}
