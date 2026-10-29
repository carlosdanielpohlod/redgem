// `redgem run` and `redgem check`: a game played with no window and looked over without playing
// it, both answering in a few lines that point at the game's own files.
// (`redgem shot` and `check --run-tests` open windows and start programs: tests/CMakeLists.txt runs them.)

#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "core/exit_codes.hpp"
#include "core/files.hpp"
#include "core/message_lines.hpp"
#include "project/game_checks.hpp"
#include "project/game_tests.hpp"
#include "project/script_messages.hpp"
#include "project/tool_commands.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::tools;
namespace fs = std::filesystem;

namespace {

const fs::path kPlatformer = REDGEM_PLATFORMER_DIR;
const fs::path kSandbox = fs::path(REDGEM_FIXTURES_DIR) / "games/sandbox";

ToolResult tool(const std::vector<std::string>& words) {
  const auto result = run_tool_command(words);
  REQUIRE(result.has_value());
  return *result;
}

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// A copy of the platformer of the test's own, gone with it.
class GameCopy {
 public:
  explicit GameCopy(const std::string& name) : dir_(fs::temp_directory_path() / ("redgem_play_tools_" + name)) {
    fs::remove_all(dir_);
    fs::copy(kPlatformer, dir_, fs::copy_options::recursive);
  }
  ~GameCopy() { fs::remove_all(dir_); }
  GameCopy(const GameCopy&) = delete;
  GameCopy& operator=(const GameCopy&) = delete;

  std::string dir() const { return dir_.string(); }
  void write(const std::string& file, const std::string& text) const {
    std::string error;
    fs::create_directories((dir_ / file).parent_path());
    REQUIRE_MESSAGE(write_text_file(dir_ / file, text, error), error);
  }

 private:
  fs::path dir_;
};

}  // namespace

TEST_CASE("a game that plays well says so in one line") {
  const ToolResult ran = tool({"run", kPlatformer.string(), "--seconds", "1"});
  CHECK(ran.exit_code == kExitSuccess);
  CHECK(ran.out == "ran main for 60 frames (1 s): 0 errors, 0 warnings\n");
}

TEST_CASE("a run says each script error once, where it happened, and fails") {
  const ToolResult ran = tool({"run", kSandbox.string(), "--frames", "30"});
  CHECK(ran.exit_code == kExitFailure);
  CHECK(has(ran.out, "error: scripts/broken.rb:5: Broken#update on 'Broken' (behaviour disabled): boom"));
  CHECK(has(ran.out, "error: loading scripts/does_not_exist.rb"));
  CHECK_FALSE(has(ran.out, kSandbox.string()));  // the game's folder is left out of every path
  CHECK(has(ran.out, "ran main for 30 frames (0.5 s): 2 errors"));
}

TEST_CASE("a game's folder typed with a slash after it is left out of the paths all the same") {
  const ToolResult ran = tool({"run", kSandbox.string() + "/", "--frames", "30"});
  CHECK(has(ran.out, "error: scripts/broken.rb:5: Broken#update"));
  CHECK(compact_message("loading ./scripts/x.rb: cannot read ./scripts/x.rb", ".") ==
        "loading scripts/x.rb: cannot read scripts/x.rb");
  CHECK(compact_message("cannot read ../other/x.rb", ".") == "cannot read ../other/x.rb");
}

TEST_CASE("a run plays the scene it is asked for, and refuses one the game lacks") {
  const ToolResult other = tool({"run", kSandbox.string(), "--scene", "level2", "--frames", "5"});
  CHECK(has(other.out, "ran level2 for 5 frames"));
  const ToolResult missing = tool({"run", kSandbox.string(), "--scene", "level9"});
  CHECK(missing.exit_code == kExitFailure);
  CHECK(has(missing.err, "its scenes are: main, level2"));
}

