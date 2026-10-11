#include <doctest/doctest.h>

#include "app/cli.hpp"
#include "app/game_project.hpp"
#include "core/config.hpp"

using namespace redgem;

namespace {

constexpr int kLargestPort = 65535;
constexpr int kWebPort = 80;

}  // namespace

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
  CHECK(built.options.build_target.empty());  // the system it runs on
  const CliResult for_windows = parse_cli({"build", "games/mine", "dist/mine", "--target", "windows"});
  REQUIRE(for_windows.ok);
  CHECK(for_windows.options.build_target == "windows");
  CHECK_FALSE(parse_cli({"build", "games/mine", "dist/mine", "--target", "amiga"}).ok);
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

TEST_CASE("relay runs a meeting point, on a port of choice") {
  const CliResult plain = parse_cli({"relay"});
  REQUIRE(plain.ok);
  CHECK(plain.options.mode == RunMode::Relay);
  CHECK(plain.options.relay_port == 0);
  const CliResult chosen = parse_cli({"relay", "--port", "9000"});
  REQUIRE(chosen.ok);
  CHECK(chosen.options.relay_port == 9000);
  CHECK_FALSE(parse_cli({"relay", "--port", "70000"}).ok);
  CHECK_FALSE(parse_cli({"relay", "--port", "many"}).ok);
  CHECK_FALSE(parse_cli({"relay", "--port"}).ok);
  CHECK_FALSE(parse_cli({"relay", "somewhere"}).ok);
}

TEST_CASE("android is a build target") {
  const CliResult android = parse_cli({"build", "games/mine", "dist/apk", "--target", "android"});
  REQUIRE(android.ok);
  CHECK(android.options.build_target == "android");
  CHECK(build_target_from_name("android") == BuildTarget::Android);
  CHECK(std::string(build_target_name(BuildTarget::Android)) == "android");
}

TEST_CASE("-h asks for help too, and help forgets the rest of the line") {
  const CliResult help = parse_cli({"games/demo", "--screenshots", "shots", "-h"});
  REQUIRE(help.ok);
  CHECK(help.options.mode == RunMode::Help);
  CHECK(help.options.game_dir.empty());
  CHECK(help.options.screenshot_dir.empty());
  CHECK(help.error.empty());
}

TEST_CASE("what is wrong with a command line is said") {
  CHECK(parse_cli({"games/demo", "--test"}).error == "--test needs a file");
  CHECK(parse_cli({"--ruby-test"}).error == "--ruby-test needs a file");
  CHECK(parse_cli({"games/demo", "--screenshots"}).error == "--screenshots needs a directory");
  CHECK(parse_cli({"new", "games/demo", "--template"}).error == "--template needs a name");
  CHECK(parse_cli({"build", "a", "b", "--target", "amiga"}).error == "--target needs linux, windows or android");
  CHECK(parse_cli({"games/demo", "--gpu", "turbo"}).error == "--gpu needs auto, dedicated or integrated");
  CHECK(parse_cli({"relay", "--port", "many"}).error == "--port needs a number up to 65535");
  CHECK(parse_cli({"games/demo", "--bogus"}).error == "unknown option --bogus");
  CHECK(parse_cli({"games/demo", "extra"}).error == "unexpected argument extra");
  CHECK(parse_cli({"relay", "somewhere"}).error == "unexpected argument somewhere");
  CHECK(parse_cli({"new", "games/demo", "--play"}).error == "choose one of: new, build, relay, --play, --test, --ruby-test");
  CHECK(parse_cli({"games/demo", "--headless"}).error == "--headless needs --test <system_test.rb>");
  CHECK(parse_cli({"--play"}).error == "missing <game_dir>");
  CHECK(parse_cli({"build", "games/demo"}).error == "missing <out_dir>");
  CHECK(parse_cli({"games/demo"}).error.empty());
}

TEST_CASE("the first thing wrong on a line is the one reported") {
  CHECK(parse_cli({"--bogus", "--test"}).error == "unknown option --bogus");
  CHECK(parse_cli({"--test", "--bogus"}).error == "--test needs a file");
  // A line that cannot be read is not checked for what it lacks.
  CHECK(parse_cli({"new", "--play", "--bogus"}).error == "unknown option --bogus");
}

TEST_CASE("a subcommand is only the first word of the line") {
  const CliResult folder = parse_cli({"games/demo", "--play", "--screenshots", "new"});
  REQUIRE(folder.ok);
  CHECK(folder.options.mode == RunMode::Game);
  CHECK(folder.options.screenshot_dir == "new");
  // A second folder has no place on a line that builds nothing.
  CHECK_FALSE(parse_cli({"games/demo", "build"}).ok);
  CHECK(parse_cli({"build", "relay", "new"}).options.game_dir == "relay");
}

TEST_CASE("a port is a number of at most five digits, up to the largest there is") {
  CHECK(parse_cli({"relay", "--port", "65535"}).options.relay_port == kLargestPort);
  CHECK(parse_cli({"relay", "--port", "00080"}).options.relay_port == kWebPort);
  CHECK_FALSE(parse_cli({"relay", "--port", "65536"}).ok);
  CHECK_FALSE(parse_cli({"relay", "--port", "000080"}).ok);
  CHECK_FALSE(parse_cli({"relay", "--port", "80a"}).ok);
  CHECK_FALSE(parse_cli({"relay", "--port", "-80"}).ok);  // (read as an option, so --port has no value)
  CHECK_FALSE(parse_cli({"relay", "--port", "8 0"}).ok);
}

TEST_CASE("the usage text names the scene file and the scripts folder") {
  const std::string text = usage();
  CHECK(text.rfind("usage:", 0) == 0);
  CHECK(text.find(config::kSceneFileName) != std::string::npos);
  CHECK(text.find(std::string(config::kScriptsDirName) + "/ folder") != std::string::npos);
  CHECK(text.find("redgem relay") != std::string::npos);
}
