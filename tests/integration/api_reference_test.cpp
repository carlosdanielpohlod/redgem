// The API reference (docs/scripting/api.json) says what the engine really has: every public
// method the engine adds to Ruby is in it, everything in it is there, the manual's page is made
// from it, and the guide new games get lists calls that exist and compile.

#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "app/engine.hpp"
#include "core/exit_codes.hpp"
#include "core/files.hpp"
#include "project/api_reference.hpp"
#include "project/closest_words.hpp"
#include "project/assistant_guide.hpp"
#include "project/game_templates.hpp"
#include "project/tool_commands.hpp"
#include "script/ruby_vm.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::tools;
namespace fs = std::filesystem;

namespace {

const fs::path kSource = REDGEM_SOURCE_DIR;
constexpr std::size_t kMostGuideLines = 120;
constexpr std::size_t kApiCalls = 25;
constexpr const char* kInternalPrefix = "__";  // the engine's own plumbing, never a game's

// Every public method of every top-level class and module, and of the nested ones the reference
// lists (the others are the engine's plumbing), as "Class.method" (called on the class) or
// "Class#method" (on an instance). `NESTED` is the names of those nested ones.
constexpr const char* kListMethods = R"ruby(
  owners = Object.constants.map(&:to_s) + NESTED
  owners.sort.map do |name|
    owner = name.split("::").inject(Object) { |space, part| space.const_get(part) }
    next [] unless owner.is_a?(Module)
    owner.singleton_methods.map { |m| "#{name}.#{m}" } + owner.public_instance_methods(false).map { |m| "#{name}##{m}" }
  end.flatten.join("\n")
)ruby";

// The nested classes the reference lists, as Ruby: NESTED = ["Animation::Graph", ...].
std::string nested_classes() {
  std::string list;
  for (const ApiClass& owner : api_reference().classes) {
    if (owner.name.find("::") != std::string::npos) list += (list.empty() ? "\"" : ", \"") + owner.name + "\"";
  }
  return "NESTED = [" + list + "]\n";
}

std::string ruby_text(RubyVm& vm, const std::string& code) {
  const RubyResult result = vm.eval(code, "api_reference_test.rb");
  REQUIRE_MESSAGE(result.ok, code << ": " << result.error);
  return RSTRING_PTR(mrb_obj_as_string(vm.state(), result.value));
}

std::set<std::string> lines_of(const std::string& text) {
  std::set<std::string> lines;
  std::stringstream stream(text);
  for (std::string line; std::getline(stream, line);) lines.insert(line);
  return lines;
}

// The engine playing a game with no scripts of its own: what Ruby has then is the engine's.
std::unique_ptr<Engine> engine() {
  const fs::path game = fs::temp_directory_path() / "redgem_api_reference_game";
  fs::remove_all(game);
  fs::create_directories(game);
  fs::copy_file(fs::path(REDGEM_FIXTURES_DIR) / "games/sandbox/scenes/level2.glb", game / "scene.glb");
  EngineOptions options;
  options.game_dir = game;
  options.fixed_step = true;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

// A text with its lines ended by '\n' alone (a Windows checkout ends them with "\r\n").
std::string without_returns(std::string text) {
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
  return text;
}

// A file of the repository, as its lines read.
std::string source_text(const char* file) {
  std::string error;
  const auto text = read_text_file(kSource / file, error);
  REQUIRE_MESSAGE(text.has_value(), error);
  return without_returns(*text);
}

// "Class.method" or "Class#method" split at its separator.
struct Named {
  std::string owner;
  std::string method;
  bool on_class;
};
Named split(const std::string& entry) {
  const std::size_t at = entry.find_first_of(".#");
  return {entry.substr(0, at), entry.substr(at + 1), entry[at] == '.'};
}

bool is_listed(const ApiReference& reference, const Named& named) {
  const ApiClass* owner = find_api_class(reference, named.owner);
  if (owner == nullptr) return false;
  for (const ApiMethod& method : owner->methods) {
    if (method.name == named.method && method.on_class == named.on_class) return true;
  }
  return std::find(owner->internal.begin(), owner->internal.end(), named.method) != owner->internal.end();
}

// Whether the engine has that method (and that class, by a path such as "Animation::Graph").
bool exists(RubyVm& vm, const std::string& owner, const std::string& method, bool on_class) {
  const std::string klass = "\"" + owner + "\".split(\"::\").inject(Object) { |space, part| space.const_get(part) }";
  const std::string name = ":\"" + method + "\"";
  const std::string question =
      on_class ? ".respond_to?(" + name + ")" : ".public_instance_methods.include?(" + name + ")";
  return ruby_text(vm, "(" + klass + ")" + question + ".to_s") == "true";
}

}  // namespace

