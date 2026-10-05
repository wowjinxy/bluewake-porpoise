#include "audio_preview_fixture.h"
#include "audio_dma_stereo.h"
#include <cstdio>
using u8=uint8_t;using u32=uint32_t;
/* Only CPU access and capture/device shell are mocked. The exact production
 * copied-output body, actual channel conversion, gain core and preview worker
 * execute; there is no native game, guest code, DSP adapter or audio device. */
struct CPUState {u8* ram;u32 ram_size;};
static u8 mem_read8(CPUState* cpu,u32 address){assert(address>=0x80000000u);const auto at=address-0x80000000u;assert(at<cpu->ram_size);return cpu->ram[at];}
struct Dma {unsigned rate=32000;};static Dma g_audio_dma;
static unsigned dol_audio_dma_sample_rate(const Dma* dma){return dma->rate;}
struct Capture {const char* path=nullptr;};static Capture g_audio_capture;
static bool g_audio_capture_failure_reported;
static std::array<uint8_t,16384> captured;static size_t captured_size;static unsigned captures;
static bool bluewake_audio_capture_append_be16_stereo(Capture*,const uint8_t* b,size_t size,unsigned rate){assert(rate==g_audio_dma.rate&&size<=captured.size());std::memcpy(captured.data(),b,size);captured_size=size;++captures;return true;}
static BwAudioPreview* g_audio_preview;
#include "copy_original.inc"
#include "copy_projected.inc"

int run_copy_seam(const std::filesystem::path& dir) {
    using namespace fixture;native_config();auto p=create();g_audio_preview=p.get();
    const auto file=dir/"copy.wav";save(file,wav({3000,-3000,4000,-4000}));
    auto ram=be({-1000,1000,-2000,2000});const auto ram_before=ram;CPUState cpu{ram.data(),static_cast<u32>(ram.size())};
    std::array<uint8_t,8> native{},projected{};
    assert(host_audio_dma_read_guest_original(&cpu,0,native.data(),8));assert((native==std::array<uint8_t,8>{0x03,0xe8,0xfc,0x18,0x07,0xd0,0xf8,0x30}));
    assert(host_audio_dma_read_guest(&cpu,0,projected.data(),8));assert(native==projected&&ram==ram_before); // Default off exact baseline.
    assert(play(p.get(),file));ready(p.get());native_config(50,50,0);
    assert(host_audio_dma_read_guest(&cpu,0,projected.data(),8));const auto expected=be({1250,-1250,2000,-2000});
    assert(std::memcmp(projected.data(),expected.data(),8)==0);assert(std::memcmp(captured.data(),projected.data(),8)==0&&captured_size==8);
    assert(status(p.get()).played_frames==2&&ram==ram_before); // Preview/Music/thenMaster once, copied bytes only.
    assert(play(p.get(),file));ready(p.get());native_config(100,100,100,true);
    assert(host_audio_dma_read_guest(&cpu,0,projected.data(),8));assert((projected==std::array<uint8_t,8>{}));assert(ram==ram_before);
    bluewake_audio_preview_reset(p.get());native_config(37,18,0);assert(host_audio_dma_read_guest_original(&cpu,0,native.data(),8));
    assert(host_audio_dma_read_guest(&cpu,0,projected.data(),8));assert(native==projected&&ram==ram_before);
    assert(!host_audio_dma_read_guest(&cpu,0,nullptr,8));assert(!host_audio_dma_read_guest(&cpu,0,projected.data(),3));
    assert(!host_audio_dma_read_guest(&cpu,4,projected.data(),8));assert(!host_audio_dma_read_guest(nullptr,0,projected.data(),8));
    assert(captures==6&&ram==ram_before);g_audio_preview=nullptr;
    std::puts("Preview copy seam: exact main.c body + one projected call, real RL/LR and Music→Master, off/reset identity, capture order, RAM untouched PASS.");return 0;
}
