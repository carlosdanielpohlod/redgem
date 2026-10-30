// MCP's game_query asks the saved game after some frames of headless play.

#include <doctest/doctest.h>

#include <stdexcept>
#include <string>

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

TEST_CASE("game_query refuses requests it cannot answer with what to change") {
  CHECK_THROWS_WITH_AS(mcp::game_query(kPlatformer, {{"action", "inspect"}}),
                       "inspect needs object (a #id, path or name)", std::invalid_argument);
  CHECK_THROWS_WITH_AS(mcp::game_query(kPlatformer, {{"action", "raycast"}, {"from", "0,5"}, {"to", "0,0,0"}}),
                       "from: needs x,y,z (three numbers), like 0,1.5,-4", std::invalid_argument);
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
}
