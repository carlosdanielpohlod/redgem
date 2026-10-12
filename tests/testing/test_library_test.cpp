#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "test_support.hpp"
#include "testing/ruby_tests.hpp"
#include "testing/system_test.hpp"
#include "testing/test_library.hpp"
#include "testing/test_notes.hpp"

using namespace redgem;
using namespace redgem::testing;
using redgem::test::fixture;
using redgem::test::LogCapture;

namespace {

constexpr float kMouseX = 12.0f;
constexpr float kMouseY = 34.0f;

std::string source_file(const std::string& relative) { return std::string(REDGEM_SOURCE_DIR) + "/" + relative; }

// A game running as the test tool runs it: its input comes from `input`.
std::unique_ptr<Engine> sandbox(ScriptedInputSource& input) {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.fixed_step = true;
  options.input = &input;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

SystemTestOptions platformer_test(const std::string& test_file) {
  SystemTestOptions options;
  options.game_dir = source_file("games/platformer");
  options.test_file = source_file(test_file);
  options.headless = true;
  return options;
}

}  // namespace

TEST_CASE("the test library gives a game's Ruby what tests are written with") {
  ScriptedInputSource input;
  auto engine = sandbox(input);
  CHECK(ruby(*engine, "Object.const_defined?(:SystemTest)") == "false");
  CHECK_FALSE(load_test_library(engine->scripts(), input).has_value());
  for (const char* name : {"TestInput", "SystemTest", "RedGemTest"}) {
    CAPTURE(name);
    CHECK(ruby(*engine, std::string("Object.const_defined?(:") + name + ")") == "true");
  }
  CHECK(ruby(*engine, "SystemTest.include?(RedGemTest::Assertions)") == "true");
}

TEST_CASE("TestInput sets what the game reads on its next frame") {
  ScriptedInputSource input;
  auto engine = sandbox(input);
  REQUIRE_FALSE(load_test_library(engine->scripts(), input).has_value());
  ruby(*engine, "TestInput.set(:a, true); TestInput.move_mouse(12.0, 34.0); TestInput.type('hi')");
  CHECK(input.is_key_down(Key::A));
  CHECK(input.mouse().x == doctest::Approx(kMouseX));
  CHECK(input.mouse().y == doctest::Approx(kMouseY));
  engine->frame(config::kFixedDeltaSeconds);
  CHECK(ruby(*engine, "[Input.down?(:a), Input.text, TestInput.down?(:a)].inspect") == "[true, \"hi\", true]");
  ruby(*engine, "TestInput.release_all");
  CHECK_FALSE(input.is_key_down(Key::A));
  CHECK(input.typed_text().empty());
}

TEST_CASE("TestInput takes names it knows, and Ruby cannot reach what it drives") {
  ScriptedInputSource input;
  auto engine = sandbox(input);
  REQUIRE_FALSE(load_test_library(engine->scripts(), input).has_value());
  CHECK(engine->scripts().eval("TestInput.set(:no_such_key, true)", "t.rb").error.find("unknown key") !=
        std::string::npos);
  CHECK(engine->scripts().eval("TestInput.set_gamepad_axis(:sideways, 1.0)", "t.rb").error.find("unknown gamepad") !=
        std::string::npos);
  CHECK(ruby(*engine, "TestInput.instance_variables.inspect") == "[]");
  CHECK_FALSE(engine->scripts().eval("TestInput.instance_variable_set('__driven_input', 1)", "t.rb").ok);
}

TEST_CASE("two games tested in one program each have their own input") {
  ScriptedInputSource first_input;
  ScriptedInputSource second_input;
  auto first = sandbox(first_input);
  auto second = sandbox(second_input);
  REQUIRE_FALSE(load_test_library(first->scripts(), first_input).has_value());
  REQUIRE_FALSE(load_test_library(second->scripts(), second_input).has_value());
  ruby(*first, "TestInput.set(:w, true)");
  CHECK(first_input.is_key_down(Key::W));
  CHECK_FALSE(second_input.is_key_down(Key::W));
  CHECK(ruby(*second, "TestInput.down?(:w)") == "false");
}

TEST_CASE("notes go from one test to another, by plain names only") {
  ScriptedInputSource input;
  auto engine = sandbox(input);
  REQUIRE_FALSE(load_test_library(engine->scripts(), input).has_value());
  ruby(*engine, "TestInput.leave_note('redgem_testing_tests', 'ABCD')");
  CHECK(ruby(*engine, "TestInput.note('redgem_testing_tests')") == "ABCD");
  CHECK(ruby(*engine, "TestInput.note('nobody_left_this_one').inspect") == "nil");
  CHECK_FALSE(engine->scripts().eval("TestInput.leave_note('../outside', 'x')", "t.rb").ok);
  CHECK_FALSE(engine->scripts().eval("TestInput.note('/etc/passwd')", "t.rb").ok);
}

TEST_CASE("a run with a folder of its own keeps its notes there, whole, away from every other run's") {
  const std::filesystem::path shared = notes_folder();
  const std::filesystem::path work = std::filesystem::temp_directory_path() / "redgem-testing-tests-work";
  std::filesystem::remove_all(work);
  ScriptedInputSource input;
  auto engine = sandbox(input);
  REQUIRE_FALSE(load_test_library(engine->scripts(), input).has_value());
  ruby(*engine, "TestInput.leave_note('shared_note', 'everyone')");
  keep_notes_in(notes_folder_in(work));
  CHECK(notes_folder().string().rfind(work.string(), 0) == 0);
  CHECK(ruby(*engine, "TestInput.note('shared_note').inspect") == "nil");
  ruby(*engine, "TestInput.leave_note('own_note', 'ours')");
  CHECK(ruby(*engine, "TestInput.note('own_note')") == "ours");
  // (Only the note itself is left: nothing half-written beside it.)
  CHECK(std::distance(std::filesystem::directory_iterator(notes_folder()), std::filesystem::directory_iterator()) == 1);
  keep_notes_in({});
  CHECK(notes_folder() == shared);
  CHECK(ruby(*engine, "TestInput.note('own_note').inspect") == "nil");
  std::filesystem::remove(shared / "shared_note");
  std::filesystem::remove_all(work);
}

TEST_CASE("a file of Ruby unit tests passes or fails as its tests do") {
  const LogCapture log;
  CHECK(run_ruby_tests(source_file("tests/ruby/vector3_test.rb")) == 0);
  CHECK(run_ruby_tests(source_file("tests/ruby/system_test_dsl_test.rb")) == 0);
  CHECK(run_ruby_tests(source_file("tests/ruby_failing/failing_test.rb")) == 1);
  CHECK(log.contains("FAIL HarnessFailureTest"));
  CHECK(run_ruby_tests(source_file("tests/ruby/no_such_test.rb")) == 1);
}

TEST_CASE("a system test plays the game and says how it went") {
  const LogCapture log;
  CHECK(run_system_test(platformer_test("tests/system/platformer/jump_test.rb")) == 0);
  CHECK(log.contains("system test passed"));
}

TEST_CASE("a system test that fails, or is not one, fails the run") {
  const LogCapture log;
  CHECK(run_system_test(platformer_test("tests/system_failing/failing_system_test.rb")) == 1);
  CHECK(log.contains("system test failed: "));
  CHECK(log.contains("this must fail"));
  CHECK(run_system_test(platformer_test("tests/system/platformer/no_such_test.rb")) == 1);
  CHECK(log.contains("cannot load system test: "));
  // A file of Ruby that defines no system test.
  CHECK(run_system_test(platformer_test("tests/ruby/vector3_test.rb")) == 1);
  CHECK(log.contains("system test did not call SystemTest.define"));
  CHECK_FALSE(log.contains("system test passed"));
}
