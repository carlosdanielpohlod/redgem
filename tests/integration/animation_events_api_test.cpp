// Animation events and root motion played through Ruby in a headless game with a fixed step: a
// hero whose Walk clip carries its hips two meters a second along x.

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "physics_game.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr int kHalfSecond = kOneSecond / 2;
constexpr int kQuarterSecond = kOneSecond / 4;
constexpr int kFewFrames = 5;
// Short of a second and a half, so that the second pass by the middle of Walk is the next frame's.
constexpr int kNearlyOneAndAHalfSeconds = kOneSecond + kHalfSecond - kFewFrames;
constexpr int kThreeSeconds = kOneSecond * 3;
constexpr float kClipSeconds = 1.0f;
constexpr float kWalkSpeed = 2.0f;  // meters a second
constexpr float kFootstep = 0.25f;
constexpr float kTolerance = 0.05f;
constexpr float kWallX = 3.0f;
constexpr float kHeroHalfWidth = 0.5f;

constexpr const char* kPlainGame = R"({"title": "Animation events"})";

AnimationDesc walk(int hips) {
  AnimationChannel channel;
  channel.node = hips;
  channel.times = {0.0f, kClipSeconds};
  channel.values = {0, 0, 0, kWalkSpeed * kClipSeconds, 0, 0};
  return {"Walk", kClipSeconds, {channel}, {{"footstep", kFootstep}}};
}

// The camera and the ground, a hero (with `props`) and its hips, and what else a test adds.
SceneDesc hero_scene(PropertyMap props, std::vector<NodeDesc> more = {}) {
  SceneDesc desc = scene_with({node("Hero", {0, 1, 0}, {1, 2, 1}, std::move(props))});
  const int hero = static_cast<int>(desc.nodes.size()) - 1;
  NodeDesc hips;
  hips.name = "Hips";
  hips.parent = hero;
  desc.nodes[hero].children = {hero + 1};
  desc.nodes.push_back(hips);
  for (NodeDesc& each : more) desc.nodes.push_back(std::move(each));
  desc.animations = {walk(hero + 1)};
  return desc;
}

float hips_x(Engine& engine) { return engine.scene().at(engine.scene().find("Hips")).local.position.x; }

}  // namespace

TEST_CASE("animation events call their blocks in the scripts' update, once on each pass") {
  const TempGame game(kPlainGame);
  const SceneDesc scene = hero_scene({});
  auto engine = engine_for(game, scene);
  ruby(*engine, R"(
    $heard = []
    class Walker < Behaviour
      def start
        animation.play(:Walk)
        animation.on(:Walk, at: 0.5) { |event| $heard << [event.clip, event.at, event.name, event.game_object.name] }
        animation.on_marker(:footstep) { |event| $heard << [event.name, event.at] }
        $late = animation.on(:Walk, seconds: 0.75) { $heard << :late }
      end
    end
    Scene.find('Hero').add_component(Walker)
  )");
  frames(*engine, kNearlyOneAndAHalfSeconds);
  CHECK(ruby(*engine, "$heard.inspect") ==
        R"([["footstep", 0.25], ["Walk", 0.5, nil, "Hero"], :late, ["footstep", 0.25]])");
  ruby(*engine, "$heard.clear; Scene.find('Hero').animation.off($late)");
  frames(*engine, kOneSecond);
  CHECK(ruby(*engine, "$heard.map(&:first).inspect") == R"(["Walk", "footstep"])");
}

TEST_CASE("animation events refuse what they cannot listen for") {
  const TempGame game(kPlainGame);
  const SceneDesc scene = hero_scene({});
  auto engine = engine_for(game, scene);
  ruby(*engine, "$hero = Scene.find('Hero')");
  CHECK(error_of(*engine, "$hero.animation.on(:Walk, at: 0.5)").find("needs a block") != std::string::npos);
  CHECK(error_of(*engine, "$hero.animation.on(:Walk, at: 0.5, seconds: 1) {}").find("either") != std::string::npos);
  CHECK(error_of(*engine, "$hero.animation.on(:Walk) {}").find("either") != std::string::npos);
  CHECK(error_of(*engine, "$hero.animation.on(:Walk, at: 1.5) {}").find("from 0 to 1") != std::string::npos);
  CHECK(error_of(*engine, "$hero.animation.on(:Walk, seconds: 2) {}").find("seconds:") != std::string::npos);
  CHECK(error_of(*engine, "$hero.animation.on(:Dance, at: 0.5) {}").find("Dance") != std::string::npos);
  CHECK(error_of(*engine, "$hero.animation.play(:Dance, root_motion: true)").find("Dance") != std::string::npos);
}

TEST_CASE("root motion moves an object by its clip's root, the mesh staying on it") {
  const TempGame game(kPlainGame);
  const SceneDesc scene = hero_scene({});
  auto engine = engine_for(game, scene);
  ruby(*engine, "$hero = Scene.find('Hero'); $hero.animation.play(:Walk, root_motion: true)");
  frames(*engine, kHalfSecond);
  CHECK(number(*engine, "$hero.transform.position.x") == doctest::Approx(kWalkSpeed / 2.0f).epsilon(kTolerance));
  CHECK(hips_x(*engine) == doctest::Approx(0.0f).epsilon(kTolerance));
  CHECK(ruby(*engine, "$hero.animation.root_motion?") == "false");  // (for that clip only)
  ruby(*engine, "$hero.animation.play(:Walk, root_motion: false)");
  frames(*engine, kQuarterSecond);
  CHECK(number(*engine, "$hero.transform.position.x") == doctest::Approx(kWalkSpeed / 2.0f).epsilon(kTolerance));
  CHECK(hips_x(*engine) > kTolerance);  // the clip moves the hips again
}

TEST_CASE("root motion walks a character controller, and a wall stops it") {
  const TempGame game(kPlainGame);
  const SceneDesc scene = hero_scene(moving("character"), {node("Wall", {kWallX, 2, 0}, {1, 4, 4}, solid())});
  auto engine = engine_for(game, scene);
  ruby(*engine, "$hero = Scene.find('Hero'); $hero.animation.root_motion = true; $hero.animation.play(:Walk)");
  CHECK(ruby(*engine, "$hero.animation.root_motion?") == "true");
  frames(*engine, kHalfSecond);
  // (A frame late: what the clip moved this frame, the physics goes through in the next.)
  CHECK(number(*engine, "$hero.transform.position.x") == doctest::Approx(kWalkSpeed / 2.0f).epsilon(kTolerance * 2));
  frames(*engine, kThreeSeconds);
  const float x = number(*engine, "$hero.transform.position.x");
  CHECK(x < kWallX - kHeroHalfWidth * 2.0f + kTolerance);  // against the wall, not through it
  CHECK(x > kWallX - kHeroHalfWidth * 2.0f - kHeroHalfWidth);
  CHECK(hips_x(*engine) == doctest::Approx(0.0f).epsilon(kTolerance));
}
