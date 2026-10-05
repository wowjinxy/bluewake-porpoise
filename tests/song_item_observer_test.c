#include "game_events.h"
#include "song_rel_owner.h"
#include "dispatch_loop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"song_item:%d: %s\n",__LINE__,#x);abort();}}while(0)
static CPUState cpu;
static uint8_t* before_ram;
static uint8_t linked_data[0x390];
static const uint32_t HR=0x80AAA3D0u, PLAYER=0x80ABF000u, STACK=0x81700000u;
static const uint32_t MODULE=0x818A59A0u, TABLE=0x818A59ECu, OWNER=0x80AE272Cu;
static const uint32_t RETURN=0xC0E9116Cu, GET=0x800C2DFCu, UNCACHED=0xC00C2DFCu;
static const uint32_t SONGS=0x803C4CC5u;
static BwSongRelAlias aliases[2];
static BwSongRelSlot slots[2];
static BwSongRelSection sections[3];
static BwSongRelView view;
static BwGameEvent events[64];static unsigned event_count,query_count,native_calls;
static bool owner_resolver_enabled=true;

static void w8(uint32_t a,uint8_t v){cpu.ram[a-0x80000000u]=v;}
static void w16(uint32_t a,uint16_t v){w8(a,(uint8_t)(v>>8));w8(a+1,(uint8_t)v);}
static void w32(uint32_t a,uint32_t v){w16(a,(uint16_t)(v>>16));w16(a+2,(uint16_t)v);}
static void data32(uint32_t a,uint32_t v){unsigned i=a-0xC0E96420u;for(unsigned n=0;n<4;++n)linked_data[i+n]=(uint8_t)(v>>((3-n)*8));}
static const uint8_t* resolve(void* user,uint32_t a,uint32_t n) {
    (void)user;if(!owner_resolver_enabled)return NULL;
    if(a>=0xC0E96420u && n<=sizeof linked_data && a-0xC0E96420u<=sizeof linked_data-n)
        return linked_data+(a-0xC0E96420u);
    if(a<0x80000000u||n>cpu.ram_size||a-0x80000000u>cpu.ram_size-n)return NULL;
    return cpu.ram+(a-0x80000000u);
}
static bool query(void* user,const CPUState* context,BwSongOwner* out) {
    (void)user;++query_count;CHECK(context==&cpu);return bw_song_rel_owner(&view,out);
}
static void record(const BwGameEvent* e,void* user){(void)user;if(e->kind!=BW_GAME_EVENT_ITEM_AWARDED)return;CHECK(event_count<64);events[event_count++]=*e;}
static void observed(uint32_t a) {
    cpu.pc=a;CPUState before=cpu;memcpy(before_ram,cpu.ram,cpu.ram_size);
    bluewake_game_events_dispatch(&cpu,a);
    CHECK(memcmp(&before,&cpu,sizeof cpu)==0);CHECK(memcmp(before_ram,cpu.ram,cpu.ram_size)==0);
}
static void setup(void) {
    uint8_t* ram=cpu.ram;memset(&cpu,0,sizeof cpu);cpu.ram=ram;cpu.ram_size=0x02000000u;
    memset(ram,0,cpu.ram_size);memset(linked_data,0,sizeof linked_data);
    owner_resolver_enabled=true;query_count=event_count=native_calls=0;
    aliases[0]=(BwSongRelAlias){0x818A5A84u,0x818ABA40u,0xC0E900E4u,0x5FBC};
    slots[0]=(BwSongRelSlot){OWNER,MODULE,45248,1};
    sections[0]=(BwSongRelSection){257,1,0xC0E900E4u,0x5FBC};
    sections[1]=(BwSongRelSection){257,5,0xC0E96420u,0x390};
    view=(BwSongRelView){resolve,NULL,aliases,1,slots,1,sections,2};
    w32(0x800030C8u,MODULE);w32(0x800030CCu,MODULE);w32(MODULE,257);
    w32(MODULE+0xC,19);w32(MODULE+0x10,TABLE);w32(MODULE+0x1C,3);
    w32(TABLE+8,0x818A5A85u);w32(TABLE+12,0x5FBC);w32(TABLE+40,0x818ABDC0u);w32(TABLE+44,0x390);
    w32(OWNER+0x10,MODULE);
    data32(0xC0E96628u,0x016F0000u);data32(0xC0E96630u,0x7C8);data32(0xC0E96644u,0xC0E96600u);
    data32(0xC0E96600u,0xC0E95DCCu);data32(0xC0E96604u,0xC0E95DECu);data32(0xC0E96608u,0xC0E95E0Cu);
    data32(0xC0E9660Cu,0xC0E95E4Cu);data32(0xC0E96610u,0xC0E95E2Cu);
    w32(0x803F6A18u,0x09130001u);w32(0x803F69D0u,0x09130005u);
    w32(HR,0x09130001u);w32(HR+4,641);w16(HR+8,0x16F);w16(HR+0xE,0x16F);
    w32(HR+0x10,0xC0E96620u);w32(HR+0xC0,0x09130005u);w32(HR+0xEC,0xC0E96600u);
    w32(HR+0x1C8,8);w8(HR+0x20A,13);w16(HR+0x608,0x1A);w8(HR+0x638,3);w32(HR+0x640,855);
    memcpy(ram+0x3C9D3C,"sea",4);w8(0x803C9D46u,13);w8(0x803F6A78u,13);w8(0x803C9EA2u,1);
    w32(0x803CA74Cu,PLAYER);w32(PLAYER+0x498,PLAYER+0x1F8);w32(PLAYER+0x1FC,0x43480000u);
    w32(0x803888C8u+0x6D*4,0x800C446Cu);
    cpu.gpr[1]=STACK;cpu.gpr[3]=0x6D;cpu.gpr[31]=HR;cpu.lr=RETURN;cpu.pc=UNCACHED;
    bluewake_game_events_attach(&cpu);bluewake_game_events_set_song_owner_query(query,NULL);
    bluewake_game_events_retrace(&cpu);event_count=query_count=0;
    BwGameScene s;CHECK(bluewake_game_events_scene(&s,NULL,NULL));CHECK(s.active&&!s.controls_ready&&s.event_running);
}
static void finish(uint32_t raw_r3){w8(SONGS,1);cpu.gpr[3]=raw_r3;observed(RETURN);}
static void positive_and_native_semantics(void) {
    static const uint32_t values[]={0,1,109,0xFFFFFFFFu,0x803C4CB4u};
    for(unsigned i=0;i<sizeof values/sizeof values[0];++i) {
        setup();CHECK(bluewake_game_events_observes(GET));CHECK(bluewake_game_events_observes(UNCACHED));
        observed(i&1?GET:UNCACHED);observed(i&1?GET:UNCACHED);CHECK(bluewake_game_events_observes(RETURN));
        finish(values[i]);CHECK(event_count==1);CHECK(events[0].item_id==0x6D&&events[0].source_address==RETURN);
        CHECK(events[0].native_call!=0&&(uint32_t)events[0].result==values[i]);CHECK(!events[0].scene.controls_ready);
        observed(RETURN);CHECK(event_count==1);CHECK(!bluewake_game_events_observes(RETURN));
    }
    setup();w8(SONGS,1);observed(GET);finish(0);CHECK(event_count==1); /* Invocation, not fact delta. */
    uint64_t token=events[0].native_call;cpu.gpr[3]=0x6D;cpu.lr=RETURN;observed(GET);finish(1);
    CHECK(event_count==2&&events[1].native_call>token); /* A distinct native invocation. */
    setup();w8(0x803C9EA2u,2);observed(GET);finish(0);CHECK(event_count==1);
}
static void owner_negative_cases(void) {
    for(unsigned bad=0;bad<21;++bad) {
        setup();BwSongOwner proof;CHECK(bw_song_rel_owner(&view,&proof));
        switch(bad) {
        case 0:w32(0x800030C8u,0);break;case 1:w32(MODULE+4,MODULE);break;
        case 2:w32(MODULE+8,OWNER);break;case 3:w32(0x800030CCu,OWNER);break;
        case 4:w32(MODULE,258);break;case 5:w32(MODULE+0xC,18);break;
        case 6:w32(MODULE+0x10,0x81FFFFFCu);break;case 7:w32(MODULE+0x1C,2);break;
        case 8:w32(TABLE+8,0x818A5A84u);break;case 9:w32(TABLE+12,0x5FB8);break;
        case 10:w32(TABLE+44,0x38C);break;case 11:w32(OWNER+0x10,0);break;
        case 12:slots[0].load_token=0;break;case 13:slots[0].capacity=100;break;
        case 14:aliases[0].raw_start+=4;break;case 15:aliases[1]=aliases[0];view.alias_count=2;break;
        case 16:sections[0].linked_start+=4;break;case 17:sections[2]=sections[0];view.section_count=3;break;
        case 18:data32(0xC0E96630u,0x7C4);break;case 19:data32(0xC0E96608u,0xC0E95E10u);break;
        case 20:owner_resolver_enabled=false;break;
        }
        memset(&proof,0xCC,sizeof proof);CHECK(!bw_song_rel_owner(&view,&proof));CHECK(proof.load_token==0);
        observed(GET);finish(0);CHECK(event_count==0);CHECK(!bluewake_game_events_observes(RETURN));
    }
}
static void actor_and_lesson_negative_cases(void) {
    for(unsigned bad=0;bad<28;++bad) {
        setup();switch(bad) {
        case 0:cpu.gpr[3]=0x6C;break;case 1:cpu.lr=RETURN+4;break;
        case 2:cpu.lr=0x818A6B0Cu;break;case 3:cpu.lr=0x80E9116Cu;break;
        case 4:cpu.gpr[31]+=1;break;case 5:w32(HR,0);break;case 6:w16(HR+8,0x170);break;
        case 7:w16(HR+0xE,0x170);break;case 8:w32(HR+0x10,0x80E96620u);break;
        case 9:w32(HR+0xEC,0xC0E96604u);break;case 10:w32(HR+0xC0,0);break;
        case 11:w32(HR+0x14,OWNER);break;case 12:w8(HR+0x5C,1);break;
        case 13:w8(HR+0xB,1);break;case 14:w8(HR+0x1BE,1);break;
        case 15:w32(HR+0x1C8,10);break;case 16:w8(HR+0x20A,14);break;
        case 17:w8(HR+0x638,11);break;case 18:w8(HR+0x63A,1);break;
        case 19:w16(HR+0x608,0x21A);break;case 20:w32(HR+0x640,0xFFFFFFFFu);break;
        case 21:w8(0x803C9EA2u,0);break;case 22:w8(0x803CA8C8u,1);break;
        case 23:w32(0x803888C8u+0x6D*4,0x800C6374u);break;
        case 24:bluewake_game_events_set_song_owner_query(NULL,NULL);break;
        case 25:memcpy(cpu.ram+0x3C9D3C,"LinkRM",7);break;
        case 26:w8(0x803F6A78u,14);break;case 27:w8(0x803F7097u,1);break;
        }
        observed(GET);finish(0);CHECK(event_count==0);CHECK(!bluewake_game_events_observes(RETURN));
    }
    setup();observed(0xC0E910ECu);w8(SONGS,1);bluewake_game_events_retrace(&cpu);observed(RETURN);
    CHECK(event_count==0); /* A sampled learned flag/outer entry does not fabricate the event. */
    setup();cpu.gpr[1]|=1;observed(GET);finish(0);CHECK(event_count==0);
}
static void lifecycle_and_completion(void) {
    for(unsigned bad=0;bad<13;++bad) {
        setup();observed(GET);CHECK(bluewake_game_events_observes(RETURN));
        switch(bad) {
        case 0:++slots[0].load_token;break;case 1:w32(OWNER+0x10,0);break;
        case 2:w32(HR+4,642);break;case 3:cpu.gpr[31]=PLAYER;break;
        case 4:w32(HR+0x640,856);break;case 5:w16(HR+0x608,0x21A);break;
        case 6:w8(HR+0x5C,1);break;case 7:w8(0x803C9D54u,1);break;
        case 8:w32(0x803CA74Cu,0);break;
        case 9:bluewake_game_events_reset(&cpu,BW_GAME_RESET_STATE_LOAD);break;
        case 10:bluewake_game_events_reset(&cpu,BW_GAME_RESET_MODULE_RELOAD);break;
        case 11:bluewake_game_events_set_song_owner_query(query,NULL);break;
        case 12:for(unsigned n=0;n<601;++n)bluewake_game_events_retrace(&cpu);break;
        }
        finish(0);CHECK(event_count==0);observed(RETURN);CHECK(event_count==0);
    }
    setup();observed(GET);cpu.gpr[3]=0;observed(RETURN);CHECK(event_count==0); /* Handler not completed. */
    setup();observed(GET);CPUState foreign=cpu;foreign.pc=RETURN;foreign.gpr[3]=0;
    CPUState before=foreign;bluewake_game_events_dispatch(&foreign,RETURN);
    CHECK(memcmp(&foreign,&before,sizeof foreign)==0);CHECK(event_count==0);finish(0);CHECK(event_count==1);
    setup();observed(GET);uint8_t* old_ram=cpu.ram;cpu.ram=malloc(cpu.ram_size);CHECK(cpu.ram);
    memcpy(cpu.ram,old_ram,cpu.ram_size);finish(0);CHECK(event_count==0);free(cpu.ram);cpu.ram=old_ram;
    setup();observed(GET);cpu.ram_size-=4;finish(0);CHECK(event_count==0);cpu.ram_size+=4;
    setup();observed(GET);++slots[0].load_token;observed(GET);finish(0);CHECK(event_count==0);
    setup();observed(GET);w32(HR+4,642);observed(GET);finish(0);CHECK(event_count==0);
    setup();observed(GET);cpu.gpr[1]=STACK-16;finish(0);CHECK(event_count==0);
    cpu.gpr[1]=STACK;finish(0);CHECK(event_count==1);
    setup();for(unsigned i=0;i<17;++i){cpu.gpr[1]=STACK-i*16;cpu.gpr[3]=0x6D;observed(GET);}
    BwGameEventStats s;bluewake_game_events_stats(&s);CHECK(s.pending_overflow==1);
    for(unsigned i=0;i<17;++i){cpu.gpr[1]=STACK-i*16;finish(0);}CHECK(event_count==16);
    for(unsigned i=0;i<16;++i)for(unsigned j=i+1;j<16;++j)CHECK(events[i].native_call!=events[j].native_call);
}

