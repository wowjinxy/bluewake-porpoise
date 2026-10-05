#include "hud_customization.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { ++checks; if (!(expr)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define TAG(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))
static unsigned checks;
#define BASE UINT32_C(0x80000000)
#define ACTOR UINT32_C(0x80410000)
#define ROOT UINT32_C(0x80440000)
#define FIRST UINT32_C(0x80450000)
#define OBJECT UINT32_C(0x80490000)
#define STACK UINT32_C(0x817F0000)
#define CHILD_STACK UINT32_C(0x817EFF60)
enum { SIZE=0x1800000u };

typedef struct Write { uint8_t reg; uint32_t value; } Write;
typedef struct Fixture {
    uint8_t* ram;
    BwHudRuntime runtime;
    BwHudStream stream;
    Write writes[4096];
    unsigned written;
} Fixture;

static void put32(Fixture* f, uint32_t p, uint32_t v) {
    CHECK(p >= BASE && p <= BASE + SIZE - 4u);
    uint8_t* b = f->ram + p - BASE;
    b[0]=(uint8_t)(v>>24); b[1]=(uint8_t)(v>>16); b[2]=(uint8_t)(v>>8); b[3]=(uint8_t)v;
}
static void putfloat(Fixture* f, uint32_t p, float v) {
    uint32_t u; memcpy(&u, &v, 4); put32(f, p, u);
}
static void byte(Fixture* f, uint32_t p, uint8_t v) { f->ram[p-BASE] = v; }
static void emitter(uint8_t reg, uint32_t v, void* user) {
    Fixture* f=(Fixture*)user;
    CHECK(f->written < sizeof f->writes / sizeof f->writes[0]);
    f->writes[f->written++] = (Write){reg,v};
    CHECK(bw_hud_stream_bp(&f->stream, reg, v));
}
static uint32_t pane(unsigned i) { return FIRST + 0x180u*i; }
static void edge(Fixture* f, uint32_t address, uint32_t stack, uint32_t object, uint32_t lr) {
    const BwHudEdge e={0x1234u,(uintptr_t)f->ram,address,stack,object,lr,
        f->runtime.memory.epoch,f->runtime.memory.generation};
    bw_hud_dispatch(&f->runtime, &e);
}
static void capture_meter(Fixture* f) { edge(f,BW_HUD_METER_CAPTURE,STACK,ACTOR,0x80022974u); }
static void enter_meter(Fixture* f) { edge(f,BW_HUD_METER1_DRAW,STACK,OBJECT,BW_HUD_METER_RETURN); }
static void enter_leaf(Fixture* f, unsigned i, uint32_t target) {
    edge(f,target,CHILD_STACK,pane(i),BW_HUD_LEAF_RETURN);
}
static void leave_leaf(Fixture* f) { edge(f,BW_HUD_LEAF_RETURN,CHILD_STACK,0,0); }
static void leave_meter(Fixture* f) { edge(f,BW_HUD_METER_RETURN,STACK,0,0); }

/* Synthetic ABI fixture, not a claim of native game execution. It uses the
 * primary JSUTree multiple-inheritance layout (head points at LINK pane+BC)
 * and source heart wrapper/tag pairs to exercise ownership and cancellation. */
