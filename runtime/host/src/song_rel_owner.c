#include "song_rel_owner.h"
#include <string.h>

static bool r32(const BwSongRelView* v, uint32_t a, uint32_t* out) {
    const uint8_t* p=v->resolve(v->user,a,4);
    if(!p) return false;
    *out=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
    return true;
}
static bool exact32(const BwSongRelView* v,uint32_t a,uint32_t expected) {
    uint32_t n;return r32(v,a,&n)&&n==expected;
}
bool bw_song_rel_owner(const BwSongRelView* v,BwSongOwner* out) {
    if(out) memset(out,0,sizeof *out);
    if(!v||!out||!v->resolve||!v->aliases||!v->slots||!v->sections||
       v->alias_count>512||v->slot_count>512||v->section_count>4096) return false;
    unsigned text_descriptors=0,data_descriptors=0;
    for(uint32_t i=0;i<v->section_count;++i) {
        const BwSongRelSection* s=&v->sections[i];
        if(s->module_id!=257)continue;
        if(s->section_index==1) {
            if(s->linked_start!=0xC0E900E4u||s->size!=0x5FBC)return false;
            ++text_descriptors;
        }
        if(s->section_index==5) {
            if(s->linked_start!=0xC0E96420u||s->size!=0x390)return false;
            ++data_descriptors;
        }
    }
    if(text_descriptors!=1||data_descriptors!=1)return false;
    uint32_t address,tail,previous=0,found=0,raw_text=0,raw_data=0;
    if(!r32(v,0x800030C8u,&address)||!r32(v,0x800030CCu,&tail))return false;
    uint32_t visited[512],visited_count=0;
    while(address) {
        if((address&3)||visited_count==512)return false;
        for(uint32_t i=0;i<visited_count;++i)if(visited[i]==address)return false;
        visited[visited_count++]=address;
        uint32_t id,next,prev,count,table,version;
        if(!r32(v,address,&id)||!r32(v,address+4,&next)||!r32(v,address+8,&prev)||prev!=previous)return false;
        if(id==257) {
            if(found||!r32(v,address+0xC,&count)||count!=19||
               !r32(v,address+0x10,&table)||(table&3)||
               !r32(v,address+0x1C,&version)||version!=3||
               !v->resolve(v->user,table,19*8))return false;
            uint32_t text_size,data_size;
            if(!r32(v,table+8,&raw_text)||(raw_text&1)!=1||
               !r32(v,table+12,&text_size)||text_size!=0x5FBC||
               !r32(v,table+40,&raw_data)||(raw_data&3)||
               !r32(v,table+44,&data_size)||data_size!=0x390)return false;
            raw_text&=~1u;found=address;
        }
        previous=address;address=next;
    }
    if(!found||previous!=tail)return false;
    const BwSongRelSlot* live=NULL;
    for(uint32_t i=0;i<v->slot_count;++i) {
        const BwSongRelSlot* s=&v->slots[i];
        if(s->address!=found)continue;
        if(live||!s->owner||!s->load_token||!exact32(v,s->owner+0x10,found)||
           (uint64_t)found+s->capacity>UINT32_MAX||raw_text<found||
           (uint64_t)raw_text+0x5FBC>(uint64_t)found+s->capacity||raw_data<found||
           (uint64_t)raw_data+0x390>(uint64_t)found+s->capacity)return false;
        live=s;
    }
    if(!live)return false;
    unsigned aliases=0;
    for(uint32_t i=0;i<v->alias_count;++i) {
        const BwSongRelAlias* a=&v->aliases[i];
        if(a->linked_start!=0xC0E900E4u)continue;
        if(a->raw_start!=raw_text||a->raw_end!=(uint64_t)raw_text+0x5FBC||a->text_size!=0x5FBC)return false;
        ++aliases;
    }
    if(aliases!=1)return false;
    /* Read the actual backing used by compiled native actors, not relocated
     * raw .data which the prepared module does not use as its fixed backing. */
    const uint8_t* profile=v->resolve(v->user,0xC0E96620u,0x30);
    if(!profile||profile[8]!=1||profile[9]!=0x6F||profile[0x2C]!=0||
       !exact32(v,0xC0E96630u,0x7C8)||
       !exact32(v,0xC0E96644u,0xC0E96600u)||
       !exact32(v,0xC0E96600u,0xC0E95DCCu)||
       !exact32(v,0xC0E96604u,0xC0E95DECu)||
       !exact32(v,0xC0E96608u,0xC0E95E0Cu)||
       !exact32(v,0xC0E9660Cu,0xC0E95E4Cu)||
       !exact32(v,0xC0E96610u,0xC0E95E2Cu))return false;
    *out=(BwSongOwner){live->load_token,found,live->owner,raw_text};
    return true;
}
