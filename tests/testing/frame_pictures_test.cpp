// The pictures a test takes of the game it plays (t.screenshot).
#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <filesystem>
#include <fstream>

#include "app/engine.hpp"
#include "testing/frame_pictures.hpp"
#include "testing/system_test.hpp"
#include "testing/test_library.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::testing;
using redgem::test::fixture;
using redgem::test::LogCapture;

namespace {

std::unique_ptr<Engine> sandbox(ScriptedInputSource& input) {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.fixed_step = true;
  options.input = &input;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

// A folder of the test's own, removed when the test ends.
struct Scratch {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem_frame_pictures_test";
  Scratch() {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
  }
  ~Scratch() { std::filesystem::remove_all(dir); }
};

}  // namespace

TEST_CASE("a picture is written in the screenshots folder or not at all") {
  const std::filesystem::path folder = "/shots";
  const LogCapture log;
  CHECK(picture_file(folder, "title") == folder / "title.png");
  CHECK(picture_file(folder, "levels/first") == folder / "levels/first.png");
  CHECK_FALSE(picture_file(folder, "../../home/me/.bashrc").has_value());
  CHECK_FALSE(picture_file(folder, "/tmp/x").has_value());
  CHECK(log.count(log::Level::Warning) == 2);
}

TEST_CASE("pictures asked for wait for the next frame drawn, and are dropped when nobody draws it") {
  const LogCapture log;
  FramePictures pictures("/shots");
  CHECK(pictures.pending().empty());
  pictures.ask("start");
  pictures.ask("../outside");
  pictures.ask("end");
  CHECK(pictures.pending() == std::vector<std::filesystem::path>{"/shots/start.png", "/shots/end.png"});
  pictures.forget_pending();
  CHECK(pictures.pending().empty());
  pictures.save_pending();  // nothing asked for: no window is needed, nothing is written
}

TEST_CASE("TestScreen.capture is how a game's Ruby asks for a picture, in a game that is being tested") {
  ScriptedInputSource input;
  FramePictures pictures("/shots");
  auto engine = sandbox(input);
  CHECK_FALSE(engine->scripts().eval("TestScreen.capture('before')", "early.rb").ok);  // the engine has none
  REQUIRE_FALSE(load_test_library(engine->scripts(), input, &pictures).has_value());
  CHECK(engine->scripts().eval("TestScreen.capture('title')", "shot.rb").ok);
  CHECK(pictures.pending() == std::vector<std::filesystem::path>{"/shots/title.png"});
  CHECK_FALSE(engine->scripts().eval("Game.screenshot('title')", "old.rb").ok);  // not a game's business any more
}

TEST_CASE("t.screenshot asks for the picture and lets one frame pass") {
  ScriptedInputSource input;
  FramePictures pictures("/shots");
  auto engine = sandbox(input);
  REQUIRE_FALSE(load_test_library(engine->scripts(), input, &pictures).has_value());
  const RubyResult ticked = engine->scripts().eval(
      "SystemTest.define { |t| t.screenshot(:menu); t.screenshot('end') }; [SystemTest.current.__tick, "
      "SystemTest.current.__tick, SystemTest.current.__tick].inspect",
      "dsl.rb");
  REQUIRE_MESSAGE(ticked.ok, ticked.error);
  const std::string statuses = RSTRING_PTR(mrb_obj_as_string(engine->scripts().vm().state(), ticked.value));
  CHECK(statuses == "[:running, :running, :passed]");
  CHECK(pictures.pending() == std::vector<std::filesystem::path>{"/shots/menu.png", "/shots/end.png"});
}

TEST_CASE("a test played without a window passes all the same, and leaves no pictures") {
  const Scratch scratch;
  const std::filesystem::path test_file = scratch.dir / "shot_test.rb";
  std::ofstream(test_file) << "SystemTest.define do |t|\n  t.wait(2)\n  t.screenshot('nobody_sees')\nend\n";
  const LogCapture log;
  SystemTestOptions options;
  options.game_dir = std::string(REDGEM_SOURCE_DIR) + "/games/platformer";
  options.test_file = test_file;
  options.screenshot_dir = scratch.dir / "shots";
  options.headless = true;
  CHECK(run_system_test(options) == 0);
  CHECK(log.contains("system test passed"));
  CHECK_FALSE(std::filesystem::exists(scratch.dir / "shots"));
}
