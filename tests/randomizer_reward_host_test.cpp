// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic host boundary fixtures only. NO native module/function is run.
#include "randomizer_reward_host_support.h"
#include "randomizer_card_payload.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <crtdbg.h>
#endif
namespace fs=std::filesystem;
using fixture::Bytes;
using fixture::put16;using fixture::put32;using fixture::put64;
namespace payload=bluewake::randomizer::card_payload;

namespace {
constexpr std::uint32_t Base=0x80000000,Info=0x803C4C08,Ctrl=0x803B39A0,Thread=0x803A2960;
constexpr std::uint32_t Link=0x80600000,Chest=0x80700000,Item=0x80702000,Menu=0x80710000;
constexpr std::uint32_t Name=0x80510000,Selector=0x80520000,Stag=0x80530000;
constexpr std::uint32_t ChestRaw=0x80010000,ItemRaw=0x80020000,ChestLoader=0x80008000,ItemLoader=0x80008100;
constexpr std::uint32_t Stack=0x81700000,Queue=0x80372028;
constexpr std::uint64_t CodeGeneration=19;
struct World {
  fixture::Environment env;
  Bytes ram=Bytes(0x2000000,0);
  CPUState cpu{};
  std::array<std::uint8_t,0x364> chest_data{};
  std::array<std::uint8_t,0x28C> item_data{};
  std::array<StaticRecompRelSection,21> chest_sections{};
  std::array<StaticRecompRelSection,20> item_sections{};
  std::array<StaticRecompRelModule,2> modules{};
  StaticRecompModuleDesc descriptor{};
  BwIcLoadedCode* code=nullptr;
  BwRandomizerRewardHost* host=nullptr;
  BwRewardHostStartup startup{};
  std::array<BwRewardHostBacking,2> backing{};
  std::array<BwRewardHostRelSlot,2> slots{};
  std::array<BwRandomizerRelAlias,2> aliases{};
  fs::path directory;
  std::string directory_utf8,profile_encoded,path;
  bool picto;
  Bytes baseline;
  std::array<std::uint8_t,0x1650> baseline_game;
  World(const fs::path& dir,bool want_picto=true):directory(dir),picto(want_picto),baseline(fixture::card(fixture::game(false,want_picto))),baseline_game(fixture::game(false,want_picto)){
    CHECK(!fixture::environment);fixture::environment=&env;env.stats.epoch=1;
    ppc_guest_alias_clear();cpu.ram=ram.data();cpu.ram_size=std::uint32_t(ram.size());
    cpu.downcount=12345;cpu.cycle_budget=67890;cpu.gpr[1]=Stack;
    setup_sections(chest_sections,113);setup_sections(item_sections,131);
    modules[0]={113,3,21,0x4C,0x6000,chest_sections.data(),21};
    modules[1]={131,3,20,0x4C,0x6000,item_sections.data(),20};
    descriptor.abi_version=STATICRECOMP_ABI_VERSION;descriptor.cpu_abi_version=GXRUNTIME_CPU_ABI_VERSION;
    descriptor.cpu_state_size=sizeof(CPUState);std::memcpy(descriptor.game_id,"GZLE01",7);
    descriptor.dispatch=fixture::forbidden_dispatch;descriptor.rel_modules=modules.data();descriptor.num_rel_modules=2;
    code=fixture::make_code(&descriptor,CodeGeneration);
    setup_fixed(true);setup_fixed(false);
    backing={BwRewardHostBacking{0xC1DF3CF0,0x364,chest_data.data()},BwRewardHostBacking{0xC07710B8,0x28C,item_data.data()}};
    CHECK(ppc_guest_alias_add_shared(backing[0].linked_start,backing[0].size,backing[0].storage));
    CHECK(ppc_guest_alias_add_shared(backing[1].linked_start,backing[1].size,backing[1].storage));
    setup_raw(true);setup_raw(false);
    put32(at(0x800030C8),ChestRaw);put32(at(0x800030CC),ItemRaw);
    put32(at(ChestRaw+4),ItemRaw);put32(at(ItemRaw+8),ChestRaw);
    slots={BwRewardHostRelSlot{ChestLoader,ChestRaw,0x6000},BwRewardHostRelSlot{ItemLoader,ItemRaw,0x6000}};
    aliases={BwRandomizerRelAlias{ChestRaw+0x100,ChestRaw+0x100+0x3A88,0xC1DF00F4,0x3A88},
             BwRandomizerRelAlias{ItemRaw+0x100,ItemRaw+0x100+0xF98,0xC07700EC,0xF98}};
    setup_actor(Link,101,0xA9,0x8038FD8C,0x8038FD68,0x4C28,2,1);
    setup_actor(Chest,102,0x126,0xC1DF3E4C,0xC1DF3E2C,0x770,2,0);
    setup_actor(Item,103,0x103,0xC07711D8,0xC07711B8,0x65C,2,0);
    for(unsigned i=0;i<3;++i){const std::uint32_t a[]={Link,Chest,Item};auto* tag=at(a[i]+0xC4);
      put32(tag,i?a[i-1]+0xC4:0);put32(tag+4,Queue);put32(tag+8,i<2?a[i+1]+0xC4:0);put32(tag+12,a[i]);tag[16]=1;}
    put32(at(Queue),Link+0xC4);put32(at(Queue+4),Item+0xC4);put32(at(Queue+8),3);
    put32(at(0x803F6A18),0x09130001);put32(at(0x803F69D0),0x09130002);
    put32(at(Chest+0xB0),0xFF000280);put16(at(Chest+0x1E0),0x06FF);
    at(Item+0x63A)[0]=picto?0x23:0x06;
    put32(at(0x803CA74C),Link);put32(at(0x803C9DA0),Stag);at(Stag+9)[0]=22;
    std::memcpy(at(0x803C9D3C),"LinkUG",7);at(0x803C9D3C+10)[0]=0;
    at(Info+0x44)[0]=0xFF;
    put32(at(0x800000E4),Thread);put32(at(0x800000D4),Thread);
    put16(at(Thread+0x2C8),2);put32(at(Thread+0x304),0x81710000);put32(at(Thread+0x308),0x81600000);
    setup_name();put32(at(Menu),0x803919A4);
    const auto p=fixture::profile(picto);profile_encoded=p.encode();directory_utf8=dir.u8string();
    startup.dedicated_seed_directory_utf8=directory_utf8.c_str();
    startup.canonical_profile=reinterpret_cast<const std::uint8_t*>(profile_encoded.data());startup.canonical_profile_size=profile_encoded.size();
    startup.quest=0;startup.explicit_initial_card=baseline.data();startup.explicit_initial_card_size=baseline.size();
    startup.admitted_code=code;startup.code_generation=CodeGeneration;startup.admitted_descriptor=&descriptor;
    const auto origin=fixture::hash(baseline);
    for(unsigned i=0;i<32;++i)startup.origin_card_sha256[i]=std::uint8_t(std::strtoul(origin.substr(i*2,2).c_str(),nullptr,16));
  }
  ~World(){
    if(host){
      env.autosave=false;
      if(status().drain_native_save)bw_randomizer_reward_host_abandon_unavailable(host); // Explicit unresolved synthetic fatal teardown.
      else CHECK(bw_randomizer_reward_host_stop(host));
      env.mounted.clear();CHECK(bw_randomizer_reward_host_destroy(host));host=nullptr;
    }
    CHECK(env.locks==env.unlocks&&!env.lock_held&&env.subscribers.empty()&&env.descriptor_dispatches==0);
    fixture::destroy_code(code);ppc_guest_alias_clear();fixture::environment=nullptr;
  }
  std::uint8_t* at(std::uint32_t address){CHECK(address>=Base&&address-Base<ram.size());return ram.data()+(address-Base);}
  template<std::size_t N> void setup_sections(std::array<StaticRecompRelSection,N>& out,unsigned id){
    const std::uint32_t c_start[]={0,0xC1DF00F4,0xC1DF3B7C,0xC1DF3B84,0xC1DF3B90,0xC1DF3CF0,0xC1DF59D8};
    const std::uint32_t c_size[]={0,0x3A88,8,8,0x15C,0x364,0x24};
    const std::uint32_t i_start[]={0,0xC07700EC,0xC0771084,0xC0771088,0xC0771090,0xC07710B8,0xC0771AC8};
    const std::uint32_t i_size[]={0,0xF98,4,8,0x24,0x28C,0xA0};
    for(unsigned j=0;j<N;++j)out[j]={id,j,j<7?(id==113?c_start[j]:i_start[j]):0,j<7?(id==113?c_size[j]:i_size[j]):0};
  }
  void setup_fixed(bool chest){
    auto* data=chest?chest_data.data():item_data.data();const auto start=chest?0xC1DF3CF0u:0xC07710B8u;
    const auto profile=chest?0xC1DF3E4Cu:0xC07711D8u,methods=chest?0xC1DF3E2Cu:0xC07711B8u;
    auto* p=data+(profile-start);put32(p,0xFFFFFFFD);put16(p+4,7);put16(p+6,0xFFFD);put16(p+8,chest?0x126:0x103);
    put32(p+12,0x803726E8);put32(p+16,chest?0x770:0x65C);put32(p+28,0x80371FF8);
    put16(p+32,chest?0x113:0xFC);put32(p+36,methods);put32(p+40,0x44000);p[45]=chest?14:0;
    const std::uint32_t c[]={0xC1DF3164,0xC1DF30CC,0xC1DF30A4,0xC1DF30C4,0xC1DF2CE4};
    const std::uint32_t i[]={0xC0770A80,0xC07709E0,0xC0770EA0,0xC0770E80,0xC0770F44};
    for(unsigned j=0;j<5;++j)put32(data+(methods-start)+j*4,chest?c[j]:i[j]);
  }
  void setup_raw(bool chest){
    const auto raw=chest?ChestRaw:ItemRaw,loader=chest?ChestLoader:ItemLoader;
    put32(at(raw),chest?113:131);put32(at(raw+12),chest?21:20);put32(at(raw+16),raw+0x4C);put32(at(raw+28),3);
    put32(at(raw+0x4C+8),(raw+0x100)|1);put32(at(raw+0x4C+12),chest?0x3A88:0xF98);
    put32(at(raw+0x4C+40),raw+0x5000);put32(at(raw+0x4C+44),chest?0x364:0x28C);put32(at(loader+16),raw);
  }
  void setup_actor(std::uint32_t a,std::uint32_t pid,std::uint16_t process,std::uint32_t profile,std::uint32_t methods,
                   std::uint32_t size,std::uint8_t init,std::uint8_t group){
    auto* p=at(a);put32(p,0x09130001);put32(p+4,pid);put16(p+8,process);put16(p+14,process);p[12]=init;p[13]=2;
    put32(p+16,profile);put32(p+0xC0,0x09130002);put32(p+0xEC,methods);p[0x1BE]=group;p[0x20A]=0;
    if(a==Link){auto* pr=at(profile);put16(pr+8,process);put32(pr+16,size);put32(pr+36,methods);pr[44]=group;}
  }
  void setup_name(){
    auto* n=at(Name);put32(n,0x09130001);put32(n+4,104);put16(n+8,12);put16(n+14,12);n[12]=2;n[13]=2;
    put32(n+16,0x80394590);put32(n+0x40,Name);n[0x44]=1;put32(n+0x48,0);put32(n+0x38,0x803BCD60);
    put32(at(0x803F6180),0x803BCD60);put32(at(0x803F6184),16);
    put32(at(0x803BCD60),Name+0x34);put32(at(0x803BCD64),Name+0x34);put32(at(0x803BCD68),1);
    put32(n+0x428,Selector);put32(at(Selector+0x3938),Name+0x560);at(Selector+0x392C)[0]=1;
  }
  BwRewardHostStatus status(){BwRewardHostStatus out{};bw_randomizer_reward_host_status(host,&out);return out;}
  void create(){CHECK(!host);host=bw_randomizer_reward_host_create(&startup);CHECK(host);CHECK(status().confirmed_generation==1);}
  void attach(){
    char out[4096];CHECK(bw_randomizer_reward_host_card_path(host,out,sizeof out));path=out;env.mounted=path;
    CHECK(bw_randomizer_reward_host_backend_opened(host));CHECK(bw_randomizer_reward_host_cpu_ready(host,&cpu,backing.data(),backing.size()));
    for(unsigned i=0;i<2;++i){CHECK(bw_randomizer_reward_host_rel_materialized(host,i,slots[i].address,slots[i].capacity));
      CHECK(bw_randomizer_reward_host_rel_loader_associated(host,i,slots[i].owner));}
    tables();
  }
  void tables(){CHECK(bw_randomizer_reward_host_rel_tables(host,aliases.data(),aliases.size(),slots.data(),slots.size()));}
  // Synthetic register/PC setup is fixture input, never an adapter mutation.
  void entry(std::uint32_t pc,std::uint32_t lr,std::uint32_t sp=Stack){cpu.pc=pc;cpu.lr=lr;cpu.gpr[1]=sp;}
  bool dispatch(std::uint32_t pc,bool permitted_r4=false){
    cpu.pc=pc;std::array<std::uint8_t,sizeof(CPUState)> before{};std::memcpy(before.data(),&cpu,sizeof cpu);
    const auto before_ram=ram;const auto before_c=chest_data;const auto before_i=item_data;
    CHECK(bw_randomizer_reward_host_observes(host,&cpu,pc));const bool result=bw_randomizer_reward_host_dispatch(host,&cpu,pc);
    CPUState expected;std::memcpy(&expected,before.data(),sizeof expected);
    if(permitted_r4&&status().substitutions)expected.gpr[4]=picto?0x23:0x06;
    CHECK(std::memcmp(&cpu,&expected,sizeof cpu)==0);CHECK(ram==before_ram&&chest_data==before_c&&item_data==before_i);
    return result;
  }
  void load(bool correlated=true){
    std::copy(baseline_game.begin(),baseline_game.end(),at(Ctrl));
    std::copy(baseline_game.begin(),baseline_game.end(),at(Name+0x560));
    put64(at(Ctrl+0x1688),0x0123456789ABCDEF);
    entry(0x80019288,0x80230CA4);cpu.gpr[31]=Name;cpu.gpr[3]=Ctrl;cpu.gpr[4]=Name+0x560;cpu.gpr[5]=0x1650;cpu.gpr[6]=0;
    CHECK(dispatch(0x80019288));cpu.gpr[3]=1;CHECK(dispatch(0x80230CA4));
    entry(0x8005EA24,0x80231B08);cpu.gpr[22]=Name;cpu.gpr[3]=Info;cpu.gpr[4]=Name+0x560;cpu.gpr[5]=0;
    CHECK(dispatch(0x8005EA24));
    // Authored model of the native copy. No guest handler executes here.
    at(Info+0x44)[0]=baseline_game[0x44];at(Info+0x59)[0]=baseline_game[0x59];
    std::copy_n(baseline_game.data()+0x500,4,at(0x803C5114));
    cpu.gpr[3]=0;CHECK(dispatch(0x80231B08));CHECK(!status().native_load_authorized);
    if(!correlated){CHECK(!bw_randomizer_reward_host_maintenance(host));CHECK(status().stop_required&&!status().drain_native_save);return;}
    fixture::emit(BW_GAME_EVENT_RESET,BW_GAME_RESET_GAME_LOAD);CHECK(bw_randomizer_reward_host_maintenance(host));
    CHECK(status().native_load_authorized&&status().native_card_load&&status().phase==BW_REWARD_HOST_READY);
  }
  void start_creation(){
    entry(0xC00261E8,0xC1DF2780);cpu.gpr[30]=Chest;cpu.gpr[3]=Chest+0x1F8;cpu.gpr[4]=6;
    cpu.gpr[5]=cpu.gpr[6]=UINT32_MAX;cpu.gpr[7]=cpu.gpr[8]=0;
    CHECK(dispatch(0xC00261E8,true));CHECK(cpu.gpr[4]==(picto?0x23u:6u));CHECK(status().substitutions==1);
  }
  void created(){cpu.gpr[3]=103;CHECK(dispatch(0xC1DF2780));CHECK(status().phase==BW_REWARD_HOST_AWAITING_DELETE);}
  void start_award(){
    at(Item+12)[0]=3;entry(0xC00C2DFC,0xC0770A08,Stack-0x100);cpu.gpr[31]=Item;cpu.gpr[3]=picto?0x23:6;
    CHECK(dispatch(0xC00C2DFC));CHECK(status().phase==BW_REWARD_HOST_AWARDING);
  }
  void completed_award(){
    if(picto){at(Info+0x44)[0]=0x23;at(Info+0x59)[0]=1;}else put32(at(0x803CA768),100);
    cpu.gpr[3]=0xDEADBEEF;CHECK(dispatch(0xC0770A08));CHECK(status().completed_awards==1&&status().phase==BW_REWARD_HOST_UNSAVED);
  }
  void earn(){start_creation();created();start_award();completed_award();}
  void serializer(){
    // Authored ordinary meter consumption, never performed by adapter.
    if(!picto)put32(at(0x803CA768),0);
    entry(0x8005E780,0x801D8994);cpu.gpr[30]=Menu;cpu.gpr[3]=Info;cpu.gpr[4]=Menu+0x554;cpu.gpr[5]=0;
    CHECK(dispatch(0x8005E780));CHECK(status().drain_native_save&&status().hold_guest_mutators);
  }
  void serialized(bool success=true){cpu.gpr[3]=success?0:UINT32_MAX;CHECK(dispatch(0x801D8994));}
  void store(const std::array<std::uint8_t,0x1650>& captured){
    std::copy(captured.begin(),captured.end(),at(Menu+0x554));
    entry(0x800191C4,0x801D89F0);cpu.gpr[30]=Menu;cpu.gpr[3]=Ctrl;cpu.gpr[4]=Menu+0x554;cpu.gpr[5]=0x1650;cpu.gpr[6]=0;
    CHECK(dispatch(0x800191C4));
    std::copy(captured.begin(),captured.end(),at(Ctrl));cpu.gpr[3]=0xF00DFACE;CHECK(dispatch(0x801D89F0));
  }
  void poll(bool immediate){
    const auto lr=immediate?0x801D89FCu:0x801D8A6Cu;entry(0x8001931C,lr,immediate?Stack:Stack-0x200);
    cpu.gpr[immediate?30:31]=Menu;cpu.gpr[3]=Ctrl;CHECK(dispatch(0x8001931C));
  }
  bool polled(unsigned result,bool immediate){put32(at(Ctrl+0x1660),result==1?1:0);cpu.gpr[3]=result;
    return dispatch(immediate?0x801D89FC:0x801D8A6C);}
  void manual_success(const std::array<std::uint8_t,0x1650>& captured,const Bytes& copied_card){
    serializer();serialized();store(captured);poll(true);CHECK(polled(0,true));CHECK(status().drain_native_save);
    poll(false);fixture::write_file(fs::u8path(path),copied_card);CHECK(polled(1,false));
    CHECK(!status().drain_native_save&&status().hold_guest_mutators&&status().phase==BW_REWARD_HOST_CHECKPOINT_PENDING);
  }
  void close(){
    CHECK(bw_randomizer_reward_host_stop(host));env.mounted.clear();CHECK(bw_randomizer_reward_host_destroy(host));host=nullptr;
  }
};

void startup_cases(const fs::path& root){
  {
    World w(root/"missing-lease");fixture::code_live(w.code,false);
    CHECK(!bw_randomizer_reward_host_create(&w.startup));CHECK(!fs::exists(w.directory));
  }
  {
    World w(root/"wrong-profile");w.profile_encoded.push_back('x');w.startup.canonical_profile=reinterpret_cast<const std::uint8_t*>(w.profile_encoded.data());
    w.startup.canonical_profile_size=w.profile_encoded.size();CHECK(!bw_randomizer_reward_host_create(&w.startup));CHECK(!fs::exists(w.directory));
  }
  {
    World w(root/"wrong-quest");w.startup.quest=3;CHECK(!bw_randomizer_reward_host_create(&w.startup));CHECK(!fs::exists(w.directory));
  }
  {
    World w(root/"invalid-native-input");w.baseline[0]^=1;const auto sum=fixture::hash(w.baseline);
    for(unsigned i=0;i<32;++i)w.startup.origin_card_sha256[i]=std::uint8_t(std::strtoul(sum.substr(i*2,2).c_str(),nullptr,16));
    CHECK(!bw_randomizer_reward_host_create(&w.startup));CHECK(!fs::exists(w.directory));
  }
  {
    World w(root/"already-mounted");w.env.mounted="synthetic other CARD";CHECK(!bw_randomizer_reward_host_create(&w.startup));CHECK(!fs::exists(w.directory));
  }
  {
    World w(root/"absent-initial");w.startup.explicit_initial_card=nullptr;w.startup.explicit_initial_card_size=0;
    CHECK(!bw_randomizer_reward_host_create(&w.startup));CHECK(!fs::exists(w.directory/"current.bwseed"));
  }
  {
    World w(root/"wrong-backend");w.create();w.env.mounted="synthetic wrong mounted path";
    CHECK(!bw_randomizer_reward_host_backend_opened(w.host));CHECK(w.status().stop_required&&w.status().confirmed_generation==1);
  }
  {
    World w(root/"thread-and-dead-cpu");w.create();const auto prior=w.status();bool rejected=false;
    const auto queries=w.env.code_queries;
    std::thread other([&]{BwRewardHostStatus out{};bw_randomizer_reward_host_status(w.host,&out);
      char name[16];rejected=!bw_randomizer_reward_host_card_path(w.host,name,sizeof name)&&!name[0]&&
        !bw_randomizer_reward_host_cpu_ready(w.host,reinterpret_cast<CPUState*>(1),nullptr,0)&&
        !bw_randomizer_reward_host_stop(w.host)&&!bw_randomizer_reward_host_destroy(w.host)&&out.confirmed_generation==0;});other.join();
    CHECK(rejected&&w.env.code_queries==queries&&w.status().confirmed_generation==prior.confirmed_generation);
    fixture::code_live(w.code,false);CHECK(!bw_randomizer_reward_host_cpu_ready(w.host,reinterpret_cast<CPUState*>(1),nullptr,0));
    CHECK(w.status().confirmed_generation==1);fixture::code_live(w.code,true);
  }
}

void load_cases(const fs::path& root){
  {World w(root/"normal-load");w.create();w.attach();CHECK(!w.status().native_load_authorized);w.load();w.close();}
  {World w(root/"missing-load-reset");w.create();w.attach();w.load(false);CHECK(w.status().stop_required&&!w.status().native_load_authorized);}
  {World w(root/"reset-without-load");w.create();w.attach();fixture::emit(BW_GAME_EVENT_RESET);CHECK(w.status().stop_required&&!w.status().native_load_authorized);}
  {World w(root/"wrong-context-load");w.create();w.attach();w.entry(0x8005EA24,0x80231B08);w.cpu.gpr[22]=Name;w.cpu.gpr[3]=Info;w.cpu.gpr[4]=Name+0x560;w.cpu.gpr[5]=0;
    CHECK(!w.dispatch(0x8005EA24));CHECK(w.status().stop_required&&!w.status().native_load_authorized);}
  {World w(root/"raw-alias-before-load");w.create();w.attach();std::uint8_t shadow=0;
    CHECK(ppc_guest_alias_add_shared(Info+0x44,1,&shadow));CHECK(!bw_randomizer_reward_host_maintenance(w.host)||!w.status().native_load_authorized);
    CHECK(!bw_randomizer_reward_host_dispatch(w.host,&w.cpu,0x80019288));CHECK(w.status().stop_required&&!w.status().native_load_authorized);}
}

void award_cases(const fs::path& root){
  for(unsigned active_bit:{0u,0x20u}){
    World w(root/("creation-active-bit-"+std::to_string(active_bit)));w.create();w.attach();w.load();
    put32(w.at(0x803C5380),active_bit);w.start_creation();CHECK(w.status().substitutions==1); // Neither value is award authority.
  }
  for(bool picto:{true,false}){
    World w(root/(picto?"picto-replay":"orange-replay"),picto);w.create();w.attach();w.load();w.start_creation();
    CHECK(w.dispatch(0xC00261E8,true));CHECK(w.status().substitutions==1);w.created();
    CHECK(!bw_randomizer_reward_host_observes(w.host,&w.cpu,0xC1DF2780));
    CHECK(bw_randomizer_reward_host_dispatch(w.host,&w.cpu,0xC1DF2780));CHECK(w.status().completed_awards==0);
    w.start_award();CHECK(w.dispatch(0xC00C2DFC));w.completed_award();
    CHECK(!bw_randomizer_reward_host_observes(w.host,&w.cpu,0xC0770A08));CHECK(bw_randomizer_reward_host_dispatch(w.host,&w.cpu,0xC0770A08));
    CHECK(w.status().substitutions==1&&w.status().completed_awards==1);w.close();
  }
  for(unsigned mode=0;mode<7;++mode){
    World w(root/("creation-negative-"+std::to_string(mode)));w.create();w.attach();w.load();w.start_creation();
    if(mode==0)w.cpu.gpr[1]-=16;
    if(mode==1)put32(w.at(Chest+4),999);
    if(mode==2)put32(w.at(Chest+0xB0),0xFF000300);
    if(mode==3)w.cpu.gpr[4]=0x123;
    if(mode==4){w.aliases[0].raw_end--;w.tables();}
    if(mode==5)put32(w.at(ChestLoader+16),ItemRaw);
    if(mode==6)put32(w.at(0x803F6160),1);
    CHECK(!w.dispatch(0xC00261E8));CHECK(w.status().stop_required&&w.status().substitutions==1&&w.status().completed_awards==0);
    CHECK(!bw_randomizer_reward_host_maintenance(w.host));
  }
  for(std::uint32_t pid:{0u,UINT32_MAX-1u,UINT32_MAX}){
    World w(root/("creation-return-invalid-pid-"+std::to_string(pid)));w.create();w.attach();w.load();w.start_creation();
    w.cpu.gpr[3]=pid;CHECK(!w.dispatch(0xC1DF2780));CHECK(w.status().stop_required&&w.status().completed_awards==0);
  }
  {
    World w(root/"creation-first-upper-bits");w.create();w.attach();w.load();
    w.entry(0xC00261E8,0xC1DF2780);w.cpu.gpr[30]=Chest;w.cpu.gpr[3]=Chest+0x1F8;w.cpu.gpr[4]=0x10006;
    w.cpu.gpr[5]=w.cpu.gpr[6]=UINT32_MAX;w.cpu.gpr[7]=w.cpu.gpr[8]=0;
    CHECK(!w.dispatch(0xC00261E8));CHECK(w.status().substitutions==0&&w.status().stop_required);
  }
  for(unsigned mode=0;mode<6;++mode){
    World w(root/("award-negative-"+std::to_string(mode)));w.create();w.attach();w.load();w.start_creation();w.created();
    w.at(Item+12)[0]=3;w.entry(0xC00C2DFC,0xC0770A08,Stack-0x100);w.cpu.gpr[31]=Item;w.cpu.gpr[3]=0x23;
    if(mode==0)w.at(Item+12)[0]=2;
    if(mode==1)put32(w.at(Item+4),999);
    if(mode==2)w.at(Item+0x659)[0]=1;
    if(mode==3)w.at(Item+0x63A)[0]=6;
    if(mode==4)w.cpu.gpr[3]=0x123;
    if(mode==5){w.aliases[1].linked_start++;w.tables();}
    CHECK(!w.dispatch(0xC00C2DFC));CHECK(w.status().stop_required&&w.status().completed_awards==0);
  }
  for(unsigned mode=0;mode<5;++mode){
    World w(root/("award-return-negative-"+std::to_string(mode)));w.create();w.attach();w.load();w.start_creation();w.created();w.start_award();
    w.at(Info+0x44)[0]=0x23;w.at(Info+0x59)[0]=1;
    if(mode==0)w.cpu.gpr[1]-=16;
    if(mode==1)w.at(Info+0x44)[0]=0x26;
    if(mode==2)w.at(Info+0x59)[0]=3;
    if(mode==3)w.at(Item+0x63A)[0]=6;
    if(mode==4)put32(w.at(0x803CA768),1);
    CHECK(!w.dispatch(0xC0770A08));CHECK(w.status().stop_required&&w.status().completed_awards==0);
  }
  {
    World w(root/"tag-revocation");w.create();w.attach();w.load();w.start_creation();
    w.entry(0x8024541C,0x8002402C);w.cpu.gpr[3]=Chest+0xC4;CHECK(!w.dispatch(0x8024541C));CHECK(w.status().stop_required);
  }
  {
    World w(root/"raw-reuse-revocation");w.create();w.attach();w.load();w.start_creation();
    bw_randomizer_reward_host_rel_will_change(w.host,0);CHECK(w.status().stop_required);
    CHECK(!bw_randomizer_reward_host_rel_materialized(w.host,0,ChestRaw,0x6000));
  }
  {
    World w(root/"idempotent-copy-rejected");w.create();w.attach();w.load();
    CHECK(!bw_randomizer_reward_host_rel_materialized(w.host,0,ChestRaw,0x6000));CHECK(!w.status().stop_required);w.earn();
  }
  {
    World w(root/"unlink-revocation");w.create();w.attach();w.load();w.start_creation();
    w.entry(0x803056BC,0x80240F8C);w.cpu.gpr[3]=ChestRaw;w.cpu.gpr[30]=ChestLoader;
    CHECK(!w.dispatch(0x803056BC));CHECK(w.status().stop_required);
  }
  {
    World w(root/"shadowed-one-byte");w.create();w.attach();w.load();
    CHECK(bw_randomizer_reward_host_before_owner_change(w.host,BW_REWARD_HOST_SHARED_ALIAS_CHANGE));
    std::uint8_t shadow=w.at(Chest+0xB1)[0];CHECK(ppc_guest_alias_add_shared(Chest+0xB1,1,&shadow));
    CHECK(bw_randomizer_reward_host_shared_backing_committed(w.host,&w.cpu,w.backing.data(),2));w.tables();
    w.entry(0xC00261E8,0xC1DF2780);w.cpu.gpr[30]=Chest;w.cpu.gpr[3]=Chest+0x1F8;w.cpu.gpr[4]=6;
    w.cpu.gpr[5]=w.cpu.gpr[6]=UINT32_MAX;w.cpu.gpr[7]=w.cpu.gpr[8]=0;
    CHECK(!w.dispatch(0xC00261E8));CHECK(w.status().stop_required&&w.status().substitutions==0);
  }
  {
    World w(root/"code-lease-revoked");w.create();w.attach();w.load();fixture::code_live(w.code,false);
    w.cpu.ram=reinterpret_cast<std::uint8_t*>(1);w.entry(0xC00261E8,0xC1DF2780);
    CHECK(!w.dispatch(0xC00261E8));CHECK(w.status().stop_required&&w.status().substitutions==0);
    w.cpu.ram=w.ram.data();fixture::code_live(w.code,true);
  }
  {
    World w(root/"cpu-ram-owner-revoked");w.create();w.attach();w.load();w.start_creation();
    const auto before=w.ram;CHECK(bw_randomizer_reward_host_before_owner_change(w.host,BW_REWARD_HOST_RAM_REPLACED));
    CHECK(w.status().stop_required&&!w.status().native_load_authorized&&w.ram==before);
    CHECK(!bw_randomizer_reward_host_cpu_ready(w.host,reinterpret_cast<CPUState*>(1),w.backing.data(),2));
    CHECK(!bw_randomizer_reward_host_cpu_ready(w.host,&w.cpu,w.backing.data(),2));
  }
  {
    World w(root/"scene-revocation");w.create();w.attach();w.load();w.start_creation();fixture::emit(BW_GAME_EVENT_SCENE_LEAVING);CHECK(w.status().stop_required);
  }
  {
    World w(root/"owned-reset-revocation");w.create();w.attach();w.load();w.earn();fixture::emit(BW_GAME_EVENT_RESET);
    CHECK(w.status().stop_required&&w.status().confirmed_generation==1&&!w.status().native_load_authorized);
  }
}

void save_cases(const fs::path& root){
  for(bool picto:{true,false}){
    World w(root/(picto?"picto-paired-save":"orange-paired-save"),picto);w.create();w.attach();w.load();w.earn();
    // Completed historical progress survives ordinary alias/scene revocation.
    CHECK(bw_randomizer_reward_host_before_owner_change(w.host,BW_REWARD_HOST_SHARED_ALIAS_CHANGE));
    CHECK(ppc_guest_alias_remove(w.backing[0].linked_start,w.backing[0].size));
    CHECK(ppc_guest_alias_add_shared(w.backing[0].linked_start,w.backing[0].size,w.backing[0].storage));
    CHECK(bw_randomizer_reward_host_shared_backing_committed(w.host,&w.cpu,w.backing.data(),2));w.tables();
    fixture::emit(BW_GAME_EVENT_SCENE_ENTERED);CHECK(!w.status().stop_required);
    const auto packed=fixture::game(true,picto);const auto updated=fixture::card(packed,42);w.manual_success(packed,updated);
    CHECK(bw_randomizer_reward_host_maintenance(w.host));CHECK(w.status().confirmed_generation==2&&w.status().published_saves==1&&!w.status().hold_guest_mutators);
    CHECK(fixture::read_file(fs::u8path(w.path))==updated);CHECK(fs::exists(w.directory/"previous.bwseed"));
    const auto old_digest=fixture::hash(w.baseline),new_digest=fixture::hash(updated);CHECK(old_digest!=new_digest);
    w.close();w.startup.explicit_initial_card=nullptr;w.startup.explicit_initial_card_size=0;
    w.host=bw_randomizer_reward_host_create(&w.startup);CHECK(w.host&&w.status().confirmed_generation==2);
    CHECK(fixture::read_file(w.directory/"working.card")==updated);w.close();
  }
  {
    World w(root/"serializer-failure-drain");w.create();w.attach();w.load();w.earn();w.serializer();w.serialized(false);
    CHECK(w.status().stop_required&&w.status().drain_native_save);CHECK(!bw_randomizer_reward_host_stop(w.host));
    CHECK(!bw_randomizer_reward_host_before_owner_change(w.host,BW_REWARD_HOST_RAM_REPLACED));
    w.store(fixture::game(true,true));w.poll(true);CHECK(w.polled(0,true));w.poll(false);CHECK(!w.polled(1,false));
    CHECK(!w.status().drain_native_save&&w.status().confirmed_generation==1&&w.status().published_saves==0);w.close();
  }
  for(unsigned mode=0;mode<5;++mode){
    World w(root/("serialize-resource-negative-"+std::to_string(mode)),mode!=4);w.create();w.attach();w.load();w.earn();
    if(mode==0)put32(w.at(Ctrl+0x1654),0x80500000);
    if(mode==1)put16(w.at(Info+0x12A0+0x48DE),1);
    if(mode==2)w.at(Info+0x12A0+0x495B)[0]=1;
    if(mode==3)w.at(Info+0x12A0+0x495E)[0]=2;
    // mode4 retains the actual synthetic Orange handler queue100.
    w.entry(0x8005E780,0x801D8994);w.cpu.gpr[30]=Menu;w.cpu.gpr[3]=Info;w.cpu.gpr[4]=Menu+0x554;w.cpu.gpr[5]=0;
    CHECK(!w.dispatch(0x8005E780));CHECK(w.status().stop_required&&!w.status().drain_native_save&&w.status().published_saves==0);
  }
  {
    World w(root/"store-resource-failure-drain");w.create();w.attach();w.load();w.earn();w.serializer();w.serialized();
    put32(w.at(Ctrl+0x1654),0x80500000);w.store(fixture::game(true,true));CHECK(w.status().stop_required&&w.status().drain_native_save);
    w.poll(true);CHECK(w.polled(0,true));w.poll(false);CHECK(!w.polled(1,false));
    CHECK(!w.status().drain_native_save&&w.status().published_saves==0);w.close();
  }
  {
    World w(root/"poll-entry-replay");w.create();w.attach();w.load();w.earn();w.serializer();w.serialized();const auto packed=fixture::game(true,true);w.store(packed);
    w.poll(true);CHECK(w.dispatch(0x8001931C));CHECK(w.polled(0,true));w.poll(false);
    fixture::write_file(fs::u8path(w.path),fixture::card(packed,42));CHECK(w.polled(1,false));CHECK(bw_randomizer_reward_host_maintenance(w.host));
    CHECK(w.status().published_saves==1&&w.status().confirmed_generation==2);w.close();
  }
  {
    World w(root/"native-error-drain");w.create();w.attach();w.load();w.earn();w.serializer();w.serialized();w.store(fixture::game(true,true));
    w.poll(true);CHECK(!w.polled(2,true));CHECK(w.status().stop_required&&!w.status().drain_native_save&&w.status().confirmed_generation==1);w.close();
  }
  {
    World w(root/"owned-autosave-drain");w.create();w.attach();w.load();w.earn();w.env.autosave=true;
    w.entry(0x8005E780,0x8180FFE0);CHECK(w.dispatch(0x8005E780));CHECK(w.status().stop_required&&w.status().drain_native_save);
    CHECK(!bw_randomizer_reward_host_stop(w.host)&&!bw_randomizer_reward_host_destroy(w.host));
    CHECK(bw_randomizer_reward_host_maintenance(w.host));w.env.autosave=false;
    CHECK(!bw_randomizer_reward_host_maintenance(w.host));CHECK(w.status().published_saves==0);w.close();
  }
  for(unsigned mode=0;mode<4;++mode){
    World w(root/("preservation-negative-"+std::to_string(mode)));w.create();w.attach();w.load();w.earn();
    auto updated=fixture::card(fixture::game(true,true),42,mode==0,mode==1,mode==2);
    std::array<std::uint8_t,0x1650> captured{};payload::Match match{};
    CHECK(payload::inspect(updated.data(),updated.size(),captured,match)==payload::Status::Matched);
    if(mode==3){updated[0]^=1;} // Container rejection, separate from preservation.
    const auto old=fixture::read_file(w.directory/"current.bwseed");w.manual_success(captured,updated);
    CHECK(!bw_randomizer_reward_host_maintenance(w.host));CHECK(w.status().stop_required&&w.status().confirmed_generation==1&&w.status().published_saves==0);
    CHECK(fixture::read_file(w.directory/"current.bwseed")==old);w.close();
  }
  {
    World w(root/"snapshot-lock-failure");w.create();w.attach();w.load();w.earn();const auto packed=fixture::game(true,true);
    w.manual_success(packed,fixture::card(packed,42));w.env.lock_allowed=false;CHECK(!bw_randomizer_reward_host_maintenance(w.host));
    CHECK(w.status().stop_required&&w.status().confirmed_generation==1&&w.status().published_saves==0);w.env.lock_allowed=true;w.close();
  }
  {
    World w(root/"fatal-drain-abandonment");w.create();w.attach();w.load();w.earn();w.serializer();
    const auto before=w.cpu;const auto before_ram=w.ram;bw_randomizer_reward_host_abandon_unavailable(w.host);
    CHECK(std::memcmp(&w.cpu,&before,sizeof before)==0&&w.ram==before_ram);
    CHECK(w.status().phase==BW_REWARD_HOST_DRAIN_UNAVAILABLE&&!w.status().drain_native_save&&w.status().confirmed_generation==1);
    w.env.mounted.clear();CHECK(bw_randomizer_reward_host_destroy(w.host));w.host=nullptr;
  }
#ifdef _WIN32
  {
    World w(root/"real-storage-file-lock");w.create();w.attach();w.load();w.earn();const auto original=fixture::read_file(w.directory/"current.bwseed");
    const auto packed=fixture::game(true,true);w.manual_success(packed,fixture::card(packed,42));
    HANDLE locked=CreateFileW((w.directory/"current.bwseed").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    CHECK(locked!=INVALID_HANDLE_VALUE);CHECK(!bw_randomizer_reward_host_maintenance(w.host));
    CHECK(w.status().stop_required&&w.status().confirmed_generation==1&&w.status().published_saves==0);
    CHECK(fixture::read_file(w.directory/"current.bwseed")==original);CHECK(CloseHandle(locked));w.close();
    w.startup.explicit_initial_card=nullptr;w.startup.explicit_initial_card_size=0;w.host=bw_randomizer_reward_host_create(&w.startup);
    CHECK(w.host&&w.status().confirmed_generation==1&&fixture::read_file(w.directory/"working.card")==w.baseline);w.close();
  }
#endif
}
} // namespace
int main(int argc,char** argv){
#ifdef _WIN32
  SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
  _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
  CHECK(argc==2);const auto root=fs::u8path(argv[1])/std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  CHECK(fs::create_directories(root));startup_cases(root);load_cases(root);award_cases(root);save_cases(root);
  std::cout<<"reward adapter authored synthetic checks "<<fixture::checks<<"; native module/helper invocations0\n";
  return 0;
}