static void native_fixture(Fixture* f) {
    memset(f->ram,0,SIZE);
    put32(f,ACTOR+4,77); put32(f,ACTOR+8,0x01F10000u); put32(f,ACTOR+0xD8,0x8039350Cu);
    for (unsigned i=0;i<3;++i) {
        const uint32_t root=ROOT+0x1000u*i;
        put32(f,0x803F7078u+4u*i,root);
        put32(f,root,i==0?0x80391360u:0x8039D6B8u);
        put32(f,root+4,TAG('P','A','N','1')); put32(f,root+0xBC,root); byte(f,root+0xAA,1);
    }
    put32(f,ROOT+0xB0,pane(0)+0xBC); put32(f,ROOT+0xB4,pane(40)+0xBC); put32(f,ROOT+0xB8,41);
    for (unsigned i=0;i<41;++i) {
        const unsigned heart=i%20u;
        const unsigned wrapper = i<20u ? 0x640u+i*0x38u : i<40u ? 0xAA0u+(i-20u)*0x38u : 0xF00u;
        const uint32_t tag = i<40u ? TAG('h',i<20u?'t':'k','0'+heart/10u,'0'+heart%10u) : TAG('h','t','f','l');
        put32(f,ACTOR+wrapper,pane(i));
        putfloat(f,ACTOR+wrapper+0xC,20.f+20.f*heart); putfloat(f,ACTOR+wrapper+0x10,i<20u?30.f:32.f);
        putfloat(f,ACTOR+wrapper+0x2C,18.f); putfloat(f,ACTOR+wrapper+0x30,18.f);
        put32(f,pane(i),0x80372648u); put32(f,pane(i)+4,TAG('P','I','C','1')); put32(f,pane(i)+8,tag);
        put32(f,pane(i)+0xBC,pane(i)); put32(f,pane(i)+0xC0,ROOT+0xB0);
        put32(f,pane(i)+0xC4,i?pane(i-1)+0xBC:0); put32(f,pane(i)+0xC8,i<40u?pane(i+1)+0xBC:0);
        byte(f,pane(i)+0xAA,1); byte(f,pane(i)+0xAC,255); byte(f,pane(i)+0xAD,255);
    }
    put32(f,OBJECT,0x80393000u); put32(f,0x8039300Cu,BW_HUD_METER1_DRAW);
}

static void init(Fixture* f) {
    memset(f,0,sizeof *f); f->ram=(uint8_t*)calloc(1,SIZE); CHECK(f->ram);
    native_fixture(f); bw_hud_stream_init(&f->stream); bw_hud_init(&f->runtime,emitter,f);
    const BwHudMemory m={f->ram,SIZE,0x1234u,BW_HUD_ABI_GZLE01,1,1,NULL,NULL};
    CHECK(bw_hud_attach(&f->runtime,&m));
}
static void customize(Fixture* f) {
    BwHudConfig c; bw_hud_config_identity(&c);
    c.groups[BW_HUD_HEARTS].offset_x=10.f; c.groups[BW_HUD_HEARTS].offset_y=-5.f;
    c.groups[BW_HUD_HEARTS].scale=2.f;
    c.groups[BW_HUD_HEARTS].anchor_x=c.groups[BW_HUD_HEARTS].anchor_y=.5f;
    CHECK(bw_hud_set_config(&f->runtime,&c));
}

static void test_identity(Fixture* f) {
    uint8_t* before=(uint8_t*)malloc(SIZE); CHECK(before); memcpy(before,f->ram,SIZE);
    CHECK(bw_hud_config_is_identity(&f->runtime.config));
    for (unsigned i=0;i<1000;++i) {
        CHECK(!bw_hud_observes(&f->runtime,BW_HUD_MY_PICTURE_DRAW));
        capture_meter(f); enter_meter(f); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW); leave_leaf(f); leave_meter(f);
        bw_hud_frame(&f->runtime,i+1u);
    }
    CHECK(!f->written); CHECK(!f->runtime.stats.captures); CHECK(!memcmp(before,f->ram,SIZE)); free(before);
    BwHudConfig c=f->runtime.config; c.groups[0].scale=NAN; CHECK(!bw_hud_config_valid(&c));
    CHECK(!bw_hud_set_config(&f->runtime,&c)); CHECK(!f->written);
    c=f->runtime.config; c.groups[0].scale=4.0001f; CHECK(!bw_hud_set_config(&f->runtime,&c));
}

