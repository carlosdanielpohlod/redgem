// Root motion: a character's own clips moving the character, the clip's root kept in place.

#include <doctest/doctest.h>

#include <cmath>
#include <string>

#include "scene/animation/animation_player.hpp"

using namespace redgem;

namespace {

constexpr ObjectId kHero = 0;
constexpr ObjectId kHips = 1;
constexpr float kClipSeconds = 1.0f;
constexpr float kHipsHeight = 1.0f;
constexpr float kWalkDistance = 2.0f;  // a lap of Walk moves the hips this far along x
constexpr float kRunDistance = 4.0f;
constexpr float kQuarter = 0.25f;
constexpr float kHalfway = 0.5f;
constexpr float kQuarterTurn = kPi / 2.0f;
constexpr float kTolerance = 1e-4f;

AnimationDesc walking(const std::string& name, float distance) {
  AnimationChannel channel;
  channel.node = kHips;
  channel.times = {0.0f, kClipSeconds};
  channel.values = {0, kHipsHeight, 0, distance, kHipsHeight, 0};
  return {name, kClipSeconds, {channel}, {}};
}

// A quarter turn to the left over the clip.
AnimationDesc turning() {
  const Quat turned = quat_from_axis_angle(Vec3::up(), kQuarterTurn);
  AnimationChannel channel;
  channel.node = kHips;
  channel.path = AnimationPath::Rotation;
  channel.times = {0.0f, kClipSeconds};
  channel.values = {0, 0, 0, 1, turned.x, turned.y, turned.z, turned.w};
  return {"Turn", kClipSeconds, {channel}, {}};
}

struct Stage {
  SceneDesc desc;
  Scene scene;
  AnimationPlayer player;

  Stage() {
    desc.nodes.resize(2);
    desc.nodes[kHero].name = "Hero";
    desc.nodes[kHero].children = {kHips};
    desc.nodes[kHips].name = "Hips";
    desc.nodes[kHips].parent = kHero;
    desc.nodes[kHips].local.position = {0, kHipsHeight, 0};
    desc.animations = {walking("Walk", kWalkDistance), walking("Run", kRunDistance), turning()};
    scene = Scene::from_desc(desc);
    player.reset(desc.animations, scene);
  }

  void play(const std::string& name) { REQUIRE(player.play_on(scene, kHero, name, {true, 1.0f, 0.0f, true})); }
  // How far the hero's root went over `seconds`.
  RootMotion moved(float seconds) {
    player.advance(seconds, scene);
    RootMotion total;
    for (const RootMotion& motion : player.take_root_motion()) {
      CHECK(motion.character == scene.handle(kHero));
      total.translation = total.translation + motion.translation;
      total.yaw += motion.yaw;
    }
    return total;
  }
  Vec3 hips() const { return scene.at(kHips).local.position; }
};

}  // namespace

TEST_CASE("a clip with root motion moves the character by its root's travel, the root staying put") {
  Stage stage;
  REQUIRE(stage.player.set_clip_root_motion(stage.scene, kHero, "Walk", true));
  stage.play("Walk");
  const RootMotion motion = stage.moved(kQuarter);
  CHECK(motion.translation.x == doctest::Approx(kWalkDistance * kQuarter));
  CHECK(motion.translation.y == doctest::Approx(0.0f));
  CHECK(motion.yaw == doctest::Approx(0.0f));
  CHECK(stage.hips().x == doctest::Approx(0.0f).epsilon(kTolerance));
  CHECK(stage.hips().y == doctest::Approx(kHipsHeight));  // vertical motion stays in the pose
}

TEST_CASE("a lap that wraps round counts the way to the end and on from the start") {
  Stage stage;
  stage.player.set_root_motion(stage.scene, kHero, true);
  CHECK(stage.player.root_motion(stage.scene, kHero));
  stage.play("Walk");
  CHECK(stage.moved(0.9f).translation.x == doctest::Approx(kWalkDistance * 0.9f));
  CHECK(stage.moved(0.2f).translation.x == doctest::Approx(kWalkDistance * 0.2f));
  CHECK(stage.moved(kClipSeconds * 2.0f).translation.x == doctest::Approx(kWalkDistance * 2.0f));
}

TEST_CASE("root motion turns the character by its root's yaw") {
  Stage stage;
  stage.player.set_root_motion(stage.scene, kHero, true);
  stage.play("Turn");
  CHECK(stage.moved(kHalfway).yaw == doctest::Approx(kQuarterTurn * kHalfway).epsilon(0.01));
  CHECK(approx_equal(stage.scene.at(kHips).local.rotation, Quat::identity(), kTolerance));
}

TEST_CASE("blended clips move the character by their weights") {
  Stage stage;
  stage.player.set_root_motion(stage.scene, kHero, true);
  AnimationGraphDefinition graph;
  graph.initial = "Move";
  graph.parameters = {{"speed", kHalfway}};
  graph.states = {{"Move", "speed", {{0.0f, "Walk"}, {1.0f, "Run"}}}};
  std::string error;
  REQUIRE(stage.player.use_graph(stage.scene, kHero, graph, error));
  const float expected = (kWalkDistance + kRunDistance) * kHalfway * kQuarter;
  CHECK(stage.moved(kQuarter).translation.x == doctest::Approx(expected));
}

TEST_CASE("without root motion the clip moves the root, and the character stays") {
  Stage stage;
  stage.play("Walk");
  CHECK(stage.moved(kQuarter).translation.x == doctest::Approx(0.0f));
  CHECK(stage.hips().x == doctest::Approx(kWalkDistance * kQuarter));
  // Root motion for another clip leaves this one as it is.
  REQUIRE(stage.player.set_clip_root_motion(stage.scene, kHero, "Run", true));
  CHECK(stage.moved(kQuarter).translation.x == doctest::Approx(0.0f));
  CHECK_FALSE(stage.player.set_clip_root_motion(stage.scene, kHero, "Dance", true));
}
