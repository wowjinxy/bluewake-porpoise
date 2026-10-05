#include "song_host_adapter.h"
#include <string.h>

_Static_assert(sizeof(BwSongHostSlot)==12,"Do not change HOSTVARS slot stride");
_Static_assert(sizeof(BwSongHostAlias)==16,"Do not change HOSTVARS alias stride");
static bool identity(const BwSongHostAdapter* h,const CPUState* cpu) {
    return h&&h->active&&!h->exhausted&&cpu&&cpu==h->cpu&&cpu->ram&&
        cpu->ram==h->ram&&cpu->ram_size==h->ram_size&&h->module&&
        h->backing&&h->generation&&h->fixed_data;
}
void bw_song_host_revoke(BwSongHostAdapter* h) {
    if(!h)return;
    const uint64_t sequence=h->sequence;const bool exhausted=h->exhausted;
    memset(h,0,sizeof *h);h->sequence=sequence;h->exhausted=exhausted;
}
bool bw_song_host_bind(BwSongHostAdapter* h,const CPUState* cpu,const StaticRecompModuleDesc* mod,
                      BwSongHostFixedBacking backing,BwSongHostAliasGeneration generation,void* user) {
    if(!h)return false;bw_song_host_revoke(h);
    if(h->exhausted||!cpu||!cpu->ram||cpu->ram_size<0x01800000u||cpu->ram_size>0x10000000u||
       !mod||!backing||!generation||!mod->dispatch||
       (mod->abi_version!=3&&mod->abi_version!=4&&mod->abi_version!=5)||
       mod->cpu_abi_version!=GXRUNTIME_CPU_ABI_VERSION||mod->cpu_state_size!=sizeof(CPUState)||
       memcmp(mod->game_id,"GZLE01",sizeof("GZLE01"))||!mod->rel_modules||
       mod->num_rel_modules>512)return false;
    const StaticRecompRelModule* hr=NULL;
    for(uint32_t i=0;i<mod->num_rel_modules;++i) {
        const StaticRecompRelModule* r=&mod->rel_modules[i];
        if(r->module_id!=257)continue;
        if(hr||!r->sections||r->section_count!=19||r->num_sections!=19||r->section_info_offset!=0x4C)return false;
        hr=r;
    }
    if(!hr)return false;
    unsigned text=0,data=0;
    for(uint32_t i=0;i<19;++i) {
        const StaticRecompRelSection* s=&hr->sections[i];
        if(s->module_id!=257||s->section_index!=i)return false;
        h->sections[i]=(BwSongRelSection){s->module_id,s->section_index,s->linked_start,s->size};
        if(i==1) {if(s->linked_start!=0xC0E900E4u||s->size!=0x5FBC)return false;++text;}
        if(i==5) {if(s->linked_start!=0xC0E96420u||s->size!=0x390)return false;++data;}
    }
    if(text!=1||data!=1)return false;
    const uint8_t* ram=cpu->ram;const uint32_t ram_size=cpu->ram_size;
    const uint8_t* fixed=NULL;uint32_t now=0;
    if(!backing(user,&fixed,&now)||!fixed||generation(user)!=now||cpu->ram!=ram||cpu->ram_size!=ram_size)return false;
    h->cpu=cpu;h->ram=cpu->ram;h->ram_size=cpu->ram_size;h->module=mod;h->fixed_data=fixed;
    h->backing=backing;h->generation=generation;h->alias_user=user;h->active=true;
    return true;
}
void bw_song_host_clear_slot(BwSongHostAdapter* h,uint32_t i){if(h&&i<BW_SONG_HOST_SLOTS)h->load_tokens[i]=0;}
void bw_song_host_materialized(BwSongHostAdapter* h,const CPUState* cpu,uint32_t i) {
    if(!h||i>=BW_SONG_HOST_SLOTS)return;
    h->load_tokens[i]=0;
    if(!identity(h,cpu))return;
    if(h->sequence==UINT64_MAX) {h->exhausted=true;h->active=false;memset(h->load_tokens,0,sizeof h->load_tokens);return;}
    h->load_tokens[i]=++h->sequence;
}
typedef struct ReadContext {const BwSongHostAdapter* host;const CPUState* cpu;uint32_t generation;} ReadContext;
static bool read_identity(const ReadContext* r) {
    const BwSongHostAdapter* h=r->host;if(!identity(h,r->cpu))return false;
    const uint32_t now=h->generation(h->alias_user);
    return identity(h,r->cpu)&&now==r->generation;
}
static const uint8_t* native_span(const ReadContext* r,uint32_t address,uint32_t size) {
    if(!read_identity(r))return NULL;
    const uint8_t* native=get_ram_ptr((CPUState*)r->cpu,address,size,NULL);
    if(!native||!read_identity(r))return NULL;
    /* The helper indexes byte fields inside its bounded profile span. A
     * smaller registered alias may shadow those native byte reads without
     * containing the entire larger span. Reject noncontiguous native backing
     * rather than borrow stale bytes from the containing RAM/data allocation. */
    if(size>4u)for(uint32_t i=0;i<size;++i) {
        if(!read_identity(r))return NULL;
        const uint8_t* byte=get_ram_ptr((CPUState*)r->cpu,address+i,1,NULL);
        if(!read_identity(r)||byte!=native+i)return NULL;
    }
    return native;
}
static const uint8_t* resolve(void* user,uint32_t address,uint32_t size) {
    const ReadContext* r=user;const BwSongHostAdapter* h=r->host;
    /* Check CPU/RAM and current alias generation BEFORE either fixed .data or
     * raw extension RAM can be read. No CPU faulting/side-effecting helpers. */
    if(!read_identity(r)||!size)return NULL;
    if(address>=0xC0E96420u&&size<=0x390u&&address-0xC0E96420u<=0x390u-size) {
        const uint8_t* expected=h->fixed_data+(address-0xC0E96420u);
        const uint8_t* native=native_span(r,address,size);
        if(native!=expected)return NULL;
        return native;
    }
    if(address<0x80000000u||size>h->ram_size||address-0x80000000u>h->ram_size-size)return NULL;
    return native_span(r,address,size);
}
static bool native_slot_live(const ReadContext* r,const BwSongHostSlot* slot) {
    const BwSongHostAdapter* h=r->host;const CPUState* cpu=r->cpu;
    if(!identity(h,cpu)||!slot->owner||!slot->address||!slot->capacity||
       slot->owner<0x80000000u||h->ram_size<0x14u||
       slot->owner-0x80000000u>h->ram_size-0x14u||
       slot->address<0x80000000u||slot->capacity>h->ram_size||
       slot->address-0x80000000u>h->ram_size-slot->capacity)return false;
    const uint8_t* p=resolve((void*)r,slot->owner+0x10u,4);if(!p)return false;
    const uint32_t a=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
    return a==slot->address;
}
bool bw_song_host_restore_slots(BwSongHostAdapter* h,const CPUState* cpu,const BwSongHostSlot* slots,uint32_t count) {
    if(!h)return false;memset(h->load_tokens,0,sizeof h->load_tokens);
    if(!identity(h,cpu)||!slots||count>BW_SONG_HOST_SLOTS)return false;
    const uint8_t* fixed=NULL;uint32_t generation=0;
    if(!h->backing(h->alias_user,&fixed,&generation)||fixed!=h->fixed_data||h->generation(h->alias_user)!=generation)return false;
    ReadContext reads={h,cpu,generation};
    for(uint32_t i=0;i<count;++i)if(native_slot_live(&reads,&slots[i]))bw_song_host_materialized(h,cpu,i);
    if(!read_identity(&reads)){memset(h->load_tokens,0,sizeof h->load_tokens);return false;}
    return true;
}
bool bw_song_host_query(const BwSongHostAdapter* h,const CPUState* cpu,const BwSongHostAlias* aliases,
                        uint32_t alias_count,const BwSongHostSlot* slots,uint32_t slot_count,BwSongOwner* out) {
    if(out)memset(out,0,sizeof *out);
    if(!out||!identity(h,cpu)||!aliases||!slots||alias_count>512||slot_count>512)return false;
    const uint8_t* fixed=NULL;uint32_t generation=0;
    if(!h->backing(h->alias_user,&fixed,&generation)||fixed!=h->fixed_data||
       h->generation(h->alias_user)!=generation)return false;
    BwSongRelAlias a[512];BwSongRelSlot s[512];
    for(uint32_t i=0;i<alias_count;++i)a[i]=(BwSongRelAlias){aliases[i].raw_start,aliases[i].raw_end,aliases[i].linked_start,aliases[i].text_size};
    for(uint32_t i=0;i<slot_count;++i)s[i]=(BwSongRelSlot){slots[i].owner,slots[i].address,slots[i].capacity,h->load_tokens[i]};
    ReadContext reads={h,cpu,generation};
    const BwSongRelView v={resolve,&reads,a,alias_count,s,slot_count,h->sections,19};
    if(!bw_song_rel_owner(&v,out))return false;
    /* A last identity check makes mutations detected by the read-only source
     * fail closed; no query changes tokens, CPU or loader structures. */
    if(!read_identity(&reads)){memset(out,0,sizeof *out);return false;}
    return true;
}
static uint32_t le32(const uint8_t* p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
bool bw_song_host_state_fields(const uint8_t* blob,uint64_t size) {
    if(!blob||size<4||size>256u*1024u*1024u)return false;
    const uint32_t count=le32(blob);if(count>65536)return false;
    uint64_t at=4;unsigned found=0;
    for(uint32_t i=0;i<count;++i) {
        if(size-at<2)return false;const uint32_t length=blob[at]|((uint32_t)blob[at+1]<<8);at+=2;
        if(size-at<(uint64_t)length+4)return false;const uint8_t* name=blob+at;at+=length;
        const uint32_t n=le32(blob+at);at+=4;if(size-at<n)return false;
        static const char* const names[]={"g_rel_slots","g_rel_aliases","g_rel_alias_count"};
        static const uint32_t lengths[]={512u*12u,512u*16u,4u};
        for(unsigned j=0;j<3;++j) {
            if(length!=strlen(names[j])||memcmp(name,names[j],length))continue;
            if((found&(1u<<j))||n!=lengths[j])return false;found|=1u<<j;
        }
        at+=n;
    }
    return found==7&&at==size;
}
