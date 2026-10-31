// MCP's game_query asks the saved game after some frames of headless play.

#include <doctest/doctest.h>

#include <csignal>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

#include "app/encoder_process.hpp"
#include "core/files.hpp"

#include "project/mcp_game_tools.hpp"
#include "project/runtime_query.hpp"

using namespace redgem;
using namespace redgem::tools;
using mcp::Json;

namespace {

const mcp::Options kPlatformer{REDGEM_PLATFORMER_DIR, false, {}};

std::string answer(const Json& args) {
  const Json result = mcp::game_query(kPlatformer, args);
  REQUIRE_FALSE(result["isError"].get<bool>());
  return result["content"][0]["text"];
}

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// A copy of the platformer whose Player runs `update`, a Ruby method body, every frame.
class ScriptedGame {
 public:
  explicit ScriptedGame(const std::string& update) {
    std::filesystem::remove_all(root_);
    std::filesystem::copy(REDGEM_PLATFORMER_DIR, root_, std::filesystem::copy_options::recursive);
    std::string error;
    const std::string script = "class Probe < Behaviour\n  def update(_dt)\n    " + update + "\n  end\nend\n";
    REQUIRE(write_text_file(root_ / "scripts" / "probe.rb", script, error));
    const Json attach{{"action", "set"}, {"object", "Player"}, {"assignments", {"script+=Probe"}}};
    REQUIRE_FALSE(mcp::invoke(options(), "scene_edit", attach)["isError"].get<bool>());
  }
  ~ScriptedGame() { std::filesystem::remove_all(root_); }
  ScriptedGame(const ScriptedGame&) = delete;
  ScriptedGame& operator=(const ScriptedGame&) = delete;
  mcp::Options options() const { return {root_, false, {}}; }
  const std::filesystem::path& root() const { return root_; }

 private:
  std::filesystem::path root_ = std::filesystem::temp_directory_path() / "redgem-game-query-test";
};

std::string answer_in(const ScriptedGame& game, const Json& args) {
  const Json result = mcp::game_query(game.options(), args);
  REQUIRE_FALSE(result["isError"].get<bool>());
  return result["content"][0]["text"];
}

}  // namespace

TEST_CASE("game_query inspects an object's live state after the frames asked for") {
  const std::string player = answer({{"action", "inspect"}, {"object", "Player"}});
  CHECK(has(player, "after 60 frames: #5 Player pos="));
  CHECK(has(player, "body=character"));
  CHECK(has(player, "grounded"));
  // Props the scene already says are left out; only what scripts changed would be listed.
  CHECK_FALSE(has(player, "jump_speed"));
  CHECK(answer({{"action", "inspect"}, {"object", "#7"}, {"frames", 0}}) ==
        "after 0 frames: #7 Coin1 pos=0,1,-4 trigger\n");
}

TEST_CASE("game_query lists what a ray between two points and a sphere meet, nearest first") {
  const std::string ray = answer({{"action", "raycast"}, {"from", "0,5,0"}, {"to", "0,-5,0"}});
  CHECK(has(ray, "2 hits, nearest first:\n#5 Player at "));
  CHECK(has(ray, "#0 Ground at 0,0,0 normal=0,1,0 dist=5"));
  CHECK(has(answer({{"action", "raycast"}, {"from", "20,5,20"}, {"to", "20,4,20"}}), "nothing between from and to"));
  const std::string near_crates = answer({{"action", "overlap"}, {"at", "4,1,-3"}, {"radius", 1.5}});
  CHECK(has(near_crates, "Crate1"));
  CHECK(has(near_crates, "Crate2"));
}

TEST_CASE("game_query says how many frames were played when the game ended sooner") {
  const ScriptedGame game("@frames = (@frames || 0) + 1; Game.quit if @frames == 5");
  CHECK(has(answer_in(game, {{"action", "inspect"}, {"object", "Player"}, {"frames", 600}}), "after 5 frames: "));
}

