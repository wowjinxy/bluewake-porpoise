// SPDX-License-Identifier: GPL-3.0-or-later
/* Real donor ZeldaAudioRenderer with source-audited, SYNTHETIC JAI/JAS
 * ownership and PCM/AFC/reverb data. No loaded native music identity, disc,
 * desktop input, SDL video/audio device or audible game qualification. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "audio_customization.h"
#include "dsp_hle_backend.h"
#include "Common/ChunkFile.h"
#include "Core/HW/DSPHLE/UCodes/Zelda.h"
#include "Core/System.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

namespace {
constexpr uint32_t BASE=0x80000000u, CRC=0x86840740u,
    AUDIO=0x803A2CE8u, DSP=0x80050000u, VPBS=0x80060000u,
    LOGICAL=0x80010000u, SEQUENCES=0x80040000u, SOUNDS=0x80041000u,
    PARAMS=0x80020000u, CHILDREN=0x80030000u, RPBS=0x80070000u,
    REVERB=0x80071000u, OUTPUT_L=0x80072000u, OUTPUT_R=0x80073000u,
    STREAM_SOUND=0x80042000u, STREAM_PARAM=0x80043000u, STREAM_UPDATE=0x80044000u,
    STREAM_SLOTS=0x803F7670u;
struct Memory {
    std::vector<uint8_t> ram=std::vector<uint8_t>(0x1800000);
    std::vector<uint8_t> aram=std::vector<uint8_t>(0x10000);
    uint64_t epoch=1, reads=0;
    uint32_t fail_address=0;
    unsigned change_after=0;
} memory;
void w8(uint32_t address,uint8_t value){memory.ram.at(address-BASE)=value;}
void w16(uint32_t address,uint16_t value){w8(address,uint8_t(value>>8));w8(address+1,uint8_t(value));}
void w32(uint32_t address,uint32_t value){w16(address,uint16_t(value>>16));w16(address+2,uint16_t(value));}
uint32_t r32(uint32_t a){return (uint32_t(memory.ram.at(a-BASE))<<24)|(uint32_t(memory.ram.at(a-BASE+1))<<16)|(uint32_t(memory.ram.at(a-BASE+2))<<8)|memory.ram.at(a-BASE+3);}
uint16_t r16(uint32_t a){return uint16_t((unsigned(memory.ram.at(a-BASE))<<8)|memory.ram.at(a-BASE+1));}
bool read_guest(void*,uint32_t address,void* out,size_t bytes) {
    memory.reads++;
    if(memory.change_after&&--memory.change_after==0)memory.epoch++;
    if(address==memory.fail_address||address<BASE||address-BASE>memory.ram.size()||bytes>memory.ram.size()-(address-BASE))return false;
    std::memcpy(out,memory.ram.data()+address-BASE,bytes);return true;
}
uint64_t epoch(void*){return memory.epoch;}
uint8_t* guest_pointer(void*,uint32_t address,uint32_t bytes) {
    uint32_t offset=address;
    if(address>=BASE&&address<BASE+0x1800000)offset=address-BASE;
    else if(address>=0xC0000000u&&address<0xC1800000u)offset=address-0xC0000000u;
    return offset<=memory.ram.size()&&bytes<=memory.ram.size()-offset?memory.ram.data()+offset:nullptr;
}
uint64_t timebase(void*){return 1234;}
void interrupt(void*){}
uint32_t root(unsigned slot){return PARAMS+slot*0x2000+0x1360;}
uint32_t child(unsigned slot){return CHILDREN+slot*0x400;}
uint32_t owner(unsigned slot){return SOUNDS+slot*0x80;}
void native_sound(uint32_t address,uint32_t id,uint32_t parameter) {
    w32(address,0x8039CAB8u);w8(address+5,4);w32(address+0xC,id);w32(address+0x3C,parameter);
}
void native_dsp(unsigned index,uint32_t logical) {
    const uint32_t dsp=DSP+index*0x14;
    w8(dsp,uint8_t(index));w8(dsp+1,0);w32(dsp+8,logical);w32(dsp+0xC,VPBS+index*0x180);
    w32(dsp+0x10,0x8028C6C4u);
    w32(logical+0x20,dsp);w8(logical+1,0);
}
void fixture() {
    std::fill(memory.ram.begin(),memory.ram.end(),0);std::fill(memory.aram.begin(),memory.aram.end(),0);
    memory.epoch++;memory.reads=0;memory.fail_address=0;memory.change_after=0;
    // Primary GZLE01 static mDoAud_zelAudio_c identity, as observed in native
    // CARD boot snapshots. The JAIZelBasic base-construction VT is rejected.
    w32(0x803F7710,AUDIO);w32(0x803F7578,AUDIO);w32(AUDIO,0x803716C8);
    w32(0x803F7538,VPBS);w32(0x803F7520,DSP);w32(0x803F754C,LOGICAL);
    w32(0x803F6468,3);w32(0x803F7620,SEQUENCES);w32(0x803F75FC,owner(1));
    w32(AUDIO+0x68,owner(0));w32(AUDIO+0x78,0x8000002E);
    const uint32_t ids[]={0x8000002E,0x80000800,0x80000002};
    for(unsigned i=0;i<3;i++) {
        const uint32_t slot=SEQUENCES+i*0x50,param=PARAMS+i*0x2000;
        native_sound(owner(i),ids[i],param);w8(owner(i)+4,uint8_t(i));
        w32(slot+0x48,owner(i));w32(param+0x135C,slot);
        w8(root(i)+0x37E,1);w8(child(i)+0x37E,1);
        w32(root(i)+0x320,child(i));w32(child(i)+0x31C,root(i));
        native_dsp(i,LOGICAL+i*0xEC);w32(LOGICAL+i*0xEC+4,child(i)+0xF8);
    }
    bluewake_audio_attach(read_guest,epoch,nullptr);
}
BwAudioVoiceInfo classified(unsigned voice,BwAudioCategory expected) {
    BwAudioVoiceInfo out{};assert(bluewake_audio_classify_voice(CRC,VPBS,uint16_t(voice),&out));
    assert(out.category==expected&&out.epoch==memory.epoch);return out;
}
void stream_fixture() {
    w32(SEQUENCES+2*0x50+0x48,0);w32(AUDIO+0x70,STREAM_SOUND);w32(AUDIO+0x7C,0xC0000001);
    native_sound(STREAM_SOUND,0xC0000001,STREAM_PARAM);w32(STREAM_PARAM+0x15C,STREAM_UPDATE);
    w32(0x803F764C,STREAM_UPDATE);w32(STREAM_UPDATE+0x14,STREAM_SOUND);
    for(unsigned i=0;i<2;i++) {
        unsigned v=i+2;const uint32_t dsp=DSP+v*0x14;
        w32(STREAM_SLOTS+4*i,dsp);w8(dsp,uint8_t(v));w8(dsp+1,0);w8(dsp+3,127);
        w32(dsp+8,STREAM_SLOTS+4*i);w32(dsp+0xC,VPBS+v*0x180);w32(dsp+0x10,0);
    }
}
void unchanged_unknown(unsigned voice=0) {
    std::array<int16_t,80> data{},before;for(size_t i=0;i<data.size();i++)data[i]=int16_t(5000-int(i)*97);
    before=data;BwAudioVoiceInfo out{};
    const unsigned change_after=memory.change_after;
    assert(!bluewake_audio_classify_voice(CRC,VPBS,uint16_t(voice),&out));
    assert(out.category==BW_AUDIO_UNKNOWN&&!out.sound);
    // Each fresh classification has its own lifetime snapshot. Re-arm the
    // simulated mid-read change instead of expecting the next stable epoch
    // to remain invalid after the first call rejected the old snapshot.
    if(change_after)memory.change_after=change_after;
    assert(!bluewake_audio_customize_voice(CRC,VPBS,uint16_t(voice),data.data(),data.size()));assert(data==before);
}
void ownership_tests() {
    fixture();assert(bluewake_audio_configure(100,0,0,false));
    assert(classified(0,BW_AUDIO_MUSIC).sound_id==0x8000002E);
    assert(classified(1,BW_AUDIO_SFX).sound_id==0x80000800);
    assert(classified(2,BW_AUDIO_FANFARE).sound_id==0x80000002);
    BwAudioVoiceInfo out{};
    assert(bluewake_audio_classify_voice(CRC,VPBS-BASE,0,&out));
    assert(!bluewake_audio_classify_voice(0,VPBS,0,&out));
    assert(!bluewake_audio_classify_voice(CRC,VPBS,64,&out));
    assert(!bluewake_audio_classify_voice(CRC,VPBS+4,0,&out));
    assert(!bluewake_audio_classify_voice(CRC,0xC0060000,0,&out));
    const std::array<uint32_t,6> fanfares={0x80000002,0x80000025,0x80000027,0x80000024,0x8000004F,0x8000005D};
    for(uint32_t id:fanfares){w32(owner(2)+0xC,id);classified(2,BW_AUDIO_FANFARE);}
    w32(owner(2)+0xC,0x80000055);unchanged_unknown(2);
    fixture();w32(AUDIO+0x68,0);w32(AUDIO+0x6C,owner(0));w32(AUDIO+0x74,0x8000002E);classified(0,BW_AUDIO_MUSIC);
    /* Native rewards are allocated into the validated sub handle. Their
     * cues retain unity at zero and partial music/SFX gains. */
    w32(owner(0)+0xC,0x80000002);unchanged_unknown();w32(AUDIO+0x74,0x80000002);classified(0,BW_AUDIO_FANFARE);
    for(uint32_t id:fanfares) {
        fixture();w32(AUDIO+0x6C,owner(2));w32(AUDIO+0x74,id);w32(owner(2)+0xC,id);
        assert(classified(2,BW_AUDIO_FANFARE).sound==owner(2));
        const auto native_ram=memory.ram;
        std::array<int16_t,80> samples{},original;
        for(size_t i=0;i<samples.size();i++)samples[i]=int16_t(int(i)*819-32768);
        original=samples;
        for(auto [music,sfx]:std::array<std::pair<unsigned,unsigned>,3>{{{0,0},{37,71},{100,0}}}) {
            assert(bluewake_audio_configure(100,music,sfx,false));
            assert(!bluewake_audio_customize_voice(CRC,VPBS,2,samples.data(),samples.size()));
            assert(samples==original&&memory.ram==native_ram);
        }
        w32(AUDIO+0x74,id^1u);unchanged_unknown(2);
        w32(AUDIO+0x74,id);memory.change_after=5;unchanged_unknown(2);
    }
    /* Main allocation semantics stay unchanged; normal sub battle music
     * still uses the Music setting, and mismatched IDs fail closed. */
    fixture();w32(owner(0)+0xC,0x80000002);w32(AUDIO+0x78,0x80000002);classified(0,BW_AUDIO_MUSIC);
    fixture();w32(AUDIO+0x6C,owner(2));w32(AUDIO+0x74,0x80000004);w32(owner(2)+0xC,0x80000004);
    classified(2,BW_AUDIO_MUSIC);assert(bluewake_audio_configure(100,0,100,false));
    std::array<int16_t,80> battle; battle.fill(-2345);
    assert(bluewake_audio_customize_voice(CRC,VPBS,2,battle.data(),battle.size()));
    assert(std::all_of(battle.begin(),battle.end(),[](int16_t value){return value==0;}));
    for(const auto& c:std::array<std::pair<uint32_t,uint32_t>,4>{{
            {AUDIO+0x74,0x80000025},{owner(2),0x8039B538},
            {PARAMS+2*0x2000+0x135C,SEQUENCES},{DSP+2*0x14+0xC,VPBS}}}) {
        fixture();w32(AUDIO+0x6C,owner(2));w32(AUDIO+0x74,0x80000002);w32(c.first,c.second);unchanged_unknown(2);
    }
    fixture();w32(AUDIO+0x6C,owner(2));w32(AUDIO+0x74,0x80000002);bluewake_audio_reset();unchanged_unknown(2);
    const struct {uint32_t address,value;} corruptions[]={
        {0x803F7710,AUDIO+4},{0x803F7578,0},{AUDIO,0x8039B538},
        {0x803F7538,0x817FFFA0},{0x803F7520,0x817FFFFC},{DSP+0xC,VPBS+0x180},
        {DSP+8,LOGICAL+4},{DSP+0x10,0x8028D218},{0x803F754C,0x817FFFFC},
        {LOGICAL+0x20,DSP+0x14},{LOGICAL+4,0},{0x803F6468,0},{0x803F6468,17},
        {0x803F7620,0x817FFFFC},{owner(0),0x8039B538},{owner(0)+0x3C,0},
        {PARAMS+0x135C,SEQUENCES+0x50},{AUDIO+0x78,0x80000001},
        {root(0)+0x320,0},{root(0)+0x324,child(0)},
        {child(0)+0x31C,child(0)},{root(0)+0x31C,child(0)}};
    for(const auto& c:corruptions){fixture();w32(c.address,c.value);unchanged_unknown();}
    for(auto [address,value]:std::array<std::pair<uint32_t,uint8_t>,6>{{{DSP,1},{DSP+1,1},{LOGICAL+1,0xFF},{owner(0)+4,2},{owner(0)+5,0},{root(0)+0x37E,0}}}){
        fixture();w8(address,value);unchanged_unknown();}
    fixture();w32(0x803F75FC,0);unchanged_unknown(1);
    fixture();w32(SEQUENCES+2*0x50+0x48,owner(0));unchanged_unknown();
    fixture();memory.fail_address=root(0)+0x320;unchanged_unknown();
    fixture();memory.change_after=5;unchanged_unknown();
    fixture();memory.epoch=0;unchanged_unknown();
    fixture();bluewake_audio_reset();unchanged_unknown();
    fixture();stream_fixture();classified(2,BW_AUDIO_STREAM);classified(3,BW_AUDIO_STREAM);
    const struct {uint32_t address,value;} stream_bad[]={
        {STREAM_SLOTS+4,DSP+2*0x14},{STREAM_SLOTS+4,DSP+3*0x14+4},
        {STREAM_UPDATE+0x14,owner(0)},{STREAM_PARAM+0x15C,0},
        {STREAM_SOUND+0xC,0x8000002E},{AUDIO+0x7C,0xC0000002},
        {DSP+3*0x14+8,STREAM_SLOTS},{DSP+2*0x14+0x10,0x8028C6C4}};
    for(const auto& c:stream_bad){fixture();stream_fixture();w32(c.address,c.value);unchanged_unknown(2);}
    fixture();stream_fixture();w8(DSP+3*0x14+3,1);unchanged_unknown(2);
    /* A replacement allocation is reclassified immediately: no stale DSP-ID
     * tag survives a free/reuse, scene lifetime or machine-state change. */
    fixture();classified(0,BW_AUDIO_MUSIC);w8(DSP+1,1);unchanged_unknown();
    native_dsp(0,LOGICAL);w32(LOGICAL+4,child(1)+0xF8);classified(0,BW_AUDIO_SFX);
}
void configuration_tests() {
    fixture();assert(bluewake_audio_configure(100,100,100,false));memory.reads=0;
    std::array<int16_t,80> data{},original;data.fill(-1234);original=data;
    assert(!bluewake_audio_customize_voice(CRC,VPBS,0,data.data(),80));
    assert(data==original&&memory.reads==0);
    BwAudioConfiguration before{},after{};bluewake_audio_configuration(&before);
    assert(!bluewake_audio_configure(101,100,100,false));assert(!bluewake_audio_configure(100,101,100,false));
    assert(!bluewake_audio_configure(100,100,~0u,false));bluewake_audio_configuration(&after);
    assert(after.generation==before.generation&&after.master_percent==before.master_percent);
    assert(bluewake_audio_configure(100,100,100,false));bluewake_audio_configuration(&after);assert(after.generation==before.generation);
    assert(!bluewake_audio_customize_voice(CRC,VPBS,0,data.data(),79));assert(data==original);
    assert(bluewake_audio_configure(25,100,100,false));memory.reads=0;
    assert(!bluewake_audio_customize_voice(CRC,VPBS,0,data.data(),80));assert(data==original&&memory.reads==0);
    uint8_t pcm[]={0x7F,0xFF,0x80,0x00,0x27,0x10,0xD8,0xF0};
    assert(bluewake_audio_customize_output_be16(pcm,sizeof pcm));
    const uint8_t quarter[]={0x1F,0xFF,0xE0,0x00,0x09,0xC4,0xF6,0x3C};assert(std::memcmp(pcm,quarter,sizeof pcm)==0);
    assert(bluewake_audio_configure(100,0,0,true));assert(bluewake_audio_customize_output_be16(pcm,sizeof pcm));
    assert(std::all_of(std::begin(pcm),std::end(pcm),[](uint8_t v){return v==0;}));
    assert(bluewake_audio_configure(100,100,100,false));assert(!bluewake_audio_customize_output_be16(pcm,sizeof pcm));
    assert(!bluewake_audio_customize_output_be16(pcm,3));assert(!bluewake_audio_customize_output_be16(nullptr,4));
    std::atomic_bool done=false;
    assert(bluewake_audio_configure(25,50,75,false));
    std::thread writer([&]{for(unsigned i=0;i<20000;i++)assert(bluewake_audio_configure(i%2?25:99,i%2?50:3,i%2?75:1,i%2==0));done=true;});
    while(!done.load()) {
        BwAudioConfiguration value{};bluewake_audio_configuration(&value);
        assert((value.master_percent==25&&value.music_percent==50&&value.sfx_percent==75&&!value.muted)||
               (value.master_percent==99&&value.music_percent==3&&value.sfx_percent==1&&value.muted));
    }
    writer.join();assert(bluewake_audio_configure(100,100,100,false));
}
void diagnostics_tests() {
    fixture();assert(bluewake_audio_configure(100,100,100,false));
    std::array<int16_t,80> samples{},original;samples.fill(-2345);original=samples;
    BwAudioDiagnostics stats{};bluewake_audio_diagnostics(&stats);
    assert(!stats.enabled&&stats.callbacks==0&&stats.identity_count==0);
    memory.reads=0;assert(!bluewake_audio_customize_voice(CRC,VPBS,0,samples.data(),80));
    assert(memory.reads==0&&samples==original);

    // Real callbacks classify once, even when both evidence and gain need the
    // result. The standalone public classifier must not inflate callback counts.
    memory.reads=0;classified(0,BW_AUDIO_MUSIC);const uint64_t one_classification=memory.reads;
    bluewake_audio_diagnostics(&stats);assert(stats.callbacks==0);
    const auto native_ram=memory.ram;
    bluewake_audio_diagnostics_enable(true);memory.reads=0;
    assert(!bluewake_audio_customize_voice(CRC,VPBS,0,samples.data(),80));
    assert(memory.reads==one_classification&&samples==original&&memory.ram==native_ram);
    bluewake_audio_diagnostics(&stats);
    assert(stats.enabled&&stats.epoch==memory.epoch&&stats.callbacks==1&&stats.classifications==1);
    assert(stats.category_callbacks[BW_AUDIO_MUSIC]==1&&stats.identity_count==1);
    assert(stats.identities[0].classified&&stats.identities[0].owner.sound==owner(0));
    assert(stats.identities[0].voice_id==0&&stats.identities[0].owner.root_track==root(0));
    assert(!bluewake_audio_customize_voice(CRC,VPBS,0,samples.data(),80));
    assert(!bluewake_audio_customize_voice(CRC,VPBS,1,samples.data(),80));
    assert(!bluewake_audio_customize_voice(CRC,VPBS,2,samples.data(),80));
    bluewake_audio_diagnostics(&stats);
    assert(stats.callbacks==4&&stats.identity_count==3&&stats.identities[0].callbacks==2);
    assert(stats.category_callbacks[BW_AUDIO_MUSIC]==2&&stats.category_callbacks[BW_AUDIO_SFX]==1&&
           stats.category_callbacks[BW_AUDIO_FANFARE]==1);
    assert(bluewake_audio_configure(100,50,100,false));memory.reads=0;
    assert(bluewake_audio_customize_voice(CRC,VPBS,0,samples.data(),80));
    assert(memory.reads==one_classification&&memory.ram==native_ram);
    assert(std::all_of(samples.begin(),samples.end(),[](int16_t s){return s==-1172;}));

    fixture();assert(bluewake_audio_configure(100,0,0,false));samples=original;
    // A fully linked native root with an unrecognized sound remains UNKNOWN:
    // evidence can name it, but it cannot authorize muting or become cached.
    w32(owner(2)+0xC,0x80000055);
    assert(!bluewake_audio_customize_voice(CRC,VPBS,2,samples.data(),80));
    bluewake_audio_diagnostics(&stats);
    assert(stats.callbacks==1&&stats.category_callbacks[BW_AUDIO_UNKNOWN]==1&&stats.identity_count==1);
    assert(!stats.identities[0].classified&&stats.identities[0].owner.sound==owner(2)&&
           stats.identities[0].owner.sound_id==0x80000055&&samples==original);
    w32(AUDIO,0);assert(!bluewake_audio_customize_voice(CRC,VPBS,0,samples.data(),80));
    assert(!bluewake_audio_customize_voice(0,VPBS,64,samples.data(),80));
    bluewake_audio_diagnostics(&stats);
    assert(stats.callbacks==3&&stats.category_callbacks[BW_AUDIO_UNKNOWN]==3&&stats.identity_count==3);
    assert(stats.identities[1].owner.sound==0&&stats.identities[1].owner.dsp_channel==0);
    assert(!bluewake_audio_customize_voice(CRC,VPBS,0,nullptr,80));
    assert(!bluewake_audio_customize_voice(CRC,VPBS,0,samples.data(),79));
    bluewake_audio_diagnostics(&stats);assert(stats.callbacks==3);

    fixture();assert(bluewake_audio_configure(100,100,100,false));
    // More than32 genuine, distinct owner IDs cannot allocate or overrun the
    // first-identity list. Totals continue and existing IDs still accumulate.
    for(unsigned i=0;i<40;i++) {
        w32(owner(0)+0xC,0x80000100+i);w32(AUDIO+0x78,0x80000100+i);
        assert(!bluewake_audio_customize_voice(CRC,VPBS,0,samples.data(),80));
    }
    bluewake_audio_diagnostics(&stats);
    assert(stats.callbacks==40&&stats.identity_count==32&&stats.unrecorded_callbacks==8);
    assert(stats.category_callbacks[BW_AUDIO_MUSIC]==40);
    w32(owner(0)+0xC,0x80000100);w32(AUDIO+0x78,0x80000100);
    assert(!bluewake_audio_customize_voice(CRC,VPBS,0,samples.data(),80));
    bluewake_audio_diagnostics(&stats);assert(stats.identities[0].callbacks==2&&stats.unrecorded_callbacks==8);

    memory.epoch++;assert(!bluewake_audio_customize_voice(CRC,VPBS,0,samples.data(),80));
    bluewake_audio_diagnostics(&stats);
    assert(stats.callbacks==1&&stats.identity_count==1&&stats.epoch==memory.epoch);
    memory.change_after=5;assert(!bluewake_audio_customize_voice(CRC,VPBS,0,samples.data(),80));
    bluewake_audio_diagnostics(&stats);
    assert(stats.callbacks==1&&stats.category_callbacks[BW_AUDIO_UNKNOWN]==1&&stats.epoch==memory.epoch);
    assert(stats.identities[0].owner.sound==0&&stats.identities[0].owner.root_track==0);
    assert(!bluewake_audio_customize_voice(CRC,VPBS,0,samples.data(),80));
    bluewake_audio_diagnostics(&stats);assert(stats.callbacks==2&&stats.identity_count==2);
    bluewake_audio_reset();bluewake_audio_diagnostics(&stats);
    assert(stats.enabled&&stats.callbacks==0&&stats.identity_count==0&&stats.epoch==0);
    fixture();stream_fixture();samples=original;
    assert(!bluewake_audio_customize_voice(CRC,VPBS,2,samples.data(),80));
    assert(!bluewake_audio_customize_voice(CRC,VPBS,3,samples.data(),80));
    bluewake_audio_diagnostics(&stats);assert(stats.category_callbacks[BW_AUDIO_STREAM]==2);
    bluewake_audio_diagnostics_enable(false);memory.reads=0;
    assert(!bluewake_audio_customize_voice(CRC,VPBS,2,samples.data(),80));
    bluewake_audio_diagnostics(&stats);
    assert(!stats.enabled&&stats.callbacks==0&&stats.identity_count==0&&memory.reads==0&&samples==original);
}