#if defined(BLUEWAKE_PRIVATE_HR_CALL_INCLUDE)
static const uint32_t CALL=0xC0E91168u;
unsigned bw_direct_depth;bool bw_direct_enabled,bw_edge_watch_ready,bw_edge_filter_enabled;
static const bool clean=false;static const u32 zero=0;
const bool* bw_host_sources_dirty=&clean;const bool* bw_host_decrementer_pending=&clean;
const u32* bw_host_pi_cause=&zero;const u32* bw_host_pi_mask=&zero;
BwHostCanSkipFn bw_host_can_skip;void* bw_host_can_skip_user;
u32 bw_edge_watch_table[BW_EDGE_WATCH_SLOTS];
static BwChunkFn chunk_fns[49];BwChunkFn* const bw_chunk_fns=chunk_fns;
static bool can_skip(void* user,const CPUState* context,u32 a){(void)user;CHECK(context==&cpu);return !bluewake_game_events_observes(a);}
static bool edge_service(void* user,CPUState* context,u32 a){(void)user;CHECK(context==&cpu);observed(a);return false;}
static void native_handler_source_fixture(CPUState* context){++native_calls;w8(SONGS,cpu.ram[SONGS-0x80000000u]|1);context->gpr[3]=0;--context->downcount;context->pc=context->lr;}
static void actual_hr_caller(CPUState* ctx){--ctx->downcount;
#include BLUEWAKE_PRIVATE_HR_CALL_INCLUDE
label_C0E9116C:ctx->pc=RETURN+4;
}
static int translated(CPUState* c,u32 a){
    if(a==CALL){actual_hr_caller(c);return 1;}
    if(a==GET||a==UNCACHED){native_handler_source_fixture(c);return 1;}
    if(a==RETURN){--c->downcount;c->pc=RETURN+4;return 1;}
    return 0;
}
static void prepared_caller_paths(void) {
    setup();cpu.pc=CALL;cpu.downcount=0;cpu.cycle_budget=100;
    bw_direct_enabled=bw_edge_watch_ready=bw_edge_filter_enabled=true;bw_direct_depth=0;
    bw_host_can_skip=can_skip;chunk_fns[48]=native_handler_source_fixture;
    memset(bw_edge_watch_table,0,sizeof bw_edge_watch_table);
    CHECK(bluewake_chassis_dispatch_loop(&cpu,CALL,translated,edge_service,NULL));
    CHECK(native_calls==1&&event_count==1&&events[0].source_address==RETURN);CHECK(bw_direct_depth==0);
    setup();cpu.pc=CALL;cpu.downcount=0;cpu.cycle_budget=1;
    CHECK(bluewake_chassis_dispatch_loop(&cpu,CALL,translated,edge_service,NULL));CHECK(native_calls==0);
    observed(cpu.pc);observed(cpu.pc);cpu.cycle_budget=100;cpu.downcount=0;
    CHECK(bluewake_chassis_dispatch_loop(&cpu,cpu.pc,translated,edge_service,NULL));
    CHECK(native_calls==1&&event_count==1);observed(RETURN);CHECK(event_count==1);
    puts("Actual identical slow/prepaid Hr caller + production direct-call/edge headers PASS; native handler is a primary-source fixture, not game proof.");
}
#else
static void prepared_caller_paths(void) { puts("Public synthetic fixtures: private optimized Hr caller omitted."); }
#endif
int main(void) {
    cpu.ram=calloc(1,0x02000000u);before_ram=malloc(0x02000000u);CHECK(cpu.ram&&before_ram);
    BwGameEventSubscription subscription=bluewake_game_events_subscribe(BW_GAME_EVENT_MASK(BW_GAME_EVENT_ITEM_AWARDED),record,NULL);CHECK(subscription);
    positive_and_native_semantics();owner_negative_cases();actor_and_lesson_negative_cases();lifecycle_and_completion();prepared_caller_paths();
    CHECK(bluewake_game_events_unsubscribe(subscription));setup();observed(GET);CHECK(query_count==0);CHECK(!bluewake_game_events_observes(GET));
    bluewake_game_events_set_song_owner_query(NULL,NULL);bluewake_game_events_reset(NULL,BW_GAME_RESET_MODULE_RELOAD);
    free(before_ram);free(cpu.ram);printf("Hr song observer: %u checks PASS\n",checks);return 0;
}
