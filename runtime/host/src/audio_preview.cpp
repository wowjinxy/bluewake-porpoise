// SPDX-License-Identifier: GPL-3.0-or-later
// External-file music preview. Decoder owns all PCM storage.
#ifdef rename
#undef rename  // Forced Windows POSIX compatibility must not rewrite filesystem.
#endif
#include "audio_preview.h"
#include "audio_customization.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr uint64_t max_file_bytes=64u*1024u*1024u;
constexpr uint64_t max_serial=(UINT64_MAX>>5);
struct Pcm {
    uint64_t command=0;
    bool looping=false;
    uint64_t loop_start=0,loop_end=0;
    unsigned rate=0;
    std::vector<int16_t> samples; // Immutable after publication; L,R.
    uint64_t frames() const noexcept {return samples.size()/2u;}
};
uint16_t le16(const uint8_t* p) noexcept {return uint16_t(p[0])|(uint16_t(p[1])<<8);}
uint32_t le32(const uint8_t* p) noexcept {return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
bool valid_utf8(const char* text,size_t size) noexcept {
    for(size_t i=0;i<size;) {
        uint32_t c=static_cast<unsigned char>(text[i++]);
        if(c<0x80u) {if(c<0x20u)return false;continue;}
        unsigned count=0;uint32_t minimum=0;
        if(c>=0xc2u&&c<=0xdfu){count=1;c&=31u;minimum=0x80u;}
        else if(c>=0xe0u&&c<=0xefu){count=2;c&=15u;minimum=0x800u;}
        else if(c>=0xf0u&&c<=0xf4u){count=3;c&=7u;minimum=0x10000u;}
        else return false;
        if(count>size-i)return false;
        while(count--){const uint32_t tail=static_cast<unsigned char>(text[i++]);if((tail&0xc0u)!=0x80u)return false;c=(c<<6)|(tail&63u);}
        if(c<minimum||c>0x10ffffu||(c>=0xd800u&&c<=0xdfffu))return false;
    }
    return true;
}
template<size_t N> void text_copy(char (&out)[N],const char* text) noexcept {
    const size_t n=std::min(std::strlen(text),N-1);std::memcpy(out,text,n);out[n]=0;
}
int16_t be16(const uint8_t* p) noexcept {
    const uint16_t bits=(uint16_t(p[0])<<8)|p[1];
    return bits<0x8000u?static_cast<int16_t>(bits):static_cast<int16_t>(int32_t(bits)-65536);
}
void store_be16(uint8_t* p,int16_t sample) noexcept {
    const uint16_t bits=static_cast<uint16_t>(sample);p[0]=uint8_t(bits>>8);p[1]=uint8_t(bits);
}
}

struct BwAudioPreview {
    /* control=(serial<<1)|play. One acquire load is the initialized OFF path.
     * Serial changes for every request/stop/cancel/reset/shutdown. */
    std::atomic<uint64_t> control{0};
    std::atomic<Pcm*> published{nullptr}, hazard{nullptr};
    std::atomic<uint64_t> audio_state{0}, played_command{0}, played_frames{0}, cursor_frame{0};
    std::atomic<unsigned> output_rate{0};
    std::mutex request_mutex,status_mutex,shutdown_mutex;
    std::condition_variable request_ready;
    uint64_t serial=0,request_command=0;
    std::string request_path;
    bool request_looping=false;
    uint64_t request_loop_start=0,request_loop_end=0;
    bool closed=false;
    BwAudioPreviewStatus view{};
    std::thread decoder;
    /* Only the single copied-output consumer touches these fields. */
    uint64_t cursor_command=0,cursor=0,played=0;
#ifdef BLUEWAKE_AUDIO_PREVIEW_TEST
    std::atomic<bool> hold_decode{false},decode_waiting{false},hold_mix{false},mix_waiting{false};
    std::mutex gate_mutex;
    std::condition_variable gate_ready;
#endif