TEST_CASE("what a run cannot take is refused with its usage") {
  CHECK(tool({"run"}).exit_code == kExitUsage);
  CHECK(tool({"run", kPlatformer.string(), "--seconds", "0"}).exit_code == kExitUsage);
  CHECK(tool({"run", kPlatformer.string(), "--frames"}).exit_code == kExitUsage);
  CHECK(has(tool({"run", "--help"}).out, "usage: redgem run"));
  CHECK(tool({"shot", kPlatformer.string(), "out.jpg"}).exit_code == kExitUsage);
  CHECK(tool({"shot", kPlatformer.string(), "out.png", "--size", "big"}).exit_code == kExitUsage);
  // A moment past what a tool plays (an hour of the game) is refused, not played for ever.
  CHECK(tool({"shot", kPlatformer.string(), "out.png", "--at", "1e300"}).exit_code == kExitUsage);
  CHECK(tool({"shot", kPlatformer.string(), "out.png", "--frames", "999999999"}).exit_code == kExitUsage);
  CHECK(tool({"shot", kPlatformer.string(), "out.png", "--at", "-1"}).exit_code == kExitUsage);
  CHECK(tool({"run", kPlatformer.string(), "--seconds", "1e300"}).exit_code == kExitUsage);
}

TEST_CASE("a message is one line, led by the place in the game it points at") {
  const fs::path game = "/games/demo";
  const std::string message =
      "Coin#update on 'Coin1' (behaviour disabled): boom (RuntimeError)\n"
      "      from /games/demo/scripts/coin.rb:12:in update\n"
      "      from engine/behaviour_timers.rb:110:in __update";
  CHECK(compact_message(message, game) ==
        "scripts/coin.rb:12: Coin#update on 'Coin1' (behaviour disabled): boom (RuntimeError)");
  CHECK(compact_message("loading /games/demo/scripts/x.rb: cannot read it", game) ==
        "loading scripts/x.rb: cannot read it");
  CHECK(compact_message("from the engine alone\n  from engine/x.rb:1:in y", game) == "from the engine alone");
}

TEST_CASE("the same message twice is said once, with how many times") {
  MessageCollector messages("/games/demo");
  log::error("boom");
  log::error("boom");
  log::warning("careful");
  log::info("not kept");
  CHECK(messages.lines() == "error: boom (x2)\nwarning: careful\n");
  CHECK(messages.errors() == 2);
  CHECK(messages.warnings() == 1);
}

TEST_CASE("a game in order passes its check") {
  const ToolResult checked = tool({"check", kPlatformer.string()});
  CHECK(checked.exit_code == kExitSuccess);
  CHECK(checked.out == "check: 0 errors, 0 warnings (13 scripts, 1 scenes)\n");
}

TEST_CASE("a check finds what keeps a game from playing, and says what to do") {
  GameCopy game("broken");
  game.write("scripts/coin.rb", "class Coin < Behaviour\n  def update(dt)\n    if true\n  end\n");
  game.write("scripts/goal.rb", "class Goals < Behaviour\nend\n");
  REQUIRE(tool({"scene", "set", game.dir(), "Camera", "script+=CameraFolow", "prop.sky_image=nowhere"}).exit_code ==
          kExitSuccess);
  const ToolResult checked = tool({"check", game.dir()});
  CHECK(checked.exit_code == kExitFailure);
  CHECK(has(checked.out, "error scripts/coin.rb:5: syntax error"));
  CHECK(has(checked.out, "error scripts/goal.rb does not define class Goal (attached in scene main #20 Goal)"));
  CHECK(has(checked.out, "scripts/camera_folow.rb is not there; did you mean camera_follow.rb?"));
  CHECK(has(checked.out, "sky_image nowhere is not in images/"));
  CHECK(has(checked.out, "check: 4 errors"));
}

TEST_CASE("a sky picture is found the way the engine finds it") {
  GameCopy game("sky");
  game.write("images/meadow.jpeg", "not really a picture: the check looks for the file");
  for (const char* name : {"meadow", "meadow.jpeg"}) {
    REQUIRE(tool({"scene", "set", game.dir(), "Sun", std::string("prop.sky_image=") + name}).exit_code == kExitSuccess);
    CHECK_MESSAGE(tool({"check", game.dir()}).exit_code == kExitSuccess, name);
  }
}

TEST_CASE("a check lists a screenful of problems unless asked for all") {
  GameCopy game("many");
  for (int file = 0; file < 40; ++file) game.write("scripts/broken" + std::to_string(file) + ".rb", "def x(");
  CHECK(has(tool({"check", game.dir()}).out, "+10 more (--all shows every one)"));
  CHECK_FALSE(has(tool({"check", game.dir(), "--all"}).out, "more (--all"));
}

TEST_CASE("a check of what is no game says so") {
  CHECK(has(tool({"check", "no/such/game"}).err, "is not a game"));
  CHECK(tool({"check"}).exit_code == kExitUsage);
}