TEST_CASE("every public method the engine adds to Ruby is in the API reference") {
  RubyVm bare;                     // mruby with the engine's gems, and nothing of the engine
  ruby_text(bare, "NESTED = []");  // (none of them is mruby's own)
  const std::set<std::string> plain = lines_of(ruby_text(bare, kListMethods));
  const auto game = engine();
  std::vector<std::string> missing;
  ruby_text(game->scripts().vm(), nested_classes());
  for (const std::string& entry : lines_of(ruby_text(game->scripts().vm(), kListMethods))) {
    const Named named = split(entry);
    const bool engine_own = plain.count(entry) == 0 && named.method.rfind(kInternalPrefix, 0) != 0;
    if (engine_own && !is_listed(api_reference(), named)) missing.push_back(entry);
  }
  CHECK_MESSAGE(missing.empty(), "not in docs/scripting/api.json: " << joined(missing));
}

TEST_CASE("everything the API reference lists is in the engine") {
  const auto game = engine();
  RubyVm& vm = game->scripts().vm();
  std::vector<std::string> gone;
  for (const ApiClass& owner : api_reference().classes) {
    for (const ApiMethod& method : owner.methods) {
      if (!exists(vm, owner.name, method.name, method.on_class)) gone.push_back(signature(owner, method));
    }
    for (const std::string& method : owner.internal) {
      if (!exists(vm, owner.name, method, false) && !exists(vm, owner.name, method, true))
        gone.push_back(owner.name + " " + method);
    }
  }
  CHECK_MESSAGE(gone.empty(), "in docs/scripting/api.json but not in the engine: " << joined(gone));
}

TEST_CASE("the manual's API page is made from the reference") {
  const std::string json = source_text("docs/scripting/api.json");
  const std::string page = source_text("docs/scripting/api.md");
  // (Run `redgem api --markdown > docs/scripting/api.md` after changing the reference.)
  CHECK(page == api_markdown(read_api_reference(json)));
  CHECK(without_returns(api_reference_json()) == json);  // what the program has built in is the file
}

TEST_CASE("the guide of new games lists 25 calls that exist, with examples that compile") {
  const std::vector<ApiCall> calls = most_used_calls(api_reference());
  REQUIRE(calls.size() == kApiCalls);
  RubyVm vm;
  for (std::size_t rank = 0; rank < calls.size(); ++rank) {
    CHECK(calls[rank].method->top == static_cast<int>(rank + 1));
    const RubyResult compiled = vm.check_syntax(calls[rank].method->example, "example.rb");
    CHECK_MESSAGE(compiled.ok, calls[rank].method->example << ": " << compiled.error);
  }
  const std::string guide = assistant_guide();
  CHECK(guide.find("{{") == std::string::npos);
  CHECK(guide.find("- `Behaviour#start`: called once, before the first update.") != std::string::npos);
  CHECK(static_cast<std::size_t>(std::count(guide.begin(), guide.end(), '\n')) <= kMostGuideLines);
}

TEST_CASE("a new game comes with its guide") {
  const fs::path game = fs::temp_directory_path() / "redgem_api_guide_game";
  fs::remove_all(game);
  std::string error;
  REQUIRE_MESSAGE(create_game(game, kDefaultTemplate, error), error);
  std::string read_error;
  CHECK(read_text_file(game / "AGENTS.md", read_error) == assistant_guide());
  CHECK(read_text_file(game / "CLAUDE.md", read_error).value_or("").find("@AGENTS.md") != std::string::npos);
  fs::remove_all(game);
}

TEST_CASE("redgem api: an index, a class, a search") {
  const auto api = [](const std::vector<std::string>& words) {
    std::vector<std::string> line{"api"};
    line.insert(line.end(), words.begin(), words.end());
    return *run_tool_command(line);
  };
  CHECK(api({}).out.find("Transform") != std::string::npos);
  CHECK(api({"transform"}).out.find("rotate(degrees)") != std::string::npos);
  CHECK(api({"Save"}).out.find("Save[key] = value") != std::string::npos);
  CHECK(api({"--search", "RAYCAST"}).out.find("Physics.raycast(origin") != std::string::npos);
  CHECK(api({"--search", "zzzz"}).out.find("no method has to do with zzzz") != std::string::npos);
  CHECK(api({"Trasform"}).err == "no class Trasform. Did you mean Transform?");
  CHECK(api({"Input", "--json"}).out.find("\"name\":\"Input\"") != std::string::npos);
  CHECK(api({"--search", "raycast", "--json"}).out.find("\"class\":\"Physics\"") != std::string::npos);
  CHECK(api({"a", "b"}).exit_code == kExitUsage);
}