static void test_balanced(Fixture* f) {
    customize(f); capture_meter(f); CHECK(f->runtime.stats.pane_count==41);
    CHECK(!bw_hud_observes(&f->runtime,BW_HUD_MY_PICTURE_DRAW));
    enter_meter(f); CHECK(bw_hud_observes(&f->runtime,BW_HUD_MY_PICTURE_DRAW));
    uint8_t* before=(uint8_t*)malloc(SIZE); CHECK(before); memcpy(before,f->ram,SIZE);
    for (unsigned count=0;count<3;++count) {
        const unsigned start=f->written;
        enter_leaf(f,count,BW_HUD_MY_PICTURE_DRAW);
        CHECK(f->written==start+16u); CHECK(f->stream.active);
        BwHudDescriptor d; CHECK(bw_hud_stream_snapshot(&f->stream,&d));
        CHECK(d.pane==pane(count)); CHECK(d.group==BW_HUD_HEARTS); CHECK(d.config.scale==2.f);
        CHECK(d.pivot_x==219.f); CHECK(d.pivot_y==40.f);
        /* 0/1/100 native draw plans keep the same semantic scope. */
        for (unsigned draw=0;draw<(count==2?100u:count);++draw) {
            BwHudDescriptor copy; CHECK(bw_hud_stream_snapshot(&f->stream,&copy)); CHECK(!memcmp(&copy,&d,sizeof d));
        }
        leave_leaf(f); CHECK(f->written==start+18u); CHECK(!f->stream.active);
        CHECK(!bw_hud_observes(&f->runtime,BW_HUD_LEAF_RETURN));
    }
    leave_meter(f); CHECK(!bw_hud_observes(&f->runtime,BW_HUD_MY_PICTURE_DRAW));
    CHECK(f->stream.begins==3 && f->stream.ends==3 && !f->stream.malformed);
    CHECK(!memcmp(before,f->ram,SIZE)); free(before);

    /* Valid captured picture/text classes have their own actual virtual edges.
     * Re-capture after class replacement; existing capture must never survive it. */
    put32(f,pane(0),0x8039D730u); capture_meter(f); enter_meter(f);
    enter_leaf(f,0,BW_HUD_PICTURE_DRAW); CHECK(f->stream.active); leave_leaf(f); leave_meter(f);
    put32(f,pane(0),0x8039D770u); put32(f,pane(0)+4,TAG('T','B','X','1')); capture_meter(f); enter_meter(f);
    enter_leaf(f,0,BW_HUD_TEXT_DRAW); CHECK(f->stream.active); leave_leaf(f); leave_meter(f);
    native_fixture(f);
}

static void test_native_hidden(Fixture* f) {
    capture_meter(f); enter_meter(f); unsigned old=f->written;
    byte(f,pane(0)+0xAA,0); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW); CHECK(f->written==old); CHECK(!f->stream.active);
    byte(f,pane(0)+0xAA,1); byte(f,pane(0)+0xAD,0); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW); CHECK(f->written==old);
    byte(f,pane(0)+0xAD,255); byte(f,ROOT+0xAA,0); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW); CHECK(f->written==old);
    byte(f,ROOT+0xAA,1); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW); CHECK(f->stream.active); leave_leaf(f); leave_meter(f);
    CHECK(f->runtime.stats.hidden_leaves==3);

    BwHudConfig c=f->runtime.config; c.groups[0].visible=false;
    CHECK(bw_hud_set_config(&f->runtime,&c)); capture_meter(f); enter_meter(f); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW);
    BwHudDescriptor d; CHECK(bw_hud_stream_snapshot(&f->stream,&d)); CHECK(!bw_hud_descriptor_visible(&d));
    leave_leaf(f); leave_meter(f); customize(f);
}

