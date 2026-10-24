// What a network message carries, between Ruby values and the JSON that travels.

#include <doctest/doctest.h>

#include <filesystem>

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#include <nlohmann/json.hpp>

#include "app/engine.hpp"
#include "script/bindings/message_values.hpp"
#include "storage/save_file.hpp"
#include "test_support.hpp"

using namespace redgem;
using nlohmann::json;
using redgem::test::fixture;

namespace {

constexpr const char* kArrivedGlobal = "$arrived";  // where the test puts what value_from made
constexpr int kTooDeep = 20;                        // more levels than a message's data may have

std::unique_ptr<Engine> engine() {
  static const SceneDesc scene;
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.fixed_step = true;
  options.scene = &scene;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

// What Ruby makes of `code`, as the JSON a message would carry.
json sent(Engine& engine, const std::string& code) {
  const RubyResult made = engine.scripts().eval(code, "sent.rb");
  REQUIRE_MESSAGE(made.ok, made.error);
  return json_from(engine.scripts().vm().state(), made.value);
}

// Hands Ruby what arrived as JSON, in a global the test's Ruby code can look at.
void arrive(Engine& engine, const json& data) {
  mrb_state* mrb = engine.scripts().vm().state();
  mrb_gv_set(mrb, mrb_intern_cstr(mrb, kArrivedGlobal), value_from(mrb, data));
}

bool refuses(Engine& engine, const std::string& code, const std::string& message) {
  const RubyResult result = engine.scripts().eval(code, "refused.rb");
  return !result.ok && result.error.find(message) != std::string::npos;
}

}  // namespace

TEST_CASE("a message carries nothing, flags, numbers, names, lists, hashes and vectors") {
  const auto game = engine();
  CHECK(sent(*game, "nil") == json(nullptr));
  CHECK(sent(*game, "true") == json(true));
  CHECK(sent(*game, "false") == json(false));
  CHECK(sent(*game, "7") == json(7));
  CHECK(sent(*game, "2.5") == json(2.5));
  CHECK(sent(*game, "'text'") == json("text"));
  CHECK(sent(*game, ":name") == json("name"));
  CHECK(sent(*game, "[1, [true, nil]]") == json::array({1, json::array({true, nullptr})}));
  CHECK(sent(*game, "{ hp: 3, 'who' => :me }") == json{{"hp", 3}, {"who", "me"}});
  CHECK(sent(*game, "Vector3.new(1, 2, 3)") == json{{"$v", {1.0, 2.0, 3.0}}});
}

TEST_CASE("what a message cannot carry is refused, saying why") {
  const auto game = engine();
  CHECK(refuses(*game, "Network.__emit('n', Object.new, 0)", "a message cannot carry"));
  CHECK(refuses(*game, "Network.__emit('n', { 1 => 2 }, 0)", "a message's hash keys must be names"));
  CHECK(refuses(*game, "Network.__emit('n', { ('k' * 100) => 2 }, 0)", "a message's hash key is too long"));
  CHECK(refuses(*game, "Network.__emit('n' * 100, 1, 0)", "a message's name is too long"));
  CHECK(refuses(*game, "Network.__emit('n', Array.new(5000, 0), 0)", "holds too many things"));
  const std::string nested = std::string(kTooDeep, '[') + std::string(kTooDeep, ']');
  CHECK(refuses(*game, "Network.__emit('n', " + nested + ", 0)", "a message's data is nested too deep"));
}

TEST_CASE("Save keeps arrays and hashes of any size, which a message could not carry, but no deeper") {
  const std::filesystem::path save = std::filesystem::temp_directory_path() / "redgem-message-values-save.json";
  remove_save(save);
  static const SceneDesc scene;
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.scene = &scene;
  options.save_file = save;
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& game = *std::get<std::unique_ptr<Engine>>(created);
  CHECK(ruby(game, "Save['many'] = Array.new(5000, 1); Save['many'].size") == "5000");
  CHECK(ruby(game, "h = {}; 5000.times { |i| h[\"user-#{i}\"] = i }; Save['players'] = h; Save['players'].size") ==
        "5000");
  const std::string nested = std::string(kTooDeep, '[') + std::string(kTooDeep, ']');
  CHECK(refuses(game, "Save['deep'] = " + nested, "a saved value's data is nested too deep"));

  // Save.update: the block makes the new value of the one there (or of the default), which it returns.
  CHECK(ruby(game, "Save.update('visits', 0) { |n| n + 1 }") == "1");
  CHECK(ruby(game, "Save.update('visits', 0) { |n| n + 1 }; Save['visits']") == "2");
  CHECK(ruby(game, "Save.update('seen') { |seen| seen.inspect }") == "nil");
  CHECK(refuses(game, "Save.update('visits') { |n| raise 'no' }", "no"));
  CHECK(ruby(game, "Save['visits']") == "2");  // a block that failed changed nothing
  CHECK(ruby(game, "Save.update('visits') { nil }; Save.keys.include?(:visits)") == "false");
  CHECK(refuses(game, "Save.update('visits')", "needs a block"));
  remove_save(save);
}

TEST_CASE("what arrives becomes Ruby values again, a vector as a Vector3") {
  const auto game = engine();
  ruby(*game, "[:hp, :ratio, :on, :none, :list]");  // names the game's scripts use
  arrive(*game, json{{"hp", 3}, {"ratio", 0.5}, {"on", true}, {"none", nullptr}, {"list", {1, "two"}}});
  CHECK(ruby(*game, "$arrived.class") == "Hash");
  CHECK(ruby(*game, "$arrived[:hp]") == "3");
  CHECK(ruby(*game, "$arrived[:ratio]") == "0.5");
  CHECK(ruby(*game, "$arrived[:on]") == "true");
  CHECK(ruby(*game, "$arrived[:none].nil?") == "true");
  CHECK(ruby(*game, "$arrived[:list][1]") == "two");

  arrive(*game, json{{"$v", {1.0, 2.0, 3.0}}});
  CHECK(ruby(*game, "$arrived.class") == "Vector3");
  CHECK(ruby(*game, "$arrived.z") == "3.0");
  // Only three numbers under that key, alone, are a vector.
  arrive(*game, json{{"$v", {1.0, 2.0}}});
  CHECK(ruby(*game, "$arrived.class") == "Hash");
  arrive(*game, json{{"$v", {1.0, 2.0, 3.0}}, {"more", 1}});
  CHECK(ruby(*game, "$arrived.class") == "Hash");
}

// Another machine's numbers past what a float holds would be undefined as a Vector3's parts.
TEST_CASE("a vector whose numbers a float cannot hold arrives as a plain hash") {
  constexpr double kPastAFloat = 1e300;
  const auto game = engine();
  for (const double part : {kPastAFloat, -kPastAFloat}) {
    arrive(*game, json{{"$v", {part, 0.0, 0.0}}});
    CHECK(ruby(*game, "$arrived.class") == "Hash");
  }
  arrive(*game, json::parse(R"({"$v": [1e400, 0, 0]})", nullptr, false));
  CHECK(ruby(*game, "$arrived.class") != "Vector3");
}

TEST_CASE("a key no script of the game uses arrives as a text, not as a new symbol") {
  const auto game = engine();
  arrive(*game, json{{"hp", 1}, {"a key nobody here ever wrote", 2}});
  CHECK(ruby(*game, "$arrived.keys.map { |key| key.class.to_s }.sort.join(' ')") == "String Symbol");
  CHECK(ruby(*game, "$arrived['a key nobody here ever wrote']") == "2");
}
