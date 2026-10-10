#include <doctest/doctest.h>

#include "app/cli.hpp"
#include "core/config.hpp"

using namespace redgem;

TEST_CASE("game directory with a system test") {
  const CliResult result = parse_cli({"games/demo", "--test", "t.rb", "--screenshots", "shots", "--headless"});
  REQUIRE(result.ok);
  CHECK(result.options.mode == RunMode::Game);  // tests always play the game
  CHECK(result.options.game_dir == "games/demo");
  CHECK(result.options.test_script == std::filesystem::path("t.rb"));
  CHECK(result.options.screenshot_dir == "shots");
  CHECK(result.options.headless);
}

TEST_CASE("a game directory alone opens the editor; --play runs the game") {
  CHECK(parse_cli({"games/demo"}).options.mode == RunMode::Editor);
  CHECK(parse_cli({"games/demo", "--play"}).options.mode == RunMode::Game);
  CHECK_FALSE(parse_cli({"--play"}).ok);
}

TEST_CASE("defaults") {
  const CliResult result = parse_cli({"games/demo"});
  REQUIRE(result.ok);
  CHECK_FALSE(result.options.test_script.has_value());
  CHECK(result.options.screenshot_dir == config::kDefaultScreenshotDir);
}

TEST_CASE("ruby test mode needs no game") {
  const CliResult result = parse_cli({"--ruby-test", "x_test.rb"});
  REQUIRE(result.ok);
  CHECK(result.options.mode == RunMode::RubyTests);
  CHECK(result.options.ruby_test_file == "x_test.rb");
}

TEST_CASE("errors") {
  CHECK(parse_cli({}).ok);  // no game: the editor opens its project hub
  CHECK(parse_cli({}).options.mode == RunMode::Editor);
  CHECK_FALSE(parse_cli({"--test"}).ok);
  CHECK_FALSE(parse_cli({"a", "b"}).ok);
  CHECK_FALSE(parse_cli({"a", "--bogus"}).ok);
  CHECK(parse_cli({"--help"}).options.mode == RunMode::Help);
}

TEST_CASE("new and build subcommands") {
  const CliResult created = parse_cli({"new", "games/mine", "--template", "platformer"});
  REQUIRE(created.ok);
  CHECK(created.options.mode == RunMode::NewGame);
  CHECK(created.options.game_dir == "games/mine");
  CHECK(created.options.template_name == "platformer");
  CHECK(parse_cli({"new", "games/mine"}).options.template_name == "empty");
  const CliResult built = parse_cli({"build", "games/mine", "dist/mine"});
  REQUIRE(built.ok);
  CHECK(built.options.mode == RunMode::BuildGame);
  CHECK(built.options.build_dir == "dist/mine");
  CHECK_FALSE(parse_cli({"build", "games/mine"}).ok);
  CHECK_FALSE(parse_cli({"new"}).ok);
}

TEST_CASE("--gpu picks the graphics card for one run and rejects unknown names") {
  const CliResult chosen = parse_cli({"games/demo", "--gpu", "integrated"});
  REQUIRE(chosen.ok);
  CHECK(chosen.options.gpu == std::string("integrated"));
  CHECK(chosen.options.mode == RunMode::Editor);
  CHECK_FALSE(parse_cli({"games/demo"}).options.gpu.has_value());
  CHECK_FALSE(parse_cli({"games/demo", "--gpu", "turbo"}).ok);
  CHECK_FALSE(parse_cli({"games/demo", "--gpu"}).ok);
}

TEST_CASE("one thing per command line: conflicting modes and swallowed options are errors") {
  CHECK_FALSE(parse_cli({"new", "games/x", "--play"}).ok);
  CHECK_FALSE(parse_cli({"build", "games/x", "out", "--headless"}).ok);
  CHECK_FALSE(parse_cli({"games/x", "--play", "--ruby-test", "t.rb"}).ok);
  CHECK_FALSE(parse_cli({"games/x", "--test", "--play"}).ok);      // --play is not a test file
  CHECK_FALSE(parse_cli({"games/x", "--headless"}).ok);            // nothing would ever end the run
  CHECK(parse_cli({"games/x", "--play", "--test", "t.rb"}).ok);    // both mean "run the game"
  const CliResult help = parse_cli({"--help", "--play"});
  REQUIRE(help.ok);
  CHECK(help.options.mode == RunMode::Help);
}

TEST_CASE("game folders are normalized: a trailing slash does not lose the game's name") {
  CHECK(parse_cli({"games/demo/"}).options.game_dir == std::filesystem::path("games/demo"));
  CHECK(parse_cli({"./games/../games/demo"}).options.game_dir == std::filesystem::path("games/demo"));
  const CliResult build = parse_cli({"build", "games/demo/", "dist/demo/"});
  REQUIRE(build.ok);
  CHECK(build.options.build_dir.filename() == "demo");
}