TEST_CASE("after a script switched scenes, game_query names the new scene's objects by name, not by old ids") {
  const ScriptedGame game("Game.load_scene('second') if Game.scene_name == 'main'");
  std::filesystem::create_directories(game.root() / "scenes");
  std::filesystem::copy_file(game.root() / "scene.glb", game.root() / "scenes" / "second.glb");
  CHECK(has(answer_in(game, {{"action", "inspect"}, {"object", "Coin1"}, {"frames", 10}}), "Coin1"));
  CHECK_THROWS_AS(mcp::game_query(game.options(), {{"action", "inspect"}, {"object", "#7"}, {"frames", 10}}),
                  std::invalid_argument);
}

TEST_CASE("game_query refuses requests it cannot answer with what to change") {
  CHECK_THROWS_WITH_AS(mcp::game_query(kPlatformer, {{"action", "inspect"}}),
                       "inspect needs object (a #id, path or name)", std::invalid_argument);
  CHECK_THROWS_WITH_AS(mcp::game_query(kPlatformer, {{"action", "raycast"}, {"from", "0,5"}, {"to", "0,0,0"}}),
                       "from: needs x,y,z (three numbers of at most 1000000 meters), like 0,1.5,-4", std::invalid_argument);
  CHECK_THROWS_WITH_AS(mcp::game_query(kPlatformer, {{"action", "raycast"}, {"from", "1,1,1"}, {"to", "1,1,1"}}),
                       "from and to must be different points", std::invalid_argument);
  CHECK_THROWS_AS(mcp::game_query(kPlatformer, {{"action", "inspect"}, {"object", "Nope"}}), std::invalid_argument);
  const Json missing = mcp::game_query(kPlatformer, {{"action", "inspect"}, {"object", "Player"}, {"scene", "nope"}});
  CHECK(missing["isError"] == true);
  CHECK(has(missing["content"][0]["text"], "its scenes are: main"));
}

TEST_CASE("query points are three finite numbers") {
  CHECK(point_argument({{"at", "1,-2.5,3"}}, "at").y == doctest::Approx(-2.5));
  for (const char* bad : {"1,2", "1,2,3,4", "a,b,c", "1,,3", "1,2,nan", ""}) {
    CHECK_THROWS_AS(point_argument({{"at", bad}}, "at"), std::invalid_argument);
  }
  CHECK_THROWS_WITH_AS(point_argument(Json::object(), "at"), "missing argument: at (x,y,z in meters)",
                       std::invalid_argument);
  // Beyond any level: rays between such points would overflow to a miss or reach physics as NaN.
  for (const char* far : {"1e30,0,0", "0,3e38,0", "0,0,-1000001"}) {
    CHECK_THROWS_AS(point_argument({{"at", far}}, "at"), std::invalid_argument);
  }
  CHECK(point_argument({{"at", "1000000,0,-1000000"}}, "at").x == doctest::Approx(1.0e6));
}

#ifndef _WIN32
TEST_CASE("a program run to its end reports its output, its exit code and a crash as its own") {
  const auto output = std::filesystem::temp_directory_path() / "redgem-run-to-end.txt";
  const ProcessEnd exited = run_to_end({"/bin/sh", "-c", "echo said; echo warned >&2; exit 3"}, output);
  CHECK(exited.started);
  CHECK(exited.exited);
  CHECK(exited.code == 3);
  std::string error;
  CHECK(read_text_file(output, error) == std::optional<std::string>("said\nwarned\n"));
  const ProcessEnd crashed = run_to_end({"/bin/sh", "-c", "kill -SEGV $$"}, output);
  CHECK(crashed.started);
  CHECK_FALSE(crashed.exited);
  CHECK(crashed.code == SIGSEGV);
  CHECK_FALSE(run_to_end({"/no/such/program"}, output).started);
  std::filesystem::remove(output);
}
#endif