TEST_CASE("whether a script defines its class is asked of the engine, not guessed from its text") {
  GameCopy game("classes");
  // A class made without the word `class`, and one named from the top: both are the class.
  game.write("scripts/coin.rb", "Coin = Class.new(Behaviour) do\n  def update(dt); end\nend\n");
  game.write("scripts/goal.rb", "class ::Goal < Behaviour\nend\n");
  // ...and one only written in a text and a comment block, which defines nothing.
  game.write("scripts/mover.rb", "NOTE = <<~TEXT\n  class Mover < Behaviour\nTEXT\n=begin\nclass Mover\n=end\n");
  const ToolResult checked = tool({"check", game.dir()});
  CHECK(has(checked.out, "error scripts/mover.rb does not define class Mover"));
  CHECK_FALSE(has(checked.out, "coin.rb does not define"));
  CHECK_FALSE(has(checked.out, "goal.rb does not define"));
  CHECK(has(checked.out, "check: 1 errors"));
}

TEST_CASE("what redgem-test says of a game's tests is read a file a line") {
  const TestsRun run = read_tests_run(
      "PASS tests/a_test.rb\nFAIL tests/b_test.rb: tests/b_test.rb:8: expected 1, got 2\ntests: 1 passed, 1 failed\n");
  CHECK(run.passed == 1);
  CHECK(run.failures == std::vector<std::string>{"tests/b_test.rb: tests/b_test.rb:8: expected 1, got 2"});
  CHECK(run_game_tests(kPlatformer, "no/redgem-test", fs::temp_directory_path()).problem.find("no redgem-test") !=
        std::string::npos);
}

#ifndef _WIN32
TEST_CASE("a redgem-test that ends without a word about the tests fails the check") {
  // (A program that says nothing and fails, standing for one that crashed.)
  const fs::path tester = fs::temp_directory_path() / "redgem_silent_tester.sh";
  std::string error;
  REQUIRE(write_text_file(tester, "#!/bin/sh\nexit 3\n", error));
  fs::permissions(tester, fs::perms::owner_all);
  const TestsRun run = run_game_tests(kPlatformer, tester, fs::temp_directory_path());
  fs::remove(tester);
  CHECK(run.passed == 0);
  CHECK(run.problem.find("redgem-test ended without saying how the tests went") != std::string::npos);
}
#endif

TEST_CASE("a seeded run repeats random values from scene script loading onward") {
  const GameCopy game("seeded");
  game.write("scripts/main_scene.rb", "$loaded = rand(1_000_000)\nclass MainScene < SceneScript; end\n");
  const std::vector<std::string> args{
      "run", game.dir(), "--frames", "3", "--seed", "42", "--print", "[$loaded, rand(1_000_000)]", "--every", "1"};
  const ToolResult first = tool(args);
  const ToolResult second = tool(args);
  CHECK(first.exit_code == kExitSuccess);
  CHECK(first.out == second.out);
  auto other = args;
  other[5] = "43";
  CHECK(tool(other).out != first.out);
}

TEST_CASE("a run replays held keys and releases them at the given frame") {
  const GameCopy game("input_replay");
  game.write("input.json", R"([{"frame":0,"keys":["w"]},{"frame":2,"keys":[]}])");
  game.write("scripts/main_scene.rb", R"(
    class ReplayCounter < Behaviour
      def start = @count = 0
      def update(_dt)
        @count += 1 if Input.down?(:w)
        $replayed = @count
      end
    end
    Scene.find('Player').add_component(ReplayCounter)
    class MainScene < SceneScript; end
  )");
  const ToolResult ran = tool({"run", game.dir(), "--frames", "4", "--input", game.dir() + "/input.json", "--print",
                               "$replayed", "--every", "1"});
  CHECK_MESSAGE(ran.exit_code == kExitSuccess, ran.out, ran.err);
  CHECK(has(ran.out, "frame 1: 1\nframe 2: 2\nframe 3: 2\nframe 4: 2\n"));
}

