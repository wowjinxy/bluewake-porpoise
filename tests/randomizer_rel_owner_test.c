// SPDX-License-Identifier: GPL-3.0-or-later
// Authored synthetic OS registration/data, never translated/native game bytes.
#include "randomizer_rel_owner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MS-compatible C enums give large guest addresses signed int semantics. */
static const uint32_t Header=0x80010000u, Loader=0x80008000u, Text=0x80010100u,
                      Data=0x80015000u, Other=0x80018000u;
typedef struct Fixture {
    uint8_t list[8],raw[0x10000],loaders[0x100],fixed[0x364];
    BwRandomizerRelAlias aliases[3];
    BwRandomizerRelSlot slots[3];
    BwRandomizerRelSection sections[23];
    BwRandomizerRelView view;
    uint32_t fixed_start,fixed_size,profile,methods,text_start,text_size;
    uint32_t denied_start,denied_size,reads;
} Fixture;
static unsigned checks;
static void check(bool b) {++checks;if(!b){fprintf(stderr,"REL ownership check %u failed\n",checks);abort();}}
static bool zero_output(const BwRandomizerRelOwner* out) {
    const unsigned char* p=(const unsigned char*)out;
    for(unsigned i=0;i<sizeof *out;++i)if(p[i])return false;
    return true;
}
static void put32(uint8_t* p,uint32_t v) {p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void put16(uint8_t* p,uint32_t v) {p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static const uint8_t* area(uint32_t a,uint32_t n,uint32_t start,uint32_t size,const uint8_t* p) {
    return n && a>=start && n<=size && a-start<=size-n ? p+(a-start) : NULL;
}
static const uint8_t* resolve(void* user,uint32_t a,uint32_t n) {
    Fixture* f=user;++f->reads;
    /* All reads remain bounded; denied spans simulate a lost lease/alias. */
    if(!n || n>0x364 || (uint64_t)a+n>UINT32_MAX)return NULL;
    if(f->denied_size && (uint64_t)a+n>f->denied_start &&
       (uint64_t)f->denied_start+f->denied_size>a)return NULL;
    const uint8_t* p=area(a,n,0x800030C8u,8,f->list);if(p)return p;
    p=area(a,n,Loader,sizeof f->loaders,f->loaders);if(p)return p;
    p=area(a,n,Header,sizeof f->raw,f->raw);if(p)return p;
    return area(a,n,f->fixed_start,f->fixed_size,f->fixed);
}
static void setup(Fixture* f,BwRandomizerRelKind kind) {
    memset(f,0,sizeof *f);
    const bool chest=kind==BW_RANDOMIZER_REL_TBOX;
    const uint32_t id=chest?113:131,count=chest?21:20;
    const uint32_t starts113[]={0,0xC1DF00F4u,0xC1DF3B7Cu,0xC1DF3B84u,0xC1DF3B90u,0xC1DF3CF0u,0xC1DF59D8u};
    const uint32_t sizes113[]={0,0x3A88,8,8,0x15C,0x364,0x24};
    const uint32_t starts131[]={0,0xC07700ECu,0xC0771084u,0xC0771088u,0xC0771090u,0xC07710B8u,0xC0771AC8u};
    const uint32_t sizes131[]={0,0xF98,4,8,0x24,0x28C,0xA0};
    for(uint32_t i=0;i<count;++i)
        f->sections[i]=(BwRandomizerRelSection){id,i,i<7?(chest?starts113[i]:starts131[i]):0,
                                                 i<7?(chest?sizes113[i]:sizes131[i]):0};
    f->fixed_start=chest?0xC1DF3CF0u:0xC07710B8u;f->fixed_size=chest?0x364:0x28C;
    f->profile=chest?0xC1DF3E4Cu:0xC07711D8u;f->methods=chest?0xC1DF3E2Cu:0xC07711B8u;
    f->text_start=chest?0xC1DF00F4u:0xC07700ECu;f->text_size=chest?0x3A88:0xF98;
    put32(f->list,Header);put32(f->list+4,Header);
    put32(f->raw,id);put32(f->raw+0xC,count);put32(f->raw+0x10,Header+0x4C);put32(f->raw+0x1C,3);
    put32(f->raw+0x4C+8,Text|1);put32(f->raw+0x4C+12,f->text_size);
    put32(f->raw+0x4C+40,Data);put32(f->raw+0x4C+44,f->fixed_size);
    put32(f->loaders+0x10,Header);
    f->slots[0]=(BwRandomizerRelSlot){Loader,Header,0x6000,UINT64_C(0x123456789)};
    f->aliases[0]=(BwRandomizerRelAlias){Text,Text+f->text_size,f->text_start,f->text_size};
    uint8_t* p=f->fixed+(f->profile-f->fixed_start);
    put32(p,0xFFFFFFFDu);put16(p+4,7);put16(p+6,0xFFFD);put16(p+8,chest?0x126:0x103);
    put32(p+0xC,0x803726E8u);put32(p+0x10,chest?0x770:0x65C);
    put32(p+0x1C,0x80371FF8u);put16(p+0x20,chest?0x113:0xFC);
    put32(p+0x24,f->methods);put32(p+0x28,0x44000);p[0x2D]=chest?14:0;
    const uint32_t functions113[]={0xC1DF3164u,0xC1DF30CCu,0xC1DF30A4u,0xC1DF30C4u,0xC1DF2CE4u};
    const uint32_t functions131[]={0xC0770A80u,0xC07709E0u,0xC0770EA0u,0xC0770E80u,0xC0770F44u};
    for(unsigned i=0;i<5;++i)put32(f->fixed+(f->methods-f->fixed_start)+4*i,chest?functions113[i]:functions131[i]);
    f->view=(BwRandomizerRelView){resolve,f,f->aliases,1,f->slots,1,f->sections,count};
}
static bool query(Fixture* f,BwRandomizerRelKind kind,BwRandomizerRelOwner* out) {
    const unsigned prior=f->reads;bool ok=bw_randomizer_rel_owner(&f->view,kind,out);
    check(f->reads-prior<1600);return ok;
}
static void rejected(Fixture* f,BwRandomizerRelKind kind) {
    BwRandomizerRelOwner out;memset(&out,0xA5,sizeof out);
    check(!query(f,kind,&out));
    check(zero_output(&out));
}
static void happy(Fixture* f,BwRandomizerRelKind kind) {
    BwRandomizerRelOwner out;
    check(query(f,kind,&out));
    check(out.materialization==UINT64_C(0x123456789) && out.module_id==(uint32_t)kind &&
          out.module_header==Header && out.loader_owner==Loader && out.raw_text==Text &&
          out.raw_data==Data && out.fixed_profile==f->profile && out.fixed_methods==f->methods);
}
int main(void) {
    for(unsigned mode=0;mode<2;++mode) {
        const BwRandomizerRelKind kind=mode?BW_RANDOMIZER_REL_DEMO_ITEM:BW_RANDOMIZER_REL_TBOX;
        Fixture f;setup(&f,kind);happy(&f,kind);happy(&f,kind);
        // Native list topology: other modules are legal, duplicates/cycles are not.
        put32(f.raw+4,Other);put32(f.raw+0x8000,777);put32(f.raw+0x8008,Header);put32(f.list+4,Other);happy(&f,kind);
        put32(f.raw+0x8000,(uint32_t)kind);rejected(&f,kind);
        setup(&f,kind);put32(f.raw+4,Header);rejected(&f,kind);
        setup(&f,kind);put32(f.raw+8,Other);rejected(&f,kind);
        setup(&f,kind);put32(f.list+4,Other);rejected(&f,kind);
        setup(&f,kind);put32(f.list,0);put32(f.list+4,0);rejected(&f,kind);
        setup(&f,kind);put32(f.raw+4,0xFFFFFFFCu);rejected(&f,kind);
        setup(&f,kind);put32(f.list,Header+1);rejected(&f,kind);
        setup(&f,kind);put32(f.raw+4,Other);
        for(unsigned i=0;i<511;++i) {
            uint8_t* p=f.raw+0x8000+i*0x20;
            put32(p,777);put32(p+4,i==510?0:Other+(i+1)*0x20);
            put32(p+8,i?Other+(i-1)*0x20:Header);
        }
        put32(f.list+4,Other+510*0x20);happy(&f,kind);
        put32(f.raw+0x8000+510*0x20+4,Other+511*0x20);
        put32(f.raw+0x8000+511*0x20,777);
        put32(f.raw+0x8000+511*0x20+8,Other+510*0x20);
        put32(f.list+4,Other+511*0x20);rejected(&f,kind);
        // Exact registered header/table/section extents, not mere linked PCs.
        const unsigned header_offsets[]={0,0xC,0x10,0x1C};
        for(unsigned i=0;i<4;++i){setup(&f,kind);put32(f.raw+header_offsets[i],0);rejected(&f,kind);}
        const unsigned section_offsets[]={0x54,0x58,0x74,0x78};
        for(unsigned i=0;i<4;++i){setup(&f,kind);put32(f.raw+section_offsets[i],0);rejected(&f,kind);}
        setup(&f,kind);put32(f.raw+0x54,Text);rejected(&f,kind);
        setup(&f,kind);put32(f.raw+0x54,(Header+0x5FFCu)|1);rejected(&f,kind);
        setup(&f,kind);put32(f.raw+0x74,Header+0x5FFCu);rejected(&f,kind);
        setup(&f,kind);put32(f.raw+0x74,Text);rejected(&f,kind);
        setup(&f,kind);put32(f.raw+0x54,Header|1);rejected(&f,kind);
        setup(&f,kind);put32(f.raw+0x74,Header+0x20);rejected(&f,kind);
        // Materialization ownership is independent of list membership.
        setup(&f,kind);f.slots[0].materialization=0;rejected(&f,kind);
        setup(&f,kind);f.slots[0].owner=0;rejected(&f,kind);
        setup(&f,kind);f.slots[0].capacity=64;rejected(&f,kind);
        setup(&f,kind);f.slots[0].capacity=UINT32_MAX;rejected(&f,kind);
        setup(&f,kind);put32(f.loaders+0x10,Other);rejected(&f,kind);
        setup(&f,kind);f.slots[1]=f.slots[0];f.view.slot_count=2;rejected(&f,kind);
        setup(&f,kind);f.slots[1]=(BwRandomizerRelSlot){Loader+0x20,Header+0x100,0x300,9};
        put32(f.loaders+0x30,Header+0x100);f.view.slot_count=2;rejected(&f,kind);
        setup(&f,kind);f.slots[1]=(BwRandomizerRelSlot){Loader+0x20,Header+0x100,0x300,9};
        f.view.slot_count=2;happy(&f,kind); // Actually unloaded owner+10==0.
        // No competing raw or fixed executable aliases may shadow this one.
        setup(&f,kind);f.aliases[1]=f.aliases[0];f.view.alias_count=2;rejected(&f,kind);
        setup(&f,kind);f.aliases[0].raw_end--;rejected(&f,kind);
        setup(&f,kind);f.aliases[0].linked_start+=4;rejected(&f,kind);
        setup(&f,kind);f.aliases[0].text_size--;rejected(&f,kind);
        setup(&f,kind);f.aliases[1]=(BwRandomizerRelAlias){Text+4,Text+8,0xC0100000u,4};f.view.alias_count=2;rejected(&f,kind);
        setup(&f,kind);f.aliases[1]=(BwRandomizerRelAlias){Other,Other+4,f.text_start+4,4};f.view.alias_count=2;rejected(&f,kind);
        setup(&f,kind);f.aliases[1]=(BwRandomizerRelAlias){Other,Other+4,0xC0100000u,4};f.view.alias_count=2;happy(&f,kind);
        // Every descriptor and every audited method/profile field matters.
        for(uint32_t i=0;i<f.view.section_count;++i){setup(&f,kind);f.sections[i].size^=1;rejected(&f,kind);}
        setup(&f,kind);f.sections[21]=f.sections[0];f.view.section_count=22;rejected(&f,kind);
        setup(&f,kind);f.view.section_count--;rejected(&f,kind);
        const unsigned profile_offsets[]={0,4,6,8,0xC,0x10,0x14,0x18,0x1C,0x20,0x24,0x28,0x2C,0x2D};
        for(unsigned i=0;i<sizeof profile_offsets/sizeof *profile_offsets;++i){setup(&f,kind);f.fixed[f.profile-f.fixed_start+profile_offsets[i]]^=1;rejected(&f,kind);}
        for(unsigned i=0;i<5;++i){setup(&f,kind);f.fixed[f.methods-f.fixed_start+4*i]^=1;rejected(&f,kind);}
        const uint32_t deny[]={0x800030C8u,Header,Header+0x4C,Loader+0x10};
        for(unsigned i=0;i<4;++i){setup(&f,kind);f.denied_start=deny[i];f.denied_size=1;rejected(&f,kind);}
        setup(&f,kind);f.denied_start=f.profile+8;f.denied_size=1;rejected(&f,kind);
        setup(&f,kind);f.denied_start=f.methods+16;f.denied_size=1;rejected(&f,kind);
        setup(&f,kind);f.view.alias_count=513;rejected(&f,kind);check(f.reads==0);
        setup(&f,kind);f.view.slot_count=513;rejected(&f,kind);check(f.reads==0);
        setup(&f,kind);f.view.section_count=4097;rejected(&f,kind);check(f.reads==0);
        setup(&f,kind);rejected(&f,(BwRandomizerRelKind)257);check(f.reads==0);
        setup(&f,kind);f.view.resolve=NULL;rejected(&f,kind);
        setup(&f,kind);check(!bw_randomizer_rel_owner(&f.view,kind,NULL));check(f.reads==0);
        BwRandomizerRelOwner out;memset(&out,0xA5,sizeof out);check(!bw_randomizer_rel_owner(NULL,kind,&out));
        check(zero_output(&out));
    }
    printf("randomizer REL ownership: %u synthetic checks passed\n",checks);
    return 0;
}
