/* Schema/accessor fixtures only: no real game, personal cards or disc assets. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "progression_sync.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const uint32_t INFO=0x803C4C08u,PLAYER=0x80010000u,STAG=0x80018000u;
static unsigned writes;
static void journal(uint32_t offset,uint32_t size,void* user){(void)user;assert(offset>=INFO-0x80000000u&&offset+size<=INFO-0x80000000u+0x79C);++writes;}
static void w32(CPUState* c,uint32_t at,uint32_t v){uint8_t* p=c->ram+at-0x80000000u;p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
int main(void){
    CPUState cpu={0};cpu.ram_size=24*1024*1024;cpu.ram=calloc(1,cpu.ram_size);assert(cpu.ram);
    memset(cpu.ram+INFO-0x80000000u+0x3C,0xFF,21);
    w32(&cpu,0x803CA74Cu,PLAYER);w32(&cpu,PLAYER+0x498,PLAYER+0x1F8);w32(&cpu,0x803C9DA0u,STAG);
    cpu.ram[STAG-0x80000000u+9]=22;memcpy(cpu.ram+0x3C9D3C,"LinkUG",7);
    BwGameScene scene={0};strcpy(scene.stage,"LinkUG");scene.player=PLAYER;scene.active=scene.player_valid=scene.controls_ready=true;
    uint8_t* before=malloc(cpu.ram_size);assert(before);memcpy(before,cpu.ram,cpu.ram_size);
    g_mem_write_journal=journal;g_mem_write_journal_user=NULL;
    assert(!bw_progression_valid((BwProgressionDelta){14,0x50})); /* bottle contents */
    assert(!bw_progression_valid((BwProgressionDelta){21,100})); /* arbitrary resource */
    assert(!bw_progression_valid((BwProgressionDelta){244,1})); /* unrelated event bit */
    assert(!bw_progression_valid((BwProgressionDelta){64,1})); /* sword removed by native fortress story */
    assert(!bw_progression_valid((BwProgressionDelta){64,2})&&!bw_progression_valid((BwProgressionDelta){65,1})&&!bw_progression_valid((BwProgressionDelta){66,1}));
    for(uint16_t key=128;key<192;++key)assert(!bw_progression_valid((BwProgressionDelta){key,1}));
    for(uint16_t key=192;key<208;++key)assert(!bw_progression_valid((BwProgressionDelta){key,0x10}));
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){2,0x22})==BW_PROGRESS_APPLIED);
    assert(cpu.ram[INFO-0x80000000u+0x3E]==0x22&&cpu.ram[INFO-0x80000000u+0x53]==1);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){2,0x22})==BW_PROGRESS_UNCHANGED);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){12,0x36})==BW_PROGRESS_APPLIED);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){12,0x27})==BW_PROGRESS_UNCHANGED);
    assert(cpu.ram[INFO-0x80000000u+0x48]==0x36&&cpu.ram[INFO-0x80000000u+0x5D]==7);
    assert(cpu.ram[INFO-0x80000000u+0x6F]==30&&cpu.ram[INFO-0x80000000u+0x67]==0);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){203,4})==BW_PROGRESS_APPLIED);
    assert(cpu.ram[INFO-0x80000000u+0x380+11*0x24+0x21]==4&&cpu.ram[INFO-0x80000000u+0x799]==4);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){193,7})==BW_PROGRESS_APPLIED);
    assert(cpu.ram[INFO-0x80000000u+0x799]==4);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){73,0x3F})==BW_PROGRESS_APPLIED);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){244,0xD0})==BW_PROGRESS_APPLIED);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){255,0x20})==BW_PROGRESS_APPLIED);
    const unsigned count=writes;scene.event_running=true;
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){74,1})==BW_PROGRESS_DEFERRED);assert(writes==count);scene.event_running=false;
    cpu.ram[0x3C9D54]=1;assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){74,1})==BW_PROGRESS_DEFERRED);cpu.ram[0x3C9D54]=0;
    cpu.ram[0x3C9EA2]=1;assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){74,1})==BW_PROGRESS_DEFERRED);cpu.ram[0x3C9EA2]=0;
    cpu.ram[PLAYER-0x80000000u+0x305]=1;assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){74,1})==BW_PROGRESS_DEFERRED);cpu.ram[PLAYER-0x80000000u+0x305]=0;
    scene.player=PLAYER+4;assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){74,1})==BW_PROGRESS_DEFERRED);scene.player=PLAYER;
    /* All unrelated bytes, including resources/selectors/switches/CPU/player,
     * camera/position/zone flags, are preserved byte for byte. */
    for(uint32_t i=0;i<cpu.ram_size;++i){const uint32_t off=i-(INFO-0x80000000u);
        bool permitted=off==0x3E||off==0x53||off==0x48||off==0x5D||off==0x6F||off==0x380+11*0x24+0x21||off==0x799||off==0x380+0x24+0x21||off==0xBD||off==0x638||off==0x64E;
        if(!permitted)assert(cpu.ram[i]==before[i]);}
    BwProgressionState known={0},snap={0};BwProgressionDelta out={0};
    BwGameEvent e={0};e.kind=BW_GAME_EVENT_PROGRESSION_CHANGED;e.fact=BW_GAME_FACT_CURRENT_STAGE;e.index=0x21;e.after=4;
    assert(bw_progression_capture_event(&known,&cpu,&e,&out)&&out.key==203&&out.value==4);
    assert(!bw_progression_capture_event(&known,&cpu,&e,&out));
    e.fact=BW_GAME_FACT_ITEM_COUNT;e.index=1;e.after=20;assert(!bw_progression_capture_event(&known,&cpu,&e,&out));
    e.fact=BW_GAME_FACT_EVENT;e.index=20;e.after=1;assert(!bw_progression_capture_event(&known,&cpu,&e,&out));
    assert(bw_progression_snapshot(&cpu,&snap)&&snap.values[2]==0x22&&snap.values[12]==0x36&&snap.values[203]==4);
    assert(!bw_progression_merge(&snap,(BwProgressionDelta){12,0x27}));assert(!bw_progression_merge(&snap,(BwProgressionDelta){203,4}));
    /* Unsupported chest/gear facts never advance known or suppress local
     * native small-key/bottle/heart/equipment rewards. */
    e.kind=BW_GAME_EVENT_PROGRESSION_CHANGED;e.fact=BW_GAME_FACT_CURRENT_STAGE;e.index=0;e.after=0;
    assert(!bw_progression_capture_event(&known,&cpu,&e,&out)&&known.values[172]==0);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){172,1})==BW_PROGRESS_INVALID);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){64,2})==BW_PROGRESS_INVALID);
    /* Native recollection loadouts are temporary even when controls are ready.
     * Both the explicit byte and each authored Xboss stage block capture/apply. */
    const unsigned blocked_writes=writes;BwProgressionState protected_snap=snap;
    cpu.ram[0x3CA8C8]=1;
    assert(!bw_progression_snapshot(&cpu,&snap)&&memcmp(&snap,&protected_snap,sizeof snap)==0);
    e.kind=BW_GAME_EVENT_INVENTORY_CHANGED;e.fact=BW_GAME_FACT_ITEM_SLOT;e.index=2;e.after=0x22;
    assert(!bw_progression_capture_event(&known,&cpu,&e,&out)&&known.values[2]==0);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){73,1})==BW_PROGRESS_DEFERRED&&writes==blocked_writes);
    cpu.ram[0x3CA8C8]=0;
    for(unsigned i=0;i<4;++i){memset(cpu.ram+0x3C9D3C,0,8);memcpy(cpu.ram+0x3C9D3C,"Xboss0",7);cpu.ram[0x3C9D41]=(uint8_t)('0'+i);
        assert(!bw_progression_snapshot(&cpu,&snap));assert(!bw_progression_capture_event(&known,&cpu,&e,&out));
        assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){73,1})==BW_PROGRESS_DEFERRED&&writes==blocked_writes);}
    /* Temporary GTower suppression is never undone by shared bow ownership. */
    memset(cpu.ram+0x3C9D3C,0,8);memcpy(cpu.ram+0x3C9D3C,"GTower",7);strcpy(scene.stage,"GTower");
    cpu.ram[INFO-0x80000000u+0x48]=0xFF;
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){12,0x36})==BW_PROGRESS_UNCHANGED);
    assert(cpu.ram[INFO-0x80000000u+0x48]==0xFF);assert(bw_progression_snapshot(&cpu,&snap)&&snap.values[12]==0x36);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){5,0x2D})==BW_PROGRESS_APPLIED);
    assert(cpu.ram[INFO-0x80000000u+0x13]==0); /* Boomerang never derives magic. */
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){6,0x34})==BW_PROGRESS_APPLIED);
    assert(cpu.ram[INFO-0x80000000u+0x13]==16&&cpu.ram[INFO-0x80000000u+0x14]==0);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){33,32})==BW_PROGRESS_APPLIED);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){33,16})==BW_PROGRESS_UNCHANGED);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){32,2})==BW_PROGRESS_APPLIED);
    assert(cpu.ram[INFO-0x80000000u+4]==0&&cpu.ram[INFO-0x80000000u+5]==0);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){34,99})==BW_PROGRESS_APPLIED);
    assert(bw_progression_apply(&cpu,&scene,(BwProgressionDelta){34,60})==BW_PROGRESS_UNCHANGED);
    assert(cpu.ram[INFO-0x80000000u+0x6F]==99&&cpu.ram[INFO-0x80000000u+0x67]==0);
    assert(!bw_progression_valid((BwProgressionDelta){35,255}));
    assert(bw_progression_snapshot(&cpu,&snap)&&snap.values[32]==2&&snap.values[33]==32&&snap.values[34]==99);
    w32(&cpu,0x803C9DA0u,0x817FFFFCu);assert(bw_progression_current_save_stage(&cpu)==-1);
    free(before);free(cpu.ram);puts("progression whitelist/apply/capture fixtures passed");return 0;
}
