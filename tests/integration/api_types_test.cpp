// The types of the API reference: the language they are written in, what read_api_reference
// refuses, how calls are written with them, and the Ruby type signatures (RBS) made from them.

#include <doctest/doctest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/api_rbs.hpp"
#include "project/api_reference.hpp"
#include "project/api_types.hpp"

using namespace redgem::tools;

namespace {

const std::vector<std::string> kClasses = {"Vector3", "GameObject", "Animation::Graph"};

// A small reference with one of each kind of parameter.
constexpr const char* kReference = R"json({
  "description": "d", "about": "a", "notes": "n",
  "classes": [
    {"name": "Mover", "kind": "class", "about": "moves", "methods": [
      {"name": "new", "static": true, "params": [
        {"name": "speed", "type": "Numeric", "optional": true, "default": "1.0"}
      ], "returns": {"type": "Mover"}, "about": "a mover"},
      {"name": "move", "params": [
        {"name": "to", "type": "Vector3 | nil"},
        {"name": "loop", "type": "bool", "keyword": true, "optional": true, "default": "false"},
        {"name": "done", "type": "^(Float) -> void", "block": true, "optional": true}
      ], "returns": {"type": "self"}, "about": "moves it"},
      {"name": "speed=", "params": [{"name": "value", "type": "Numeric"}], "returns": {"type": "Numeric"},
       "about": "sets it"}
    ]},
    {"name": "Vector3", "kind": "class", "about": "a point", "methods": []},
    {"name": "Runner", "kind": "class", "superclass": "Mover", "about": "runs", "methods": []},
    {"name": "Tools", "kind": "module", "about": "tools", "methods": [
      {"name": "tint", "static": true, "params": [
        {"name": "color", "type": "Color"},
        {"name": "names", "type": "String", "rest": true},
        {"name": "settings", "type": "untyped", "keyword": true, "rest": true}
      ], "returns": {"type": "Array[[Integer, Integer]]"}, "about": "tints"}
    ]}
  ]
})json";

nlohmann::json reference_data() { return nlohmann::json::parse(kReference); }

// What read_api_reference says is wrong with a reference; nothing when it reads it.
std::string refusal(const nlohmann::json& data) {
  try {
    read_api_reference(data.dump());
  } catch (const std::invalid_argument& refused) {
    return refused.what();
  }
  return {};
}

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

nlohmann::json& move_params(nlohmann::json& data) { return data["classes"][0]["methods"][1]["params"]; }

}  // namespace

TEST_CASE("the type language: the closed vocabulary, generics, tuples, unions and blocks") {
  for (const char* type : {"Integer",
                           "Float",
                           "Numeric",
                           "String",
                           "Symbol",
                           "bool",
                           "nil",
                           "untyped",
                           "self",
                           "Proc",
                           "Class",
                           "Color",
                           "Vector3",
                           "Animation::Graph",
                           "GameObject | nil",
                           "Array[String]",
                           "Hash[Symbol, Array[Integer]]",
                           "Range[Numeric]",
                           "[Vector3, Vector3] | nil",
                           "Array[[String, Numeric]]"}) {
    CHECK_MESSAGE(api_type_problem(type, kClasses, TypePlace::Parameter).empty(), type);
  }
  CHECK(api_type_problem("void", kClasses, TypePlace::Returned).empty());
  CHECK(api_type_problem("^(Float, GameObject | nil) -> void", kClasses, TypePlace::Block).empty());
  CHECK(api_type_problem("^() -> untyped", kClasses, TypePlace::Block).empty());

  CHECK(api_type_problem("Strin", kClasses, TypePlace::Parameter) == "unknown type Strin");
  CHECK(api_type_problem("Camera", kClasses, TypePlace::Parameter) == "unknown type Camera");
  CHECK(api_type_problem("void", kClasses, TypePlace::Parameter) == "void is only returned");
  CHECK(api_type_problem("Array", kClasses, TypePlace::Parameter) == "expected [");
  CHECK(api_type_problem("Hash[Symbol]", kClasses, TypePlace::Parameter) == "Hash takes 2");
  CHECK(api_type_problem("Array[String", kClasses, TypePlace::Parameter) == "expected ]");
  CHECK(api_type_problem("String |", kClasses, TypePlace::Parameter) == "unknown type (none)");
  CHECK(api_type_problem("String Symbol", kClasses, TypePlace::Parameter) == "unexpected Symbol");
  CHECK(api_type_problem("", kClasses, TypePlace::Returned) == "unknown type (none)");
  CHECK(api_type_problem("^(Float) -> void", kClasses, TypePlace::Parameter) ==
        "a block's type is only a block parameter's");
  CHECK(api_type_problem("Float", kClasses, TypePlace::Block) == "expected ^");
  CHECK(api_type_problem("^(Float) -> void | nil", kClasses, TypePlace::Block) == "unexpected |");
}

