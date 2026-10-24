#include <doctest/doctest.h>

#include "core/config.hpp"
#include "testing/test_cli.hpp"
#include "testing/testing_config.hpp"

using namespace redgem::testing;

TEST_CASE("a game and its system test, with where the pictures go and no window") {
  const TestCliResult result = parse_test_cli({"games/demo/", "t.rb", "--screenshots", "shots", "--headless"});
  REQUIRE(result.ok);
  CHECK(result.options.mode == TestMode::SystemTest);
  CHECK(result.options.game_dir == "games/demo");
  CHECK(result.options.test_file == "t.rb");
  CHECK(result.options.screenshot_dir == "shots");
  CHECK(result.options.headless);
}

TEST_CASE("a system test by default opens a window and keeps its pictures in the usual folder") {
  const TestCliResult result = parse_test_cli({"games/demo", "t.rb"});
  REQUIRE(result.ok);
  CHECK_FALSE(result.options.headless);
  CHECK(result.options.screenshot_dir == redgem::config::kDefaultScreenshotDir);
  CHECK_FALSE(result.options.gpu.has_value());
  CHECK(parse_test_cli({"--gpu", "integrated", "games/demo", "t.rb"}).options.gpu == std::string("integrated"));
}

TEST_CASE("a file of Ruby unit tests needs no game") {
  const TestCliResult result = parse_test_cli({"--ruby", "x_test.rb"});
  REQUIRE(result.ok);
  CHECK(result.options.mode == TestMode::RubyTests);
  CHECK(result.options.test_file == "x_test.rb");
  CHECK(result.options.game_dir.empty());
}

TEST_CASE("a file of Ruby unit tests can use a game's own classes") {
  const TestCliResult result = parse_test_cli({"--ruby", "x_test.rb", "--game", "games/demo/"});
  REQUIRE(result.ok);
  CHECK(result.options.mode == TestMode::RubyTests);
  CHECK(result.options.scripts_game == "games/demo");
  CHECK(result.options.game_dir.empty());
  CHECK(parse_test_cli({"--ruby", "x_test.rb"}).options.scripts_game.empty());
  CHECK(parse_test_cli({"--ruby", "x_test.rb", "--game"}).error == "--game needs a game's folder");
  CHECK(parse_test_cli({"games/demo", "t.rb", "--game", "games/demo"}).error ==
        "--game is for Ruby tests (a system test names its game first)");
  CHECK(test_usage().find("redgem-test --ruby <test_file.rb> [--game <game_dir>]") != std::string::npos);
}

TEST_CASE("help wins over anything else on the line, and says how the tool is used") {
  for (const char* help : {"--help", "-h"}) {
    const TestCliResult result = parse_test_cli({"games/demo", help, "--bogus"});
    REQUIRE(result.ok);
    CHECK(result.options.mode == TestMode::Help);
    CHECK(result.options.game_dir.empty());
  }
  const std::string usage = test_usage();
  CHECK(usage.find("redgem-test <game_dir> <system_test.rb> [--screenshots <dir>] [--headless] [--work <dir>]") !=
        std::string::npos);
  CHECK(usage.find("redgem-test --ruby <test_file.rb>") != std::string::npos);
  CHECK(usage.find("development tool") != std::string::npos);
}

TEST_CASE("what is wrong with a test command line is said") {
  CHECK(parse_test_cli({}).error == "missing <game_dir>");
  CHECK(parse_test_cli({"games/demo"}).error == "missing <system_test.rb>");
  CHECK(parse_test_cli({"games/demo", "t.rb", "more"}).error == "unexpected argument more");
  CHECK(parse_test_cli({"--ruby"}).error == "--ruby needs a file");
  CHECK(parse_test_cli({"--ruby", "--headless"}).error == "--ruby needs a file");
  CHECK(parse_test_cli({"games/demo", "t.rb", "--screenshots"}).error == "--screenshots needs a directory");
  CHECK(parse_test_cli({"games/demo", "t.rb", "--gpu", "turbo"}).error == "--gpu needs auto, dedicated or integrated");
  CHECK(parse_test_cli({"games/demo", "t.rb", "--bogus"}).error == "unknown option --bogus");
  CHECK(parse_test_cli({"games/demo", "--ruby", "x_test.rb"}).error ==
        "--ruby runs one file of Ruby tests, without a game");
  CHECK(parse_test_cli({"--ruby", "x_test.rb", "--headless"}).error ==
        "--headless is for system tests (Ruby tests never open a window)");
  CHECK(parse_test_cli({"games/demo", "t.rb"}).error.empty());
}

TEST_CASE("a system test can be given a folder of its run's own; Ruby tests have no use for one") {
  CHECK(parse_test_cli({"games/demo", "t.rb"}).options.work_dir.empty());
  const TestCliResult result = parse_test_cli({"games/demo", "t.rb", "--work", "/tmp/run"});
  REQUIRE(result.ok);
  CHECK(result.options.work_dir == "/tmp/run");
  CHECK(parse_test_cli({"games/demo", "t.rb", "--work"}).error == "--work needs a directory");
  CHECK(parse_test_cli({"--ruby", "x_test.rb", "--work", "/tmp/run"}).error ==
        "--work is for system tests (Ruby tests leave nothing behind)");
}

TEST_CASE("the flags the engine's own program once had mean nothing here either") {
  CHECK(parse_test_cli({"games/demo", "--test", "t.rb"}).error == "unknown option --test");
  CHECK(parse_test_cli({"--ruby-test", "x_test.rb"}).error == "unknown option --ruby-test");
}

TEST_CASE("every test of a game, as they are") {
  const TestCliResult result = parse_test_cli({"--all", "games/demo/", "--work", "w"});
  REQUIRE(result.ok);
  CHECK(result.options.mode == TestMode::GameTests);
  CHECK(result.options.game_dir == "games/demo");
  CHECK(result.options.work_dir == "w");
  CHECK_FALSE(parse_test_cli({"--all"}).ok);
  CHECK_FALSE(parse_test_cli({"--all", "games/demo", "t.rb"}).ok);
  CHECK_FALSE(parse_test_cli({"--all", "games/demo", "--headless"}).ok);
}
