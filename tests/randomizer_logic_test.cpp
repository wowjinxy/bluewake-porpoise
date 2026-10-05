#include "randomizer_logic.h"
#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <thread>
using namespace bluewake::randomizer;
#include "randomizer_goldens.h"

static std::size_t checks;
static void check(bool yes) { ++checks; if (!yes) { std::cerr << "Check failed: " << checks << "\n"; std::abort(); } }
static void expected(const Evaluation& result, bool reached) {
  check(result.valid); check(result.errors.empty()); check(result.reachable == reached);
}
static void invalid(const Evaluation& result) {
  check(!result.valid); check(!result.reachable); check(!result.errors.empty());
}
static Context options() {
  Context c;
  c.options = {{"logic_obscurity", std::string("None")}, {"logic_precision", std::string("None")},
               {"required_bosses", false}, {"skip_rematch_bosses", false}, {"sword_mode", std::string("No Starting Sword")}};
  return c;
}
// Finite, shallow work-limit canary. Each wide program is below the existing
// text/token bounds; this does not run the old exponential shared-DAG case.
static Catalog finite_wide_catalog() {
  Catalog c;
  c.items.push_back({"fixture:item:bombs", "Bombs", true, {}});
  std::string wide;
  for (unsigned i = 0; i < 2048; ++i) wide += (i ? " & Bombs" : "Bombs");
  std::string root;
  for (unsigned i = 0; i < 20; ++i) {
    const auto name = "Wide" + std::to_string(i);
    c.macros.push_back({"fixture:macro:" + name, name, wide});
    root += (i ? " & " : "") + name;
  }
  c.macros.push_back({"fixture:macro:root", "Root", root});
  return c;
}
static void bounded_evaluation_tests() {
  Catalog c;
  c.items.push_back({"fixture:item:bombs", "Bombs", true, {}});
  c.macros.push_back({"fixture:macro:0", "Shared0", "Bombs"});
  for (unsigned i = 1; i <= 30; ++i) {
    const auto previous = "Shared" + std::to_string(i - 1);
    c.macros.push_back({"fixture:macro:" + std::to_string(i), "Shared" + std::to_string(i), previous + " & " + previous});
  }
  Logic shared(std::move(c)); check(shared.valid());
  Context owned; owned.items["Bombs"] = 1;
  Context empty;
  for (unsigned i = 0; i < 8; ++i) {
    // Each call constructs a cold memo. A mutable/context-shared cache would
    // incorrectly keep the true result when the next snapshot has no Bombs.
    const auto yes = shared.macro("Shared30", owned), no = shared.macro("Shared30", empty);
    expected(yes, true); expected(no, false);
    check(yes.work < 8 * 31 && no.work < 8 * 31);
    check(yes.memo_hits >= 30 && no.memo_hits >= 30);
  }
  bool thread_owned = true, thread_empty = true;
  std::thread owns([&] { for (unsigned i = 0; i < 32; ++i) { const auto r = shared.macro("Shared30", owned); thread_owned &= r.valid && r.reachable && r.work < 8 * 31; } });
  std::thread lacks([&] { for (unsigned i = 0; i < 32; ++i) { const auto r = shared.macro("Shared30", empty); thread_empty &= r.valid && !r.reachable && r.work < 8 * 31; } });
  owns.join(); lacks.join(); check(thread_owned); check(thread_empty);
  auto location_catalog = Catalog{};
  location_catalog.items.push_back({"fixture:item:bombs", "Bombs", true, {}});
  location_catalog.locations.push_back({"fixture:location:reward", "Reward", "Bombs", "Bombs", {}, {{PathKind::ActorResource,"fixture/Actor000"}}});
  location_catalog.macros.push_back({"fixture:macro:root", "Root", "Can Access Item Location \"Reward\" & Can Access Item Location \"Reward\""});
  Logic locations(std::move(location_catalog)); check(locations.valid());
  const auto location_yes = locations.macro("Root", owned), location_no = locations.macro("Root", empty);
  expected(location_yes, true); expected(location_no, false);
  check(location_yes.memo_hits > 0 && location_no.memo_hits > 0);
  Catalog options_catalog;
  options_catalog.options.push_back({"gate", OptionKind::Boolean, {}});
  options_catalog.macros.push_back({"fixture:macro:branch", "Branch", "Option \"gate\" Enabled"});
  options_catalog.macros.push_back({"fixture:macro:root", "Root", "Nothing | Branch | Branch"});
  Logic eager(std::move(options_catalog)); check(eager.valid());
  const auto missing = eager.macro("Root", {}); invalid(missing); check(missing.memo_hits > 0);
  Context disabled; disabled.options["gate"] = false; expected(eager.macro("Root", disabled), true);
  // A completed false/error result cannot hide the prior missing-option error
  // and a new call with an explicit option must start a fresh memo.
  invalid(eager.macro("Root", {}));
  auto bad_kind = Catalog{};
  bad_kind.options.push_back({"bad", static_cast<OptionKind>(99), {"known"}});
  Logic unknown_kind(std::move(bad_kind)); check(!unknown_kind.valid()); invalid(unknown_kind.expression("Nothing", {}));
  Logic finite(finite_wide_catalog()); check(finite.valid());
  const auto capped = finite.macro("Root", owned); invalid(capped);
  check(capped.work == Logic::MaxEvaluationWork); check(capped.errors.size() == 1);
  check(capped.errors[0].find("Evaluation work exceeds") != std::string::npos);
  const auto partial_true = finite.expression("Nothing | Root", owned); invalid(partial_true);
  check(partial_true.work == Logic::MaxEvaluationWork);
  // The budget also covers context/list validation, before expression visits.
  auto list_catalog = Catalog{};
  list_catalog.options.push_back({"list", OptionKind::List, {"known"}});
  Logic list(std::move(list_catalog)); check(list.valid());
  Context huge_list; huge_list.options["list"] = std::vector<std::string>(Logic::MaxEvaluationWork,"known");
  const auto list_capped = list.expression("Nothing", huge_list); invalid(list_capped);
  check(list_capped.work == Logic::MaxEvaluationWork && list_capped.errors.size() == 1);
  expected(list.expression("Nothing", {}), true);
}
int main() {
  Logic logic(imported_catalog());
  for (const auto& error : logic.errors()) std::cerr << error << "\n";
  check(logic.valid());
  const auto& catalog = logic.catalog();
  check(catalog.locations.size() == 320); check(catalog.macros.size() == 307); check(catalog.items.size() == 237);
  std::size_t paths = 0, multiple = 0, logic_items = 0;
  std::size_t kinds[7] = {};
  std::set<std::string> ids;
  for (const auto& item : catalog.items) { check(ids.insert(item.source_id).second); logic_items += item.logic_item; }
  for (const auto& macro : catalog.macros) check(ids.insert(macro.source_id).second);
  for (const auto& location : catalog.locations) {
    check(ids.insert(location.source_id).second); check(!location.paths.empty());
    multiple += location.paths.size() > 1;
    for (const auto& path : location.paths) { ++paths; ++kinds[static_cast<unsigned>(path.kind)]; check(!path.raw.empty()); }
  }
  check(logic_items == 154); check(paths == 511); check(multiple == 77);
  const std::size_t want[] = {7, 76, 24, 186, 3, 11, 204};
  for (unsigned i = 0; i < 7; ++i) check(kinds[i] == want[i]);
  // Entire real-data corpus checked against an independent postfix oracle.
  for (const auto& golden : goldens()) {
    check(std::string(golden.macros).size() == 307); check(std::string(golden.locations).size() == 320);
    for (std::size_t i = 0; i < catalog.macros.size(); ++i)
      expected(logic.macro(catalog.macros[i].name, golden.context), golden.macros[i] == '1');
    for (std::size_t i = 0; i < catalog.locations.size(); ++i)
      expected(logic.location(catalog.locations[i].name, golden.context), golden.locations[i] == '1');
  }
  // Hand-authored expected outcomes from the actual imported DSL.
  Context c = options();
  expected(logic.location("Outset Island - Underneath Link's House", c), true);
  expected(logic.location("Outset Island - Jabun's Cave", c), false);
  c.items["Bombs"] = 1; expected(logic.location("Outset Island - Jabun's Cave", c), true);
  expected(logic.macro("Can Play Wind's Requiem", c), false);
  c.items["Wind Waker"] = 1; c.items["Wind's Requiem"] = 1;
  expected(logic.macro("Can Play Wind's Requiem", c), true);
  expected(logic.macro("Can Aim Mirror Shield", c), false);
  c.items["Progressive Shield"] = 1; expected(logic.macro("Can Aim Mirror Shield", c), false);
  c.items["Progressive Shield"] = 2; expected(logic.macro("Can Aim Mirror Shield", c), true);
  c.items.erase("Wind Waker"); c.items.erase("Bombs");
  expected(logic.macro("Can Aim Mirror Shield", c), false);
  c.items["Grappling Hook"] = 1; expected(logic.macro("Can Aim Mirror Shield", c), true);
  c = options(); c.items["Deku Leaf"] = 1;
  expected(logic.macro("Can Fan With Deku Leaf", c), true); // Randomizer tweak assumption, unlike vanilla.
  expected(logic.macro("Can Fly With Deku Leaf Indoors", c), false);
  c.items["Progressive Magic Meter"] = 1;
  expected(logic.macro("Can Fly With Deku Leaf Indoors", c), true);
  expected(logic.macro("Can Fly With Deku Leaf Outdoors", c), false);
  c.items["Wind Waker"] = 1; c.items["Wind's Requiem"] = 1;
  expected(logic.macro("Can Fly With Deku Leaf Outdoors", c), true);
  c = options(); c.items["Triforce Chart 2"] = 1;
  expected(logic.macro("Chart for Island 4", c), false);
  c.items["Progressive Wallet"] = 1; expected(logic.macro("Chart for Island 4", c), true);
  expected(logic.macro("Can Sword Fight with Orca", c), false);
  c.options["sword_mode"] = std::string("Swordless"); expected(logic.macro("Can Sword Fight with Orca", c), true);
  c = options(); c.items["DRC Small Key"] = 1;
  expected(logic.expression("DRC Small Key x2", c), false);
  c.items["DRC Small Key"] = 2; expected(logic.expression("DRC Small Key x2", c), true);
  c.options["logic_obscurity"] = std::string("Normal");
  expected(logic.macro("Obscure 1", c), true); expected(logic.macro("Obscure 2", c), false);
  c.options["logic_obscurity"] = std::string("Hard");
  expected(logic.macro("Obscure 2", c), true); expected(logic.macro("Obscure 3", c), false);
  c.options["logic_obscurity"] = std::string("Very Hard"); expected(logic.macro("Obscure 3", c), true);
  c = options();
  expected(logic.expression("Can Access Item Location \"Outset Island - Underneath Link's House\"", c), true);
  expected(logic.expression("Nothing & (Impossible | Nothing)", c), true);
  expected(logic.expression("(& Nothing & Nothing)", c), true);
  for (const auto* text : {"", "()", "Nothing &", "| Nothing", "&& Nothing", "Nothing || Nothing", "Nothing Nothing",
                           "(Nothing", "Nothing)", "Nothing & Nothing | Nothing", "Progressive Sword", "Progressive Sword x99999",
                           "Nothing | Invented Macro", "Fake Item", "Option \"unknown\" Enabled",
                           "Option \"sword_mode\" Enabled", "Option \"sword_mode\" Is \"Typo\"",
                           "Option \"required_bosses\" Is \"True\"", "Option \"required_bosses\" Enabled \"True\"",
                           "Option \"required_bosses Enabled", "Can Access Item Location \"Unknown Place\""})
    invalid(logic.expression(text, c));
  invalid(logic.expression(std::string(16385, 'x'), c));
  invalid(logic.expression(std::string(130, '(') + "Nothing" + std::string(130, ')'), c));
  invalid(logic.macro("Unknown Macro", c)); invalid(logic.location("Unknown Place", c));
  c.options.erase("sword_mode"); invalid(logic.macro("Can Sword Fight with Orca", c));
  c.items["Progressive Sword"] = 1;
  // Invalid/missing options fail closed even when another OR branch is true.
  invalid(logic.macro("Can Sword Fight with Orca", c));
  c = options(); c.options["sword_mode"] = true; invalid(logic.expression("Nothing", c));
  c = options(); c.options["required_bosses"] = std::string("False"); invalid(logic.expression("Nothing", c));
  c = options(); c.options["logic_precision"] = std::string("Typo"); invalid(logic.expression("Nothing", c));
  c = options(); c.options["unknown"] = false; invalid(logic.expression("Nothing", c));
  c = options(); c.items["Fake Item"] = 1; invalid(logic.expression("Nothing", c));
  c = options(); c.items["Bombs"] = 4097; invalid(logic.expression("Nothing", c));
  auto bad = imported_catalog(); bad.macros[0].requirement = "Nothing | Missing Condition";
  Logic unknown(std::move(bad)); check(!unknown.valid()); invalid(unknown.expression("Nothing", options()));
  bad = imported_catalog(); bad.macros[0].requirement = bad.macros[0].name;
  Logic self_cycle(std::move(bad)); check(!self_cycle.valid());
  check(self_cycle.errors()[0].find("Requirement cycle:") != std::string::npos);
  bad = imported_catalog();
  bad.macros[0].requirement = bad.macros[1].name; bad.macros[1].requirement = bad.macros[0].name;
  Logic pair_cycle(std::move(bad)); check(!pair_cycle.valid());
  bad = imported_catalog();
  bad.macros[0].requirement = "Can Access Item Location \"" + bad.locations[0].name + "\"";
  bad.locations[0].requirement = bad.macros[0].name;
  Logic location_cycle(std::move(bad)); check(!location_cycle.valid());
  bad = imported_catalog(); bad.locations[0].paths.clear(); Logic missing_path(std::move(bad)); check(!missing_path.valid());
  bad = imported_catalog(); bad.locations[0].source_id = bad.macros[0].source_id;
  Logic duplicate(std::move(bad)); check(!duplicate.valid());
  // List forms belong to the DSL, but no current imported option has that type.
  bad = imported_catalog(); bad.options.push_back({"fixture_list", OptionKind::List, {"Bombs", "Deku Leaf"}});
  Logic lists(std::move(bad)); check(lists.valid());
  c = options(); c.options["fixture_list"] = std::vector<std::string>{"Bombs"};
  expected(lists.expression("Option \"fixture_list\" Contains \"Bombs\"", c), true);
  expected(lists.expression("Option \"fixture_list\" Does Not Contain \"Deku Leaf\"", c), true);
  invalid(lists.expression("Option \"fixture_list\" Contains \"Typo\"", c));
  c.options["fixture_list"] = std::vector<std::string>{"Typo"}; invalid(lists.expression("Nothing", c));
  // No retained context cache: changing item ownership/options changes result.
  c = options(); c.items["Bombs"] = 1; expected(logic.location("Outset Island - Jabun's Cave", c), true);
  c.items.clear(); expected(logic.location("Outset Island - Jabun's Cave", c), false);
  bounded_evaluation_tests();
  std::cout << "PASS " << checks << " checks; 3135 real-corpus oracle comparisons; source logic only\n";
}
