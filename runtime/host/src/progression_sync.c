// SPDX-License-Identifier: GPL-3.0-or-later
#include "progression_sync.h"
#include <string.h>

/* Primary GZLE01 audit: zeldaret/tww include/d/d_save.h, d_com_inf_game.h,
 * d_stage.h; src/d/d_save.cpp and d_item.cpp. dInvSlot is NOT item ID.
 * onItem ORs a per-slot rank bit; onCollect/onTact/onTriforce/onSymbol OR
 * explicit ownership masks. ALL Tbox bits and collected gear0..2 are excluded:
 * they need item-aware rewards/derived equipment qualification. Dungeon bits
 *0/1/2/3/5 are map, compass, bosskey, bossdefeat and bossdemo; STAGE_LIFE is
 * excluded because it suppresses an unshared native heart-container reward.
 * Switches/key counts and room/zone/item registers are also excluded.
 * Named event bits whitelist only pearl placements1480/1440/1410 and permanent
 * GRANDMA_HEALED2A20. Unknown event/register bits never cross the network.
 * getStageStagInfo is gameInfo+12A0+3EB0+48; SaveTbl=(mProp>>1)&7F.
 * The serialized CARD offsets differ and are never used as native RAM offsets. */
static const uint32_t kInfo=0x803C4C08u, kStagSlot=0x803C9DA0u;
static const uint8_t kItems[21]={0x20,0x78,0x22,0x25,0x24,0x2D,0x34,0x21,
    0x23,0x29,0x2A,0x2C,0x27,0x31,0,0,0,0,0x30,0x2F,0x33};