    BwAudioPreview(){view.state=BW_PREVIEW_IDLE;view.accepting_requests=true;decoder=std::thread(&BwAudioPreview::work,this);}
    bool still(uint64_t command) const noexcept {return control.load(std::memory_order_acquire)==command;}
    void set_view(uint64_t command,BwAudioPreviewState state,const char* error="",unsigned rate=0,uint64_t frames=0) {
        std::lock_guard<std::mutex> lock(status_mutex);
        if(!still(command))return;
        view.generation=command>>1;view.state=state;view.sample_rate=rate;view.total_frames=frames;
        text_copy(view.error,error);
    }
    bool publish_ready(Pcm* pcm) {
        std::lock_guard<std::mutex> lock(status_mutex);
        if(!still(pcm->command))return false;
        view.generation=pcm->command>>1;view.sample_rate=pcm->rate;
        view.total_frames=pcm->frames();view.error[0]=0;
        /* Status readers cannot observe a playing generation with old asset
         * metadata: publication and ready metadata share this UI-only lock.
         * Copied-output consumer still takes no lock. */
        published.store(pcm,std::memory_order_seq_cst);view.state=BW_PREVIEW_READY;
        return true;
    }
    bool request(const char* path,BwAudioPreviewState state,bool play,
                 bool looping=false,uint64_t loop_start=0,uint64_t loop_end=0) {
        std::lock_guard<std::mutex> lock(request_mutex);
        if(closed||serial==max_serial)return false;
        const uint64_t command=(++serial<<1)|(play?1u:0u);
        request_command=command;request_path=path?path:"";
        request_looping=looping;request_loop_start=loop_start;request_loop_end=loop_end;
        {
            std::lock_guard<std::mutex> status_lock(status_mutex);
            view={};view.accepting_requests=true;view.generation=serial;view.state=state;
            text_copy(view.path_utf8,request_path.c_str());
            view.looping=looping;view.loop_start=loop_start;view.loop_end=loop_end;
            if(state==BW_PREVIEW_FAILED)text_copy(view.error,"Path must be bounded, valid UTF-8 and nonempty");
            control.store(command,std::memory_order_release);
        }
        request_ready.notify_one();
#ifdef BLUEWAKE_AUDIO_PREVIEW_TEST
        gate_ready.notify_all();
#endif
        return play;
    }

