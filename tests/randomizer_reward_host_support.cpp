// SPDX-License-Identifier: GPL-3.0-or-later
// Deliberately authored fixtures. No real Windows module/native game is loaded.
#include "randomizer_reward_host_support.h"
#include "autosave.h"
extern "C" {
#include "card_runtime.h"
}
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <numeric>

struct BwIcLoadedCode {
  const std::thread::id owner=std::this_thread::get_id();
  const StaticRecompModuleDesc* descriptor=nullptr;
  std::uint64_t generation=0;
  bool alive=true;
  std::array<std::uint8_t,32> digest{};
};
namespace fixture {
Environment* environment=nullptr;
unsigned checks=0;
void require(bool ok,const char* expression,int line){
  ++checks;
  if(!ok){std::fprintf(stderr,"reward synthetic fixture line%d: %s\n",line,expression);std::abort();}
}
void put16(std::uint8_t* p,std::uint16_t n){p[0]=std::uint8_t(n>>8);p[1]=std::uint8_t(n);}
void put32(std::uint8_t* p,std::uint32_t n){for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(n>>(24-8*i));}
void put64(std::uint8_t* p,std::uint64_t n){for(unsigned i=0;i<8;++i)p[i]=std::uint8_t(n>>(56-8*i));}
std::string hash(const Bytes& b){return bluewake::randomizer::seed::sha256(std::string(b.begin(),b.end()));}
Bytes read_file(const std::filesystem::path& p){
  std::ifstream f(p,std::ios::binary);CHECK(bool(f));Bytes out(std::istreambuf_iterator<char>(f),{});CHECK(!f.bad());return out;
}
void write_file(const std::filesystem::path& p,const Bytes& b){
  std::ofstream f(p,std::ios::binary|std::ios::trunc);CHECK(bool(f));
  f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));f.close();CHECK(!f.fail());
}
bluewake::randomizer::seed::Profile profile(bool picto){
  namespace r=bluewake::randomizer;
  r::seed::Request request;request.seed=7;
  request.logic_contract_digest=r::seed::sha256("synthetic logic identity - not beatability");
  request.start_policy_digest=r::seed::sha256("synthetic start metadata - not native authority");
  request.options={{"logic_obscurity",std::string("None")},{"logic_precision",std::string("None")},
    {"required_bosses",false},{"skip_rematch_bosses",false},{"sword_mode",std::string("No Starting Sword")}};
  request.placements={{r::seed::linkug_location_id(),picto?r::seed::basic_picto_id():r::seed::orange_rupee_id()}};
  auto result=r::seed::ProfileBuilder(r::imported_catalog()).build(request);CHECK(bool(result));return *result.value;
}
static std::uint32_t fnv(const std::uint8_t* p,std::size_t size){
  std::uint32_t n=0x811C9DC5u;for(std::size_t i=0;i<size;++i)n=(n^p[i])*0x01000193u;return n;
}
static void quest_checksum(std::uint8_t* p){
  const auto sum=std::accumulate(p,p+0x768,std::uint32_t(0));
  put64(p+0x768,(std::uint64_t(sum)<<32)|std::uint32_t(0u-sum-0x768u));
}
std::array<std::uint8_t,0x1650> game(bool awarded,bool picto){
  std::array<std::uint8_t,0x1650> out{};
  for(unsigned quest=0;quest<3;++quest){
    auto* q=out.data()+quest*0x770;
    // Other records are stable authored sentinels, not copied game data.
    if(quest)for(unsigned i=0;i<0x768;++i)q[i]=std::uint8_t(i*13+quest*17);
    else {q[0x44]=awarded&&picto?0x23:0xFF;q[0x59]=awarded&&picto?1:0;put32(q+0x500,awarded?0x20:0);}
    quest_checksum(q);
  }
  return out;
}
Bytes card(const std::array<std::uint8_t,0x1650>& input,std::uint32_t save_count,
           bool other_quest,bool photo,bool other_file){
  auto packed=input;
  if(other_quest){packed[0x770+0x22]^=0x80;quest_checksum(packed.data()+0x770);}
  Bytes payload(0x18000,0);
  for(std::size_t i=0x6000;i<payload.size();++i)payload[i]=std::uint8_t(i*7+3);
  if(photo)payload[0x6123]^=0x40;
  Bytes block(0x2000,0);put32(block.data(),save_count);put32(block.data()+4,0);
  std::copy(packed.begin(),packed.end(),block.begin()+8);
  std::uint32_t sum=0;for(unsigned i=0;i<0x1FFC;i+=2)sum+=unsigned(block[i])*256u+block[i+1];
  put32(block.data()+0x1FFC,(std::uint32_t(std::uint16_t(sum))<<16)|std::uint16_t(0u-sum-0xFFEu));
  std::copy(block.begin(),block.end(),payload.begin()+0x2000);
  std::copy(block.begin(),block.end(),payload.begin()+0x4000);
  Bytes out(40,0);std::memcpy(out.data(),"DOLCARD1",8);put32(out.data()+8,1);put16(out.data()+12,4);
  put32(out.data()+16,0x2000);put64(out.data()+20,0x0123456789ABCDEFull);put32(out.data()+28,2);
  auto append=[&](std::uint16_t id,const char* name,const char* game_id,const char* company,const Bytes& data,bool native){
    const auto start=out.size();out.resize(start+68+data.size(),0);auto* p=out.data()+start;
    put16(p,id);put32(p+4,std::uint32_t(data.size()));put32(p+8,0xABCDEF01u);
    std::memcpy(p+12,name,std::strlen(name));std::memcpy(p+44,game_id,4);std::memcpy(p+48,company,2);
    p[50]=1;p[51]=4;put16(p+56,native?1:0);put16(p+58,3);put32(p+60,native?0x1C00:0);
    put32(p+64,fnv(data.data(),data.size()));std::copy(data.begin(),data.end(),out.begin()+start+68);
  };
  append(7,"gczelda","GZLE","01",payload,true);
  Bytes foreign(19,0x6B);if(other_file)foreign[7]^=0x11;
  append(8,"synthetic-other","ABCD","ZZ",foreign,false);
  put32(out.data()+32,std::uint32_t(out.size()-40));put32(out.data()+36,fnv(out.data()+40,out.size()-40));return out;
}
BwIcLoadedCode* make_code(const StaticRecompModuleDesc* descriptor,std::uint64_t generation){
  auto* code=new BwIcLoadedCode;code->descriptor=descriptor;code->generation=generation;
  const auto digest=bluewake::randomizer::seed::sha256("fixture-owned admitted artifact - not a game policy");
  for(unsigned i=0;i<32;++i)code->digest[i]=std::uint8_t(std::strtoul(digest.substr(i*2,2).c_str(),nullptr,16));return code;
}
void code_live(BwIcLoadedCode* code,bool live){CHECK(code);code->alive=live;}
void destroy_code(BwIcLoadedCode* code){delete code;}
void emit(BwGameEventKind kind,BwGameResetReason reason){
  CHECK(environment);BwGameEvent event{};event.kind=kind;event.reset_reason=reason;
  if(kind==BW_GAME_EVENT_RESET)++environment->stats.epoch;
  event.epoch=environment->stats.epoch;
  const auto subscribers=environment->subscribers;
  for(const auto& sub:subscribers)if(sub.id&&(sub.mask&BW_GAME_EVENT_MASK(kind)))sub.callback(&event,sub.user);
}
int forbidden_dispatch(CPUState*,u32){++environment->descriptor_dispatches;CHECK(false);return 0;}
} // namespace fixture

