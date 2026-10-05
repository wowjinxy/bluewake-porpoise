// SPDX-License-Identifier: GPL-3.0-or-later
/* Actual donor DownloadRawSamplesFromMRAM and AddVoice. The only test seam
 * opens access to the private method in isolated copied source; it contains
 * no replacement copy/fill/resampler implementation. No audio/video device. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "dsp_hle_backend.h"
#include "Common/ChunkFile.h"
#include "Core/HW/DSPHLE/UCodes/Zelda.h"
#include "Core/System.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
constexpr uint32_t BASE=0x80000000u, VPB=0x80060000u, PCM=0x80080000u,
    LEFT=0x80072000u, RIGHT=0x80073000u;
constexpr int16_t CANARY=-12578;
std::vector<uint8_t> ram(0x1800000),aram(0x10000);
void w16(uint32_t a,uint16_t value){ram.at(a-BASE)=uint8_t(value>>8);ram.at(a-BASE+1)=uint8_t(value);}
void w32(uint32_t a,uint32_t value){w16(a,uint16_t(value>>16));w16(a+2,uint16_t(value));}
uint16_t r16(uint32_t a){return uint16_t((unsigned(ram.at(a-BASE))<<8)|ram.at(a-BASE+1));}
uint32_t r32(uint32_t a){return (uint32_t(r16(a))<<16)|r16(a+2);}
uint8_t* pointer(void*,uint32_t address,uint32_t size) {
    const uint32_t offset=address>=BASE&&address<BASE+ram.size()?address-BASE:address;
    return offset<=ram.size()&&size<=ram.size()-offset?ram.data()+offset:nullptr;
}
uint64_t timebase(void*){return 1234;}
void interrupt(void*){}
void fixture(uint32_t position,uint32_t remaining) {
    std::fill(ram.begin(),ram.end(),0);
    w16(VPB,1);w16(VPB+4,0x1800);w16(VPB+8,1);
    w16(VPB+0x10,0xD00);w16(VPB+0x12,0x4000);w16(VPB+0x14,0x4000);
    w16(VPB+0x18,0xD60);w16(VPB+0x1A,0x4000);w16(VPB+0x1C,0x4000);
    w32(VPB+0x68,position<<16);w32(VPB+0x74,remaining);
    for(unsigned i=0;i<4;i++)w16(VPB+0x78+i*2,uint16_t(-101-int(i)));
    w16(VPB+0x100,33);w32(VPB+0x110,PCM);w32(VPB+0x114,1024u<<16);w32(VPB+0x118,PCM);
    for(unsigned i=0;i<1024;i++)w16(PCM+i*2,uint16_t(2000+i));
}
void initialize(DSP::HLE::ZeldaAudioRenderer& renderer) {
    renderer.SetFlags(0);renderer.SetVPBBaseAddress(VPB);renderer.SetReverbPBBaseAddress(0);
    renderer.SetOutputLeftBufferAddr(LEFT);renderer.SetOutputRightBufferAddr(RIGHT);renderer.SetOutputVolume(0x1000);
    std::array<int16_t,256> coefficients{};
    // Genuine native four-tap resampler: last coefficient selects the fourth
    // input with Q1.15 unity-minus-one. Terminal raw history should be3023.
    for(unsigned i=0;i<256;i+=4)coefficients[i+3]=0x7FFF;
    renderer.SetResamplingCoeffs(std::move(coefficients));
}
void raw_partial_test() {
    fixture(960,64);
    const auto source=std::vector<uint8_t>(ram.begin()+PCM-BASE,ram.begin()+PCM-BASE+2048);
    DSP::HLE::ZeldaAudioRenderer renderer(Core::System::GetInstance());initialize(renderer);
    std::array<int16_t,122> output;output.fill(CANARY);
    renderer.TestDownloadRawSamples(0,output.data()+1,120);
    assert(output.front()==CANARY&&output.back()==CANARY);
    assert(r16(VPB+0x68)==1024&&r32(VPB+0x74)==0&&r16(VPB+2)==1);
    assert(std::equal(source.begin(),source.end(),ram.begin()+PCM-BASE));
    std::array<int16_t,120> expected{};
    for(unsigned i=0;i<64;i++)expected[i]=int16_t(2960+i);
    std::fill(expected.begin()+64,expected.end(),3023);
#ifdef BLUEWAKE_DSP_COMPLETION_EXPECT_DEFECT
    // Deterministic negative control with initialized destination. The real
    // old implementation overwrites its valid prefix and never fills the
    // last56 samples. No undefined stack reads are used to prove the defect.
    assert(!std::equal(expected.begin(),expected.end(),output.begin()+1));
    assert(output[1]==3023);
    assert(std::all_of(output.begin()+65,output.end()-1,[](int16_t s){return s==CANARY;}));
    std::cout<<"Original donor defect reproduced: 64valid samples; overwritten prefix/unfilled56sample tail\n";
#else
    assert(std::equal(expected.begin(),expected.end(),output.begin()+1));
    std::cout<<"Corrected donor: retained64valid samples then filled56sample tail with native last sample\n";
#endif
}
void boundary_tests() {
    // No remaining samples: full silence and completion on native clock.
    fixture(1024,0);DSP::HLE::ZeldaAudioRenderer zero(Core::System::GetInstance());initialize(zero);
    std::array<int16_t,122> output;output.fill(CANARY);
    zero.TestDownloadRawSamples(0,output.data()+1,120);
    assert(std::all_of(output.begin()+1,output.end()-1,[](int16_t s){return s==0;}));
    assert(output.front()==CANARY&&output.back()==CANARY&&r16(VPB+2)==1&&r16(VPB+0x68)==1024);
    // Exact remaining count consumes original samples and leaves done unset
    // until the following native call, matching the original branch behavior.
    fixture(960,64);DSP::HLE::ZeldaAudioRenderer exact(Core::System::GetInstance());initialize(exact);
    output.fill(CANARY);exact.TestDownloadRawSamples(0,output.data()+1,64);
    for(unsigned i=0;i<64;i++)assert(output[i+1]==int16_t(2960+i));
    assert(output[65]==CANARY&&r32(VPB+0x74)==0&&r16(VPB+2)==0&&r16(VPB+0x68)==1024);
    // Ordinary full block and looping path stay native and retain all data.
    fixture(0,960);DSP::HLE::ZeldaAudioRenderer full(Core::System::GetInstance());initialize(full);
    output.fill(CANARY);full.TestDownloadRawSamples(0,output.data()+1,120);
    for(unsigned i=0;i<120;i++)assert(output[i+1]==int16_t(2000+i));
    assert(r32(VPB+0x74)==840&&r16(VPB+0x68)==120&&r16(VPB+2)==0);
    fixture(1000,1000);DSP::HLE::ZeldaAudioRenderer loop(Core::System::GetInstance());initialize(loop);
    output.fill(CANARY);loop.TestDownloadRawSamples(0,output.data()+1,120);
    for(unsigned i=0;i<24;i++)assert(output[i+1]==int16_t(3000+i));
    for(unsigned i=0;i<96;i++)assert(output[25+i]==int16_t(2000+i));
    assert(r32(VPB+0x74)==880&&r16(VPB+0x68)==96&&r16(VPB+2)==0);
}
#ifndef BLUEWAKE_DSP_COMPLETION_EXPECT_DEFECT
std::vector<uint8_t> terminal_block() {
    fixture(960,64);
    DSP::HLE::ZeldaAudioRenderer renderer(Core::System::GetInstance());initialize(renderer);
    renderer.PrepareFrame();renderer.AddVoice(0);renderer.FinalizeFrame();
    assert(r16(VPB+2)==1&&r32(VPB+0x74)==0&&r16(VPB+0x68)==1024&&r16(VPB+0x60)==0);
    for(unsigned i=0;i<4;i++)assert(r16(VPB+0x78+i*2)==3023);
    assert(r16(VPB+0x66)==3022); // actual resampler Q1.15 result at terminal sample
    const auto previous=std::vector<uint8_t>(ram.begin()+VPB-BASE,ram.begin()+VPB-BASE+0x180);
    // The native done flag skips AddVoice; renderer and guest sample-clock
    // state cannot be restarted merely because a category volume changes.
    renderer.PrepareFrame();renderer.AddVoice(0);renderer.FinalizeFrame();
    assert(std::equal(previous.begin(),previous.end(),ram.begin()+VPB-BASE));
    std::vector<uint8_t> result=previous;
    result.insert(result.end(),ram.begin()+LEFT-BASE,ram.begin()+LEFT-BASE+320);
    result.insert(result.end(),ram.begin()+RIGHT-BASE,ram.begin()+RIGHT-BASE+320);
    return result;
}
#endif
} // namespace
int main() {
    bluewake::dsp::HleBackend backend;
    assert(backend.initialize(nullptr,pointer,aram.data(),uint32_t(aram.size()),timebase,interrupt));
    raw_partial_test();boundary_tests();
#ifndef BLUEWAKE_DSP_COMPLETION_EXPECT_DEFECT
    const auto first=terminal_block(),second=terminal_block();assert(first==second);
    std::cout<<"Actual corrected AddVoice terminal PCM/filter-history/VPB completion is deterministic\n";
#endif
}