TEST_CASE("types as RBS writes them") {
  CHECK(rbs_type("Array[Color] | nil") == "Array[RedGem::color] | nil");
  CHECK(rbs_type("Hash[Symbol, Colors]") == "Hash[Symbol, Colors]");  // (only the name Color)
  CHECK(rbs_returned("GameObject | nil") == "(GameObject | nil)");    // (a bare | parts overloads there)
  CHECK(rbs_returned("Array[String | Symbol]") == "Array[String | Symbol]");
  CHECK(rbs_returned("[Color, Integer]") == "[RedGem::color, Integer]");
  CHECK(block_signature("^(Float) -> void") == "(Float) -> void");
  CHECK(rbs_block("^(Color, Integer) -> void") == "{ (RedGem::color, Integer) -> void }");
}

TEST_CASE("the reference reads typed parameters and returns") {
  const ApiReference reference = read_api_reference(kReference);
  REQUIRE(reference.classes.size() == 4);
  const ApiMethod& move = reference.classes[0].methods[1];
  REQUIRE(move.params.size() == 3);
  CHECK(move.params[0].type == "Vector3 | nil");
  CHECK(move.params[1].keyword);
  CHECK(move.params[1].default_value == "false");
  CHECK(move.params[2].block);
  CHECK(move.params[2].optional);
  CHECK(move.returns == "self");
  CHECK(reference.classes[2].superclass == "Mover");
  CHECK(reference.classes[3].is_module);
}

TEST_CASE("the reference refuses what it may not hold, saying where") {
  CHECK(refusal(reference_data()).empty());
  nlohmann::json data = reference_data();
  move_params(data)[0]["type"] = "Vectr3";
  CHECK(has(refusal(data), "Mover#move to: type Vectr3: unknown type Vectr3"));
  data = reference_data();
  data["classes"][0]["methods"][1]["args"] = "to, loop: false";  // the prose of before the types
  CHECK(has(refusal(data), "Mover#move: no field is called args"));
  data = reference_data();
  move_params(data)[1].erase("default");
  CHECK(has(refusal(data), "Mover#move loop: optional, with no default"));
  data = reference_data();
  move_params(data)[0]["default"] = "nil";
  CHECK(has(refusal(data), "Mover#move to: a default, but it is not optional"));
  data = reference_data();
  std::swap(move_params(data)[1], move_params(data)[2]);
  CHECK(has(refusal(data), "Mover#move: loop is out of place"));
  data = reference_data();
  move_params(data)[2]["type"] = "Proc";
  CHECK(has(refusal(data), "Mover#move done: type Proc: expected ^"));
  data = reference_data();
  data["classes"][3]["methods"][0]["params"][1]["optional"] = true;
  CHECK(has(refusal(data), "Tools.tint names: a rest is never optional"));
  data = reference_data();
  data["classes"][0]["methods"][2]["returns"]["type"] = "Vector";
  CHECK(has(refusal(data), "Mover#speed=: type Vector: unknown type Vector"));
  data = reference_data();
  data["classes"][3]["kind"] = "namespace";
  CHECK(has(refusal(data), "Tools: its kind is class or module, not namespace"));
  data = reference_data();
  data["classes"][3]["superclass"] = "Mover";
  CHECK(has(refusal(data), "Tools: it cannot come from Mover"));
  data = reference_data();
  data["classes"][0]["methods"][2].erase("returns");
  CHECK_THROWS(read_api_reference(data.dump()));  // (every method says what it returns)
}