static void test_ownership(Fixture* f) {
    /* Invalid/missing/recycled actor; invalid root and ABI, malformed sibling
     * chain, mismatched source tag, forged parent, and foreign CPU all revoke. */
    put32(f,ACTOR+8,0x01F20000); capture_meter(f); CHECK(!f->runtime.actor);
    native_fixture(f); put32(f,ROOT+0xB8,257); capture_meter(f); CHECK(!f->runtime.actor);
    native_fixture(f); put32(f,pane(0)+0xC8,pane(0)+0xBC); capture_meter(f); CHECK(!f->runtime.actor);
    native_fixture(f); put32(f,pane(1)+0xC4,0); capture_meter(f); CHECK(!f->runtime.actor);
    native_fixture(f); put32(f,pane(1)+0xBC,pane(2)); capture_meter(f); CHECK(!f->runtime.actor);
    native_fixture(f); put32(f,pane(0)+0xC0,ROOT+0x1000+0xB0); capture_meter(f); CHECK(!f->runtime.actor);
    native_fixture(f); put32(f,ACTOR+0x640,BASE+SIZE-4); capture_meter(f); CHECK(!f->runtime.actor);
    native_fixture(f); put32(f,pane(0)+8,TAG('m','e','n','u')); capture_meter(f); CHECK(!f->runtime.actor);
    native_fixture(f); capture_meter(f); enter_meter(f); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW); CHECK(f->stream.active);
    put32(f,ACTOR+4,78); leave_leaf(f); CHECK(!f->stream.active); CHECK(!f->runtime.actor);
    native_fixture(f); capture_meter(f); enter_meter(f);
    put32(f,pane(0),0x8039D730u); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW); CHECK(!f->stream.active); CHECK(!f->runtime.actor);
    native_fixture(f); capture_meter(f);
    const BwHudEdge foreign={0x4321u,(uintptr_t)f->ram,BW_HUD_METER1_DRAW,STACK,OBJECT,BW_HUD_METER_RETURN,
        f->runtime.memory.epoch,f->runtime.memory.generation};
    bw_hud_dispatch(&f->runtime,&foreign); CHECK(!f->runtime.actor);
    capture_meter(f); BwHudEdge stale_ram=foreign;stale_ram.cpu_identity=0x1234u;
    stale_ram.memory_identity+=4u;bw_hud_dispatch(&f->runtime,&stale_ram);CHECK(!f->runtime.actor);
    capture_meter(f);BwHudEdge stale_epoch=foreign;stale_epoch.cpu_identity=0x1234u;++stale_epoch.epoch;
    bw_hud_dispatch(&f->runtime,&stale_epoch);CHECK(!f->runtime.actor);
    capture_meter(f);BwHudEdge stale_scene=foreign;stale_scene.cpu_identity=0x1234u;++stale_scene.generation;
    bw_hud_dispatch(&f->runtime,&stale_scene);CHECK(!f->runtime.actor);
    native_fixture(f); capture_meter(f);
    edge(f,BW_HUD_METER1_DRAW,STACK,OBJECT,0x800865A8u); CHECK(!f->runtime.actor);
    capture_meter(f); enter_meter(f); edge(f,BW_HUD_MY_PICTURE_DRAW,CHILD_STACK,pane(0),0x802D04D0u);
    CHECK(!f->runtime.actor); CHECK(!f->stream.active);
    capture_meter(f); enter_meter(f);
    /* Unrelated valid-looking picture never appears in captured wrappers. */
    edge(f,BW_HUD_MY_PICTURE_DRAW,CHILD_STACK,pane(41),BW_HUD_LEAF_RETURN); CHECK(!f->stream.active);
    enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW); CHECK(f->stream.active);
    enter_leaf(f,1,BW_HUD_MY_PICTURE_DRAW); CHECK(!f->stream.active); CHECK(!f->runtime.actor);
}

