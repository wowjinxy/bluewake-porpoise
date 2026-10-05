// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio_customization.h"
#include <stdatomic.h>
#include <string.h>

/* GZLE01 primary symbols / JAISound, JAISequenceMgr, JASChannel, JASTrack,
 * JASDSPChannel, JAIStreamMgr, JAIZelBasic and m_Do_audio headers. Native pointers are
 * cached MEM1; never turn arbitrary userdata into an invented TTrack. */
enum { RAM_BASE=0x80000000u, RAM_END=0x81800000u,
    /* The static g_mDoAud_zelAudio is mDoAud_zelAudio_c, whose completed
     * construction replaces the JAIZelBasic base vtable (8039B538). */
    WW_UCODE=0x86840740u, AUDIO=0x803A2CE8u, AUDIO_VT=0x803716C8u,
    SOUND_VT=0x8039CAB8u, ZEL_BASIC=0x803F7710u, JAI_BASIC=0x803F7578u,
    CH_BUF=0x803F7538u, DSP_CH=0x803F7520u, LOGICAL=0x803F754Cu,
    SE_HANDLE=0x803F75FCu, SEQ_INFO=0x803F7620u, SEQ_MAX=0x803F6468u,
    STREAM_INFO=0x803F764Cu, STREAM_CHANNELS=0x803F7670u,
    DSP_SIZE=0x14u, VPB_SIZE=0x180u, LOGICAL_SIZE=0xECu,
    SEQ_SIZE=0x50u, TRACK_SIZE=0x38Cu, SEQ_PARAM_SIZE=0x1718u,
    CHANNEL_CALLBACK=0x8028C6C4u, SE_SEQUENCE=0x80000800u,
    MAX_SEQUENCES=16u, MAX_DEPTH=8u };
static _Atomic(uint64_t) g_config=UINT64_C(0x00646464);
static BwAudioGuestReadFn g_read;
static BwAudioEpochFn g_epoch;
static void* g_user;
static BwAudioDiagnostics g_diagnostics;

static void diagnostics_clear(void) {
    const bool enabled=g_diagnostics.enabled;
    memset(&g_diagnostics,0,sizeof g_diagnostics);
    g_diagnostics.enabled=enabled;
}
void bluewake_audio_diagnostics_enable(bool enabled) {
    if(g_diagnostics.enabled!=enabled) {
        g_diagnostics.enabled=enabled;diagnostics_clear();
    }
}
void bluewake_audio_diagnostics(BwAudioDiagnostics* out) {
    if(out)*out=g_diagnostics;
}

