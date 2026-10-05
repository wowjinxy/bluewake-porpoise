// Public source catalog and passive logic only. No game, save or seed API.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace bluewake::randomizer {
enum class PathKind { DOLBytePatch, RELBytePatch, ActorResource, ChestResource,
                      CustomSymbolPatch, EventResource, ScalableObjectResource };
struct Path { PathKind kind; std::string raw; };
struct Item {
  std::string source_id, name;
  bool logic_item;
  // Exact name matches from upstream item_names.txt, descriptive only.
  // Progressive conversions and runtime/native award identities are separate.
  std::vector<std::uint8_t> native_name_ids;
};
struct Macro { std::string source_id, name, requirement; };
struct Location {
  std::string source_id, name, requirement, original_item;
  std::vector<std::string> types;
  std::vector<Path> paths; // One location, one reward, regardless of path count.
};
enum class OptionKind { Boolean, Choice, List };
struct OptionSpec { std::string name; OptionKind kind; std::vector<std::string> choices; };
struct Catalog {
  std::vector<Item> items;
  std::vector<Macro> macros;
  std::vector<Location> locations;
  std::vector<OptionSpec> options;
};
Catalog imported_catalog();
using OptionValue = std::variant<bool, std::string, std::vector<std::string>>;
struct Context {
  // Caller-provided hypothetical or observed data. No default starting items.
  // Keep this snapshot immutable for the duration of an evaluation call.
  std::map<std::string, std::uint32_t> items;
  std::map<std::string, OptionValue> options;
};
struct Evaluation {
  bool valid = false;
  bool reachable = false;
  std::vector<std::string> errors;
  // Per-call diagnostics, never retained across inventory/option snapshots.
  std::uint32_t work = 0, memo_hits = 0;
};
class Logic {
public:
  // Context entries/list members and evaluator/reference visits share this
  // budget. Exhaustion is invalid/unreachable, never a partial true result.
  static constexpr std::uint32_t MaxEvaluationWork = 32768;
  explicit Logic(Catalog catalog);
  bool valid() const { return errors_.empty(); }
  const std::vector<std::string>& errors() const { return errors_; }
  const Catalog& catalog() const { return catalog_; }
  Evaluation macro(const std::string& name, const Context& context) const;
  Evaluation location(const std::string& name, const Context& context) const;
  Evaluation expression(const std::string& text, const Context& context) const;
private:
  enum class Kind { All, Any, True, False, Item, Macro, Location,
                    Enabled, Disabled, Is, IsNot, Contains, NotContains };
  struct Node {
    Kind kind = Kind::False;
    std::size_t index = 0;
    std::uint32_t quantity = 1;
    std::string value;
    std::vector<std::size_t> children;
  };
  struct Program { std::vector<Node> nodes; std::size_t root = 0; };
  struct Run {
    const Context& context;
    std::vector<std::string> errors;
    std::vector<std::string> active;
    // 0 unseen, 1 active, 2 completed false, 3 completed true. Only this Run
    // owns the memo: caller contexts and future macro rewrites cannot reuse it.
    std::vector<unsigned char> memo;
    std::uint32_t work = 0, memo_hits = 0;
    bool exhausted = false;
  };
  Catalog catalog_;
  std::map<std::string, std::size_t> items_, macros_, locations_, options_;
  std::vector<Program> macro_programs_, location_programs_;
  std::vector<std::string> errors_;
  bool parse(const std::string& text, Program& out, std::string& error) const;
  bool atom(const std::string& text, Node& out, std::string& error) const;
  bool evaluate(const Program& program, std::size_t node, Run& run, unsigned depth) const;
  bool reference(bool macro, std::size_t index, Run& run, unsigned depth) const;
  bool check_context(Run& run) const;
  bool spend(Run& run) const;
  Evaluation finish(const Program* program, bool is_macro, std::size_t index,
                    const Context& context) const;
};
}