static void test_lifecycle(Fixture* f) {
    native_fixture(f); capture_meter(f); enter_meter(f); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW);
    edge(f,BW_HUD_LEAF_RETURN,CHILD_STACK-32u,0,0); CHECK(!f->stream.active); CHECK(!f->runtime.stats.meter_active);
    capture_meter(f); enter_meter(f); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW);
    bw_hud_frame(&f->runtime,f->runtime.frame+1u); CHECK(!f->stream.active); CHECK(!f->runtime.actor);
    capture_meter(f); enter_meter(f); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW);
    BwHudMemory m=f->runtime.memory; ++m.generation; CHECK(bw_hud_attach(&f->runtime,&m));
    CHECK(!f->stream.active); CHECK(!f->runtime.actor);
    capture_meter(f); enter_meter(f); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW);
    edge(f,BW_HUD_METER_DELETE,STACK,ACTOR,0); CHECK(!f->stream.active); CHECK(!f->runtime.actor);
    capture_meter(f); enter_meter(f); enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW);
    BwHudConfig identity; bw_hud_config_identity(&identity); CHECK(bw_hud_set_config(&f->runtime,&identity));
    CHECK(!f->stream.active); CHECK(!bw_hud_observes(&f->runtime,BW_HUD_METER_CAPTURE));
    unsigned before=f->written; bw_hud_detach(&f->runtime); bw_hud_frame(&f->runtime,777777u); CHECK(f->written==before);
    m.size=SIZE-4u; CHECK(!bw_hud_attach(&f->runtime,&m));
    m.size=SIZE; m.abi=99; CHECK(!bw_hud_attach(&f->runtime,&m));
    m.abi=BW_HUD_ABI_GZLE01; CHECK(bw_hud_attach(&f->runtime,&m)); customize(f);
}

static void nested_fixture(Fixture* f, unsigned depth) {
    native_fixture(f);
    CHECK(depth>0u && depth<=31u);
    const uint32_t first=0x804A0000u, deepest=first+(depth-1u)*0x100u;
    put32(f,ROOT+0xB0,first+0xBC);put32(f,ROOT+0xB4,first+0xBC);put32(f,ROOT+0xB8,1);
    for(unsigned i=0;i<depth;++i) {
        const uint32_t p=first+i*0x100u, parent=i? p-0x100u:ROOT;
        put32(f,p,0x8039D680u);put32(f,p+4,TAG('P','A','N','1'));
        put32(f,p+0xBC,p);put32(f,p+0xC0,parent+0xB0);byte(f,p+0xAA,1);
        put32(f,p+0xB0,i+1u<depth?p+0x100u+0xBC:pane(0)+0xBC);
        put32(f,p+0xB4,i+1u<depth?p+0x100u+0xBC:pane(40)+0xBC);
        put32(f,p+0xB8,i+1u<depth?1:41);
    }
    for(unsigned i=0;i<41;++i)put32(f,pane(i)+0xC0,deepest+0xB0);
}
static void test_bounded_ancestry(Fixture* f) {
    nested_fixture(f,2);capture_meter(f);CHECK(f->runtime.stats.pane_count==41);
    enter_meter(f);enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW);CHECK(f->stream.active);leave_leaf(f);leave_meter(f);
    nested_fixture(f,30);capture_meter(f);CHECK(f->runtime.stats.pane_count==41);
    enter_meter(f);enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW);CHECK(f->stream.active);leave_leaf(f);leave_meter(f);
    nested_fixture(f,31);capture_meter(f);CHECK(!f->runtime.actor);CHECK(!f->stream.active);
    nested_fixture(f,2);put32(f,0x804A0100u+0xC0,0x804A0100u+0xB0);
    capture_meter(f);CHECK(!f->runtime.actor);
    native_fixture(f);
}