TEST_CASE("calls written with and without their types") {
  const ApiReference reference = read_api_reference(kReference);
  const ApiClass& mover = reference.classes[0];
  const ApiClass& tools = reference.classes[3];
  CHECK(signature(mover, mover.methods[1]) == "Mover#move(to, loop: false, &done)");
  CHECK(arguments_of(mover.methods[1]) == "to, loop: false, &done");
  CHECK(typed_signature(mover, mover.methods[1]) ==
        "Mover#move(Vector3 | nil to, loop: bool = false) ?{ (Float) -> void } -> self");
  CHECK(typed_signature(mover, mover.methods[0]) == "Mover.new(Numeric speed = 1.0) -> Mover");
  CHECK(typed_signature(mover, mover.methods[2]) == "Mover#speed = Numeric value");
  CHECK(typed_signature(tools, tools.methods[0]) ==
        "Tools.tint(Color color, *String names, **untyped settings) -> Array[[Integer, Integer]]");
  CHECK(
      has(api_class_text(mover), "  move(Vector3 | nil to, loop: bool = false) ?{ (Float) -> void } -> self moves it"));
}

TEST_CASE("the type signatures (RBS) of a reference") {
  const std::string rbs = api_rbs(read_api_reference(kReference));
  CHECK(rbs.rfind("# The scripting API of RedGem games as Ruby type signatures (RBS)", 0) == 0);
  CHECK(has(rbs,
            "module RedGem\n  # A color: its name (:orange), or [r, g, b] from 0 to 255.\n"
            "  type color = Symbol | Array[Numeric]\nend\n"));
  CHECK(has(rbs,
            "\n# moves\nclass Mover\n"
            "  # a mover\n  def initialize: (?Numeric speed) -> void\n"
            "  # moves it\n  def move: (Vector3 | nil to, ?loop: bool) ?{ (Float) -> void } -> self\n"
            "  # sets it\n  def speed=: (Numeric value) -> Numeric\nend\n"));
  CHECK(has(rbs, "\n# a point\nclass Vector3\nend\n"));
  CHECK(has(rbs, "\n# runs\nclass Runner < Mover\nend\n"));
  CHECK(has(rbs,
            "\n# tools\nmodule Tools\n  # tints\n"
            "  def self.tint: (RedGem::color color, *String names, **untyped settings) -> "
            "Array[[Integer, Integer]]\nend\n"));
}

TEST_CASE("the engine's type signatures have every class and method of its reference") {
  const std::string rbs = api_rbs(api_reference());
  std::size_t methods = 0;
  for (const ApiClass& owner : api_reference().classes) {
    CHECK_MESSAGE(has(rbs, (owner.is_module ? "\nmodule " : "\nclass ") + owner.name), owner.name);
    methods += owner.methods.size();
  }
  std::size_t definitions = 0;
  for (std::size_t at = rbs.find("\n  def "); at != std::string::npos; at = rbs.find("\n  def ", at + 1)) ++definitions;
  CHECK(definitions == methods);
  CHECK(has(rbs, "  def self.axis: (Symbol name, ?pad: Integer) -> Float\n"));
  CHECK(has(rbs, "  def tween: (Numeric seconds, ?ease: Symbol) { (Float) -> void } -> Behaviour::Timer\n"));
  CHECK(has(rbs, "class GameMenu < Behaviour\n"));
}
