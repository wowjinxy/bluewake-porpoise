#include "randomizer_logic.h"
#include <algorithm>
#include <functional>
#include <regex>
#include <set>
#include <sstream>

namespace bluewake::randomizer {
namespace {
constexpr std::size_t MaxText = 16384, MaxNodes = 4096;
constexpr unsigned MaxDepth = 128;
std::string trim(std::string s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
bool contains(const std::vector<std::string>& values, const std::string& v) {
  return std::find(values.begin(), values.end(), v) != values.end();
}
}
Logic::Logic(Catalog catalog) : catalog_(std::move(catalog)) {
  std::set<std::string> ids;
  auto records = [&](const auto& entries, auto& names, const char* kind) {
    for (std::size_t i = 0; i < entries.size(); ++i) {
      const auto& entry = entries[i];
      if (entry.name.empty() || !names.emplace(entry.name, i).second)
        errors_.push_back(std::string("Duplicate/empty ") + kind + " name: " + entry.name);
      if (entry.source_id.empty() || !ids.insert(entry.source_id).second)
        errors_.push_back(std::string("Duplicate/empty source schema ID: ") + entry.source_id);
    }
  };
  records(catalog_.items, items_, "item");
  records(catalog_.macros, macros_, "macro");
  records(catalog_.locations, locations_, "location");
  for (std::size_t i = 0; i < catalog_.options.size(); ++i) {
    const auto& option = catalog_.options[i];
    if (option.name.empty() || !options_.emplace(option.name, i).second)
      errors_.push_back("Duplicate/empty option: " + option.name);
    if (option.kind != OptionKind::Boolean && option.kind != OptionKind::Choice && option.kind != OptionKind::List)
      errors_.push_back("Unknown option kind: " + option.name);
    if (option.kind != OptionKind::Boolean && option.choices.empty())
      errors_.push_back("Choice/list option has no declared values: " + option.name);
    std::set<std::string> values;
    for (const auto& value : option.choices)
      if (value.empty() || !values.insert(value).second)
        errors_.push_back("Duplicate/empty option value: " + option.name);
  }
  macro_programs_.resize(catalog_.macros.size());
  location_programs_.resize(catalog_.locations.size());
  auto compile = [&](const auto& entries, auto& programs) {
    for (std::size_t i = 0; i < entries.size(); ++i) {
      std::string error;
      if (!parse(entries[i].requirement, programs[i], error))
        errors_.push_back(entries[i].name + ": " + error);
    }
  };
  compile(catalog_.macros, macro_programs_);
  compile(catalog_.locations, location_programs_);
  for (const auto& location : catalog_.locations)
    if (location.paths.empty()) errors_.push_back("Location has no patch paths: " + location.name);
  // Reject every cycle, including an OR branch that could otherwise bypass it.
  // Upstream nested-entrance exceptions require a separate verified adapter.
  if (!errors_.empty()) return;
  const auto size = macro_programs_.size() + location_programs_.size();
  std::vector<unsigned char> color(size, 0);
  std::vector<std::size_t> stack;
  auto label = [&](std::size_t i) {
    return i < macro_programs_.size() ? "macro " + catalog_.macros[i].name
      : "location " + catalog_.locations[i - macro_programs_.size()].name;
  };
  std::function<void(std::size_t)> visit = [&](std::size_t i) {
    if (color[i] == 2) return;
    if (color[i] == 1) {
      std::string cycle = "Requirement cycle:";
      auto first = std::find(stack.begin(), stack.end(), i);
      for (; first != stack.end(); ++first) cycle += " -> " + label(*first);
      errors_.push_back(cycle + " -> " + label(i));
      return;
    }
    if (stack.size() >= MaxDepth) { errors_.push_back("Requirement graph depth exceeds 128"); return; }
    color[i] = 1; stack.push_back(i);
    const auto& p = i < macro_programs_.size() ? macro_programs_[i]
      : location_programs_[i - macro_programs_.size()];
    for (const auto& node : p.nodes) {
      if (node.kind == Kind::Macro) visit(node.index);
      if (node.kind == Kind::Location) visit(macro_programs_.size() + node.index);
    }
    stack.pop_back(); color[i] = 2;
  };
  for (std::size_t i = 0; i < size; ++i) visit(i);
}
bool Logic::atom(const std::string& text, Node& out, std::string& error) const {
  if (text == "Nothing") { out.kind = Kind::True; return true; }
  if (text == "Impossible") { out.kind = Kind::False; return true; }
  std::smatch match;
  if (text.rfind("Progressive ", 0) == 0 || text.find(" Small Key x") != std::string::npos) {
    static const std::regex quantity(R"(^(.+) x([0-9]+)$)");
    if (!std::regex_match(text, match, quantity) || match[2].length() > 4) {
      error = "Invalid item quantity: " + text; return false;
    }
    const auto count = std::stoul(match[2]);
    auto item = items_.find(match[1]);
    if (count > 4096 || item == items_.end() || !catalog_.items[item->second].logic_item) {
      error = "Unknown/bounded quantified item: " + text; return false;
    }
    out.kind = Kind::Item; out.index = item->second;
    out.quantity = static_cast<std::uint32_t>(count); return true;
  }
  if (text.rfind("Can Access Item Location ", 0) == 0) {
    static const std::regex location(R"loc(^Can Access Item Location "([^"]+)"$)loc");
    if (!std::regex_match(text, match, location)) { error = "Invalid location reference: " + text; return false; }
    auto target = locations_.find(match[1]);
    if (target == locations_.end()) { error = "Unknown location: " + match[1].str(); return false; }
    out.kind = Kind::Location; out.index = target->second; return true;
  }
  if (text.rfind("Option ", 0) == 0) {
    static const std::regex option(R"opt(^Option "([^"]+)" (Enabled|Disabled|Is Not|Is|Does Not Contain|Contains)(?: "([^"]+)")?$)opt");
    if (!std::regex_match(text, match, option)) { error = "Invalid option requirement: " + text; return false; }
    auto target = options_.find(match[1]);
    if (target == options_.end()) { error = "Unknown option: " + match[1].str(); return false; }
    const auto& spec = catalog_.options[target->second];
    const auto op = match[2].str(), value = match[3].str();
    out.index = target->second; out.value = value;
    if (op == "Enabled" || op == "Disabled") {
      if (spec.kind != OptionKind::Boolean || match[3].matched) { error = "Boolean option type mismatch: " + text; return false; }
      out.kind = op == "Enabled" ? Kind::Enabled : Kind::Disabled;
    } else {
      const bool list = op == "Contains" || op == "Does Not Contain";
      if (spec.kind != (list ? OptionKind::List : OptionKind::Choice) || !contains(spec.choices, value)) {
        error = "Unknown value/option type mismatch: " + text; return false;
      }
      out.kind = op == "Is" ? Kind::Is : op == "Is Not" ? Kind::IsNot
        : op == "Contains" ? Kind::Contains : Kind::NotContains;
    }
    return true;
  }
  auto item = items_.find(text);
  if (item != items_.end() && catalog_.items[item->second].logic_item) {
    out.kind = Kind::Item; out.index = item->second; return true;
  }
  auto macro = macros_.find(text);
  if (macro != macros_.end()) { out.kind = Kind::Macro; out.index = macro->second; return true; }
  error = "Unknown condition/macro: " + text; return false;
}
bool Logic::parse(const std::string& text, Program& out, std::string& error) const {
  out = {};
  if (text.empty() || text.size() > MaxText) { error = "Empty/oversized expression"; return false; }
  std::vector<std::string> tokens;
  std::string current;
  bool quoted = false;
  for (char c : text) {
    if (c == '"') quoted = !quoted;
    if (!quoted && (c == '(' || c == ')' || c == '&' || c == '|')) {
      if (!trim(current).empty()) tokens.push_back(trim(current));
      current.clear(); tokens.emplace_back(1, c);
    } else current += c;
  }
  if (quoted) { error = "Unclosed quote"; return false; }
  if (!trim(current).empty()) tokens.push_back(trim(current));
  if (tokens.empty() || tokens.size() > MaxNodes) { error = "Empty/oversized token stream"; return false; }
  std::size_t pos = 0;
  std::function<bool(unsigned, std::size_t&)> group = [&](unsigned depth, std::size_t& root) {
    if (depth >= MaxDepth) { error = "Expression depth exceeds 128"; return false; }
    std::vector<std::size_t> children;
    char op = 0;
    // The pinned Wind Temple Many Cyclones macro contains "(& Iron Boots...)".
    // Upstream evaluates this as conjunction's true identity followed by its
    // operands. Preserve and explicitly accept that legacy leading AND form.
    // Leading OR, repeated operators and missing/trailing operands still fail.
    if (pos < tokens.size() && tokens[pos] == "&") { op = '&'; ++pos; }
    while (true) {
      if (pos == tokens.size() || tokens[pos] == ")" || tokens[pos] == "&" || tokens[pos] == "|") {
        error = "Missing operand"; return false;
      }
      std::size_t child;
      if (tokens[pos] == "(") {
        ++pos;
        if (!group(depth + 1, child)) return false;
        if (pos == tokens.size() || tokens[pos++] != ")") { error = "Unclosed parenthesis"; return false; }
      } else {
        Node leaf;
        if (!atom(tokens[pos++], leaf, error)) return false;
        child = out.nodes.size(); out.nodes.push_back(std::move(leaf));
      }
      children.push_back(child);
      if (pos == tokens.size() || tokens[pos] == ")") break;
      if (tokens[pos] != "&" && tokens[pos] != "|") { error = "Missing operator"; return false; }
      char next = tokens[pos++][0];
      if (op && op != next) { error = "Mixed & and | require parentheses at each nesting level"; return false; }
      op = next;
    }
    if (children.size() == 1) root = children[0];
    else {
      Node node; node.kind = op == '|' ? Kind::Any : Kind::All; node.children = std::move(children);
      root = out.nodes.size(); out.nodes.push_back(std::move(node));
    }
    return true;
  };
  if (!group(0, out.root)) return false;
  if (pos != tokens.size()) { error = "Unexpected closing parenthesis"; return false; }
  return true;
}
bool Logic::check_context(Run& run) const {
  for (const auto& [name, count] : run.context.items) {
    if (!spend(run)) return false;
    auto item = items_.find(name);
    if (item == items_.end() || !catalog_.items[item->second].logic_item || count > 4096)
      run.errors.push_back("Unknown/bounded context item: " + name);
  }
  for (const auto& [name, value] : run.context.options) {
    if (!spend(run)) return false;
    auto option = options_.find(name);
    if (option == options_.end()) { run.errors.push_back("Unknown context option: " + name); continue; }
    const auto& spec = catalog_.options[option->second];
    if (spec.kind == OptionKind::Boolean) {
      if (!std::holds_alternative<bool>(value)) run.errors.push_back("Context boolean type mismatch: " + name);
    } else if (spec.kind == OptionKind::Choice) {
      const auto* choice = std::get_if<std::string>(&value);
      if (!choice || !contains(spec.choices, *choice)) run.errors.push_back("Unknown/context choice type mismatch: " + name);
    } else {
      const auto* list = std::get_if<std::vector<std::string>>(&value);
      if (!list) run.errors.push_back("Context list type mismatch: " + name);
      else for (const auto& choice : *list) {
        if (!spend(run)) return false;
        if (!contains(spec.choices, choice)) run.errors.push_back("Unknown context list member: " + name);
      }
    }
  }
  return run.errors.empty();
}
bool Logic::spend(Run& run) const {
  if (run.exhausted) return false;
  if (run.work == MaxEvaluationWork) {
    run.exhausted = true;
    run.errors.push_back("Evaluation work exceeds 32768 visits");
    return false;
  }
  ++run.work;
  return true;
}
bool Logic::reference(bool is_macro, std::size_t index, Run& run, unsigned depth) const {
  if (!spend(run)) return false;
  const std::size_t slot = is_macro ? index : macro_programs_.size() + index;
  if (run.memo[slot] >= 2) { ++run.memo_hits; return run.memo[slot] == 3; }
  auto name = is_macro ? "macro " + catalog_.macros[index].name : "location " + catalog_.locations[index].name;
  if (contains(run.active, name)) { run.errors.push_back("Requirement cycle at " + name); return false; }
  if (depth >= MaxDepth) { run.errors.push_back("Evaluation depth exceeds 128 at " + name); return false; }
  run.memo[slot] = 1;
  run.active.push_back(name);
  const auto& p = is_macro ? macro_programs_[index] : location_programs_[index];
  bool result = evaluate(p, p.root, run, depth + 1);
  run.active.pop_back();
  if (!run.exhausted) run.memo[slot] = result ? 3 : 2;
  return result;
}
bool Logic::evaluate(const Program& program, std::size_t index, Run& run, unsigned depth) const {
  if (!spend(run)) return false;
  if (depth >= MaxDepth) { run.errors.push_back("Evaluation depth exceeds 128"); return false; }
  const auto& n = program.nodes[index];
  if (n.kind == Kind::All || n.kind == Kind::Any) {
    bool result = n.kind == Kind::All;
    for (auto child : n.children) {
      // Evaluate every branch so an invalid/missing option cannot hide behind OR.
      const bool value = evaluate(program, child, run, depth + 1);
      if (run.exhausted) break;
      result = n.kind == Kind::All ? result && value : result || value;
    }
    return result;
  }
  if (n.kind == Kind::True) return true;
  if (n.kind == Kind::False) return false;
  if (n.kind == Kind::Item) {
    auto found = run.context.items.find(catalog_.items[n.index].name);
    const std::uint32_t owned = found == run.context.items.end() ? 0 : found->second;
    return owned >= n.quantity;
  }
  if (n.kind == Kind::Macro) return reference(true, n.index, run, depth);
  if (n.kind == Kind::Location) return reference(false, n.index, run, depth);
  const auto& spec = catalog_.options[n.index];
  auto option = run.context.options.find(spec.name);
  if (option == run.context.options.end()) { run.errors.push_back("Missing explicit option: " + spec.name); return false; }
  const auto& value = option->second;
  if (n.kind == Kind::Enabled) return std::get<bool>(value);
  if (n.kind == Kind::Disabled) return !std::get<bool>(value);
  if (n.kind == Kind::Is) return std::get<std::string>(value) == n.value;
  if (n.kind == Kind::IsNot) return std::get<std::string>(value) != n.value;
  const auto& list = std::get<std::vector<std::string>>(value);
  return n.kind == Kind::Contains ? contains(list, n.value) : !contains(list, n.value);
}
Evaluation Logic::finish(const Program* program, bool is_macro, std::size_t index, const Context& context) const {
  if (!valid()) return {false, false, errors_};
  Run run{context, {}, {}, std::vector<unsigned char>(macro_programs_.size() + location_programs_.size(), 0)};
  if (!check_context(run)) return {false, false, std::move(run.errors), run.work, run.memo_hits};
  const bool result = program ? evaluate(*program, program->root, run, 0) : reference(is_macro, index, run, 0);
  const bool good = run.errors.empty();
  return {good, good && result, std::move(run.errors), run.work, run.memo_hits};
}
Evaluation Logic::macro(const std::string& name, const Context& context) const {
  auto found = macros_.find(name);
  if (found == macros_.end()) return {false, false, {"Unknown macro: " + name}};
  return finish(nullptr, true, found->second, context);
}
Evaluation Logic::location(const std::string& name, const Context& context) const {
  auto found = locations_.find(name);
  if (found == locations_.end()) return {false, false, {"Unknown location: " + name}};
  return finish(nullptr, false, found->second, context);
}
Evaluation Logic::expression(const std::string& text, const Context& context) const {
  Program program; std::string error;
  if (!parse(text, program, error)) return {false, false, {error}};
  return finish(&program, false, 0, context);
}
}
