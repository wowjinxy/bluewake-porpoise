// SPDX-License-Identifier: GPL-3.0-or-later
// Private authored synthetic host boundary stand-ins. Never game authority.
#pragma once
#include "randomizer_reward_host.h"
#include "game_events.h"
#include "randomizer_seed.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace fixture {
using Bytes=std::vector<std::uint8_t>;
struct Subscriber { BwGameEventSubscription id=0;std::uint64_t mask=0;BwGameEventCallback callback=nullptr;void* user=nullptr; };
struct Environment {
  std::string mounted;
  bool lock_allowed=true,lock_held=false,autosave=false;
  std::uint64_t locks=0,unlocks=0,code_queries=0,descriptor_dispatches=0;
  BwGameEventStats stats{};
  std::vector<Subscriber> subscribers;
  BwGameEventSubscription next_subscription=1;
};
extern Environment* environment;
void require(bool,const char*,int);
#define CHECK(x) ::fixture::require(bool(x),#x,__LINE__)
extern unsigned checks;
void put16(std::uint8_t*,std::uint16_t);
void put32(std::uint8_t*,std::uint32_t);
void put64(std::uint8_t*,std::uint64_t);
std::string hash(const Bytes&);
Bytes read_file(const std::filesystem::path&);
void write_file(const std::filesystem::path&,const Bytes&);
bluewake::randomizer::seed::Profile profile(bool picto);
std::array<std::uint8_t,0x1650> game(bool awarded,bool picto);
Bytes card(const std::array<std::uint8_t,0x1650>&,std::uint32_t save_count=41,
           bool changed_other_quest=false,bool changed_photo=false,bool changed_other_file=false);
BwIcLoadedCode* make_code(const StaticRecompModuleDesc*,std::uint64_t);
void code_live(BwIcLoadedCode*,bool);
void destroy_code(BwIcLoadedCode*);
void emit(BwGameEventKind,BwGameResetReason=BW_GAME_RESET_GAME_LOAD);
int forbidden_dispatch(CPUState*,u32);
} // namespace fixture
