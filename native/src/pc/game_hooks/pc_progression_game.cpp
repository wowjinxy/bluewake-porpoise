// SPDX-License-Identifier: GPL-3.0-or-later
// Native permanent progression: accepted producer + completed message + actual
// native effect + normal event completion. Inventory snapshots are never origin.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_progression.h"
#include "pc/pc_progression_logic.h"
#include "pc/pc_network_game.h"
#include "pc_internal.h"
#include "d/actor/d_a_player_main.h"
#include "d/actor/d_a_bigelf.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_manager.h"
#include "d/d_item.h"
#include "d/d_meter.h"
#include "d/d_s_play.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_overlap_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include <cstring>
#include <unistd.h>

namespace {
constexpr uint32_t kAbsent=UINT32_MAX;
constexpr unsigned kQueueSize=64, kAwards=8;
struct ItemSpec { u8 item; u16 key; u32 value; int slot; };
// Explicit permanent catalogue. No consumables, training gear, bottles,
// sword/shield/bracelets, chest flags, small keys or boss-heart suppression.
const ItemSpec kItems[]={
 {0x20,1,1,0},{0x78,2,1,1},{0x22,3,1,2},{0x25,4,1,3},
 {0x24,5,1,4},{0x2d,6,1,5},{0x34,7,1,6},{0x21,8,1,7},
 {0x23,9,1,8},{0x26,9,2,8},{0x29,10,1,9},{0x2a,11,1,10},
 {0x2c,12,1,11},{0x27,13,1,12},{0x35,13,2,12},{0x36,13,3,12},
 {0x31,14,1,13},{0x30,19,1,18},{0x2f,20,1,19},{0x33,21,1,20},
 {0x6d,32,1,-1},{0x6e,33,1,-1},{0x6f,34,1,-1},
 {0x70,35,1,-1},{0x71,36,1,-1},{0x72,37,1,-1},
 {0xab,48,1,-1},{0xac,48,2,-1},{0xb2,49,32,-1},
 {0xaf,50,60,-1},{0xb0,50,99,-1},{0xad,51,60,-1},{0xae,51,99,-1},
 {0x69,64,1,-1},{0x6a,65,1,-1},{0x6b,66,1,-1},
 {0x61,72,1,-1},{0x62,73,1,-1},{0x63,74,1,-1},{0x64,75,1,-1},
 {0x65,76,1,-1},{0x66,77,1,-1},{0x67,78,1,-1},{0x68,79,1,-1}
};
const ItemSpec* specFor(u32 item) {
 for(const auto& spec:kItems) if(spec.item==item) return &spec;
 return nullptr;
}
PcProgressIdentity sIdentity={};
PcNativeEvent sEvents[kQueueSize]={};
unsigned sHead=0,sCount=0;
bool sOverflow=false,sRemote=false,sPlayerValid=false,sFrameStarted=false,sClaimOverflow=false,sQuestReady=false;
uint64_t sInvocation=0;
struct NativeAward {
 pc::progression::Award evidence;
 u32 key=0,value=0,before=0,operation=0,operationValue=0;
 u32 sourceKind=0,sourceSlot=0,itemNo=0;
 uint64_t authored=0;
 bool aryll=false;
};
NativeAward sAwards[kAwards];
PcProgressClaim sClaims[PC_PROGRESS_MAX_PENDING_CLAIMS]={};
bool sClaimUsed[PC_PROGRESS_MAX_PENDING_CLAIMS]={};
struct PreparedSave {
 bool active=false,armed=false,requestReady=false; uint64_t epoch=0,clientToken=0;
 int32_t slot=-1,armedSlot=-1;
 u32 facts[PC_PERMANENT_FACT_COUNT]={};
} sPrepared;
struct CardTransaction {
 bool active=false,prepared=false; uint64_t invocation=0,clientToken=0;
 char readHash[65]={};
 PcProgressIdentity identity={}; u32 facts[PC_PERMANENT_FACT_COUNT]={};
} sWrite,sRead;
char sPreparedReadHash[65]={};
struct QualifiedRead {
 uint64_t invocation=0; char hash[65]={}; PcProgressIdentity identity={};
} sQualifiedRead;

bool temporaryInventory() {
 const char* stage=dComIfGp_getStartStageName();
 return dComIfGp_getPlayerInfoBufferStageNo()!=0 ||
  (stage&&(!std::strcmp(stage,"Xboss0")||!std::strcmp(stage,"Xboss1")||
           !std::strcmp(stage,"Xboss2")||!std::strcmp(stage,"Xboss3")));
}
u32 factValue(u32 key) {
 u32 value=0;
 for(const auto& spec:kItems) {
  if(spec.key!=key||spec.slot<0) continue;
  if(dComIfGs_isGetItem(spec.slot,static_cast<u8>(spec.value-1))&&spec.value>value) value=spec.value;
 }
 if(key>=32&&key<=37) value=dComIfGs_isTact(key-32)?1:0;
 else if(key==48) value=dComIfGs_getWalletSize();
 else if(key==49) value=dComIfGs_getMaxMagic();
 else if(key==50) value=dComIfGs_getArrowMax();
 else if(key==51) value=dComIfGs_getBombMax();
 else if(key>=64&&key<=66) value=dComIfGs_isSymbol(key-64)?1:0;
 else if(key>=72&&key<=79) value=dComIfGs_isTriforce(key-72)?1:0;
 return key<PC_PERMANENT_FACT_COUNT&&pc_permanent_fact_valid(static_cast<u16>(key),value)?value:0;
}
u32 telescopeOwned() { return dComIfGs_getItem(0)==dItemNo_TELESCOPE_e&&factValue(1)?1:0; }
void captureFacts(u32* values) {
 for(u32 key=0;key<PC_PERMANENT_FACT_COUNT;++key) values[key]=factValue(key);
}
void* playScene(void* proc,void*) { return fpcM_GetName(proc)==fpcNm_PLAY_SCENE_e?proc:nullptr; }
PcNativeEvent eventValue(u32 kind,u32 origin=PC_NATIVE_SYSTEM) {
 PcNativeEvent event={}; event.kind=kind; event.origin=origin; event.identity=sIdentity;
 const char* stage=dComIfGp_getStartStageName();
 if(stage) std::strncpy(event.stage,stage,sizeof(event.stage)-1);
 auto* player=sPlayerValid?dComIfGp_getPlayer(0):nullptr;
 event.room=player?fopAcM_GetRoomNo(player):-1;
 return event;
}
void push(const PcNativeEvent& event) {
 if(sCount==kQueueSize) {
  sOverflow=true; sHead=sCount=0;
  pc::writef(STDERR_FILENO,"[cos] progression: event queue overflow; reconciliation required\n");
 }
 sEvents[(sHead+sCount++)%kQueueSize]=event;
}
PcProgressClaim* claimFor(uint64_t invocation) {
 for(unsigned i=0;i<PC_PROGRESS_MAX_PENDING_CLAIMS;++i)
  if(sClaimUsed[i]&&sClaims[i].invocation_id==invocation) return &sClaims[i];
 return nullptr;
}
void dropClaim(uint64_t invocation) {
 for(unsigned i=0;i<PC_PROGRESS_MAX_PENDING_CLAIMS;++i)
  if(sClaimUsed[i]&&sClaims[i].invocation_id==invocation) sClaimUsed[i]=false;
}
void addClaim(const NativeAward& award) {
 if((!award.authored&&award.operation!=PC_PERMANENT_STEP)||claimFor(award.evidence.invocation)) return;
 for(unsigned i=0;i<PC_PROGRESS_MAX_PENDING_CLAIMS;++i) if(!sClaimUsed[i]) {
  sClaimUsed[i]=true; auto& claim=sClaims[i]; claim={};
  claim.invocation_id=award.evidence.invocation; claim.authored_reward_id=award.authored;
  claim.key=award.key; claim.operation=award.operation; claim.value=award.operationValue;
  claim.observed_value=factValue(award.key); claim.state=PC_PROGRESS_CLAIM_ARMED;
  claim.identity=sIdentity; return;
 }
 sClaimOverflow=true; // Never permit a save of an untracked progressive effect.
}
void describe(PcNativeEvent& event,const NativeAward& award) {
 event.reward_id=award.aryll?PC_NATIVE_REWARD_ARYLL_TELESCOPE:0;
 event.producer_id=award.evidence.producer; event.invocation_id=award.evidence.invocation;
 event.authored_reward_id=award.authored; event.source_kind=award.sourceKind;
 event.source_slot=award.sourceSlot; event.item_actor_id=award.evidence.item;
 event.item_no=award.itemNo; event.fact_key=award.key; event.fact_value=award.value;
 event.operation=award.operation; event.operation_value=award.operationValue;
}
void cancelAward(NativeAward& award) {
 if(!award.evidence.active) return;
 auto event=eventValue(PC_NATIVE_AWARD_CANCELLED,PC_NATIVE_LOCAL); describe(event,award); push(event);
 if(auto* claim=claimFor(award.evidence.invocation)) {
  if(pc::progression::retainCancelledClaim(*claim,award.evidence.effect)) claim->state=PC_PROGRESS_CLAIM_CANCELLED_EFFECT;
  else dropClaim(award.evidence.invocation);
 }
 award={};
}
void cancelAwards() { for(auto& award:sAwards) cancelAward(award); }
void revoke(u32 kind,bool quest=false) {
 const QualifiedRead source=kind==PC_NATIVE_QUEST_LOADED?sQualifiedRead:QualifiedRead{};
 cancelAwards(); sPrepared={}; ++sIdentity.epoch;
 if(quest) {
  const bool trusted=kind==PC_NATIVE_QUEST_INITIALIZED||kind==PC_NATIVE_QUEST_LOADED;
  ++sIdentity.quest_generation;
  for(unsigned i=0;i<PC_PROGRESS_MAX_PENDING_CLAIMS;++i) if(sClaimUsed[i]) {
   if(pc::progression::retireClaimOnActivation(sClaims[i],sIdentity,trusted)) sClaimUsed[i]=false;
   else sClaims[i].state=PC_PROGRESS_CLAIM_STALE;
  }
  sQualifiedRead={}; std::memset(sPreparedReadHash,0,sizeof(sPreparedReadHash));
 }
 auto event=eventValue(kind);
 if(kind==PC_NATIVE_QUEST_INITIALIZED||kind==PC_NATIVE_QUEST_LOADED) {
  event.saved_facts_valid=1; captureFacts(event.saved_facts);
 }
 if(source.invocation&&pc::progression::sameProfile(source.identity,sIdentity)) {
  event.source_read_invocation=source.invocation;
  std::memcpy(event.read_source_sha256,source.hash,sizeof(event.read_source_sha256));
 }
 push(event);
}
void refresh() {
 if(!sIdentity.session_id) {
  sIdentity.session_id=pc::monotonicNs();
  if(!sIdentity.session_id) sIdentity.session_id=1;
  std::strcpy(sIdentity.profile_id,"personal");
 }
 auto* scene=sFrameStarted?fpcM_Search(playScene,nullptr):nullptr;
 auto* player=sFrameStarted?dComIfGp_getPlayer(0):nullptr;
 bool valid=scene&&player&&player==dComIfGp_getLinkPlayer()&&fpcM_GetName(player)==fpcNm_PLAYER_e;
 u32 sceneId=scene?fpcM_GetID(scene):kAbsent,playerId=valid?fpcM_GetID(player):kAbsent;
 valid=valid&&!fpcM_IsErrorID(sceneId)&&!fpcM_IsErrorID(playerId)&&
  fpcM_IsExecuting(sceneId)&&fpcM_IsExecuting(playerId)&&!fpcM_IsCreating(sceneId)&&!fpcM_IsCreating(playerId);
 if(!valid) playerId=kAbsent;
 char name[17]={}; std::memcpy(name,dComIfGs_getPlayerName(),16);
 const u64 serial=dComIfGs_getMemCardCheckID(); const int slot=dComIfGs_getDataNum();
 if(sIdentity.scene_id!=sceneId||sIdentity.player_id!=playerId||sIdentity.card_serial!=serial||
    sIdentity.quest_slot!=slot||std::memcmp(sIdentity.player_name,name,17)) {
  cancelAwards(); sPrepared={}; sIdentity.scene_id=sceneId; sIdentity.player_id=playerId;
  sIdentity.card_serial=serial; sIdentity.quest_slot=slot; std::memcpy(sIdentity.player_name,name,17);
  sPlayerValid=valid; ++sIdentity.epoch; push(eventValue(PC_NATIVE_LIFECYCLE_CHANGED));
 } else sPlayerValid=valid;
}
bool anyAward() { for(const auto& award:sAwards) if(award.evidence.active) return true; return false; }
bool safeToApply() {
 if(!sQuestReady||!sPlayerValid||sIdentity.quest_slot<0||sIdentity.quest_slot>2||temporaryInventory()||
    anyAward()||sWrite.active||sRead.active||pc::menuOpen()||dMenu_flag()||
    dScnPly_ply_c::pauseTimer||dScnPly_ply_c::nextPauseTimer||fopOvlpM_IsPeek()||
    fopOvlpM_IsDoingReq()||dComIfGp_isEnableNextStage()||dComIfGp_event_runCheck()||
    dComIfGp_getMiniGameType()!=0||!dComIfGp_2dShowCheck()||dComIfGp_isHeapLockFlag()==6||
    dComIfGp_getOperateWind()==2||!dComIfGs_getLife()||daPy_dmEcallBack_c::checkCurse()) return false;
 auto* player=static_cast<daPy_lk_c*>(dComIfGp_getPlayer(0));
 return !player->checkPlayerDemoMode()&&!player->checkFreezeState()&&
  !player->checkEquipAnime()&&
  player->getPcActionProc()!=daPy_lk_c::daPyProc_SCOPE_e&&
  player->getPcActionProc()!=daPy_lk_c::daPyProc_SHIP_SCOPE_e&&
  !player->checkModeFlg(daPy_lk_c::ModeFlg_DAMAGE|daPy_lk_c::ModeFlg_CAUGHT|
   daPy_lk_c::ModeFlg_SUBJECT|daPy_lk_c::ModeFlg_HOOKSHOT|daPy_lk_c::ModeFlg_ROPE);
}
void hashByte(uint64_t& hash,u8 value) { hash^=value; hash*=UINT64_C(1099511628211); }
void hashWord(uint64_t& hash,u32 value) { for(unsigned i=0;i<4;++i) hashByte(hash,static_cast<u8>(value>>(i*8))); }
void hashText(uint64_t& hash,const char* text) {
 size_t size=text?std::strlen(text):0; hashWord(hash,static_cast<u32>(size));
 for(size_t i=0;i<size;++i) hashByte(hash,static_cast<u8>(text[i]));
}
uint64_t authoredId(u32 kind,u32 slot,u32 profile,u32 parameter,const char* eventName,int room) {
 uint64_t hash=UINT64_C(14695981039346656037);
 hashText(hash,"GZLE01.native.reward.1"); hashText(hash,dComIfGp_getStartStageName());
 hashWord(hash,static_cast<u32>(room)); hashWord(hash,kind); hashWord(hash,slot);
 hashWord(hash,profile); hashWord(hash,parameter); hashText(hash,eventName);
 hash&=UINT64_C(0x00ffffffffffffff); if(!hash) hash=1;
 return (static_cast<uint64_t>(kind)<<56)|hash;
}
const char* currentEventName(int eventId) {
 auto* data=dComIfGp_getPEvtManager()->getEventData(static_cast<s16>(eventId));
 return data?data->getName():nullptr;
}
fopAc_ac_c* sourceActor(u32 explicitId=kAbsent) {
 fopAc_ac_c* actor=nullptr;
 if(explicitId!=kAbsent&&fopAcM_SearchByID(explicitId,&actor)&&actor) return actor;
 auto* control=g_dComIfG_gameInfo.play.getEvent();
 fopAc_ac_c* candidates[]={control->getPt1(),control->getPt2(),control->getPtT()};
 for(auto* candidate:candidates) if(candidate&&candidate!=dComIfGp_getPlayer(0)&&
  fpcM_GetName(candidate)!=fpcNm_Demo_Item_e&&fpcM_GetName(candidate)!=fpcNm_ITEM_e) return candidate;
 return nullptr;
}
NativeAward* byItem(u32 actor) {
 for(auto& award:sAwards) if(award.evidence.active&&award.evidence.item==actor) return &award;
 return nullptr;
}
NativeAward* byInvocation(uint64_t invocation) {
 for(auto& award:sAwards) if(award.evidence.active&&award.evidence.invocation==invocation) return &award;
 return nullptr;
}
NativeAward* beginAward(u32 actor,u32 item,u32 explicitProducer=kAbsent,u32 explicitKind=0,
                       u32 explicitSlot=0,int explicitRoom=-1) {
 const auto* spec=specFor(item);
 if(sRemote||!spec||!sQuestReady||!sPlayerValid||temporaryInventory()) return nullptr;
 if(auto* existing=byItem(actor)) return existing;
 int eventId=g_dComIfG_gameInfo.play.getEvent()->mEventId;
 if(eventId<0||!dComIfGp_event_runCheck()) return nullptr;
 u32 before=factValue(spec->key); bool step=pc_permanent_server_authoritative(spec->key);
 if(!step&&before>=spec->value) return nullptr;
 NativeAward* result=nullptr;
 // Aryll acceptance precedes item creation. Bind the actual item to that invocation.
 for(auto& award:sAwards) if(award.evidence.active&&award.aryll&&item==0x20&&
  award.evidence.event==eventId&&award.evidence.item==kAbsent) { result=&award; break; }
 if(!result) for(auto& award:sAwards) if(!award.evidence.active) { result=&award; break; }
 if(!result) { sOverflow=true; return nullptr; }
 bool acceptedAryll=result->evidence.active&&result->aryll;
 auto* source=sourceActor(explicitProducer); const char* name=currentEventName(eventId);
 u32 kind=explicitKind,slot=explicitSlot,profile=source?fpcM_GetName(source):0;
 u32 parameter=source?fopAcM_GetParam(source):0;
 int room=explicitRoom>=0?explicitRoom:(source?fopAcM_GetRoomNo(source):fopAcM_GetRoomNo(dComIfGp_getPlayer(0)));
 if(!kind&&source&&profile==fpcNm_BIGELF_e) {
  auto* fairy=static_cast<daBigelf_c*>(source); kind=PC_PERMANENT_PRODUCER_FAIRY;
  slot=fairy->getEventFlag(); parameter=fairy->getType();
 }
 if(!kind) { kind=PC_PERMANENT_PRODUCER_EVENT; slot=spec->key; }
 uint64_t stable=0;
 // DEFAULT_* demos alone do not identify an authored reward location.
 if(kind==PC_PERMANENT_PRODUCER_CHEST||kind==PC_PERMANENT_PRODUCER_FAIRY||
    (name&&name[0]&&std::strncmp(name,"DEFAULT_",8)!=0))
  stable=authoredId(kind,slot,profile,parameter,
   kind==PC_PERMANENT_PRODUCER_CHEST||kind==PC_PERMANENT_PRODUCER_FAIRY?nullptr:name,room);
 // An unidentified progressive producer remains a bounded protective claim.
 // Cancellation before its effect releases it; it never becomes a wire reward.
 if(!acceptedAryll) {
  *result={}; result->evidence.begin(source?fpcM_GetID(source):explicitProducer,eventId,sIdentity.epoch,++sInvocation);
 }
 result->evidence.item=actor; result->evidence.graceFrames=120;
 result->key=spec->key; result->value=spec->value; result->before=before;
 result->operation=step?PC_PERMANENT_STEP:PC_PERMANENT_MAX;
 result->operationValue=step?1:spec->value; result->sourceKind=kind;
 result->sourceSlot=slot; result->authored=stable; result->itemNo=item;
 if(step) addClaim(*result);
 if(!acceptedAryll) { auto event=eventValue(PC_NATIVE_AWARD_BEGAN,PC_NATIVE_LOCAL); describe(event,*result); push(event); }
 return result;
}
void finishAward(NativeAward& award) {
 // Native queued max-magic updates can settle after item Delete(). The read is
 // restricted to this correlated real transaction; it never invents an origin.
 u32 actual=factValue(award.key);
 if(!award.evidence.ready(sIdentity.epoch,actual>=award.value)) return;
 addClaim(award);
 if(auto* claim=claimFor(award.evidence.invocation)) {
  claim->state=PC_PROGRESS_CLAIM_COMPLETED; claim->observed_value=actual;
  claim->completed_proof=1;
 }
 auto event=eventValue(PC_NATIVE_AWARD_COMPLETED,PC_NATIVE_LOCAL); describe(event,award); push(event);
 pc::writef(STDERR_FILENO,"[cos] progression: completed native reward key=%u value=%u source=%llu invocation=%llu\n",
  award.key,actual,static_cast<unsigned long long>(award.authored),static_cast<unsigned long long>(award.evidence.invocation));
 award={};
}
int applyFact(u32 key,u32 value,u64 expected,bool correction=false) {
 if(key>UINT16_MAX) return PC_PROGRESS_REJECTED;
 if(correction ? !pc::progression::canonicalCorrection(key,value) :
    !pc_permanent_fact_valid(static_cast<u16>(key),value)) return PC_PROGRESS_REJECTED;
 refresh(); if(expected!=sIdentity.epoch) return PC_PROGRESS_STALE;
 if(!safeToApply()) return PC_PROGRESS_DEFERRED;
 auto* player=static_cast<daPy_lk_c*>(dComIfGp_getPlayer(0));
 // Selection-cache refresh does not replace mEquipItem or its retained model/
 // arrow actor. Let normal native put-away/re-equip finish before rank changes.
 if((key==PC_PERMANENT_CAMERA&&player->checkPhotoBoxItem(player->getPcEquippedItem()))||
    (key==PC_PERMANENT_BOW&&player->checkBowItem(player->getPcEquippedItem()))) return PC_PROGRESS_DEFERRED;
 const char* stage=dComIfGp_getStartStageName();
 if(key==13&&stage&&!std::strcmp(stage,"GTower")) return PC_PROGRESS_DEFERRED;
 u32 current=factValue(key); bool authoritative=pc_permanent_server_authoritative(static_cast<u16>(key));
 if(authoritative?current==value:current>=value) return PC_PROGRESS_ALREADY;
 sRemote=true;
 if(key==48) dComIfGs_setWalletSize(static_cast<u8>(value));
 else if(key==49) dComIfGs_setMaxMagic(static_cast<u8>(value));
 else if(key==50) dComIfGs_setArrowMax(static_cast<u8>(value));
 else if(key==51) dComIfGs_setBombMax(static_cast<u8>(value));
 else if(key>=32&&key<=37) dComIfGs_onTact(static_cast<u8>(key-32));
 else if(key>=64&&key<=66) dComIfGs_onSymbol(static_cast<u8>(key-64));
 else if(key>=72&&key<=79) dComIfGs_onTriforce(static_cast<u8>(key-72));
 else {
  const ItemSpec* chosen=nullptr;
  for(const auto& spec:kItems) if(spec.key==key&&spec.slot>=0&&spec.value==value) chosen=&spec;
  if(!chosen) { sRemote=false; return PC_PROGRESS_REJECTED; }
  for(u32 rank=0;rank<value;++rank) dComIfGs_onGetItem(chosen->slot,static_cast<u8>(rank));
  dComIfGs_setItem(chosen->slot,chosen->item);
  // Refresh existing native caches, never change assigned X/Y/Z slots.
  for(int button=0;button<3;++button)
   if(dComIfGs_getSelectItem(button)==chosen->slot) dComIfGp_setSelectItem(button);
  if(key==13&&dComIfGs_getArrowMax()<30) dComIfGs_setArrowMax(30);
  if(key==14&&dComIfGs_getBombMax()<30) dComIfGs_setBombMax(30);
  if(key==7&&dComIfGs_getMaxMagic()<16) dComIfGs_setMaxMagic(16);
 }
 sRemote=false;
 if(factValue(key)!=value) return PC_PROGRESS_REJECTED;
 auto event=eventValue(PC_NATIVE_FACT_APPLIED,PC_NATIVE_REMOTE);
 event.fact_key=key; event.fact_value=value; push(event);
 return PC_PROGRESS_APPLIED;
}
} // namespace