TEST_CASE("run assertions and stop conditions are evaluated after each frame") {
  const GameCopy game("run_conditions");
  const ToolResult stopped = tool({"run", game.dir(), "--frames", "20", "--until", "($n ||= 0; $n += 1) == 3",
                                   "--assert", "Scene.find('Player') != nil", "--print", "$n"});
  CHECK_MESSAGE(stopped.exit_code == kExitSuccess, stopped.out, stopped.err);
  CHECK(has(stopped.out, "ran main for 3 frames"));
  CHECK(has(stopped.out, "frame 3: 3"));
  const ToolResult failed = tool({"run", game.dir(), "--frames", "5", "--assert", "false"});
  CHECK(failed.exit_code == kExitFailure);
  CHECK(has(failed.err, "frame 1: --assert was false"));
  CHECK(has(failed.out, "ran main for 1 frames"));
  const ToolResult expired = tool({"run", game.dir(), "--frames", "2", "--until", "false"});
  CHECK(expired.exit_code == kExitFailure);
  CHECK(has(expired.err, "--until was not reached"));
}

TEST_CASE("run expression errors and runaway expressions end the run with a diagnostic") {
  const GameCopy game("run_expression_error");
  game.write("game.json", R"({"title":"Expression errors","script_budget_ms":20})");
  for (const auto* option : {"--assert", "--until", "--print"}) {
    const ToolResult failed = tool({"run", game.dir(), "--frames", "1", option, "raise 'bad expression'"});
    CHECK(failed.exit_code == kExitFailure);
    CHECK(has(failed.err, "bad expression"));
  }
  const ToolResult runaway = tool({"run", game.dir(), "--frames", "1", "--assert", "loop {}"});
  CHECK(runaway.exit_code == kExitFailure);
  CHECK(has(runaway.err, "ScriptTimeout"));
}

TEST_CASE("run plans reject invalid seeds and malformed input before playing") {
  const GameCopy game("run_plan_errors");
  for (const auto* seed : {"-1", "1.5", "1e30", "2147483648", "nan"}) {
    CHECK(tool({"run", game.dir(), "--seed", seed}).exit_code == kExitUsage);
  }
  for (const auto* json :
       {"bad", "{}", R"([{"frame":0,"keys":["missing"]}])", R"([{"frame":-1,"keys":[]}])", R"([{"frame":2,"keys":[]}])",
        R"([{"frame":0,"keys":[]},{"frame":0,"keys":[]}])", R"([{"frame":0,"keys":[],"typo":true}])"}) {
    game.write("bad-input.json", json);
    CHECK(tool({"run", game.dir(), "--frames", "2", "--input", game.dir() + "/bad-input.json"}).exit_code ==
          kExitUsage);
  }
  CHECK(tool({"run", game.dir(), "--every", "1"}).exit_code == kExitUsage);
  CHECK(tool({"run", game.dir(), "--print", "1", "--every", "0"}).exit_code == kExitUsage);
}

TEST_CASE("run output is bounded and expressions can return ordinary Ruby values") {
  const GameCopy game("run_print_limits");
  const ToolResult ran = tool({"run", game.dir(), "--frames", "100", "--print", "'x' * 1000", "--every", "1"});
  CHECK(ran.exit_code == kExitSuccess);
  CHECK(ran.out.size() < 9000);
  CHECK(has(ran.out, "output limit reached"));
  CHECK(has(ran.out, "ran main for 100 frames"));
  const ToolResult nil = tool({"run", game.dir(), "--frames", "1", "--print", "nil"});
  CHECK(has(nil.out, "frame 1: nil"));
}

TEST_CASE("run input refuses deep JSON and non-regular files") {
  const GameCopy game("run_input_bounds");
  game.write("deep.json", std::string(1000, '[') + "0" + std::string(1000, ']'));
  CHECK(tool({"run", game.dir(), "--input", game.dir() + "/deep.json"}).exit_code == kExitUsage);
  CHECK(tool({"run", game.dir(), "--input", game.dir()}).exit_code == kExitUsage);
}

TEST_CASE("printed temporary values stay alive through their inspect and can reject an invalid inspect result") {
  const GameCopy game("run_inspect");
  const ToolResult collected = tool({"run", game.dir(), "--frames", "2", "--print",
                                     "Class.new { def inspect; GC.start; 'still alive'; end }.new", "--every", "1"});
  CHECK_MESSAGE(collected.exit_code == kExitSuccess, collected.err);
  CHECK(has(collected.out, "frame 1: still alive"));
  const ToolResult invalid =
      tool({"run", game.dir(), "--frames", "1", "--print", "Class.new { def inspect = 42 }.new"});
  CHECK(invalid.exit_code == kExitFailure);
  CHECK(has(invalid.err, "inspect must return a string"));
}