    std::unique_ptr<Pcm> decode(const std::string& filename,uint64_t command,
                               bool looping,uint64_t loop_start,uint64_t loop_end,
                               const char*& error) {
        namespace fs=std::filesystem;
        error="";
        auto fail=[&](const char* message)->std::unique_ptr<Pcm>{error=message;return {};};
        const auto path=fs::u8path(filename);
        std::error_code ec;
        if(!fs::is_regular_file(path,ec)||ec)return fail("Selected preview file is not a readable regular file");
        const uint64_t file_size=fs::file_size(path,ec);
        if(ec)return fail("Preview file size could not be read");
        const auto file_time=fs::last_write_time(path,ec);
        if(ec)return fail("Preview file timestamp could not be read");
        if(file_size<12u||file_size>max_file_bytes)return fail("WAV file must be bounded to 64 MiB");
        std::ifstream file(path,std::ios::binary);
        if(!file)return fail("Preview file could not be opened");
        auto input=[&](uint8_t* out,size_t bytes)->bool {
            if(!still(command))return false;
            file.read(reinterpret_cast<char*>(out),static_cast<std::streamsize>(bytes));
            if(file.gcount()!=static_cast<std::streamsize>(bytes)){error="Truncated WAV data";return false;}
            return true;
        };
        uint8_t header[18];if(!input(header,12))return {};
        if(std::memcmp(header,"RIFF",4)||std::memcmp(header+8,"WAVE",4)||uint64_t(le32(header+4))+8u!=file_size)
            return fail("Only complete little-endian RIFF/WAVE is supported");
        bool have_fmt=false,have_data=false;uint64_t pos=12,data_offset=0,data_bytes=0;unsigned rate=0;
        while(pos<file_size) {
            if(file_size-pos<8u)return fail("Truncated WAV chunk header");
            file.seekg(static_cast<std::streamoff>(pos));if(!input(header,8))return {};pos+=8;
            const uint64_t size=le32(header+4);
            if(size>file_size-pos)return fail("WAV chunk exceeds file bounds");
            if(std::memcmp(header,"fmt ",4)==0) {
                if(have_fmt||(size!=16u&&size!=18u))return fail("One standard PCM fmt chunk is required");
                have_fmt=true;if(!input(header,size))return {};
                rate=le32(header+4);
                if(le16(header)!=1u||le16(header+2)!=2u||le16(header+12)!=4u||le16(header+14)!=16u||
                   (size==18u&&le16(header+16)!=0u))return fail("Preview requires PCM16 stereo WAV");
                if((rate!=32000u&&rate!=48000u)||le32(header+8)!=rate*4u)
                    return fail("Preview supports exact 32000/48000 Hz PCM rates");
            } else if(std::memcmp(header,"data",4)==0) {
                if(have_data)return fail("Duplicate WAV data chunk");
                have_data=true;data_offset=pos;data_bytes=size;
            }
            pos+=size+(size&1u);
            if(pos>file_size)return fail("Missing odd WAV chunk padding");
        }
        if(!have_fmt||!have_data||!data_bytes||(data_bytes&3u)||data_bytes/4u>uint64_t(rate)*BW_PREVIEW_MAX_SECONDS)
            return fail("Complete stereo frames of at most five minutes are required");
        if(!still(command))return {};
        auto pcm=std::make_unique<Pcm>();pcm->command=command;pcm->rate=rate;
        if(looping&&(!(loop_start<loop_end)||loop_end>data_bytes/4u))
            return fail("Loop bounds must be end-exclusive stereo frames within WAV data");
        pcm->looping=looping;pcm->loop_start=loop_start;pcm->loop_end=loop_end;
        pcm->samples.resize(static_cast<size_t>(data_bytes/2u));
        file.clear();file.seekg(static_cast<std::streamoff>(data_offset));
        std::array<uint8_t,65536> block{};
        for(uint64_t at=0;at<data_bytes;) {
            const size_t bytes=static_cast<size_t>(std::min<uint64_t>(block.size(),data_bytes-at));
            if(!input(block.data(),bytes))return {};
            for(size_t i=0;i<bytes;i+=2){const uint16_t bits=le16(block.data()+i);pcm->samples[at/2u+i/2u]=bits<0x8000u?static_cast<int16_t>(bits):static_cast<int16_t>(int32_t(bits)-65536);}
            at+=bytes;
        }
        if(!still(command))return {};
        const uint64_t after_size=fs::file_size(path,ec);
        if(ec||after_size!=file_size)return fail("Preview file changed during decoding");
        const auto after_time=fs::last_write_time(path,ec);
        if(ec||after_time!=file_time)return fail("Preview file changed during decoding");
        return pcm;
    }

    void work() noexcept {
        uint64_t handled=0;
        std::unique_ptr<Pcm> current;
        std::array<std::unique_ptr<Pcm>,2> retired;
        auto collect=[&]() {
            const Pcm* held=hazard.load(std::memory_order_seq_cst);
            for(auto& item:retired)if(item.get()!=held)item.reset();
        };
        auto revoke=[&]() {
            published.exchange(nullptr,std::memory_order_seq_cst);
            collect();
            if(current) {
                for(auto& item:retired)if(!item){item=std::move(current);break;}
                /* One consumer can hazard at most one retired asset. */
                if(current)std::terminate();
            }
            collect();
        };
        for(;;) {
            uint64_t command=0;std::string filename;bool ending=false,looping=false;
            uint64_t loop_start=0,loop_end=0;
            try {
                {
                    std::unique_lock<std::mutex> lock(request_mutex);
                    request_ready.wait(lock,[&]{return closed||request_command!=handled;});
                    ending=closed;command=request_command;filename=request_path;handled=command;
                    looping=request_looping;loop_start=request_loop_start;loop_end=request_loop_end;
                }
                revoke();
                if(ending)break;
                if(!(command&1u))continue;
                const char* error="";
                auto pcm=decode(filename,command,looping,loop_start,loop_end,error);
                if(!pcm){if(error[0])set_view(command,BW_PREVIEW_FAILED,error);continue;}
#ifdef BLUEWAKE_AUDIO_PREVIEW_TEST
                {
                    std::unique_lock<std::mutex> gate(gate_mutex);decode_waiting.store(true);
                    gate_ready.wait(gate,[&]{return !hold_decode.load()||!still(command);});
                    decode_waiting.store(false);
                }
#endif
                if(!still(command))continue;
                current=std::move(pcm);
                if(!publish_ready(current.get()))current.reset();
            } catch(...) {
                // Invalid input/cancellation use explicit results above. This
                // is only an unexpected allocation/OS synchronization failure.
                set_view(command,BW_PREVIEW_FAILED,"Preview decoder failed");
            }
        }
        revoke();
        while(hazard.load(std::memory_order_seq_cst)!=nullptr)std::this_thread::yield();
        collect(); // Decoder thread, never copied-output consumer, destroys PCM.
    }
};

