// Source-only synthetic observation fixtures. No CPU, native reads or game launch.
#include "native_inventory_context.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <thread>
#include <type_traits>
#include <vector>
using namespace bluewake::randomizer;
using namespace bluewake::randomizer::native_subset;
static std::atomic<unsigned> checks{0};
static void check(bool value, int line) {
  ++checks;
  if (!value) { std::fprintf(stderr, "Check failed at line %d\n", line); std::abort(); }
}
#define CHECK(v) check((v), __LINE__)
static_assert(!std::is_invocable_v<decltype(&CapabilityLogic::evaluate),
    const CapabilityLogic&, std::string, const PartialInventory&, const std::optional<OwnerStamp>&>);
static_assert(!std::is_convertible_v<std::string, ApprovedCapability>);

static ModuleFingerprint module() { ModuleFingerprint out{}; out.fill(0xA5); return out; }
static OwnerStamp owner() {
  OwnerStamp out; out.module = module(); out.cpu_lifetime = 1; out.memory_lifetime = 2;
  out.native_epoch = 3; out.scene_generation = 4; out.player = 0x80AB0000;
  out.native_frame = 80; std::memcpy(out.stage.data(), "sea", 4); return out;
}
static CopiedInventoryObservation fixture(bool waker, bool hook, bool song) {
  CopiedInventoryObservation out; out.before = owner(); out.after = owner();
  ContextSafety safety; safety.scene_active = safety.player_valid = safety.controls_ready = true;
  out.safety = safety; out.waker_slot = waker ? 0x22 : 0xFF; out.waker_obtained = waker ? 1 : 0;
  out.hook_slot = hook ? 0x25 : 0xFF; out.hook_obtained = hook ? 1 : 0;
  out.songs = song ? 1 : 0; return out;
}
static CapabilityResult eval(const CapabilityLogic& logic, ApprovedCapability capability,
    const CopiedInventoryObservation& data) {
  return logic.evaluate(capability, project_inventory(data, owner(), module()), owner());
}
static void known(const CapabilityResult& result, bool value) {
  CHECK(result.status == Status::Ready); CHECK(result.value.has_value());
  CHECK(*result.value == value); CHECK(result.work > 0 && result.work <= 32);
}
static void unavailable(const CapabilityResult& result, Status status) {
  CHECK(result.status == status); CHECK(!result.value.has_value()); CHECK(result.work == 0);
}
static void truth_table() {
  CapabilityLogic logic(imported_catalog()); CHECK(logic.catalog_status() == Status::Ready);
  for (unsigned bits = 0; bits < 8; ++bits) {
    const bool w = bits & 1, h = bits & 2, s = bits & 4;
    auto data = fixture(w, h, s);
    // Exact input object representation is unchanged; copied scalar evaluation
    // has no native CPU/RAM facade at all.
    std::array<unsigned char, sizeof(data)> before{};
    std::memcpy(before.data(), &data, sizeof(data));
    auto projected = project_inventory(data, owner(), module()); CHECK(projected.status() == Status::Ready);
    CHECK(projected.fact(CoveredItem::WindWaker) == (w ? FactState::Present : FactState::Absent));
    CHECK(projected.fact(CoveredItem::GrapplingHook) == (h ? FactState::Present : FactState::Absent));
    CHECK(projected.fact(CoveredItem::WindsRequiem) == (s ? FactState::Present : FactState::Absent));
    known(logic.evaluate(ApprovedCapability::OwnWindWaker, projected, owner()), w);
    known(logic.evaluate(ApprovedCapability::OwnGrapplingHook, projected, owner()), h);
    known(logic.evaluate(ApprovedCapability::LearnedWindsRequiem, projected, owner()), s);
    known(logic.evaluate(ApprovedCapability::PlayWindsRequiem, projected, owner()), w && s);
    known(logic.evaluate(ApprovedCapability::DefeatGohma, projected, owner()), h);
    CHECK(std::memcmp(before.data(), &data, sizeof(data)) == 0);
  }
  // Demonstrate the passive-library hazard: absent names evaluate false. The
  // wrapper rejects incomplete data before this unrestricted API can run.
  Logic passive(imported_catalog()); auto absent = passive.expression("Wind Waker", {});
  CHECK(absent.valid && !absent.reachable);
  auto missing = fixture(false, false, false); missing.waker_slot.reset();
  unavailable(eval(logic, ApprovedCapability::OwnWindWaker, missing), Status::Incomplete);
  unavailable(logic.evaluate(ApprovedCapability::OwnWindWaker, {}, owner()), Status::Incomplete);
  for (unsigned value : {5u, 6u, 99u, 0xFFFFFFFFu})
    unavailable(logic.evaluate(static_cast<ApprovedCapability>(value), project_inventory(fixture(true,true,true), owner(), module()), owner()), Status::UnsupportedCapability);
}
static void bitmap_and_sword() {
  CapabilityLogic logic(imported_catalog());
  for (unsigned flags = 0; flags < 256; ++flags) {
    const bool acquired = flags & 1;
    auto data = fixture(acquired, acquired, acquired);
    data.waker_obtained = static_cast<std::uint8_t>(flags);
    data.hook_obtained = static_cast<std::uint8_t>(flags);
    data.songs = static_cast<std::uint8_t>(flags);
    known(eval(logic, ApprovedCapability::OwnWindWaker, data), acquired);
    known(eval(logic, ApprovedCapability::OwnGrapplingHook, data), acquired);
    known(eval(logic, ApprovedCapability::LearnedWindsRequiem, data), acquired);
  }
  for (unsigned flags = 0; flags < 256; ++flags) {
    auto data = fixture(true, true, true); data.collected_swords = static_cast<std::uint8_t>(flags);
    data.equipped_sword = 0x38;
    auto projected = project_inventory(data, owner(), module()); auto sword = projected.sword_facts();
    CHECK(sword.permanent_hero_sword.has_value()); CHECK(*sword.permanent_hero_sword == bool(flags & 1));
    CHECK(sword.equipped_id == 0x38); known(logic.evaluate(ApprovedCapability::DefeatGohma, projected, owner()), true);
  }
  auto practice = fixture(false, false, false); practice.collected_swords = 0; practice.equipped_sword = 0x38;
  auto projected = project_inventory(practice, owner(), module());
  CHECK(projected.sword_facts().permanent_hero_sword == false); CHECK(projected.sword_facts().equipped_id == 0x38);
  known(logic.evaluate(ApprovedCapability::OwnWindWaker, projected, owner()), false);
  practice.collected_swords = 1; practice.equipped_sword = 0xFF;
  projected = project_inventory(practice, owner(), module());
  CHECK(projected.sword_facts().permanent_hero_sword == true); CHECK(projected.sword_facts().equipped_id == 0xFF);
  known(logic.evaluate(ApprovedCapability::DefeatGohma, projected, owner()), false);
  practice.collected_swords.reset(); practice.equipped_sword.reset();
  projected = project_inventory(practice, owner(), module());
  CHECK(!projected.sword_facts().permanent_hero_sword); CHECK(!projected.sword_facts().equipped_id);
  known(logic.evaluate(ApprovedCapability::PlayWindsRequiem, projected, owner()), false);
}
static void partial_and_contradiction() {
  CapabilityLogic logic(imported_catalog());
  using Change = std::function<void(CopiedInventoryObservation&)>;
  const std::vector<Change> missing = {
    [](auto& d){d.before.reset();}, [](auto& d){d.after.reset();}, [](auto& d){d.safety.reset();},
    [](auto& d){d.waker_slot.reset();}, [](auto& d){d.waker_obtained.reset();},
    [](auto& d){d.hook_slot.reset();}, [](auto& d){d.hook_obtained.reset();}, [](auto& d){d.songs.reset();}
  };
  for (const auto& change : missing) {
    auto d = fixture(true,true,true); change(d);
    unavailable(eval(logic, ApprovedCapability::PlayWindsRequiem, d), Status::Incomplete);
  }
  for (const auto& change : std::vector<Change>{
      [](auto& d){d.waker_slot=0xFF;}, [](auto& d){d.waker_obtained=0;},
      [](auto& d){d.hook_slot=0xFF;}, [](auto& d){d.hook_obtained=0;},
      [](auto& d){d.waker_slot=0x25;}, [](auto& d){d.hook_slot=0x22;}}) {
    auto d = fixture(true,true,true); change(d);
    unavailable(eval(logic, ApprovedCapability::PlayWindsRequiem, d), Status::Contradictory);
  }
  for (unsigned slot=0; slot<256; ++slot) {
    if (slot == 0x22 || slot == 0xFF) continue;
    auto d = fixture(false,false,false); d.waker_slot=static_cast<std::uint8_t>(slot);
    unavailable(eval(logic, ApprovedCapability::OwnWindWaker, d), Status::Contradictory);
  }
  auto both = fixture(true,true,true); both.waker_slot = 0xFF; both.songs.reset();
  CHECK(project_inventory(both, owner(), module()).status() == Status::Contradictory);
  auto ready = project_inventory(fixture(true,true,true), owner(), module());
  CHECK(ready.fact(static_cast<CoveredItem>(999)) == FactState::Unknown);
  unavailable(logic.evaluate(ApprovedCapability::OwnWindWaker, ready, std::nullopt), Status::Incomplete);
}
static void lifecycle_and_safety() {
  CapabilityLogic logic(imported_catalog());
  using Change = std::function<void(OwnerStamp&)>;
  const std::vector<Change> changes = {
    [](auto& s){++s.cpu_lifetime;}, [](auto& s){++s.memory_lifetime;},
    [](auto& s){++s.module_generation;}, [](auto& s){++s.native_epoch;},
    [](auto& s){++s.scene_generation;}, [](auto& s){++s.alias_generation;},
    [](auto& s){++s.native_frame;}, [](auto& s){s.player += 4;},
    [](auto& s){s.module[3] ^= 1;}, [](auto& s){std::memcpy(s.stage.data(), "Name",5);}
  };
  auto projected = project_inventory(fixture(true,true,true), owner(), module());
  for (const auto& change : changes) {
    auto now=owner(); change(now);
    unavailable(logic.evaluate(ApprovedCapability::DefeatGohma, projected, now), Status::Stale);
    auto d=fixture(true,true,true); change(*d.after);
    unavailable(eval(logic, ApprovedCapability::DefeatGohma, d), Status::Stale);
  }
  // Same addresses after each reset/scene/memory lifetime still expire data.
  for (unsigned i=0; i<128; ++i) {
    auto next=owner(); next.native_epoch += i+1;
    unavailable(logic.evaluate(ApprovedCapability::PlayWindsRequiem, projected, next), Status::Stale);
  }
  for (const auto& change : std::vector<Change>{
      [](auto& s){s.cpu_lifetime=0;}, [](auto& s){s.memory_lifetime=0;},
      [](auto& s){s.native_epoch=0;}, [](auto& s){s.scene_generation=0;},
      [](auto& s){s.module.fill(0);}, [](auto& s){s.player=0;},
      [](auto& s){s.player=0x81800000;}, [](auto& s){s.player|=1;},
      [](auto& s){s.stage.fill('a');}, [](auto& s){s.stage[0]=0;},
      [](auto& s){s.stage[0]='\x01';}}) {
    auto d=fixture(true,true,true); change(*d.before); d.after=d.before;
    auto partial=project_inventory(d,d.after,module()); CHECK(partial.status()==Status::Incomplete);
    unavailable(logic.evaluate(ApprovedCapability::OwnWindWaker, partial, d.after), Status::Incomplete);
  }
  auto d=fixture(true,true,true); d.before->module.fill(0x42); d.after=d.before;
  CHECK(project_inventory(d,d.after,module()).status()==Status::UnsupportedOwner);
  ModuleFingerprint unconfigured{};
  CHECK(project_inventory(fixture(true,true,true),owner(),unconfigured).status()==Status::UnsupportedOwner);
  using SafetyChange=std::function<void(ContextSafety&)>;
  for(const auto& change:std::vector<SafetyChange>{
      [](auto& s){s.scene_active=false;}, [](auto& s){s.player_valid=false;},
      [](auto& s){s.controls_ready=false;}, [](auto& s){s.paused=true;},
      [](auto& s){s.event_running=true;}, [](auto& s){s.transitioning=true;},
      [](auto& s){s.save_or_load_active=true;}, [](auto& s){s.machine_request_pending=true;},
      [](auto& s){s.recollection_stage=1;}}) {
    auto unsafe=fixture(true,true,true); change(*unsafe.safety);
    auto partial=project_inventory(unsafe,owner(),module()); CHECK(!partial.owner());
    CHECK(!partial.sword_facts().permanent_hero_sword);
    unavailable(logic.evaluate(ApprovedCapability::DefeatGohma,partial,owner()),Status::UnsafeContext);
  }
  for(char digit='0';digit<='3';++digit) {
    auto unsafe=fixture(true,true,true); std::memcpy(unsafe.before->stage.data(),"Xboss0",7);
    unsafe.before->stage[5]=digit; unsafe.after=unsafe.before;
    auto partial=project_inventory(unsafe,unsafe.after,module()); CHECK(partial.status()==Status::UnsafeContext);
  }
  // Zero frame, alias and module generations are valid initial scalar values;
  // lifetime/epoch identity, not guessed nonzero counters, decides freshness.
  auto initial=fixture(true,true,true); initial.before->native_frame=0; initial.after=initial.before;
  auto first=project_inventory(initial,initial.after,module()); CHECK(first.status()==Status::Ready);
  known(logic.evaluate(ApprovedCapability::OwnWindWaker,first,initial.after),true);
}
static void catalog_drift() {
  auto reference=imported_catalog();
  for(const std::string name:{"Wind Waker","Grappling Hook","Wind's Requiem"}) {
    for(unsigned mode=0;mode<5;++mode) {
      auto catalog=reference; auto found=std::find_if(catalog.items.begin(),catalog.items.end(),[&](const auto& i){return i.name==name;}); CHECK(found!=catalog.items.end());
      if(mode==0)found->source_id+="drift";
      if(mode==1)found->name+="drift";
      if(mode==2)found->logic_item=false;
      if(mode==3)catalog.items.erase(found);
      if(mode==4){auto duplicate=*found;catalog.items.push_back(duplicate);}
      CapabilityLogic logic(std::move(catalog)); CHECK(logic.catalog_status()==Status::CatalogDrift);
      unavailable(logic.evaluate(ApprovedCapability::OwnWindWaker,project_inventory(fixture(true,true,true),owner(),module()),owner()),Status::CatalogDrift);
    }
  }
  for(const std::string name:{"Can Defeat Gohma","Can Play Wind's Requiem"}) {
    for(const std::string requirement:{"Nothing","Nothing | Hero's Shield","Grappling Hook | Hero's Sword",
        "Wind Waker & Wind's Requiem & Progressive Sword x1", "Can Access Dragon Roost Cavern",
        "Option \"required_bosses\" Enabled", "Can Access Item Location \"Dragon Roost Island - Wind Shrine\""}) {
      auto catalog=reference; auto found=std::find_if(catalog.macros.begin(),catalog.macros.end(),[&](const auto& m){return m.name==name;}); CHECK(found!=catalog.macros.end());
      found->requirement=requirement; CapabilityLogic logic(std::move(catalog)); CHECK(logic.catalog_status()==Status::CatalogDrift);
      unavailable(logic.evaluate(ApprovedCapability::DefeatGohma,project_inventory(fixture(true,true,true),owner(),module()),owner()),Status::CatalogDrift);
    }
    for(unsigned mode=0;mode<4;++mode) {
      auto catalog=reference; auto found=std::find_if(catalog.macros.begin(),catalog.macros.end(),[&](const auto& m){return m.name==name;});
      if(mode==0)found->source_id+="drift";
      if(mode==1)found->name+="drift";
      if(mode==2)catalog.macros.erase(found);
      if(mode==3){auto duplicate=*found;catalog.macros.push_back(duplicate);}
      CapabilityLogic logic(std::move(catalog)); CHECK(logic.catalog_status()==Status::CatalogDrift);
    }
  }
  auto bad=reference; bad.macros.push_back({"fixture:invalid","Unrelated invalid macro","Unknown Item"});
  CapabilityLogic invalid(std::move(bad)); CHECK(invalid.catalog_status()==Status::InvalidCatalog);
  unavailable(invalid.evaluate(ApprovedCapability::OwnWindWaker,project_inventory(fixture(true,true,true),owner(),module()),owner()),Status::InvalidCatalog);
  auto descriptive=reference;
  for(auto& item:descriptive.items)item.native_name_ids={0x22,0x25,0x38};
  CapabilityLogic no_inference(std::move(descriptive)); CHECK(no_inference.catalog_status()==Status::Ready);
  known(eval(no_inference,ApprovedCapability::DefeatGohma,fixture(false,false,false)),false);
  auto huge=reference; huge.items.resize(513); CapabilityLogic bounded(std::move(huge)); CHECK(bounded.catalog_status()==Status::CatalogDrift);
}
static void parallel_contexts() {
  CapabilityLogic logic(imported_catalog()); std::vector<std::thread> workers;
  for(unsigned bits=0;bits<8;++bits)workers.emplace_back([&,bits]{
    const bool w=bits&1,h=bits&2,s=bits&4;
    auto projected=project_inventory(fixture(w,h,s),owner(),module());
    for(unsigned repeat=0;repeat<128;++repeat) {
      known(logic.evaluate(ApprovedCapability::PlayWindsRequiem,projected,owner()),w&&s);
      known(logic.evaluate(ApprovedCapability::DefeatGohma,projected,owner()),h);
    }
  });
  for(auto& worker:workers)worker.join();
}
int main() {
  truth_table();bitmap_and_sword();partial_and_contradiction();lifecycle_and_safety();catalog_drift();parallel_contexts();
  std::printf("PASS %u checks: synthetic values only; truth tables, bitmaps/Sword separation, incomplete/contradictory/stale/unsafe, exact catalog drift, closed capabilities, bounded parallel evaluation; no native capture or qualification\n",checks.load());
}
