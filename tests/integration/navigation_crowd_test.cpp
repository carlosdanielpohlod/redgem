// Crowds played through Ruby in a headless game: two NavAgents walking at each other down a
// corridor pass without touching and both get there, and one finds another way around a crate
// that stands in a doorway.

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "physics_game.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr const char* kCrowdGame = R"({
  "title": "Crowd",
  "navigation": {"cell_size": 0.25, "agent_radius": 0.3}
})";

constexpr float kCapsuleRadius = 0.3f;  // the characters' bodies: 0.6 m wide, 2 m tall
constexpr float kTouching = kCapsuleRadius * 2;
constexpr float kGap = 0.05f;  // what a capsule's padding may take of the space between two
constexpr int kTwentySeconds = kOneSecond * 20;

NodeDesc walker(const std::string& name, Vec3 position) {
  return node(name, position, {kCapsuleRadius * 2, 2, kCapsuleRadius * 2}, moving("character"));
}

// A corridor 3 m wide along x (walls at z = -2 and 2, a meter thick), with a walker at each end.
SceneDesc corridor_scene() {
  return scene_with({node("North", {0, 1, -2}, {20, 2, 1}, solid()), node("South", {0, 1, 2}, {20, 2, 1}, solid()),
                     walker("West", {-6, 1, 0}), walker("East", {6, 1, 0})});
}

// A wall along z = 0 with two doorways (one near the walker's way, one far up), and a crate that
// is not part of the level standing in the near one.
SceneDesc doorway_scene() {
  return scene_with({node("WallLow", {0, 1, -6}, {0.5f, 2, 9}, solid()),
                     node("WallMiddle", {0, 1, 4}, {0.5f, 2, 5}, solid()),
                     node("WallHigh", {0, 1, 10}, {0.5f, 2, 2}, solid()),
                     node("Crate", {0, 1, 0}, {1, 2, 3}, moving("kinematic")), walker("Walker", {-5, 1, 0})});
}

float flat_gap(Engine& engine) {
  return number(engine, "(Scene.find('West').transform.position - Scene.find('East').transform.position).flat.length");
}

}  // namespace

TEST_CASE("two agents walking at each other down a corridor pass without touching, and both arrive") {
  const TempGame game(kCrowdGame);
  auto engine = engine_for(game, corridor_scene());
  ruby(*engine, R"(
    $west = Scene.find('West').add_component(NavAgent, speed: 2.0, radius: 0.4)
    $east = Scene.find('East').add_component(NavAgent, speed: 2.0, radius: 0.4)
    $ways = [$west.go_to(Vector3.new(6, 0, 0)), $east.go_to(Vector3.new(-6, 0, 0))]
  )");
  CHECK(ruby(*engine, "$ways") == "[true, true]");
  float closest = flat_gap(*engine);
  for (int frame = 0; frame < kTwentySeconds && ruby(*engine, "$west.arrived? && $east.arrived?") == "false"; ++frame) {
    frames(*engine, 1);
    closest = std::min(closest, flat_gap(*engine));
  }
  CHECK(ruby(*engine, "$west.arrived? && $east.arrived?") == "true");
  CHECK(closest > kTouching + kGap);
  CHECK(number(*engine, "Scene.find('West').transform.position.x") > 5.0f);
  CHECK(number(*engine, "Scene.find('East').transform.position.x") < -5.0f);
}

TEST_CASE("an agent stuck at a crate in a doorway finds another way through the other one") {
  const TempGame game(kCrowdGame);
  auto engine = engine_for(game, doorway_scene());
  ruby(*engine, R"(
    $walker = Scene.find('Walker').add_component(NavAgent, speed: 3.0, radius: 0.4)
    $way = $walker.go_to(Vector3.new(5, 0, 0))
    $first = $walker.waypoints.dup
    $furthest = 0.0
  )");
  CHECK(ruby(*engine, "$way") == "true");
  CHECK(ruby(*engine, "$first.map(&:z).max < 2") == "true");  // the level alone: through the crate's doorway
  for (int frame = 0; frame < kTwentySeconds && ruby(*engine, "$walker.arrived?") == "false"; ++frame) {
    frames(*engine, 1);
    ruby(*engine, "$furthest = [$furthest, Scene.find('Walker').transform.position.z].max");
  }
  CHECK(ruby(*engine, "$walker.arrived?") == "true");
  CHECK(number(*engine, "$furthest") > 6.0f);  // through the far doorway
}

TEST_CASE("Navigation.path goes around the bodies it is told to avoid, and Navigation.steer turns aside") {
  const TempGame game(kCrowdGame);
  auto engine = engine_for(game, doorway_scene());
  ruby(*engine, "$crate = Scene.find('Crate')");
  CHECK(ruby(*engine, "Navigation.path(Vector3.new(-5, 0, 0), Vector3.new(5, 0, 0)).map(&:z).max < 2") == "true");
  CHECK(ruby(*engine, "Navigation.path(Vector3.new(-5, 0, 0), Vector3.new(5, 0, 0), avoid: $crate).map(&:z).max > 6") ==
        "true");
  ruby(*engine, R"(
    $turn = Navigation.steer(Vector3.new(-5, 1, -3), Vector3.zero, Vector3.new(2, 0, 0), speed: 2.0, radius: 0.4,
                             crowd: [[Vector3.new(-4, 1, -3), Vector3.new(-2, 0, 0), 0.4]])
  )");
  CHECK(ruby(*engine, "$turn.is_a?(Vector3) && $turn.z.abs > 0.1 && $turn.y.zero?") == "true");
  CHECK(error_of(*engine, "Navigation.steer(Vector3.zero, Vector3.zero, Vector3.zero, speed: 1, radius: 1, crowd: [1])")
            .find("crowd entry") != std::string::npos);
}