static bool span(const CPUState* cpu,uint32_t a,uint32_t n) {
    if(!cpu||!cpu->ram||a<0x80000000u||n>0x01800000u)return false;
    const uint32_t off=a-0x80000000u;
    return off<=0x01800000u-n&&n<=cpu->ram_size&&off<=cpu->ram_size-n;
}
static uint8_t r8(const CPUState* cpu,uint32_t a){return cpu->ram[a-0x80000000u];}
static uint32_t r32(const CPUState* cpu,uint32_t a){const uint8_t* p=cpu->ram+(a-0x80000000u);return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static bool recollection(const CPUState* cpu){
    /* Native recollection replaces inventory/collected stores and later
     * restores its saved buffer. No temporary loadout may be exported, nor
     * may a shared reward be written into the soon-to-be-discarded loadout. */
    if(!span(cpu,0x803CA8C8u,1)||!span(cpu,0x803C9D3Cu,8))return true;
    if(r8(cpu,0x803CA8C8u))return true;
    const uint8_t* stage=cpu->ram+(0x803C9D3Cu-0x80000000u);
    return memcmp(stage,"Xboss",5)==0&&stage[5]>='0'&&stage[5]<='3'&&stage[6]==0;
}
static unsigned rank(uint16_t key,uint32_t value){
    if(key>=21||!kItems[key])return 0;
    if(value==kItems[key])return 1;
    if(key==8&&value==0x26)return 2;
    if(key==12&&value==0x35)return 2;
    if(key==12&&value==0x36)return 3;
    return 0;
}
static uint32_t mask(uint16_t key){
    switch(key){case 67:case 68:return 1;
        case 73:return 0x3F;case 74:return 0xFF;case 75:return 7;
        case 244:return 0xD0;default:break;}
    /* Chest markers consume native rewards, including unsynchronized small
     * keys/hearts/bottles. Collected gear needs native derived equip stores.
     * Both are excluded until an item-aware/native equip path is qualified. */
    if(key>=192&&key<208)return 0x2F; /* STAGE_LIFE suppresses an unshared heart. */
    return 0;
}
/* Event byte42 lives outside224..255. Assign a compact explicit stable key255,
 * rather than exposing the unknown intervening bytes. */
static uint32_t allowed(uint16_t key){return key==255?0x20:mask(key);}
bool bw_progression_valid(BwProgressionDelta d){
    if(d.key>=BW_PROGRESSION_KEYS||d.value==0)return false;
    if(d.key<21)return rank(d.key,d.value)>0;
    if(d.key==32)return d.value==1||d.value==2; /* wallet tiers */
    if(d.key==33)return d.value==16||d.value==32; /* magic capacity */
    if(d.key==34||d.key==35)return d.value==30||d.value==60||d.value==99;
    const uint32_t m=allowed(d.key);return m&&!(d.value&~m);
}
void bw_progression_clear(BwProgressionState* s){if(s)memset(s,0,sizeof *s);}
bool bw_progression_merge(BwProgressionState* s,BwProgressionDelta d){
    if(!s||!bw_progression_valid(d))return false;
    const uint32_t old=s->values[d.key];
    const uint32_t next=d.key<21?(rank(d.key,d.value)>rank(d.key,old)?d.value:old):
        (d.key>=32&&d.key<=35?(d.value>old?d.value:old):(old|d.value));
    s->values[d.key]=next;return next!=old;
}
const char* bw_progression_key_name(uint16_t k){
    if(k<21&&kItems[k])return "Equipment ownership";
    if(k>=32&&k<=35)return "Permanent capacity upgrade";
    if(k>=64&&k<77&&allowed(k))return "Permanent collected reward";
    if(k>=192&&k<208)return "Dungeon reward/completion";
    if(k==244)return "Placed pearls";if(k==255)return "Grandma healed";return NULL;
}
int bw_progression_current_save_stage(const CPUState* cpu){
    if(!span(cpu,kStagSlot,4))return -1;
    const uint32_t p=r32(cpu,kStagSlot);if((p&3)||!span(cpu,p,0x20))return -1;
    const int stage=(r8(cpu,p+9)>>1)&0x7F;return stage<16?stage:-1;
}
static uint32_t offset(uint16_t key){
    if(key<21)return 0x3Cu+key;
    if(key==32)return 0x12;if(key==33)return 0x13;
    if(key==34)return 0x6F;if(key==35)return 0x70;
    if(key>=64&&key<77&&allowed(key))return 0xB4u+key-64;
    if(key>=192&&key<208)return 0x380u+(key-192)*0x24u+0x21u;
    if(key==244)return 0x624u+20;if(key==255)return 0x624u+42;return UINT32_MAX;
}
static uint32_t current_offset(uint16_t key,int stage){
    if(stage<0)return UINT32_MAX;
    if(key>=192&&key<208&&key-192==stage)return 0x778u+0x21u;
    return UINT32_MAX;
}
bool bw_progression_snapshot(const CPUState* cpu,BwProgressionState* s){
    if(!s||!span(cpu,kInfo,0x79C)||recollection(cpu))return false;bw_progression_clear(s);
    const int stage=bw_progression_current_save_stage(cpu);
    for(uint16_t k=0;k<BW_PROGRESSION_KEYS;++k){
        const uint32_t off=offset(k);if(off==UINT32_MAX)continue;
        uint32_t value=r8(cpu,kInfo+off),cur=current_offset(k,stage);
        if(cur!=UINT32_MAX)value|=r8(cpu,kInfo+cur);
        if(k<21&&kItems[k]){
            const unsigned flags=r8(cpu,kInfo+0x51+k);uint32_t obtained=0;
            if(k==12)obtained=(flags&4)?0x36:(flags&2)?0x35:(flags&1)?0x27:0;
            else if(k==8)obtained=(flags&2)?0x26:(flags&1)?0x23:0;
            else if(flags&1)obtained=kItems[k];
            if(rank(k,obtained)>rank(k,value))value=obtained;
        }
        if(k>=64)value&=allowed(k);
        const BwProgressionDelta d={k,value};if(bw_progression_valid(d))s->values[k]=value;
    }return true;
}
bool bw_progression_capture_event(BwProgressionState* known,const CPUState* cpu,const BwGameEvent* e,BwProgressionDelta* out){
    if(!known||!e||!out||!span(cpu,kInfo,0x79C)||recollection(cpu))return false;
    if(e->kind!=BW_GAME_EVENT_INVENTORY_CHANGED&&e->kind!=BW_GAME_EVENT_PROGRESSION_CHANGED)return false;
    uint16_t key=UINT16_MAX;uint32_t live_off=UINT32_MAX;
    switch(e->fact){
        case BW_GAME_FACT_ITEM_SLOT:if(e->index<21){key=e->index;live_off=0x3C+e->index;}break;
        case BW_GAME_FACT_COLLECTED:if(e->index<13){key=64+e->index;live_off=0xB4+e->index;}break;
        case BW_GAME_FACT_ITEM_OBTAINED:if(e->index<21){BwProgressionState all;
            if(!bw_progression_snapshot(cpu,&all))return false;const BwProgressionDelta d={e->index,all.values[e->index]};
            if(r8(cpu,kInfo+0x51+e->index)!=e->after||!bw_progression_merge(known,d))return false;*out=d;return true;}break;
        case BW_GAME_FACT_ITEM_CAPACITY:if(e->index==1||e->index==2){key=e->index==1?34:35;live_off=0x6E + e->index;}break;
        case BW_GAME_FACT_EVENT:if(e->index==20||e->index==42){key=e->index==20?244:255;live_off=0x624+e->index;}break;
        case BW_GAME_FACT_SAVED_STAGE:{unsigned stage=e->index/0x24,byte=e->index%0x24;
            if(stage<16&&byte==0x21)key=192+stage;
            live_off=0x380+e->index;break;}
        case BW_GAME_FACT_CURRENT_STAGE:{const int stage=bw_progression_current_save_stage(cpu);
            if(stage>=0&&e->index==0x21)key=192+stage;
            live_off=0x778+e->index;break;}
        default:return false;
    }
    if(key>=BW_PROGRESSION_KEYS||live_off>=0x79C||r8(cpu,kInfo+live_off)!=e->after)return false;
    uint32_t value=e->after;if(key>=64)value&=allowed(key);
    const BwProgressionDelta d={key,value};if(!bw_progression_merge(known,d))return false;*out=d;return true;
}
BwProgressionApply bw_progression_apply(CPUState* cpu,const BwGameScene* scene,BwProgressionDelta d){
    if(!bw_progression_valid(d)||!span(cpu,kInfo,0x79C))return BW_PROGRESS_INVALID;
    if(recollection(cpu))return BW_PROGRESS_DEFERRED;
    if(!scene||!scene->active||!scene->player_valid||!scene->controls_ready||scene->paused||scene->event_running||scene->transitioning||
       !span(cpu,0x803CA74Cu,4)||r32(cpu,0x803CA74Cu)!=scene->player||!span(cpu,scene->player,0x361C)||
       r32(cpu,scene->player+0x498)!=scene->player+0x1F8||r8(cpu,0x803C9D54u)||r8(cpu,0x803F7097u)||r8(cpu,0x803C9EA2u)||
       r8(cpu,scene->player+0x304)||r8(cpu,scene->player+0x305)||r32(cpu,scene->player+0x314)||
       r32(cpu,0x803F6160u)||memcmp(cpu->ram+(0x803C9D3Cu-0x80000000u),scene->stage,8)!=0)return BW_PROGRESS_DEFERRED;
    const uint32_t off=offset(d.key),old=r8(cpu,kInfo+off);
    const uint32_t next=d.key<21?(rank(d.key,d.value)>rank(d.key,old)?d.value:old):
        (d.key>=32&&d.key<=35?(d.value>old?d.value:old):(old|d.value));
    /* Native GTower removes the bow slot and restores it from permanent
     * obtained flags on exit. Preserve that temporary story restriction. */
    const bool suppressed=d.key==12&&strcmp(scene->stage,"GTower")==0;
    bool changed=!suppressed&&next!=old;if(changed)mem_write8(cpu,kInfo+off,(uint8_t)next);
    if(d.key<21){
        const unsigned bits=(1u<<rank(d.key,next))-1u;
        const uint8_t flags=r8(cpu,kInfo+0x51u+d.key),after=(uint8_t)(flags|bits);
        if(after!=flags){mem_write8(cpu,kInfo+0x51u+d.key,after);changed=true;}
        /* Receiving ownership does not refill arrows/bombs or change selectors.
         * Minimum capacities are permanent prerequisites of native tools. */
        if(d.key==12&&r8(cpu,kInfo+0x6F)<30){mem_write8(cpu,kInfo+0x6F,30);changed=true;}
        if(d.key==13&&r8(cpu,kInfo+0x70)<30){mem_write8(cpu,kInfo+0x70,30);changed=true;}
        if(d.key==6&&r8(cpu,kInfo+0x13)<16){mem_write8(cpu,kInfo+0x13,16);changed=true;}
    }
    const uint32_t cur=current_offset(d.key,bw_progression_current_save_stage(cpu));
    if(cur!=UINT32_MAX){const uint8_t a=r8(cpu,kInfo+cur),b=(uint8_t)(a|d.value);if(a!=b){mem_write8(cpu,kInfo+cur,b);changed=true;}}
    return changed?BW_PROGRESS_APPLIED:BW_PROGRESS_UNCHANGED;
}
