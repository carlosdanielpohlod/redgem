// The API reference (docs/scripting/api.json) says what the engine really has: every public
// method the engine adds to Ruby is in it, everything in it is there, each class as the class or
// module it is and each method of the engine's Ruby with the parameters it takes; the manual's
// page and the type signatures are made from it, and the guide new games get lists calls that
// exist and compile.

#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/proc.h>
#include <mruby/string.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "app/engine.hpp"
#include "core/exit_codes.hpp"
#include "core/files.hpp"
#include "project/api_rbs.hpp"
#include "project/api_reference.hpp"
#include "project/closest_words.hpp"
#include "project/assistant_guide.hpp"
#include "project/game_build.hpp"
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
// Of the reference's methods, at least this many are written in Ruby, whose parameters Ruby can
// tell (the others are C++'s): with fewer, the test of parameters would be checking next to nothing.
constexpr std::size_t kLeastRubyMethods = 100;
constexpr const char* kTestFile = "api_reference_test.rb";
constexpr const char* kNewMethod = "new";
constexpr const char* kInitializeMethod = "initialize";

// What Ruby says of every class and module reachable from the top, nested ones included, each
// under its path: "Class.method" for a method of its own called on it, "Class#method" for one of
// its own called on its instances. And whether one of them has a method of its own or of a class
// it comes from (Object and what is above it aside: those are Ruby's), what it is, and how a
// method takes its arguments.
constexpr const char* kReflection = R"ruby(
  def __api_walk(space, name, seen, out)
    return if seen[space.object_id]
    seen[space.object_id] = true
    out << [name, space] unless name.empty?
    names = begin; space.constants; rescue NotImplementedError; []; end
    names.sort.each do |constant|
      value = space.const_get(constant)
      __api_walk(value, name.empty? ? constant.to_s : "#{name}::#{constant}", seen, out) if value.is_a?(Module)
    end
  end

  def __api_methods
    owners = []
    __api_walk(Object, "", {}, owners)
    owners.map do |name, owner|
      owner.singleton_methods(false).map { |m| "#{name}.#{m}" } +
        owner.public_instance_methods(false).map { |m| "#{name}##{m}" }
    end.flatten.join("\n")
  end

  def __api_owner(path) = path.split("::").inject(Object) { |space, part| space.const_get(part) }

  def __api_has?(path, method, on_class)
    owner = __api_owner(path)
    chain = owner.is_a?(Class) ? owner.ancestors.take_while { |above| above != Object } : [owner]
    return chain.any? { |each| each.public_instance_methods(false).include?(method) } unless on_class
    (method == :new && owner.is_a?(Class)) || chain.any? { |each| each.singleton_methods(false).include?(method) }
  end

  # "module", "class", or "class < Behaviour" for one that comes from another than Object.
  def __api_kind(path)
    owner = __api_owner(path)
    return "module" unless owner.is_a?(Class)

    owner.superclass == Object ? "class" : "class < #{owner.superclass}"
  end

  # A method's parameters as Proc#parameters says them, a word each: "req opt key key block". (The
  # kinds only: mruby counts each kind exactly, and finds their names elsewhere.)
  def __api_shape(parameters) = parameters.map { |kind, _name| kind.to_s }.join(" ")
)ruby";

