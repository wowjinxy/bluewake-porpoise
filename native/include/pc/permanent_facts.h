// SPDX-License-Identifier: MIT
#ifndef PC_PERMANENT_FACTS_H
#define PC_PERMANENT_FACTS_H
#include <stdbool.h>
#include <stdint.h>

/* Stable semantic IDs; zero means absence in snapshots, never an award.
 * Sparse holes are reserved, not a license to synchronize raw save fields. */
#define PC_PERMANENT_FACT_COUNT 128u
typedef enum PcPermanentFactKey {
    PC_PERMANENT_TELESCOPE=1, PC_PERMANENT_SAIL=2, PC_PERMANENT_WAKER=3,
    PC_PERMANENT_GRAPPLE=4, PC_PERMANENT_SPOILS=5, PC_PERMANENT_BOOMERANG=6,
    PC_PERMANENT_LEAF=7, PC_PERMANENT_TINGLE=8, PC_PERMANENT_CAMERA=9,
    PC_PERMANENT_IRON_BOOTS=10, PC_PERMANENT_MAGIC_ARMOR=11,
    PC_PERMANENT_BAIT_BAG=12, PC_PERMANENT_BOW=13, PC_PERMANENT_BOMBS=14,
    PC_PERMANENT_DELIVERY_BAG=19, PC_PERMANENT_HOOKSHOT=20, PC_PERMANENT_HAMMER=21,
    // 22 remains reserved: sword ownership and temporary FF1 availability differ.
    PC_PERMANENT_SHIELD=23,
    PC_PERMANENT_POWER_BRACELETS=24,
    PC_PERMANENT_SONG_FIRST=32, PC_PERMANENT_SONG_LAST=37,
    PC_PERMANENT_WALLET=48, PC_PERMANENT_MAGIC_MAX=49,
    PC_PERMANENT_ARROW_MAX=50, PC_PERMANENT_BOMB_MAX=51,
    PC_PERMANENT_PEARL_FIRST=64, PC_PERMANENT_PEARL_LAST=66,
    PC_PERMANENT_SHARD_FIRST=72, PC_PERMANENT_SHARD_LAST=79
} PcPermanentFactKey;
typedef enum PcPermanentOperation {
    PC_PERMANENT_MAX=1, PC_PERMANENT_STEP=2
} PcPermanentOperation;
typedef enum PcPermanentProducerKind {
    PC_PERMANENT_PRODUCER_CHEST=1, PC_PERMANENT_PRODUCER_EVENT=2,
    PC_PERMANENT_PRODUCER_FAIRY=3, PC_PERMANENT_PRODUCER_BOSS=4,
    PC_PERMANENT_PRODUCER_TRADE=5, PC_PERMANENT_PRODUCER_STORY=6
} PcPermanentProducerKind;

static inline bool pc_permanent_fact_valid(uint16_t key,uint32_t value) {
    if(!value)return false;
    if(key==PC_PERMANENT_CAMERA||key==PC_PERMANENT_SHIELD)return value<=2;
    if(key==PC_PERMANENT_BOW)return value<=3;
    if(key==PC_PERMANENT_WALLET)return value<=2;
    if(key==PC_PERMANENT_MAGIC_MAX)return value==16||value==32;
    if(key==PC_PERMANENT_ARROW_MAX||key==PC_PERMANENT_BOMB_MAX)return value==30||value==60||value==99;
    if((key>=1&&key<=14)||(key>=19&&key<=21)||key==PC_PERMANENT_POWER_BRACELETS||
       (key>=32&&key<=37)||(key>=64&&key<=66)||(key>=72&&key<=79))return value==1;
    return false;
}
static inline bool pc_permanent_server_authoritative(uint16_t key) {
    return key==PC_PERMANENT_WALLET||key==PC_PERMANENT_ARROW_MAX||key==PC_PERMANENT_BOMB_MAX;
}
static inline bool pc_permanent_operation_valid(uint16_t key,uint16_t operation,uint32_t value) {
    if(pc_permanent_server_authoritative(key))return operation==PC_PERMANENT_STEP&&value==1;
    return operation==PC_PERMANENT_MAX&&pc_permanent_fact_valid(key,value);
}
/* Called once for a NEW authored producer, after ledger deduplication.
 * The inventory's base capacities are30; absent facts do not grant resources. */
static inline uint32_t pc_permanent_operation_result(uint16_t key,uint32_t current,uint16_t operation,uint32_t value) {
    if(!pc_permanent_operation_valid(key,operation,value))return current;
    if(operation==PC_PERMANENT_MAX)return value>current?value:current;
    if(key==PC_PERMANENT_WALLET)return current<2?current+1:2;
    return current<60?60:99;
}
static inline bool pc_permanent_producer_valid(uint64_t id) {
    const unsigned kind=(unsigned)(id>>56);
    return kind>=PC_PERMANENT_PRODUCER_CHEST&&kind<=PC_PERMANENT_PRODUCER_STORY&&
        (id&UINT64_C(0x00ffffffffffffff))!=0;
}
static inline const char* pc_permanent_fact_name(uint16_t key) {
    switch(key) {
    case 1:return "Telescope";case 2:return "Sail";case 3:return "Waker";
    case 4:return "Grapple";case 5:return "Spoils bag";case 6:return "Boomerang";
    case 7:return "Leaf";case 8:return "Tingle tuner";case 9:return "Camera";
    case 10:return "Iron boots";case 11:return "Magic armor";case 12:return "Bait bag";
    case 13:return "Bow";case 14:return "Bombs";case 19:return "Delivery bag";
    case 20:return "Hookshot";case 21:return "Hammer";
    case PC_PERMANENT_SHIELD:return "Shield rank";
    case PC_PERMANENT_POWER_BRACELETS:return "Power bracelets";
    case 48:return "Wallet tier";case 49:return "Magic capacity";
    case 50:return "Arrow capacity";case 51:return "Bomb capacity";
    default:if(key>=32&&key<=37)return "Song";if(key>=64&&key<=66)return "Pearl";
        if(key>=72&&key<=79)return "Shard";return "Unsupported";
    }
}
#endif
