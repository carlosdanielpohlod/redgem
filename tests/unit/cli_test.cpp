#include <doctest/doctest.h>

#include "project/cli.hpp"
#include "app/bundled_game.hpp"
#include "app/game_settings.hpp"
#include "core/files.hpp"
#include "project/game_build.hpp"
#include "project/game_templates.hpp"
#include "project/recent_games.hpp"
#include "core/config.hpp"

using namespace redgem;

TEST_CASE("a game played from the command line") {
  const CliResult result = parse_cli({"games/demo", "--play"});
  REQUIRE(result.ok);
  CHECK(result.options.mode == RunMode::Game);
  CHECK(result.options.game_dir == "games/demo");
  // It is the developer who plays this way: the frame counter is on, as it is nowhere else.
  const GameWindowOptions window = developer_play_options(result.options);
  CHECK(window.game_dir == "games/demo");
  CHECK(window.show_fps);
  CHECK(window.capture_cursor);
  CHECK(developer_play_options(result.options, GpuChoice::Integrated).gpu == GpuChoice::Integrated);
}

TEST_CASE("the program knows nothing of tests: what runs them is another program") {
  // (Taking pictures of a game is something tests do: --screenshots went with them.)
  for (const char* option : {"--test", "--ruby-test", "--headless", "--screenshots"}) {
    CHECK(parse_cli({"games/demo", option, "t.rb"}).error == std::string("unknown option ") + option);
    CHECK(usage().find(option) == std::string::npos);
  }
}

TEST_CASE("a game directory alone opens the editor; --play runs the game") {
  CHECK(parse_cli({"games/demo"}).options.mode == RunMode::Editor);
  CHECK(parse_cli({"games/demo", "--play"}).options.mode == RunMode::Game);
  CHECK_FALSE(parse_cli({"--play"}).ok);
}

TEST_CASE("defaults") {
  const CliResult result = parse_cli({"games/demo"});
  REQUIRE(result.ok);
  CHECK(result.options.mode == RunMode::Editor);
  CHECK(result.options.template_name == "empty");
}