std::string ruby_text(RubyVm& vm, const std::string& code) {
  const RubyResult result = vm.eval(code, kTestFile);
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

// Where two texts first part: "line 12: <one> | <other>"; nothing when they are the same. (A
// failure says that instead of both whole pages.)
std::string first_difference(const std::string& one, const std::string& other) {
  std::stringstream ones(one);
  std::stringstream others(other);
  std::string a;
  std::string b;
  for (std::size_t line = 1;; ++line) {
    const bool more_a = static_cast<bool>(std::getline(ones, a));
    const bool more_b = static_cast<bool>(std::getline(others, b));
    if (!more_a && !more_b) return one == other ? "" : "the end of the text";
    if (a != b || more_a != more_b) return "line " + std::to_string(line) + ": " + a + " | " + b;
  }
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

// Whether a class is one the engine keeps for itself (its timers, its network's plumbing).
bool is_internal_class(const ApiReference& reference, const std::string& name) {
  const std::vector<std::string>& internal = reference.internal_classes;
  return std::find(internal.begin(), internal.end(), name) != internal.end();
}

bool is_listed(const ApiReference& reference, const Named& named) {
  if (is_internal_class(reference, named.owner)) return true;
  const ApiClass* owner = find_api_class(reference, named.owner);
  if (owner == nullptr) return false;
  for (const ApiMethod& method : owner->methods) {
    if (method.name == named.method && method.on_class == named.on_class) return true;
  }
  return std::find(owner->internal.begin(), owner->internal.end(), named.method) != owner->internal.end();
}

// Whether the engine has that method (and that class, by a path such as "Animation::Graph").
bool exists(RubyVm& vm, const std::string& owner, const std::string& method, bool on_class) {
  const std::string call = "__api_has?(\"" + owner + "\", :\"" + method + "\", " + (on_class ? "true" : "false") + ")";
  return ruby_text(vm, call + ".to_s") == "true";
}

// What the reference says a class is, as __api_kind says it.
std::string documented_kind(const ApiClass& owner) {
  if (owner.is_module) return "module";
  return owner.superclass.empty() ? "class" : "class < " + owner.superclass;
}

// How a method takes its arguments: what the reference says, or what Ruby says.
struct Shape {
  std::size_t required = 0;
  std::size_t optional = 0;
  bool rest = false;
  std::size_t keywords = 0;
  bool keyword_rest = false;
  bool block = false;
};

void add_to_shape(Shape& shape, const ApiParam& param) {
  if (param.block) {
    shape.block = true;
  } else if (param.rest) {
    (param.keyword ? shape.keyword_rest : shape.rest) = true;
  } else if (param.keyword) {
    ++shape.keywords;
  } else {
    ++(param.optional ? shape.optional : shape.required);
  }
}

Shape documented_shape(const ApiMethod& method) {
  Shape shape;
  for (const ApiParam& param : method.params) add_to_shape(shape, param);
  return shape;
}

// One of the words __api_shape says.
void add_word_to_shape(Shape& shape, const std::string& word) {
  if (word == "req") ++shape.required;
  if (word == "opt") ++shape.optional;
  if (word == "rest") shape.rest = true;
  if (word == "key") ++shape.keywords;
  if (word == "keyrest") shape.keyword_rest = true;
  if (word == "block") shape.block = true;
}

// The proc Ruby runs for a method written in Ruby; nothing for one in C++ or one of mruby's own
// (Class#new is a proc with no class, which Ruby cannot even ask its parameters). An alias's
// proc leads to the method it names.
bool is_written_in_cpp(const mrb_method_t& body) { return MRB_METHOD_UNDEF_P(body) || MRB_METHOD_CFUNC_P(body); }
bool is_alias(const RProc* proc) { return proc != nullptr && MRB_PROC_ALIAS_P(proc); }
bool is_ruby_proc(const RProc* proc) { return proc != nullptr && proc->c != nullptr && !MRB_PROC_CFUNC_P(proc); }

const RProc* ruby_proc(const mrb_method_t& body) {
  if (is_written_in_cpp(body)) return nullptr;
  const RProc* proc = MRB_METHOD_PROC(body);
  while (is_alias(proc)) proc = proc->upper;
  return is_ruby_proc(proc) ? proc : nullptr;
}

bool is_class_new(const ApiMethod& method) { return method.on_class && method.name == kNewMethod; }

// The method Ruby runs for a call: for new, the new object's initialize.
mrb_method_t method_body(mrb_state* mrb, mrb_value found, const ApiMethod& method) {
  const bool is_new = is_class_new(method);
  const bool on_singleton = method.on_class && !is_new;
  RClass* klass = mrb_class_ptr(on_singleton ? mrb_singleton_class(mrb, found) : found);
  const char* name = is_new ? kInitializeMethod : method.name.c_str();
  return mrb_method_search_vm(mrb, &klass, mrb_intern_cstr(mrb, name));
}

// What Ruby says of a method's parameters; nothing for one written in C++, of which it cannot tell.
std::optional<Shape> ruby_shape(RubyVm& vm, const ApiClass& owner, const ApiMethod& method) {
  mrb_state* mrb = vm.state();
  const RubyResult found = vm.eval(owner.name, kTestFile);
  REQUIRE_MESSAGE(found.ok, found.error);
  const RProc* proc = ruby_proc(method_body(mrb, found.value, method));
  if (proc == nullptr) return std::nullopt;
  // (mrb_obj_value takes a void*; the proc is only read)
  const RubyResult parameters = vm.call(mrb_obj_value(const_cast<RProc*>(proc)), "parameters");  // NOLINT
  REQUIRE_MESSAGE(parameters.ok, owner.name + " " + method.name + ": " + parameters.error);
  const RubyResult words = vm.call(mrb_top_self(mrb), "__api_shape", {parameters.value});
  REQUIRE_MESSAGE(words.ok, words.error);
  Shape shape;
  std::stringstream stream(RSTRING_PTR(mrb_obj_as_string(mrb, words.value)));
  for (std::string word; stream >> word;) add_word_to_shape(shape, word);
  return shape;
}

// Whether the keywords agree: as many, or, for a method that takes **, the reference may name more
// (what the method passes on).
bool keywords_agree(const Shape& ruby, const Shape& documented) {
  return ruby.keyword_rest ? ruby.keywords <= documented.keywords : ruby.keywords == documented.keywords;
}

// What the reference says wrong about a method Ruby knows the parameters of; nothing when right.
// (It may say a block that the method yields to without naming it.)
std::string shape_mismatch(const Shape& ruby, const Shape& documented) {
  if (ruby.required != documented.required) return "the positional parameters it needs";
  if (ruby.optional != documented.optional) return "the positional parameters it may go without";
  if (ruby.rest != documented.rest) return "*rest";
  if (!keywords_agree(ruby, documented)) return "keywords";
  if (documented.keyword_rest && !ruby.keyword_rest) return "**rest";
  if (ruby.block && !documented.block) return "block";
  return {};
}

}  // namespace

TEST_CASE("every public method the engine adds to Ruby is in the API reference") {
  RubyVm bare;  // mruby with the engine's gems, and nothing of the engine
  ruby_text(bare, kReflection);
  const std::set<std::string> plain = lines_of(ruby_text(bare, "__api_methods"));
  const auto game = engine();
  RubyVm& vm = game->scripts().vm();
  ruby_text(vm, kReflection);
  std::vector<std::string> missing;
  for (const std::string& entry : lines_of(ruby_text(vm, "__api_methods"))) {
    const Named named = split(entry);
    const bool engine_own = plain.count(entry) == 0 && named.method.rfind(kInternalPrefix, 0) != 0;
    if (engine_own && !is_listed(api_reference(), named)) missing.push_back(entry);
  }
  CHECK_MESSAGE(missing.empty(), "not in docs/scripting/api.json: " << joined(missing));
}

TEST_CASE("everything the API reference lists is in the engine") {
  const auto game = engine();
  RubyVm& vm = game->scripts().vm();
  ruby_text(vm, kReflection);
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

TEST_CASE("each class of the API reference is the class or module the engine has") {
  const auto game = engine();
  RubyVm& vm = game->scripts().vm();
  ruby_text(vm, kReflection);
  for (const ApiClass& owner : api_reference().classes) {
    CHECK_MESSAGE(ruby_text(vm, "__api_kind(\"" + owner.name + "\")") == documented_kind(owner), owner.name);
  }
}

TEST_CASE("the API reference gives the engine's Ruby methods the parameters they take") {
  const auto game = engine();
  RubyVm& vm = game->scripts().vm();
  ruby_text(vm, kReflection);
  std::vector<std::string> wrong;
  std::size_t checked = 0;
  for (const ApiClass& owner : api_reference().classes) {
    for (const ApiMethod& method : owner.methods) {
      const std::optional<Shape> ruby = ruby_shape(vm, owner, method);
      if (!ruby) continue;
      ++checked;
      const std::string mismatch = shape_mismatch(*ruby, documented_shape(method));
      if (!mismatch.empty()) wrong.push_back(signature(owner, method) + " (" + mismatch + ")");
    }
  }
  CHECK(checked >= kLeastRubyMethods);
  CHECK_MESSAGE(wrong.empty(), "docs/scripting/api.json says other parameters than the engine: " << joined(wrong));
}

TEST_CASE("a method's shape: what the reference says against what Ruby says") {
  Shape ruby;
  ruby.required = 1;
  ruby.keywords = 1;
  Shape documented = ruby;
  CHECK(shape_mismatch(ruby, documented).empty());
  documented.keywords = 2;
  CHECK(shape_mismatch(ruby, documented) == "keywords");
  ruby.keyword_rest = true;  // ... unless Ruby takes **: the reference may name what it passes on
  CHECK(shape_mismatch(ruby, documented).empty());
  documented.block = true;  // (a block it yields to, unnamed)
  CHECK(shape_mismatch(ruby, documented).empty());
  ruby.block = true;
  documented.block = false;
  CHECK(shape_mismatch(ruby, documented) == "block");
  documented.optional = 1;
  CHECK(shape_mismatch(ruby, documented) == "the positional parameters it may go without");
}

TEST_CASE("the manual's API page and the type signatures are made from the reference") {
  const std::string json = source_text("docs/scripting/api.json");
  const ApiReference reference = read_api_reference(json);
  // (Run `redgem api --markdown > docs/scripting/api.md` and `redgem api --rbs >
  // docs/scripting/redgem.rbs` after changing the reference.)
  const std::string page = source_text("docs/scripting/api.md");
  CHECK_MESSAGE(page == api_markdown(reference), first_difference(page, api_markdown(reference)));
  const std::string signatures = source_text("docs/scripting/redgem.rbs");
  CHECK_MESSAGE(signatures == api_rbs(reference), first_difference(signatures, api_rbs(reference)));
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

TEST_CASE("a new game comes with its guide and the API's type signatures, which stay out of its builds") {
  const fs::path game = fs::temp_directory_path() / "redgem_api_guide_game";
  fs::remove_all(game);
  std::string error;
  REQUIRE_MESSAGE(create_game(game, kDefaultTemplate, error), error);
  std::string read_error;
  CHECK(read_text_file(game / "AGENTS.md", read_error) == assistant_guide());
  CHECK(read_text_file(game / "CLAUDE.md", read_error).value_or("").find("@AGENTS.md") != std::string::npos);
  CHECK(read_text_file(game / kSignaturesFile, read_error) == api_rbs(api_reference()));
  const std::vector<fs::path> shipped = shipped_files(game);
  CHECK(std::find(shipped.begin(), shipped.end(), fs::path(kSignaturesFile)) == shipped.end());
  fs::remove_all(game);
}

TEST_CASE("redgem api: an index, a class with its types, a search, the type signatures") {
  const auto api = [](const std::vector<std::string>& words) {
    std::vector<std::string> line{"api"};
    line.insert(line.end(), words.begin(), words.end());
    return *run_tool_command(line);
  };
  CHECK(api({}).out.find("Transform") != std::string::npos);
  CHECK(api({"transform"}).out.find("rotate(Vector3 degrees) -> void") != std::string::npos);
  CHECK(api({"Save"}).out.find("Save[String | Symbol key] = untyped value") != std::string::npos);
  CHECK(api({"Input"}).out.find("Input.axis(Symbol name, pad: Integer = 0) -> Float") != std::string::npos);
  CHECK(api({"Behaviour"}).out.find("tween(Numeric seconds, ease: Symbol = :smooth) { (Float) -> void }") !=
        std::string::npos);
  CHECK(api({"--search", "RAYCAST"}).out.find("Physics.raycast(Vector3 origin") != std::string::npos);
  CHECK(api({"--search", "zzzz"}).out.find("no method has to do with zzzz") != std::string::npos);
  CHECK(api({"Trasform"}).err == "no class Trasform. Did you mean Transform?");
  CHECK(api({"Input", "--json"}).out.find("\"name\":\"Input\"") != std::string::npos);
  CHECK(api({"Input", "--json"}).out.find("\"returns\":{\"type\":\"Float\"}") != std::string::npos);
  CHECK(api({"--search", "raycast", "--json"}).out.find("\"class\":\"Physics\"") != std::string::npos);
  CHECK(api({"--rbs"}).out == api_rbs(api_reference()));
  CHECK(api({"Transform", "--rbs"}).exit_code == kExitUsage);  // (signatures are of the whole API)
  CHECK(api({"--search", "ray", "--rbs"}).exit_code == kExitUsage);
  CHECK(api({"--search", "ray"}).out.find("GameObject#children") == std::string::npos);  // (not Array's "ray")
  CHECK(api({"a", "b"}).exit_code == kExitUsage);
}