struct Run {
    std::vector<uint8_t> output,vpbs,renderer;
    std::vector<uint32_t> positions;
};
void append(std::vector<uint8_t>& out,uint32_t address,size_t size) {
    out.insert(out.end(),memory.ram.begin()+address-BASE,memory.ram.begin()+address-BASE+size);
}
void setup_vpb(unsigned voice,bool afc,bool dolby,bool filters,unsigned sample_length) {
    const uint32_t v=VPBS+voice*0x180,pcm=0x80080000+voice*0x2000;
    w16(v,1);w16(v+4,0x1800);w16(v+8,1);
    const uint16_t buses[]={0xD00,0xD60,0xE80,0xEE0};
    for(unsigned i=0;i<4;i++){w16(v+0x10+i*8,buses[i]);w16(v+0x12+i*8,0x1800);w16(v+0x14+i*8,0x2400);}
    if(dolby){w16(v+0x50,0x4040);w16(v+0x52,0x3000);w16(v+0x54,0x3000);w16(v+0x56,0x4000);w16(v+0x58,1);}
    // Default960 is eight full120-sample blocks. A separately corrected donor
    // baseline also qualifies the genuine partial final block at length1024.
    w32(v+0x74,sample_length);w16(v+0x100,afc?9:33);w32(v+0x114,1024u<<16);
    w32(v+0x118,afc?0x1000:pcm);w32(v+0x110,afc?0:pcm);
    if(afc){w16(v+0x102,1);w32(v+0x114,128);}
    if(filters){w16(v+0x108,0x20);w16(v+0x148,30000);w16(v+0x14A,1000);w16(v+0x14C,uint16_t(-1000));w16(v+0x14E,100);w16(v+0x150,64);}
    for(unsigned i=0;i<1024;i++)w16(pcm+i*2,uint16_t(int16_t((int(i%61)-30)*150+int(voice)*200)));
}
void reverb_fixture() {
    // Native RPB 2 owns front-left reverb. A half-level delayed feed returns
    // to front-left so a live music/SFX mix has a genuine shared wet path.
    const uint32_t r=RPBS+2*0x20;
    w16(r,1);w16(r+2,2);w32(r+4,REVERB);w16(r+8,0xD00);w16(r+10,0x4000);w16(r+16+14,0x4000);
}
Run render(unsigned mask,unsigned music,unsigned sfx,bool dolby,bool afc,bool filters,
           bool streams=false,unsigned frames=4,unsigned change_frame=~0u,
           unsigned next_music=100,unsigned next_sfx=100,unsigned sample_length=960,bool diagnostic=false,
           uint32_t sub_reward_id=0) {
    fixture();bluewake_audio_diagnostics_enable(diagnostic);
    if(sub_reward_id){w32(AUDIO+0x6C,owner(2));w32(AUDIO+0x74,sub_reward_id);w32(owner(2)+0xC,sub_reward_id);}
    if(streams)stream_fixture();assert(bluewake_audio_configure(100,music,sfx,false));
    for(unsigned i=0;i<4;i++)setup_vpb(i,afc&&i==0,dolby&&i==0,filters,sample_length);
    for(unsigned block=0;block<64;block++){
        memory.aram[0x1000+block*9]=0x80;
        for(unsigned n=1;n<9;n++)memory.aram[0x1000+block*9+n]=uint8_t(n%2?0x34:0xB2);
    }
    reverb_fixture();
    DSP::HLE::ZeldaAudioRenderer renderer(Core::System::GetInstance());
    renderer.SetFlags(0);renderer.SetVPBBaseAddress(VPBS);renderer.SetReverbPBBaseAddress(RPBS);
    renderer.SetOutputLeftBufferAddr(OUTPUT_L);renderer.SetOutputRightBufferAddr(OUTPUT_R);renderer.SetOutputVolume(0x1000);
    std::array<int16_t,256> coefficients{};
    for(unsigned i=0;i<256;i+=4){coefficients[i]=0x4000;coefficients[i+1]=0x3FFF;}
    renderer.SetResamplingCoeffs(std::move(coefficients));
    std::array<int16_t,128> sine{};for(unsigned i=0;i<128;i++)sine[i]=int16_t(i*32767/127);renderer.SetSineTable(std::move(sine));
    std::array<int16_t,32> afc_coefficients{};afc_coefficients[0]=0x200;renderer.SetAfcCoeffs(std::move(afc_coefficients));
    Run out;
    for(unsigned frame=0;frame<frames;frame++) {
        if(frame==change_frame)assert(bluewake_audio_configure(100,next_music,next_sfx,false));
        renderer.PrepareFrame();
        for(unsigned voice=0;voice<4;voice++)if(mask&(1u<<voice)) {
#ifdef BLUEWAKE_DSP_AUDIO_CUSTOMIZATION
            renderer.AddVoice(uint16_t(voice),CRC);
#else
            renderer.AddVoice(uint16_t(voice));
#endif
        }
        renderer.FinalizeFrame();
        append(out.output,OUTPUT_L+frame*160,160);append(out.output,OUTPUT_R+frame*160,160);
        append(out.vpbs,VPBS,4*0x180);
        out.positions.push_back(r32(VPBS+0x68));
        std::array<uint8_t,32768> state{};uint8_t* cursor=state.data();
        PointerWrap wrap(&cursor,state.size(),PointerWrap::Mode::Write);renderer.DoState(wrap);
        out.renderer.insert(out.renderer.end(),state.data(),cursor);
    }
    assert(std::any_of(out.output.begin(),out.output.end(),[](uint8_t b){return b!=0;}));
    return out;
}
std::vector<uint8_t> native_records() {
    std::vector<uint8_t> out;
    for(unsigned scenario=0;scenario<4;scenario++) {
        Run r=render(scenario==3?0xCu:3u,100,100,scenario==1,scenario==2,true,scenario==3);
        for(const auto* data:{&r.output,&r.vpbs,&r.renderer})out.insert(out.end(),data->begin(),data->end());
    }
#ifdef BLUEWAKE_DSP_COMPLETION_FIXED
    // Corrected native baseline is distinct from the original partial-block
    // defect. Category unity must match every corrected terminal sample,
    // native DSP feedback and renderer/reverb state exactly.
    Run r=render(3,100,100,false,false,true,false,12,~0u,100,100,1024);
    for(const auto* data:{&r.output,&r.vpbs,&r.renderer})out.insert(out.end(),data->begin(),data->end());
#endif
    return out;
}
void mixer_tests() {
#ifdef BLUEWAKE_DSP_AUDIO_CUSTOMIZATION
    const Run untraced=render(3,100,100,false,false,true);
    const Run traced=render(3,100,100,false,false,true,false,4,~0u,100,100,960,true);
    assert(traced.output==untraced.output&&traced.vpbs==untraced.vpbs&&traced.renderer==untraced.renderer);
    BwAudioDiagnostics evidence{};bluewake_audio_diagnostics(&evidence);
    assert(evidence.callbacks==8&&evidence.classifications==8&&evidence.identity_count==2&&
           evidence.category_callbacks[BW_AUDIO_MUSIC]==4&&evidence.category_callbacks[BW_AUDIO_SFX]==4);
    bluewake_audio_diagnostics_enable(false);
    for(bool dolby:{false,true})for(bool afc:{false,true}) {
        const Run native=render(3,100,100,dolby,afc,true);
        const Run sfx_only=render(2,100,100,dolby,afc,true);
        const Run music_muted=render(3,0,100,dolby,afc,true);
        assert(music_muted.output==sfx_only.output);
        assert(music_muted.vpbs==native.vpbs); // Resample/AFC/filter/ramps/position native.
        assert(music_muted.positions==native.positions);
        const Run music_only=render(1,100,100,dolby,afc,true);
        const Run sfx_muted=render(3,100,0,dolby,afc,true);
        assert(sfx_muted.output==music_only.output&&sfx_muted.vpbs==native.vpbs);
        const Run attenuated=render(3,37,71,dolby,afc,true);
        assert(attenuated.output!=native.output&&attenuated.vpbs==native.vpbs);
    }
    const Run fanfare=render(4,100,100,false,false,true);
    const Run protected_fanfare=render(7,0,0,false,false,true);
    assert(protected_fanfare.output==fanfare.output);
    for(uint32_t id:std::array<uint32_t,6>{0x80000002,0x80000025,0x80000027,0x80000024,0x8000004F,0x8000005D}) {
        const Run native_cue=render(4,100,100,false,false,true,false,4,~0u,100,100,960,false,id);
        const Run zero_categories=render(7,0,0,false,false,true,false,4,~0u,100,100,960,false,id);
        const Run partial_categories=render(4,37,71,false,false,true,false,4,~0u,100,100,960,false,id);
        assert(zero_categories.output==native_cue.output&&partial_categories.output==native_cue.output);
        assert(partial_categories.vpbs==native_cue.vpbs&&partial_categories.renderer==native_cue.renderer);
        assert(partial_categories.positions==native_cue.positions);
    }
    const Run stream=render(0xCu,100,100,false,false,true,true);
    /* Music and stream muted together leave a silent mix. render requires a
     * nonzero result, so instead compare streams+SE to the SE-only native run. */
    const Run stream_se=render(0xEu,0,100,false,false,true,true);
    const Run se=render(2,100,100,false,false,true,true);
    assert(stream_se.output==se.output);
    const Run stream_half=render(0xCu,50,100,false,false,true,true);
    assert(stream_half.output!=stream.output&&stream_half.vpbs==stream.vpbs);
    // Live changes preserve already-mixed reverb tails while new dry/wet
    // voice input stops. The native JAS envelopes and playback still advance.
    const Run live_native=render(1,100,100,false,false,true,false,4);
    const Run live_mute=render(1,100,100,false,false,true,false,4,2,0,100);
    assert(live_mute.vpbs==live_native.vpbs&&live_mute.positions==live_native.positions);
    assert(std::equal(live_native.output.begin(),live_native.output.begin()+640,live_mute.output.begin()));
    assert(live_mute.output!=live_native.output);
    assert(std::any_of(live_mute.output.begin()+640,live_mute.output.end(),[](uint8_t b){return b!=0;}));
    // A muted voice must finish on the native sample clock, not pause or end
    // early. Ordinary MRAM PCM runs past its actual 960-sample completion.
    const Run completed=render(3,100,100,false,false,true,false,12);
    const Run completed_muted=render(3,0,100,false,false,true,false,12);
    assert(completed_muted.vpbs==completed.vpbs&&completed_muted.positions==completed.positions);
    assert(r16(VPBS+2)==1&&r32(VPBS+0x74)==0);
    assert(completed.positions.front()!=completed.positions.back());
#ifdef BLUEWAKE_DSP_COMPLETION_FIXED
    const Run partial=render(3,100,100,false,false,true,false,12,~0u,100,100,1024);
    const Run partial_muted=render(3,0,100,false,false,true,false,12,~0u,100,100,1024);
    assert(partial_muted.vpbs==partial.vpbs&&partial_muted.positions==partial.positions);
    assert(r16(VPBS+2)==1&&r32(VPBS+0x74)==0);
    const Run partial_again=render(3,100,100,false,false,true,false,12,~0u,100,100,1024);
    assert(partial_again.output==partial.output&&partial_again.vpbs==partial.vpbs&&partial_again.renderer==partial.renderer);
#endif
    // Apply master to an actual donor-output COPY after planar -> stereo
    // normalization. Guest DMA PCM, VPBs and reverb state stay untouched.
    const Run final_native=render(3,100,100,true,true,true);
    const auto guest_before=memory.ram;
    std::vector<uint8_t> stereo;
    for(size_t frame=0;frame<final_native.output.size()/320;frame++)
        for(size_t sample=0;sample<80;sample++) {
            const size_t left=frame*320+sample*2,right=left+160;
            stereo.insert(stereo.end(),{final_native.output[left],final_native.output[left+1],
                                       final_native.output[right],final_native.output[right+1]});
        }
    const auto stereo_original=stereo;
    assert(!bluewake_audio_customize_output_be16(stereo.data(),stereo.size()));
    assert(stereo==stereo_original);
    assert(bluewake_audio_configure(0,100,100,false));
    assert(bluewake_audio_customize_output_be16(stereo.data(),stereo.size()));
    assert(std::all_of(stereo.begin(),stereo.end(),[](uint8_t b){return b==0;}));
    assert(memory.ram==guest_before);
#endif
}
} // namespace
int main(int argc,char** argv) {
    assert(argc==3);
    BwAudioConfiguration defaults{};bluewake_audio_configuration(&defaults);
    assert(defaults.master_percent==100&&defaults.music_percent==100&&defaults.sfx_percent==100&&!defaults.muted);
    bluewake::dsp::HleBackend backend;
    assert(backend.initialize(nullptr,guest_pointer,memory.aram.data(),uint32_t(memory.aram.size()),timebase,interrupt));
    ownership_tests();configuration_tests();diagnostics_tests();
    const auto native=native_records();
#ifdef BLUEWAKE_AUDIO_NATIVE_BASELINE
    assert(std::strcmp(argv[1],"--write-native")==0);
    std::ofstream out(std::filesystem::u8path(argv[2]),std::ios::binary|std::ios::trunc);
    assert(out);out.write(reinterpret_cast<const char*>(native.data()),std::streamsize(native.size()));out.close();assert(out);
    std::cout<<"Original donor native records: "<<native.size()<<" bytes\n";
#else
    assert(std::strcmp(argv[1],"--compare-native")==0);
    std::ifstream input(std::filesystem::u8path(argv[2]),std::ios::binary);
    assert(input);const std::vector<uint8_t> original((std::istreambuf_iterator<char>(input)),{});
    assert(native==original);mixer_tests();
    std::cout<<"Audio customization: original donor unity records bitwise equal; native ownership, dry/wet separation, AFC/PCM/filter state, fanfares and atomic settings passed\n";
#endif
    bluewake_audio_reset();assert(bluewake_audio_configure(100,100,100,false));
}