bool bluewake_audio_configure(unsigned master,unsigned music,unsigned sfx,bool muted) {
    if(master>100u||music>100u||sfx>100u)return false;
    const uint32_t bits=master|(music<<8)|(sfx<<16)|((uint32_t)muted<<24);
    uint64_t old=atomic_load_explicit(&g_config,memory_order_relaxed),next;
    do {
        if((uint32_t)old==bits)return true;
        next=((uint64_t)((uint32_t)(old>>32)+1u)<<32)|bits;
    } while(!atomic_compare_exchange_weak_explicit(&g_config,&old,next,
                       memory_order_release,memory_order_relaxed));
    return true;
}
void bluewake_audio_configuration(BwAudioConfiguration* out) {
    if(!out)return;
    const uint64_t bits=atomic_load_explicit(&g_config,memory_order_acquire);
    out->master_percent=(unsigned)bits&255u;
    out->music_percent=(unsigned)(bits>>8)&255u;
    out->sfx_percent=(unsigned)(bits>>16)&255u;
    out->muted=((bits>>24)&1u)!=0;
    out->generation=(uint32_t)(bits>>32);
}
void bluewake_audio_attach(BwAudioGuestReadFn read,BwAudioEpochFn epoch,void* user) {
    diagnostics_clear();g_read=read;g_epoch=epoch;g_user=user;
}
void bluewake_audio_reset(void) {diagnostics_clear();g_read=NULL;g_epoch=NULL;g_user=NULL;}
static bool range(uint32_t address,size_t size) {
    return address>=RAM_BASE&&address<RAM_END&&size<=RAM_END-address;
}
static bool audio_read_guest(uint32_t address,void* out,size_t size) {
    return g_read&&out&&range(address,size)&&g_read(g_user,address,out,size);
}
static bool u32(uint32_t address,uint32_t* out) {
    uint8_t bytes[4];if(!audio_read_guest(address,bytes,sizeof bytes))return false;
    *out=((uint32_t)bytes[0]<<24)|((uint32_t)bytes[1]<<16)|((uint32_t)bytes[2]<<8)|bytes[3];return true;
}
static bool u8(uint32_t address,uint8_t* out) {return audio_read_guest(address,out,1);}
static bool aligned(uint32_t address,size_t size) {return (address&3u)==0&&range(address,size);}
static bool sound(uint32_t address,uint32_t type,uint32_t* id,uint32_t* parameter) {
    uint32_t vt;uint8_t state;
    return aligned(address,0x44)&&u32(address,&vt)&&vt==SOUND_VT&&
           u8(address+5,&state)&&state>=3&&state<=5&&u32(address+0xC,id)&&
           (*id&0xC0000000u)==type&&u32(address+0x3C,parameter)&&aligned(*parameter,4);
}
static bool track_active(uint32_t track) {
    uint8_t state;return aligned(track,TRACK_SIZE)&&u8(track+0x37E,&state)&&(state==1||state==3);
}
static bool root_of(uint32_t track,uint32_t* root) {
    uint32_t visited[MAX_DEPTH];unsigned depth=0;
    while(depth<MAX_DEPTH) {
        if(!track_active(track))return false;
        for(unsigned i=0;i<depth;i++)if(visited[i]==track)return false;
        visited[depth++]=track;
        uint32_t parent;if(!u32(track+0x31C,&parent))return false;
        if(parent==0){*root=track;return true;}
        if(!track_active(parent))return false;
        unsigned matches=0;
        for(unsigned i=0;i<16;i++){uint32_t child;if(!u32(parent+0x320+4*i,&child))return false;matches+=child==track;}
        if(matches!=1)return false;
        track=parent;
    }
    return false;
}
static bool fanfare(uint32_t id) {
    /* Actual compiled checkDemoFanfarePlaying 802AC300 calls 802AC258 with
     * precisely these IDs and excludes main/sub handles and SE sequence. */
    return id==0x80000002u||id==0x80000025u||id==0x80000027u||
           id==0x80000024u||id==0x8000004Fu||id==0x8000005Du;
}
static bool classify_voice(uint32_t crc,uint32_t vpb_base,uint16_t voice,
                           BwAudioVoiceInfo* result) {
    BwAudioVoiceInfo out={0};*result=out;
    if(!g_epoch||!g_read)return false;
    const uint64_t epoch=g_epoch(g_user);if(!epoch)return false;
    result->epoch=epoch;
    if(crc!=WW_UCODE||voice>=64)return false;
    uint32_t audio,jai,vt,buffers,dsp,main,sub,stream;
    if(!u32(ZEL_BASIC,&audio)||audio!=AUDIO||!u32(JAI_BASIC,&jai)||jai!=audio||
       !u32(audio,&vt)||vt!=AUDIO_VT||!u32(CH_BUF,&buffers)||!aligned(buffers,64*VPB_SIZE)||
       !u32(DSP_CH,&dsp)||!aligned(dsp,64*DSP_SIZE))return false;
    /* The DSP can mail physical addresses; accept ONLY the exact physical
     * counterpart of the verified CH_BUF, never general alias folding. */
    if(vpb_base!=buffers&&vpb_base!=buffers-RAM_BASE)return false;
    const uint32_t channel=dsp+voice*DSP_SIZE,vpb=buffers+voice*VPB_SIZE;
    uint32_t backlink,sign,callback;uint8_t number,status;
    if(!u8(channel,&number)||number!=voice||!u8(channel+1,&status)||(status!=0&&status!=2)||
       !u32(channel+0xC,&backlink)||backlink!=vpb||!u32(channel+8,&sign)||
       !u32(channel+0x10,&callback)||!u32(audio+0x68,&main)||!u32(audio+0x6C,&sub)||
       !u32(audio+0x70,&stream))return false;
    out.dsp_channel=channel;out.epoch=epoch;*result=out;
    uint32_t stream_channels[2],stream_info,id,param;
    if(u32(STREAM_CHANNELS,&stream_channels[0])&&u32(STREAM_CHANNELS+4,&stream_channels[1])&&
       (channel==stream_channels[0]||channel==stream_channels[1])) {
        uint32_t owner,update;
        if(stream_channels[0]==stream_channels[1]||!sound(stream,0xC0000000u,&id,&param)||
           !aligned(param,0x160)||!u32(STREAM_INFO,&stream_info)||!aligned(stream_info,0x18)||
           !u32(stream_info+0x14,&owner)||owner!=stream||!u32(param+0x15C,&update)||update!=stream_info||
           !u32(audio+0x7C,&owner)||owner!=id)return false;
        /* Actual StreamLib::callBack 8029D874/8029D888 allocates each DSP
         * channel with &assign_ch[side] as its sign, leaves callback null, and
         * raises priority to 127. Never reinterpret that sign as TChannel. */
        for(unsigned i=0;i<2;i++) {
            const uint32_t other=stream_channels[i];uint8_t n,s,p;
            uint32_t back,tag,cb;
            if(other<dsp||other>=dsp+64*DSP_SIZE||(other-dsp)%DSP_SIZE||
               !u8(other,&n)||n!=(other-dsp)/DSP_SIZE||!u8(other+1,&s)||(s!=0&&s!=2)||
               !u8(other+3,&p)||p!=127||!u32(other+8,&tag)||tag!=STREAM_CHANNELS+4*i||
               !u32(other+0x10,&cb)||cb!=0||!u32(other+0xC,&back)||back!=buffers+n*VPB_SIZE)return false;
        }
        out.category=BW_AUDIO_STREAM;out.sound=stream;out.sound_id=id;
    } else {
        uint32_t logical_base,manager,root,table,count;
        uint8_t logical_state;
        if(callback!=CHANNEL_CALLBACK||!u32(LOGICAL,&logical_base)||!aligned(logical_base,256*LOGICAL_SIZE)||
           sign<logical_base||sign>=logical_base+256*LOGICAL_SIZE||(sign-logical_base)%LOGICAL_SIZE||
           !u8(sign+1,&logical_state)||logical_state==0xFF||!u32(sign+0x20,&backlink)||backlink!=channel||
           !u32(sign+4,&manager)||manager<0xF8||!root_of(manager-0xF8,&root)||
           !u32(SEQ_MAX,&count)||count==0||count>MAX_SEQUENCES||!u32(SEQ_INFO,&table)||
           !aligned(table,count*SEQ_SIZE))return false;
        out.logical_channel=sign;out.track=manager-0xF8;out.root_track=root;*result=out;
        unsigned matched=0;uint32_t se;
        if(!u32(SE_HANDLE,&se))return false;
        for(unsigned i=0;i<count;i++) {
            uint32_t owner,slot=table+i*SEQ_SIZE,update;uint8_t index;
            if(!u32(slot+0x48,&owner))return false;
            if(!owner)continue;
            if(!sound(owner,0x80000000u,&id,&param)||!aligned(param,SEQ_PARAM_SIZE))continue;
            if(param+0x1360!=root)continue;
            if(!u8(owner+4,&index)||index!=i||!u32(param+0x135C,&update)||update!=slot)return false;
            matched++;out.sound=owner;out.sound_id=id;*result=out;
            if(owner==se&&id==SE_SEQUENCE)out.category=BW_AUDIO_SFX;
            else if(id==SE_SEQUENCE)return false;
            else if(owner==main||owner==sub) {
                uint32_t handle_id;
                if(!u32(audio+(owner==main?0x78:0x74),&handle_id)||handle_id!=id)return false;
                /* Native setGetItemSound 8012E28C and the song/pearl/box
                 * reward paths use subBgmStart 802A47B8. That routine stores
                 * the sound at +6C and its ID at +74, including reward cues.
                 * checkDemoFanfarePlaying excludes these handles only when
                 * deciding separate demo ducking; it is not a category rule.
                 * Keep main handling unchanged and validate the sub handle
                 * before admitting the source-proven reward ID set. */
                out.category=owner!=main&&fanfare(id)?BW_AUDIO_FANFARE:BW_AUDIO_MUSIC;
            } else if(fanfare(id))out.category=BW_AUDIO_FANFARE;
            else out.category=BW_AUDIO_UNKNOWN;
        }
        if(matched!=1||out.category==BW_AUDIO_UNKNOWN)return false;
    }
    *result=out;return true;
}
static bool classify_once(uint32_t crc,uint32_t base,uint16_t voice,BwAudioVoiceInfo* out) {
    const bool classified=classify_voice(crc,base,voice,out);
    const uint64_t current_epoch=g_epoch?g_epoch(g_user):0;
    if(out->epoch&&current_epoch!=out->epoch) {
        /* An observed lifetime change invalidates even diagnostic pointers. */
        memset(out,0,sizeof *out);out->epoch=current_epoch;return false;
    }
    if(!classified)out->category=BW_AUDIO_UNKNOWN;
    return classified;
}
bool bluewake_audio_classify_voice(uint32_t crc,uint32_t base,uint16_t voice,
                                  BwAudioVoiceInfo* result) {
    BwAudioVoiceInfo out={0};const bool classified=classify_once(crc,base,voice,&out);
    if(result) {
        memset(result,0,sizeof *result);
        if(classified)*result=out;
    }
    return classified;
}
static void increment(uint64_t* value) {if(*value!=UINT64_MAX)(*value)++;}
static bool same_owner(const BwAudioVoiceInfo* a,const BwAudioVoiceInfo* b) {
    return a->category==b->category&&a->dsp_channel==b->dsp_channel&&
        a->logical_channel==b->logical_channel&&a->track==b->track&&
        a->root_track==b->root_track&&a->sound==b->sound&&
        a->sound_id==b->sound_id&&a->epoch==b->epoch;
}
static void record(uint32_t crc,uint32_t base,uint16_t voice,bool classified,
                   const BwAudioVoiceInfo* owner) {
    if(g_diagnostics.epoch!=owner->epoch) {
        diagnostics_clear();g_diagnostics.epoch=owner->epoch;
    }
    increment(&g_diagnostics.callbacks);increment(&g_diagnostics.classifications);
    increment(&g_diagnostics.category_callbacks[owner->category]);
    for(unsigned i=0;i<g_diagnostics.identity_count;i++) {
        BwAudioDiagnosticIdentity* identity=&g_diagnostics.identities[i];
        if(identity->ucode_crc==crc&&identity->vpb_base==base&&identity->voice_id==voice&&
           identity->classified==classified&&same_owner(&identity->owner,owner)) {
            increment(&identity->callbacks);return;
        }
    }
    if(g_diagnostics.identity_count==BW_AUDIO_DIAGNOSTIC_IDENTITIES) {
        increment(&g_diagnostics.unrecorded_callbacks);return;
    }
    BwAudioDiagnosticIdentity* identity=&g_diagnostics.identities[g_diagnostics.identity_count++];
    identity->ucode_crc=crc;identity->vpb_base=base;identity->voice_id=voice;
    identity->classified=classified;identity->owner=*owner;identity->callbacks=1;
}
static int16_t scale(int16_t sample,unsigned percent) {
    return (int16_t)(((int32_t)sample*(int32_t)percent)/100);
}
bool bluewake_audio_customize_voice(uint32_t crc,uint32_t base,uint16_t voice,
                                    int16_t* samples,size_t count) {
    if(!samples||count!=0x50)return false;
    BwAudioConfiguration config;bluewake_audio_configuration(&config);
    if(config.music_percent==100&&config.sfx_percent==100&&!g_diagnostics.enabled)return false;
    BwAudioVoiceInfo owner;const bool classified=classify_once(crc,base,voice,&owner);
    if(g_diagnostics.enabled)record(crc,base,voice,classified,&owner);
    if(!classified)return false;
    const unsigned gain=owner.category==BW_AUDIO_MUSIC||owner.category==BW_AUDIO_STREAM?
        config.music_percent:owner.category==BW_AUDIO_SFX?config.sfx_percent:100u;
    if(gain==100)return false;
    for(size_t i=0;i<count;i++)samples[i]=scale(samples[i],gain);
    return true;
}
bool bluewake_audio_customize_output_be16(uint8_t* stereo,size_t bytes) {
    if(!stereo||bytes%4||bytes==0)return false;
    BwAudioConfiguration config;bluewake_audio_configuration(&config);
    const unsigned gain=config.muted?0:config.master_percent;
    if(gain==100)return false;
    for(size_t i=0;i<bytes;i+=2) {
        const uint16_t bits=(uint16_t)(((unsigned)stereo[i]<<8)|stereo[i+1]);
        const int16_t sample=(int16_t)bits;
        const uint16_t output=(uint16_t)scale(sample,gain);
        stereo[i]=(uint8_t)(output>>8);stereo[i+1]=(uint8_t)output;
    }
    return true;
}