extern "C" BwAudioPreview* bluewake_audio_preview_create(void) {
    try{return new BwAudioPreview;}catch(...){return nullptr;}
}
extern "C" bool bluewake_audio_preview_play_utf8(BwAudioPreview* self,const char* path) {
    if(!self)return false;
    size_t size=0;if(path)while(size<BW_PREVIEW_PATH_BYTES&&path[size])++size;
    if(!path||!size||size>=BW_PREVIEW_PATH_BYTES||!valid_utf8(path,size)) {
        self->request(nullptr,BW_PREVIEW_FAILED,false);return false;
    }
    try{return self->request(path,BW_PREVIEW_LOADING,true);}catch(...){self->request(nullptr,BW_PREVIEW_FAILED,false);return false;}
}
extern "C" bool bluewake_audio_preview_play_loop_utf8(BwAudioPreview* self,const char* path,uint64_t start,uint64_t end) {
    if(!self)return false;
    size_t size=0;if(path)while(size<BW_PREVIEW_PATH_BYTES&&path[size])++size;
    if(!path||!size||size>=BW_PREVIEW_PATH_BYTES||!valid_utf8(path,size)||start>=end||end>uint64_t(48000u)*BW_PREVIEW_MAX_SECONDS) {
        self->request(nullptr,BW_PREVIEW_FAILED,false);return false;
    }
    try{return self->request(path,BW_PREVIEW_LOADING,true,true,start,end);}catch(...){self->request(nullptr,BW_PREVIEW_FAILED,false);return false;}
}
extern "C" void bluewake_audio_preview_stop(BwAudioPreview* self){if(self)self->request(nullptr,BW_PREVIEW_STOPPED,false);}
extern "C" void bluewake_audio_preview_cancel(BwAudioPreview* self){if(self)self->request(nullptr,BW_PREVIEW_CANCELLED,false);}
extern "C" void bluewake_audio_preview_reset(BwAudioPreview* self){if(self)self->request(nullptr,BW_PREVIEW_RESET,false);}
extern "C" void bluewake_audio_preview_shutdown(BwAudioPreview* self) {
    if(!self)return;
    std::lock_guard<std::mutex> shutdown_lock(self->shutdown_mutex);
    {
        std::lock_guard<std::mutex> lock(self->request_mutex);
        if(!self->closed) {
            self->closed=true;
            const uint64_t command=(++self->serial<<1);
            self->request_command=command;self->request_path.clear();
            std::lock_guard<std::mutex> status_lock(self->status_mutex);
            self->view={};self->view.generation=self->serial;self->view.state=BW_PREVIEW_SHUTDOWN;
            self->control.store(command,std::memory_order_release);
        }
    }
    self->request_ready.notify_all();
#ifdef BLUEWAKE_AUDIO_PREVIEW_TEST
    self->gate_ready.notify_all();
#endif
    if(self->decoder.joinable())self->decoder.join();
}
extern "C" void bluewake_audio_preview_destroy(BwAudioPreview* self){if(self){bluewake_audio_preview_shutdown(self);delete self;}}
extern "C" bool bluewake_audio_preview_status(BwAudioPreview* self,BwAudioPreviewStatus* out) {
    if(!self||!out)return false;
    std::lock_guard<std::mutex> lock(self->status_mutex);*out=self->view;
    const uint64_t command=self->control.load(std::memory_order_acquire);
    const uint64_t audio=self->audio_state.load(std::memory_order_acquire);
    if((audio>>4)==command)out->state=static_cast<BwAudioPreviewState>(audio&15u);
    if(self->played_command.load(std::memory_order_acquire)==command) {
        out->played_frames=self->played_frames.load(std::memory_order_relaxed);
        out->cursor_frame=self->cursor_frame.load(std::memory_order_relaxed);
    }
    out->observed_output_rate=(audio>>4)==command?self->output_rate.load(std::memory_order_relaxed):0;
    return true;
}
extern "C" bool bluewake_audio_preview_mix_be16(BwAudioPreview* self,uint8_t* stereo,size_t bytes,unsigned rate) {
    if(!self)return false;
    const uint64_t command=self->control.load(std::memory_order_acquire);
    if(!(command&1u))return false; // Default OFF: no PCM/config access.
    if(!stereo||!bytes||(bytes&3u)||bytes>BW_PREVIEW_MAX_OUTPUT_BYTES)return false;
    Pcm* pcm=self->published.load(std::memory_order_seq_cst);
    self->hazard.store(pcm,std::memory_order_seq_cst);
    if(!pcm||self->published.load(std::memory_order_seq_cst)!=pcm||!self->still(command)) {
        self->hazard.store(nullptr,std::memory_order_seq_cst);return false;
    }
#ifdef BLUEWAKE_AUDIO_PREVIEW_TEST
    self->mix_waiting.store(true);
    while(self->hold_mix.load())std::this_thread::yield();
    self->mix_waiting.store(false);
#endif
    if(!self->still(command)||pcm->command!=command) {
        self->hazard.store(nullptr,std::memory_order_seq_cst);return false;
    }
    self->output_rate.store(rate,std::memory_order_relaxed);
    if(rate!=pcm->rate) {
        self->audio_state.store((command<<4)|BW_PREVIEW_RATE_MISMATCH,std::memory_order_release);
        self->hazard.store(nullptr,std::memory_order_seq_cst);return false;
    }
    if(self->cursor_command!=command){self->cursor_command=command;self->cursor=0;self->played=0;}
    const size_t count=static_cast<size_t>(pcm->looping?bytes/4u:std::min<uint64_t>(bytes/4u,pcm->frames()-self->cursor));
    if(!count){self->hazard.store(nullptr,std::memory_order_seq_cst);return false;}
    BwAudioConfiguration config{};bluewake_audio_configuration(&config);
    bool changed=false;
    for(size_t frame=0;frame<count;++frame) {
        for(size_t channel=0;channel<2u;++channel) {
            const size_t at=(frame*2u+channel)*2u;
            const int32_t original=be16(stereo+at);
            const int32_t preview=(int32_t(pcm->samples[self->cursor*2u+channel])*int32_t(config.music_percent))/100;
            const int16_t mixed=static_cast<int16_t>(std::clamp(original+preview,int32_t(-32768),int32_t(32767)));
            if(mixed!=original){store_be16(stereo+at,mixed);changed=true;}
        }
        ++self->cursor;
        if(pcm->looping&&self->cursor==pcm->loop_end)self->cursor=pcm->loop_start;
    }
    self->played=self->played>UINT64_MAX-count?UINT64_MAX:self->played+count;
    self->played_frames.store(self->played,std::memory_order_relaxed);
    self->cursor_frame.store(self->cursor,std::memory_order_relaxed);
    self->played_command.store(command,std::memory_order_release);
    self->audio_state.store((command<<4)|(!pcm->looping&&self->cursor==pcm->frames()?BW_PREVIEW_FINISHED:BW_PREVIEW_PLAYING),std::memory_order_release);
    self->hazard.store(nullptr,std::memory_order_seq_cst);
    return changed;
}
#ifdef BLUEWAKE_AUDIO_PREVIEW_TEST
#include "audio_preview_test_api.h"
extern "C" void bluewake_audio_preview_test_hold_decode(BwAudioPreview* p,bool hold){p->hold_decode.store(hold);p->gate_ready.notify_all();}
extern "C" bool bluewake_audio_preview_test_decode_waiting(BwAudioPreview* p){return p->decode_waiting.load();}
extern "C" void bluewake_audio_preview_test_hold_mix(BwAudioPreview* p,bool hold){p->hold_mix.store(hold);}
extern "C" bool bluewake_audio_preview_test_mix_waiting(BwAudioPreview* p){return p->mix_waiting.load();}
#endif