static void replay(BwHudStream* s, const Write packet[16]) {
    for (unsigned i=0;i<16;++i) CHECK(bw_hud_stream_bp(s,packet[i].reg,packet[i].value));
}
static void test_packets(Fixture* f) {
    capture_meter(f); enter_meter(f); const unsigned start=f->written;
    enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW); Write packet[16]; memcpy(packet,f->writes+start,sizeof packet);
    leave_leaf(f); leave_meter(f);
    BwHudStream s; bw_hud_stream_init(&s); replay(&s,packet); CHECK(s.active);
    CHECK(!bw_hud_stream_bp(&s,0x7B,1)); CHECK(s.active); /* cloth register unrelated */
    CHECK(bw_hud_stream_bp(&s,0x6B,s.descriptor.sequence)); CHECK(bw_hud_stream_bp(&s,0x6A,0x10002)); CHECK(!s.active);
    replay(&s,packet); CHECK(!s.active); CHECK(s.malformed==1); /* repeated invocation */
    CHECK(bw_hud_stream_bp(&s,0x6A,0x10003)); replay(&s,packet); CHECK(s.active);
    replay(&s,packet); CHECK(!s.active); /* nested semantic begin */
    bw_hud_stream_init(&s); replay(&s,packet); bw_hud_stream_frame(&s); CHECK(!s.active);
    replay(&s,packet); CHECK(!s.active); /* previous-frame replay */
    bw_hud_stream_init(&s);
    for (unsigned i=0;i<15;++i) if(i!=3) CHECK(bw_hud_stream_bp(&s,packet[i].reg,packet[i].value));
    CHECK(bw_hud_stream_bp(&s,packet[15].reg,packet[15].value)); CHECK(!s.active); CHECK(s.malformed==1);
    bw_hud_stream_init(&s); packet[7].value^=1u; replay(&s,packet); CHECK(!s.active); packet[7].value^=1u;
    bw_hud_stream_init(&s); replay(&s,packet);
    CHECK(bw_hud_stream_bp(&s,0x6B,s.descriptor.sequence+1u)); CHECK(bw_hud_stream_bp(&s,0x6A,0x10002)); CHECK(!s.active);
    bw_hud_stream_init(&s); replay(&s,packet); CHECK(bw_hud_stream_bp(&s,0x6A,0x20001)); CHECK(!s.active);
    CHECK(bw_hud_stream_bp(&s,0x6B,0x1000000)); CHECK(!s.active);
    /* Deterministic malformed FIFO fuzz: bounded storage, no host/guest pointer
     * dereferences, and every successful descriptor remains finite/valid. */
    uint32_t random=0x6A04CCu;
    for (unsigned i=0;i<200000u;++i) {
        random=random*1664525u+1013904223u;
        const uint8_t reg=(uint8_t)(0x69u+((random>>24)%19u));
        (void)bw_hud_stream_bp(&s,reg,random & 0x1FFFFFFu);
        BwHudDescriptor d;
        if (bw_hud_stream_snapshot(&s,&d)) {
            CHECK(d.group<BW_HUD_GROUP_COUNT); CHECK(isfinite(d.config.scale)); CHECK(d.config.scale>=.25f && d.config.scale<=4.f);
        }
        if (!(i%73u)) bw_hud_stream_frame(&s);
    }
}

static void test_transforms(void) {
    BwHudConfig c; bw_hud_config_identity(&c); BwHudDescriptor d;
    memset(&d,0,sizeof d); d.config=c.groups[0]; d.pivot_x=10; d.pivot_y=20;
    float point[2]={-0.f,NAN}, before[2]; memcpy(before,point,sizeof point);
    bw_hud_transform_point(&d,point); CHECK(!memcmp(point,before,sizeof point));
    float color[4]={.75f,.5f,.25f,.5f}, cb[4]; memcpy(cb,color,sizeof color);
    bw_hud_final_color(&d,color); CHECK(!memcmp(color,cb,sizeof color));
    d.config.scale=2; d.config.offset_x=5; d.config.offset_y=-3;
    point[0]=12; point[1]=24; bw_hud_transform_point(&d,point); CHECK(point[0]==19 && point[1]==25);
    float rect[4]={10,20,20,30}; CHECK(bw_hud_transform_rect(&d,rect));
    CHECK(rect[0]==15 && rect[1]==17 && rect[2]==35 && rect[3]==37);
    d.config.opacity=.5f; d.config.tint[0]=128; d.config.tint[3]=128;
    bw_hud_final_color(&d,color); CHECK(fabsf(color[0]-.75f*128.f/255.f)<1e-6f);
    CHECK(fabsf(color[3]-.5f*.5f*128.f/255.f)<1e-6f);
    d.config.visible=false; CHECK(!bw_hud_descriptor_visible(&d));
    d.config.visible=true; d.config.opacity=0; CHECK(!bw_hud_descriptor_visible(&d));
    d.config.opacity=1; d.config.scale=INFINITY; CHECK(!bw_hud_transform_rect(&d,rect));
}