TEST_CASE("errors") {
  CHECK(parse_cli({}).ok);  // no game: the editor opens its project hub
  CHECK(parse_cli({}).options.mode == RunMode::Editor);
  CHECK_FALSE(parse_cli({"--template"}).ok);
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
  CHECK_FALSE(parse_cli({"build", "games/x", "out", "--play"}).ok);
  CHECK_FALSE(parse_cli({"games/x", "--gpu", "--play"}).ok);  // --play is not a graphics card
  CHECK(parse_cli({"games/x", "--play", "--play"}).ok);       // said twice, it is still one thing
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

TEST_CASE("the relay is a program of its own: here its name is a folder like any other") {
  const CliResult relay = parse_cli({"relay"});
  REQUIRE(relay.ok);
  CHECK(relay.options.mode == RunMode::Editor);
  CHECK(relay.options.game_dir == "relay");
  CHECK(parse_cli({"relay", "--port", "9000"}).error == "unknown option --port");
  CHECK(usage().find("redgem-relay") != std::string::npos);
}

TEST_CASE("android is a build target") {
  const CliResult android = parse_cli({"build", "games/mine", "dist/apk", "--target", "android"});
  REQUIRE(android.ok);
  CHECK(android.options.build_target == "android");
  CHECK(build_target_from_name("android") == BuildTarget::Android);
  CHECK(std::string(build_target_name(BuildTarget::Android)) == "android");
}

TEST_CASE("macos is a build target") {
  const CliResult mac = parse_cli({"build", "games/mine", "dist/mine", "--target", "macos"});
  REQUIRE(mac.ok);
  CHECK(mac.options.build_target == "macos");
  CHECK(build_target_from_name("macos") == BuildTarget::MacOS);
  CHECK(std::string(build_target_name(BuildTarget::MacOS)) == "macos");
  CHECK(usage().find("--target linux|windows|macos|android") != std::string::npos);
}

TEST_CASE("-h asks for help too, and help forgets the rest of the line") {
  const CliResult help = parse_cli({"games/demo", "--gpu", "integrated", "-h"});
  REQUIRE(help.ok);
  CHECK(help.options.mode == RunMode::Help);
  CHECK(help.options.game_dir.empty());
  CHECK_FALSE(help.options.gpu.has_value());
  CHECK(help.error.empty());
}

TEST_CASE("--version says which RedGem this is, and forgets the rest of the line") {
  const CliResult version = parse_cli({"games/demo", "--play", "--version"});
  REQUIRE(version.ok);
  CHECK(version.options.mode == RunMode::Version);
  CHECK(version.options.game_dir.empty());
  const std::string said = version_line();
  const std::string prefix = "RedGem ";
  REQUIRE(said.rfind(prefix, 0) == 0);
  const std::string number = said.substr(prefix.size());
  CHECK_FALSE(number.empty());
  const std::string version_characters = "0123456789.";  // a version as project() gives it: 0.3.0
  CHECK(number.find_first_not_of(version_characters) == std::string::npos);
  CHECK(usage().find("--version") != std::string::npos);
}

TEST_CASE("what is wrong with a command line is said") {
  CHECK(parse_cli({"new", "games/demo", "--template"}).error == "--template needs a name");
  CHECK(parse_cli({"build", "a", "b", "--target", "amiga"}).error == "--target needs linux, windows, macos or android");
  CHECK(parse_cli({"games/demo", "--gpu", "turbo"}).error == "--gpu needs auto, dedicated or integrated");
  CHECK(parse_cli({"games/demo", "--bogus"}).error == "unknown option --bogus");
  CHECK(parse_cli({"games/demo", "extra"}).error == "unexpected argument extra");
  CHECK(parse_cli({"new", "games/demo", "--play"}).error == "choose one of: new, build, --play");
  CHECK(parse_cli({"--play"}).error == "missing <game_dir>");
  CHECK(parse_cli({"build", "games/demo"}).error == "missing <out_dir>");
  CHECK(parse_cli({"games/demo"}).error.empty());
}

TEST_CASE("the first thing wrong on a line is the one reported") {
  CHECK(parse_cli({"--bogus", "--template"}).error == "unknown option --bogus");
  CHECK(parse_cli({"--template", "--bogus"}).error == "--template needs a name");
  // A line that cannot be read is not checked for what it lacks.
  CHECK(parse_cli({"new", "--play", "--bogus"}).error == "unknown option --bogus");
}

TEST_CASE("a subcommand is only the first word of the line") {
  const CliResult folder = parse_cli({"games/demo", "--play", "--gpu", "auto"});
  REQUIRE(folder.ok);
  CHECK(folder.options.mode == RunMode::Game);
  const CliResult named_new = parse_cli({"new", "games/demo", "--template", "new"});
  REQUIRE(named_new.ok);
  CHECK(named_new.options.template_name == "new");
  // A second folder has no place on a line that builds nothing.
  CHECK_FALSE(parse_cli({"games/demo", "build"}).ok);
  CHECK(parse_cli({"build", "relay", "new"}).options.game_dir == "relay");
}

TEST_CASE("the usage text names the scene file and the scripts folder") {
  const std::string text = usage();
  CHECK(text.rfind("usage:", 0) == 0);
  CHECK(text.find(config::kSceneFileName) != std::string::npos);
  CHECK(text.find(std::string(config::kScriptsDirName) + "/ folder") != std::string::npos);
}

TEST_CASE("a game recorded to a video from the command line, with what the recording asks for") {
  const CliResult result = parse_cli({"games/demo", "--record", "out.mp4", "--record-fps", "60", "--record-size",
                                      "3840x2160", "--record-scale", "1920x1080", "--record-seconds", "2.5"});
  REQUIRE(result.ok);
  CHECK(result.options.mode == RunMode::Game);  // recording plays the game
  const RecordOptions& record = result.options.record;
  CHECK(record.file == "out.mp4");
  CHECK(record.fps == 60);
  CHECK(record.size == FrameSize{3840, 2160});
  CHECK(record.scale == FrameSize{1920, 1080});
  REQUIRE(record.seconds);
  CHECK(*record.seconds == doctest::Approx(2.5));
  const GameWindowOptions window = developer_play_options(result.options);
  REQUIRE(window.record);
  CHECK(window.record->file == "out.mp4");
  CHECK_FALSE(developer_play_options(parse_cli({"games/demo", "--play"}).options).record);
}

TEST_CASE("a recording asked for wrongly is refused, saying what it needs") {
  CHECK_FALSE(parse_cli({"games/demo", "--record"}).ok);
  CHECK_FALSE(parse_cli({"games/demo", "--record", "out.mp4", "--record-fps", "0"}).ok);
  CHECK_FALSE(parse_cli({"games/demo", "--record", "out.mp4", "--record-size", "1920"}).ok);
  CHECK_FALSE(parse_cli({"games/demo", "--record", "out.mp4", "--record-scale", "0x1080"}).ok);
  CHECK_FALSE(parse_cli({"games/demo", "--record", "out.mp4", "--record-seconds", "-1"}).ok);
  CHECK(usage().find("--record") != std::string::npos);
}