extern "C" {
int pc_progress_bind_profile(const char* identity) {
 if(!identity) return 0;
 size_t length=std::strlen(identity);
 if(std::strcmp(identity,"personal")) {
  if(length!=64) return 0;
  for(size_t i=0;i<length;++i)
   if(!((identity[i]>='0'&&identity[i]<='9')||(identity[i]>='a'&&identity[i]<='f'))) return 0;
 }
 refresh();
 if(std::strcmp(sIdentity.profile_id,identity)) {
  cancelAwards(); std::memset(sIdentity.profile_id,0,65); std::memcpy(sIdentity.profile_id,identity,length);
  revoke(PC_NATIVE_LIFECYCLE_CHANGED,true);
 }
 return 1;
}
void pc_progress_frame(void) {
 sFrameStarted=true; refresh(); auto* event=g_dComIfG_gameInfo.play.getEvent();
 for(auto& award:sAwards) if(award.evidence.active) {
  finishAward(award);
  if(award.evidence.mustCancel(sIdentity.epoch,dComIfGp_event_runCheck()!=0,event->mEventId,pc_frame_count()))
   cancelAward(award);
 }
}
void pc_progress_copy_identity(PcProgressIdentity* out) {
 if(out) *out=sIdentity;
}
void pc_progress_capture(PcProgressSnapshot* out) {
 if(!out) return; refresh(); *out={}; out->identity=sIdentity;
 out->telescope_owned=telescopeOwned(); out->safe_to_apply=safeToApply()?1:0;
 out->event_overflow=sOverflow?1:0; out->facts_valid=sQuestReady&&!temporaryInventory()?1:0;
 if(out->facts_valid) captureFacts(out->facts);
 for(unsigned i=0;i<PC_PROGRESS_MAX_PENDING_CLAIMS;++i) if(sClaimUsed[i]) {
  ++out->pending_claims;
  if(!sClaims[i].authored_reward_id||sClaims[i].state==PC_PROGRESS_CLAIM_STALE||
     sClaims[i].state==PC_PROGRESS_CLAIM_CANCELLED_EFFECT) out->recovery_required=1;
 }
 if(sClaimOverflow) out->recovery_required=1;
}
int pc_progress_pop_event(PcNativeEvent* out) {
 if(!out) return 0;
 if(sOverflow) { sOverflow=false; return -1; }
 if(!sCount) return 0;
 *out=sEvents[sHead]; sHead=(sHead+1)%kQueueSize; --sCount; return 1;
}
int pc_progress_apply(u32 key,u32 value,u64 expected) { return applyFact(key,value,expected); }
unsigned pc_progress_copy_claims(PcProgressClaim* out,unsigned capacity) {
 if(!out) return 0; unsigned count=0;
 for(unsigned i=0;i<PC_PROGRESS_MAX_PENDING_CLAIMS&&count<capacity;++i)
  if(sClaimUsed[i]) out[count++]=sClaims[i];
 return count;
}
int pc_progress_claim_durable(u64 invocation,u64 authored,u64 expected) {
 refresh(); auto* claim=claimFor(invocation);
 if(!claim||claim->authored_reward_id!=authored||
    !pc::progression::claimCanBeDurable(*claim,sIdentity,expected)) return 0;
 claim->durably_queued=1; return 1;
}
int pc_progress_claim_journaled(u64 invocation,u64 authored,u32 key,u32 operation,u32 value) {
 refresh(); auto* claim=claimFor(invocation);
 if(!claim||!pc::progression::journaledClaimMatches(*claim,sIdentity,invocation,authored,key,operation,value)) return 0;
 claim->durably_queued=1;
 if(claim->state==PC_PROGRESS_CLAIM_STALE||!pc::progression::sameQuest(claim->identity,sIdentity)) dropClaim(invocation);
 return 1;
}
int pc_progress_resolve_claim(u64 invocation,u64 authored,u32 key,u32 value,u64 expected) {
 refresh();
 auto* claim=claimFor(invocation);
 if(!claim||claim->authored_reward_id!=authored||claim->key!=key||claim->state==PC_PROGRESS_CLAIM_ARMED) return PC_PROGRESS_REJECTED;
 if(!pc::progression::sameQuest(claim->identity,sIdentity)) return PC_PROGRESS_STALE;
 int result=applyFact(key,value,expected,true);
 if(result==PC_PROGRESS_APPLIED||result==PC_PROGRESS_ALREADY) dropClaim(invocation);
 return result;
}
int pc_progress_save_allowed(void) {
 if(!pc_network_game_room_mode()) return 1;
 refresh();
 if(sClaimOverflow) return 0;
 for(unsigned i=0;i<PC_PROGRESS_MAX_PENDING_CLAIMS;++i)
  if(sClaimUsed[i]&&(!pc::progression::claimPermitsSave(sClaims[i])||
     !pc::progression::sameQuest(sClaims[i].identity,sIdentity))) return 0;
 return 1;
}
const char* pc_progress_save_block_reason(void) {
 return pc_progress_save_allowed()?"":"Permanent reward has no safe durable local record; no CARD write was queued";
}
void pc_progress_save_refused(void) { push(eventValue(PC_NATIVE_SAVE_PROGRESS_PENDING)); }
void pc_progress_arm_quest_save(int32_t slot) {
 refresh(); sPrepared.armed=true; sPrepared.armedSlot=slot;
 sPrepared.requestReady=false; sPrepared.clientToken=0;
}
void pc_progress_cancel_card_save(void) { sPrepared={}; }
int pc_progress_prepare_card_save(void) {
 const bool armed=sPrepared.armed;
 refresh(); sPrepared.armed=false; sPrepared.requestReady=false; sPrepared.clientToken=0;
 // A file-creation/photo/diagnostic STORE cannot inherit a previous packed
 // quest snapshot. Its accepted/result events remain real but unqualified.
 if(!armed) { sPrepared={}; return 1; }
 const bool matches=sPrepared.active&&sPrepared.epoch==sIdentity.epoch&&
  sPrepared.slot==sPrepared.armedSlot&&sPrepared.slot==sIdentity.quest_slot;
 if(!pc_network_game_room_mode()) { sPrepared.requestReady=matches; return 1; }
 if(!matches||sClaimOverflow) return 0;
 PcProgressClaim copied[PC_PROGRESS_MAX_PENDING_CLAIMS]={};
 const unsigned count=pc_progress_copy_claims(copied,PC_PROGRESS_MAX_PENDING_CLAIMS);
 for(unsigned i=0;i<count;++i)
  if(!pc::progression::claimCanBeDurable(copied[i],sIdentity,sIdentity.epoch)) return 0;
 const PcProgressIdentity preparedIdentity=sIdentity;
 u64 token=0;
 if(!pc_network_game_native_save_prepare(&preparedIdentity,sPrepared.facts,&token)||!token) return 0;
 // This marks local durability only after the complete intent/outbox was
 // persisted. It neither publishes an earned event nor acknowledges a host.
 for(unsigned i=0;i<count;++i)
  if(!pc_progress_claim_durable(copied[i].invocation_id,copied[i].authored_reward_id,preparedIdentity.epoch)) return 0;
 if(!pc_progress_save_allowed()) return 0;
 sPrepared.clientToken=token; sPrepared.requestReady=true; return 1;
}
void pc_progress_prepare_card_read(void) {
 sQualifiedRead={}; std::memset(sPreparedReadHash,0,sizeof(sPreparedReadHash));
 if(pc_network_game_room_mode()) pc_network_game_native_read_prepare(sPreparedReadHash);
 sPreparedReadHash[64]=0;
}
void pc_progress_aryll_begin(u32 producer,int32_t nativeEvent) {
 refresh(); if(sRemote||telescopeOwned()||!sQuestReady||!sPlayerValid||temporaryInventory()||nativeEvent<0) return;
 for(auto& award:sAwards) if(award.evidence.same(producer,nativeEvent,sIdentity.epoch)) return;
 for(auto& award:sAwards) if(!award.evidence.active) {
  award={}; award.aryll=true; award.itemNo=0x20; award.key=1; award.value=1;
  award.operation=PC_PERMANENT_MAX; award.operationValue=1;
  award.evidence.begin(producer,nativeEvent,sIdentity.epoch,++sInvocation);
  auto event=eventValue(PC_NATIVE_AWARD_BEGAN,PC_NATIVE_LOCAL); describe(event,award); push(event); return;
 }
 sOverflow=true;
}
void pc_progress_aryll_end(u32 producer) {
 refresh();
 for(auto& award:sAwards) if(award.aryll&&award.evidence.eventEnded(producer,sIdentity.epoch,pc_frame_count())) finishAward(award);
}
void pc_progress_aryll_cancel(u32 producer) {
 for(auto& award:sAwards) if(award.aryll&&award.evidence.producer==producer) cancelAward(award);
}
void pc_progress_item_presented(u32 actor,u32 item) { refresh(); beginAward(actor,item); }
void pc_progress_chest_item(u32 producer,u32 actor,u32 item,u32 treasure,int32_t room) {
 refresh(); beginAward(actor,item,producer,PC_PERMANENT_PRODUCER_CHEST,treasure,room);
}
void pc_progress_item_message_completed(u32 actor,u32 item) {
 if(!specFor(item)||sRemote) return; refresh();
 if(auto* award=byItem(actor)) award->evidence.messageCompleted(actor,sIdentity.epoch,g_dComIfG_gameInfo.play.getEvent()->mEventId);
}
void pc_progress_item_effect(u32 actor,u32 item) {
 if(!specFor(item)||sRemote) return; refresh();
 if(auto* award=byItem(actor)) { award->evidence.effectApplied(actor,sIdentity.epoch); finishAward(*award); }
}
void pc_progress_native_event_completed(int32_t nativeEvent) {
 refresh();
 for(auto& award:sAwards) if(award.evidence.active&&!award.aryll&&award.evidence.event==nativeEvent) {
  award.evidence.eventEnded(award.evidence.producer,sIdentity.epoch,pc_frame_count()); finishAward(award);
 }
}
void pc_progress_song_begin(u32 player,u32 melody) {
 if(melody>=6) return; refresh();
 if(auto* award=beginAward(player,0x6d+melody)) award->evidence.effectBeforeMessage=true;
}
void pc_progress_song_effect(u32 player,u32 melody) { if(melody<6) pc_progress_item_effect(player,0x6d+melody); }
void pc_progress_song_message_completed(u32 player,u32 melody) { if(melody<6) pc_progress_item_message_completed(player,0x6d+melody); }
u64 pc_progress_script_item_begin(u32 producer,u32 item,u32 authoredStep) {
 refresh(); auto* award=beginAward(producer,item,producer,PC_PERMANENT_PRODUCER_STORY,authoredStep);
 if(!award) return 0;
 award->evidence.messageCompleted(producer,sIdentity.epoch,g_dComIfG_gameInfo.play.getEvent()->mEventId);
 return award->evidence.invocation;
}
void pc_progress_script_item_effect(u64 invocation) {
 if(auto* award=byInvocation(invocation)) pc_progress_item_effect(award->evidence.item,award->itemNo);
}
void pc_progress_quest_initialized(void) {
 refresh(); sQuestReady=true; sIdentity.save_generation=0; sWrite={}; sRead={}; revoke(PC_NATIVE_QUEST_INITIALIZED,true);
}
void pc_progress_quest_loaded(int32_t slot,int32_t success) {
 refresh(); sIdentity.quest_slot=slot; sIdentity.save_generation=0; sWrite={}; sRead={};
 sQuestReady=success!=0;
 revoke(success?PC_NATIVE_QUEST_LOADED:PC_NATIVE_QUEST_LOAD_FAILED,true);
}
void pc_progress_save_prepared(int32_t slot) {
 refresh(); sPrepared={}; sPrepared.active=sQuestReady&&slot>=0&&slot<=2&&slot==sIdentity.quest_slot&&!temporaryInventory();
 sPrepared.slot=slot; sPrepared.epoch=sIdentity.epoch;
 if(sPrepared.active) captureFacts(sPrepared.facts);
}
void pc_progress_card_request(int32_t save) {
 refresh(); auto& tx=save?sWrite:sRead; tx={}; tx.active=true;
 tx.invocation=++sInvocation; tx.identity=sIdentity;
 if(!save) {
  std::memcpy(tx.readHash,sPreparedReadHash,sizeof(tx.readHash));
  std::memset(sPreparedReadHash,0,sizeof(sPreparedReadHash)); sQualifiedRead={};
 }
 tx.prepared=save&&sPrepared.requestReady&&sPrepared.active&&sPrepared.epoch==sIdentity.epoch;
 tx.clientToken=tx.prepared?sPrepared.clientToken:0;
 if(tx.prepared) std::memcpy(tx.facts,sPrepared.facts,sizeof(tx.facts));
 if(save) sPrepared={};
 auto event=eventValue(save?PC_NATIVE_SAVE_REQUESTED:PC_NATIVE_CARD_READ_REQUESTED);
 event.invocation_id=tx.invocation; event.saved_facts_valid=tx.prepared?1:0;
 event.client_save_token=tx.clientToken;
 if(!save) {
  event.source_read_invocation=tx.invocation;
  std::memcpy(event.read_source_sha256,tx.readHash,sizeof(event.read_source_sha256));
 }
 if(tx.prepared) std::memcpy(event.saved_facts,tx.facts,sizeof(tx.facts));
 event.fact_key=tx.prepared?1:0; event.fact_value=tx.facts[1]; push(event);
}
void pc_progress_card_result(int32_t save,int32_t result) {
 if(!result) return; refresh(); auto& tx=save?sWrite:sRead;
 if(!save&&result!=1) sQualifiedRead={};
 if(!tx.active) return;
 bool success=result==1;
 if(!save&&success) {
  sQualifiedRead.invocation=tx.invocation; sQualifiedRead.identity=tx.identity;
  std::memcpy(sQualifiedRead.hash,tx.readHash,sizeof(sQualifiedRead.hash));
 }
 bool confirmed=save&&pc::progression::confirmsActiveQuestSave(tx.prepared,result,tx.identity,sIdentity);
 if(confirmed) ++sIdentity.save_generation;
 auto event=eventValue(save?(success?PC_NATIVE_SAVE_COMMITTED:PC_NATIVE_SAVE_FAILED):
  (success?PC_NATIVE_CARD_READ_COMPLETED:PC_NATIVE_CARD_READ_FAILED));
 event.identity=tx.identity; event.identity.save_generation=sIdentity.save_generation;
 event.invocation_id=tx.invocation;
 event.client_save_token=tx.clientToken;
 if(!save) {
  event.source_read_invocation=tx.invocation;
  std::memcpy(event.read_source_sha256,tx.readHash,sizeof(event.read_source_sha256));
 }
 // Disk success alone does not authorize an active-quest baseline: the exact
 // prepared transaction must still match this quest and advance its generation.
 event.saved_facts_valid=tx.prepared&&(!save||!success||confirmed)?1:0;
 if(tx.prepared) std::memcpy(event.saved_facts,tx.facts,sizeof(tx.facts));
 event.fact_key=tx.prepared?1:0; event.fact_value=tx.facts[1]; push(event); tx={};
}
} // extern "C"