/* A resolver must admit every byte of a sibling record. Never reuse its
 * borrowed pointer after a draw edge or carry stale ownership after failure. */
typedef struct LinkAliasProbe { Fixture* fixture; uint32_t denied; } LinkAliasProbe;
static const uint8_t* link_alias_resolve(void* user,uint32_t address,uint32_t size) {
    LinkAliasProbe* p=(LinkAliasProbe*)user;
    if(address<BASE||size>SIZE||address-BASE>SIZE-size)return NULL;
    if(p->denied && address<=p->denied && p->denied-address<size)return NULL;
    return p->fixture->ram+address-BASE;
}
static void test_link_record_alias(void) {
    Fixture* f=(Fixture*)calloc(1,sizeof *f);CHECK(f);init(f);customize(f);
    uint8_t* original=(uint8_t*)malloc(SIZE);CHECK(original);memcpy(original,f->ram,SIZE);
    LinkAliasProbe probe={f,0};f->runtime.memory.resolve=link_alias_resolve;
    f->runtime.memory.resolve_user=&probe;
    bw_hud_frame(&f->runtime,1);capture_meter(f);enter_meter(f);
    enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW);CHECK(f->stream.active);leave_leaf(f);leave_meter(f);
    CHECK(!memcmp(original,f->ram,SIZE));
    /* Parent, prev, owner and next each independently revoke ownership when
     * their bytes become unavailable; the unrelated native pane stays intact. */
    const unsigned fields[]={0,4,8,12};
    for(unsigned i=0;i<4;++i) {
        probe.denied=pane(20)+0xBCu+fields[i]+3u;
        bw_hud_frame(&f->runtime,2+i);capture_meter(f);
        CHECK(f->runtime.stats.pane_count==0 && !f->stream.active);
        CHECK(!memcmp(original,f->ram,SIZE));
        probe.denied=0;
        bw_hud_frame(&f->runtime,10+i);capture_meter(f);enter_meter(f);
        enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW);CHECK(f->stream.active);leave_leaf(f);leave_meter(f);
    }
    /* A failure after capture is detected again by the next native draw. */
    bw_hud_frame(&f->runtime,20);capture_meter(f);enter_meter(f);
    probe.denied=pane(20)+0xC4u;
    enter_leaf(f,0,BW_HUD_MY_PICTURE_DRAW);CHECK(!f->stream.active);
    CHECK(!memcmp(original,f->ram,SIZE));
    bw_hud_detach(&f->runtime);free(original);free(f->ram);free(f);
}

int main(void) {
    Fixture* f=(Fixture*)calloc(1,sizeof *f); CHECK(f); init(f);
    test_identity(f); test_balanced(f); test_native_hidden(f); test_ownership(f);
    test_lifecycle(f); test_bounded_ancestry(f); test_packets(f); test_transforms();
    test_link_record_alias();
    bw_hud_detach(&f->runtime); free(f->ram); free(f);
    printf("HUD ownership, exact-edge, semantic FIFO and affine fixtures: %u checks PASS\n",checks);
    return 0;
}
