// SPDX-License-Identifier: GPL-3.0-or-later
#include "randomizer_rel_owner.h"
#include <stddef.h>
#include <string.h>

typedef struct Section { uint32_t start, size; } Section;
typedef struct Spec {
    uint32_t id, count, profile, methods, process, actor_size, draw_priority, cull;
    Section sections[21];
    uint32_t functions[5];
} Spec;
/* GZLE01: primary actor profiles/methods in d_a_tbox.cpp/d_a_demo_item.cpp;
 * fixed linked section addresses from the qualified module descriptor.
 * This table describes source identities, not copied translated code. */
static const Spec kTbox = {
    113,21,0xC1DF3E4C,0xC1DF3E2C,0x126,0x770,0x113,14,
    {{0,0},{0xC1DF00F4,0x3A88},{0xC1DF3B7C,8},{0xC1DF3B84,8},
     {0xC1DF3B90,0x15C},{0xC1DF3CF0,0x364},{0xC1DF59D8,0x24}},
    {0xC1DF3164,0xC1DF30CC,0xC1DF30A4,0xC1DF30C4,0xC1DF2CE4}
};
static const Spec kItem = {
    131,20,0xC07711D8,0xC07711B8,0x103,0x65C,0xFC,0,
    {{0,0},{0xC07700EC,0xF98},{0xC0771084,4},{0xC0771088,8},
     {0xC0771090,0x24},{0xC07710B8,0x28C},{0xC0771AC8,0xA0}},
    {0xC0770A80,0xC07709E0,0xC0770EA0,0xC0770E80,0xC0770F44}
};
static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static uint32_t be16(const uint8_t* p) { return ((uint32_t)p[0]<<8)|p[1]; }
static bool span(uint32_t start, uint32_t size) {
    return size && (uint64_t)start+size <= UINT32_MAX;
}
static bool raw_address(uint32_t a) {
    return a>=0x80000000u && a<0xC0000000u && !(a&3u);
}
static const uint8_t* read_span(const BwRandomizerRelView* v,uint32_t a,uint32_t n) {
    return span(a,n) ? v->resolve(v->user,a,n) : NULL;
}
static bool r32(const BwRandomizerRelView* v,uint32_t a,uint32_t* n) {
    const uint8_t* p=read_span(v,a,4);if(!p)return false;*n=be32(p);return true;
}
static bool inside(uint32_t a,uint32_t n,uint32_t base,uint32_t capacity) {
    return span(a,n) && a>=base && (uint64_t)a+n<=(uint64_t)base+capacity;
}
static bool overlap(uint32_t a,uint32_t n,uint32_t b,uint32_t m) {
    return n && m && (uint64_t)a+n>b && (uint64_t)b+m>a;
}
static bool descriptors(const BwRandomizerRelView* v,const Spec* s) {
    uint32_t seen=0,count=0;
    for(uint32_t i=0;i<v->section_count;++i) {
        const BwRandomizerRelSection* d=&v->sections[i];
        if(d->module_id!=s->id)continue;
        if(d->section_index>=s->count || (seen&(1u<<d->section_index)))return false;
        const Section* expected=&s->sections[d->section_index];
        if(d->linked_start!=expected->start || d->size!=expected->size)return false;
        seen|=1u<<d->section_index;++count;
    }
    return count==s->count;
}
static bool fixed_profile(const BwRandomizerRelView* v,const Spec* s) {
    const Section* d=&s->sections[5];
    const uint8_t* bytes=read_span(v,d->start,d->size);
    if(!bytes)return false;
    const uint8_t* p=bytes+(s->profile-d->start);
    const uint8_t* m=bytes+(s->methods-d->start);
    if(be32(p)!=0xFFFFFFFDu || be16(p+4)!=7 || be16(p+6)!=0xFFFDu ||
       be16(p+8)!=s->process || be32(p+0xC)!=0x803726E8u ||
       be32(p+0x10)!=s->actor_size || be32(p+0x14) || be32(p+0x18) ||
       be32(p+0x1C)!=0x80371FF8u || be16(p+0x20)!=s->draw_priority ||
       be32(p+0x24)!=s->methods || be32(p+0x28)!=0x00044000u ||
       p[0x2C]!=0 || p[0x2D]!=s->cull)return false;
    for(unsigned i=0;i<5;++i)if(be32(m+4*i)!=s->functions[i])return false;
    return true;
}
bool bw_randomizer_rel_owner(const BwRandomizerRelView* v,BwRandomizerRelKind kind,
                            BwRandomizerRelOwner* out) {
    if(out)memset(out,0,sizeof *out);
    const Spec* s=kind==BW_RANDOMIZER_REL_TBOX?&kTbox:
                  kind==BW_RANDOMIZER_REL_DEMO_ITEM?&kItem:NULL;
    if(!s || !out || !v || !v->resolve || !v->aliases || !v->slots || !v->sections ||
       !v->alias_count || v->alias_count>512 || !v->slot_count || v->slot_count>512 ||
       v->section_count>4096 || !descriptors(v,s))return false;
    uint32_t node,tail,previous=0,found=0,raw_text=0,raw_data=0;
    if(!r32(v,0x800030C8u,&node) || !r32(v,0x800030CCu,&tail))return false;
    uint32_t visited[512],visited_count=0;
    while(node) {
        if(!raw_address(node) || visited_count==512)return false;
        for(uint32_t i=0;i<visited_count;++i)if(visited[i]==node)return false;
        visited[visited_count++]=node;
        const uint8_t* h=read_span(v,node,0x20);if(!h)return false;
        const uint32_t id=be32(h),next=be32(h+4),prev=be32(h+8);
        if(prev!=previous)return false;
        if(id==s->id) {
            const uint32_t table=be32(h+0x10);
            if(found || be32(h+0xC)!=s->count || be32(h+0x1C)!=3 ||
               (uint64_t)node+0x4C>UINT32_MAX || table!=node+0x4C)return false;
            const uint8_t* entries=read_span(v,table,s->count*8);if(!entries)return false;
            const uint32_t text=be32(entries+8),data=be32(entries+40);
            if((text&3u)!=1 || !raw_address(text&~1u) || !raw_address(data) ||
               be32(entries+12)!=s->sections[1].size ||
               be32(entries+44)!=s->sections[5].size)return false;
            raw_text=text&~1u;raw_data=data;found=node;
        }
        previous=node;node=next;
    }
    if(!found || previous!=tail)return false;
    const BwRandomizerRelSlot* live=NULL;
    for(uint32_t i=0;i<v->slot_count;++i) {
        const BwRandomizerRelSlot* slot=&v->slots[i];
        if(slot->address!=found)continue;
        uint32_t owned;
        if(live || !raw_address(slot->owner) || !slot->materialization ||
           !span(found,slot->capacity) ||
           !inside(found,0x4C+s->count*8,found,slot->capacity) ||
           !inside(raw_text,s->sections[1].size,found,slot->capacity) ||
           !inside(raw_data,s->sections[5].size,found,slot->capacity) ||
           !r32(v,slot->owner+0x10,&owned) || owned!=found)return false;
        live=slot;
    }
    if(!live || overlap(raw_text,s->sections[1].size,raw_data,s->sections[5].size))return false;
    const uint32_t metadata_size=0x4C+s->count*8;
    if(overlap(found,metadata_size,raw_text,s->sections[1].size) ||
       overlap(found,metadata_size,raw_data,s->sections[5].size))return false;
    /* Another live slot or executable mapping must not shadow this owner. */
    for(uint32_t i=0;i<v->slot_count;++i) {
        const BwRandomizerRelSlot* other=&v->slots[i];
        if(other==live || !other->owner || !other->materialization)continue;
        uint32_t owned;
        if(overlap(found,live->capacity,other->address,other->capacity)) {
            if(!raw_address(other->owner) || !r32(v,other->owner+0x10,&owned) ||
               owned==other->address)return false;
        }
    }
    unsigned matches=0;
    for(uint32_t i=0;i<v->alias_count;++i) {
        const BwRandomizerRelAlias* a=&v->aliases[i];
        const bool touches_raw=a->raw_end>a->raw_start &&
            overlap(a->raw_start,a->raw_end-a->raw_start,raw_text,s->sections[1].size);
        const bool touches_fixed=overlap(a->linked_start,a->text_size,
                                       s->sections[1].start,s->sections[1].size);
        if(!touches_raw && !touches_fixed)continue;
        if(a->raw_start!=raw_text || a->raw_end!=(uint64_t)raw_text+s->sections[1].size ||
           a->linked_start!=s->sections[1].start || a->text_size!=s->sections[1].size)return false;
        ++matches;
    }
    if(matches!=1 || !fixed_profile(v,s))return false;
    *out=(BwRandomizerRelOwner){live->materialization,s->id,found,live->owner,
                              raw_text,raw_data,s->profile,s->methods};
    return true;
}
