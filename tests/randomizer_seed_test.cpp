// SPDX-License-Identifier: GPL-3.0-or-later
#include "randomizer_seed.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace bluewake::randomizer;
using namespace bluewake::randomizer::seed;
static std::size_t checks;
static void check(bool b) { ++checks; if(!b){std::cerr<<"seed check "<<checks<<" failed\n";std::abort();} }
static Request request() {
  Request r; r.seed=7;r.logic_contract_digest=sha256("identity-only logic v1");r.start_policy_digest=sha256("identity-only starting policy v1");
  r.options={{"logic_obscurity",std::string("None")},{"logic_precision",std::string("None")},
    {"required_bosses",false},{"skip_rematch_bosses",false},{"sword_mode",std::string("No Starting Sword")}};
  r.placements={{linkug_location_id(),basic_picto_id()}};return r;
}
static void invalid(const Result<Profile>& r) { check(!r);check(!r.errors.empty()); }
static Catalog fixture() {
  Catalog c;
  for(unsigned i=0;i<6;++i)c.locations.push_back({"loc:"+std::to_string(i),"Location"+std::to_string(i),"Nothing","Red",{},{{PathKind::ChestResource,"fixture/Chest"+std::to_string(i)}}});
  c.items={{"item:blue","Blue",true,{2}},{"item:red","Red",true,{4}},{"item:green","Green",true,{1}}};
  c.options={{"boolean",OptionKind::Boolean,{}},{"choice",OptionKind::Choice,{"A","B"}},{"list",OptionKind::List,{"a","b","c"}}};return c;
}
int main() {
  check(sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  check(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  StableRandom random(0);
  check(random.next()==UINT64_C(0xe220a8397b1dcdaf));check(random.next()==UINT64_C(0x6e789e6aa1b965f4));
  check(random.next()==UINT64_C(0x06c45d188009454f));check(random.next()==UINT64_C(0xf88bb8a8724c81ec));
  check(!random.bounded(0));
  for(auto bound:{UINT64_C(1),UINT64_C(2),UINT64_C(17),UINT64_C(0x8000000000000001),UINT64_MAX}) {
    StableRandom a(83),b(83);for(unsigned i=0;i<256;++i){const auto x=a.bounded(bound),y=b.bounded(bound);check(x&&y&&*x<bound&&x==y);}
  }
  auto catalog=imported_catalog();ProfileBuilder builder(catalog);check(builder.valid());check(is_digest(builder.catalog_digest()));
  const auto r=request();const auto p=builder.build(r);check(bool(p));check(p.errors.empty());check(p.value->audited_linkug_binding());
  check(p.value->reward_at(linkug_location_id())==basic_picto_id());check(!p.value->reward_at("absent"));
  const auto decoded=builder.decode(p.value->encode());check(bool(decoded));check(decoded.value->identity()==p.value->identity());
  check(decoded.value->encode()==p.value->encode());check(native_reward(basic_picto_id())->item==35);check(native_reward(orange_rupee_id())->item==6);
  check(!native_reward("Picto Box"));check(!native_reward(catalog.items.front().source_id));
  // Determinism is independent of catalog record/input placement ordering.
  std::reverse(catalog.items.begin(),catalog.items.end());std::reverse(catalog.macros.begin(),catalog.macros.end());std::reverse(catalog.locations.begin(),catalog.locations.end());std::reverse(catalog.options.begin(),catalog.options.end());
  ProfileBuilder reversed(catalog);check(reversed.valid());check(reversed.catalog_digest()==builder.catalog_digest());check(reversed.build(r).value->encode()==p.value->encode());
  for(std::size_t i=0;i<p.value->encode().size();++i)invalid(builder.decode(p.value->encode().substr(0,i)));
  invalid(builder.decode(p.value->encode()+std::string("\0",1)));
  invalid(builder.decode(std::string(MaxProfileBytes+1,'x')));
  auto bad=r;bad.logic_contract_digest="ABC";invalid(builder.build(bad));
  bad=r;bad.start_policy_digest=std::string(64,'A');invalid(builder.build(bad));
  bad=r;bad.options.erase("required_bosses");invalid(builder.build(bad));
  bad=r;bad.options.erase("required_bosses");bad.options["unknown"]=false;invalid(builder.build(bad));
  bad=r;bad.options["required_bosses"]=std::string("false");invalid(builder.build(bad));
  bad=r;bad.options["sword_mode"]=std::string("bogus");invalid(builder.build(bad));
  bad=r;bad.placements.push_back(bad.placements.front());invalid(builder.build(bad));
  bad=r;bad.placements.front().location_id="Unknown";invalid(builder.build(bad));
  bad=r;bad.placements.front().item_id="Unknown";invalid(builder.build(bad));
  bad=r;bad.placements.clear();invalid(builder.build(bad));
  bad=r;bad.placements.resize(MaxPlacements+1);invalid(builder.build(bad));
  auto changed=r;changed.seed++;check(builder.build(changed).value->identity()!=p.value->identity());
  changed=r;changed.options["required_bosses"]=true;check(builder.build(changed).value->identity()!=p.value->identity());
  changed=r;changed.start_policy_digest=sha256("another metadata");check(builder.build(changed).value->identity()!=p.value->identity());
  changed=r;changed.placements.front().item_id=orange_rupee_id();check(builder.build(changed).value->identity()!=p.value->identity());
  // Canonical decoder rejects boolean2, unknown tags and altered digest.
  auto bytes=p.value->encode();const auto at=bytes.find("required_bosses")+std::string("required_bosses").size();check(at<bytes.size());
  bytes[at+1]=2;invalid(builder.decode(bytes));bytes=p.value->encode();bytes[at]=9;invalid(builder.decode(bytes));
  bytes=p.value->encode();const auto digest_at=bytes.find(builder.catalog_digest());check(digest_at!=std::string::npos);bytes[digest_at]='z';invalid(builder.decode(bytes));
  auto drift=imported_catalog();drift.locations.front().requirement="Nothing";ProfileBuilder drifted(drift);check(drifted.valid());check(drifted.catalog_digest()!=builder.catalog_digest());invalid(drifted.decode(p.value->encode()));
  drift=imported_catalog();for(auto& l:drift.locations)if(l.source_id==linkug_location_id())l.paths.front().raw="Elsewhere/Chest000";
  ProfileBuilder wrong_binding(drift);check(wrong_binding.valid());check(!wrong_binding.build(r).value->audited_linkug_binding());
  auto malformed=fixture();malformed.items[1].source_id=malformed.items[0].source_id;check(!ProfileBuilder(malformed).valid());
  malformed=fixture();malformed.options.front().kind=static_cast<OptionKind>(99);check(!ProfileBuilder(malformed).valid());
  malformed=fixture();malformed.locations.front().paths.front().kind=static_cast<PathKind>(99);check(!ProfileBuilder(malformed).valid());
  malformed=fixture();malformed.items.resize(2049);check(!ProfileBuilder(malformed).valid());
  malformed=fixture();malformed.locations.front().paths.resize(129);check(!ProfileBuilder(malformed).valid());
  ProfileBuilder f(fixture());check(f.valid());
  GenerateRequest g;g.identity.seed=123;g.identity.logic_contract_digest=sha256("fixture");g.identity.start_policy_digest=sha256("fixture-start");
  g.identity.options={{"boolean",true},{"choice",std::string("B")},{"list",std::vector<std::string>{"c","a"}}};
  g.locations={"loc:5","loc:4","loc:3","loc:2","loc:1","loc:0"};g.reward_pool={"item:red","item:blue","item:red","item:green","item:blue","item:green"};g.plando={{"loc:3","item:red"}};
  const auto generated=f.generate(g);check(bool(generated));check(generated.value->reward_at("loc:3")=="item:red");
  auto again=g;std::reverse(again.locations.begin(),again.locations.end());std::reverse(again.reward_pool.begin(),again.reward_pool.end());check(f.generate(again).value->encode()==generated.value->encode());
  auto expected_pool=g.reward_pool;std::sort(expected_pool.begin(),expected_pool.end());std::vector<std::string> actual_pool;
  for(const auto& x:generated.value->placements())actual_pool.push_back(x.item_id);std::sort(actual_pool.begin(),actual_pool.end());check(actual_pool==expected_pool);
  check(std::get<std::vector<std::string>>(generated.value->options().at("list"))==std::vector<std::string>({"a","c"}));
  check(f.decode(generated.value->encode()).value->identity()==generated.value->identity());
  again=g;again.plando.push_back({"loc:1","item:red"});again.plando.push_back({"loc:2","item:red"});invalid(f.generate(again));
  again=g;again.plando.push_back(g.plando.front());invalid(f.generate(again));
  again=g;again.locations.push_back("loc:0");again.reward_pool.push_back("item:red");invalid(f.generate(again));
  again=g;again.identity.placements={{"loc:0","item:red"}};invalid(f.generate(again));
  again=g;again.identity.options["list"]=std::vector<std::string>{"a","a"};invalid(f.generate(again));
  again=g;again.identity.options["list"]=std::vector<std::string>{"d"};invalid(f.generate(again));
  // Repeated rewards are legitimate multiset occurrences, never collapsed.
  again=g;again.reward_pool.assign(6,"item:red");again.plando={{"loc:0","item:red"},{"loc:4","item:red"}};
  const auto repeated=f.generate(again);check(bool(repeated));for(const auto& x:repeated.value->placements())check(x.item_id=="item:red");
  for(unsigned s=0;s<128;++s){again=g;again.identity.seed=s;auto a=f.generate(again),b=f.generate(again);check(bool(a)&&bool(b));check(a.value->encode()==b.value->encode());check(bool(f.decode(a.value->encode())));}
  // Full explicit imported pool remains declarative, with no beatability claim.
  GenerateRequest full;full.identity=r;full.identity.placements.clear();
  for(const auto& l:catalog.locations){full.locations.push_back(l.source_id);full.reward_pool.push_back(orange_rupee_id());}
  full.reward_pool.back()=basic_picto_id();full.plando={{linkug_location_id(),basic_picto_id()}};
  auto all=builder.generate(full);check(bool(all));check(all.value->placements().size()==320);check(all.value->reward_at(linkug_location_id())==basic_picto_id());check(bool(builder.decode(all.value->encode())));
  std::cout<<"randomizer seed checks "<<checks<<" PASS (declarative only)\n";
}