extern "C" bool bw_ic_code_is_live(const BwIcLoadedCode* code,std::uint64_t generation,const StaticRecompModuleDesc* descriptor) noexcept {
  if(fixture::environment)++fixture::environment->code_queries;
  return code&&code->alive&&code->owner==std::this_thread::get_id()&&code->generation==generation&&code->descriptor==descriptor;
}
extern "C" bool bw_ic_code_artifact_sha256(const BwIcLoadedCode* code,std::uint64_t generation,const StaticRecompModuleDesc* descriptor,std::uint8_t out[32]) noexcept {
  if(out)std::memset(out,0,32);if(!out||!bw_ic_code_is_live(code,generation,descriptor))return false;
  std::copy(code->digest.begin(),code->digest.end(),out);return true;
}
extern "C" bool bluewake_autosave_active(void){return fixture::environment&&fixture::environment->autosave;}
extern "C" const char* bluewake_card_runtime_path(void){
  return fixture::environment&&!fixture::environment->mounted.empty()?fixture::environment->mounted.c_str():nullptr;
}
extern "C" bool bluewake_card_runtime_begin_snapshot(const char* expected){
  auto* e=fixture::environment;if(!e||!e->lock_allowed||e->lock_held||!expected||e->mounted!=expected)return false;
  ++e->locks;e->lock_held=true;return true;
}
extern "C" void bluewake_card_runtime_end_snapshot(void){
  auto* e=fixture::environment;CHECK(e&&e->lock_held);e->lock_held=false;++e->unlocks;
}
extern "C" BwGameEventSubscription bluewake_game_events_subscribe(std::uint64_t mask,BwGameEventCallback callback,void* user){
  auto* e=fixture::environment;if(!e||!mask||!callback||e->subscribers.size()>=16)return 0;
  const auto id=e->next_subscription++;e->subscribers.push_back({id,mask,callback,user});return id;
}
extern "C" bool bluewake_game_events_unsubscribe(BwGameEventSubscription id){
  auto* e=fixture::environment;if(!e)return false;
  const auto found=std::find_if(e->subscribers.begin(),e->subscribers.end(),[&](const auto& s){return s.id==id;});
  if(found==e->subscribers.end())return false;e->subscribers.erase(found);return true;
}
extern "C" void bluewake_game_events_stats(BwGameEventStats* out){if(out)*out=fixture::environment?fixture::environment->stats:BwGameEventStats{};}
