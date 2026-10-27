#include <doctest/doctest.h>

#include <cmath>

#include "net/pose_codec.hpp"
#include "scene/animation/animation_blend.hpp"
#include "scene/animation/animation_player.hpp"

using namespace redgem;

namespace {

constexpr ObjectId kHero = 0;
constexpr ObjectId kBone = 1;
constexpr float kDistance = 10.0f;
constexpr float kDuration = 2.0f;

AnimationDesc constant_clip(const std::string& name, Vec3 position) {
  AnimationChannel channel;
  channel.node = kBone;
  channel.times = {0.0f};
  channel.values = {position.x, position.y, position.z};
  return {name, kDuration, {channel}};
}

struct GraphScene {
  SceneDesc desc;
  Scene scene;
  AnimationPlayer player;
  AnimationGraphDefinition graph;
  std::string error;

  GraphScene() {
    desc.nodes.resize(2);
    desc.nodes[kHero].children = {kBone};
    desc.nodes[kBone].parent = kHero;
    desc.animations = {constant_clip("Slide", {kDistance, 0, 0}), constant_clip("Lift", {0, kDistance, 0})};
    scene = Scene::from_desc(desc);
    player.reset(desc.animations, scene);
    graph.initial = "Move";
    graph.parameters = {{"speed", 0.0f}, {"jump", false}};
    graph.states = {{"Move", "speed", {{0.0f, "Slide"}, {1.0f, "Lift"}}}, {"Jump", "", {{0.0f, "Lift"}}}};
    graph.transitions = {{"Move", "Jump", "jump", AnimationComparison::Equal, true, 1.0f}};
  }

  void use(ObjectId id = kHero) { REQUIRE(player.use_graph(scene, id, graph, error)); }
  void advance(float dt) { player.advance(dt, scene); }
  Vec3 position(ObjectId id = kBone) const { return scene.at(id).local.position; }
};

std::vector<net::WeightedClipPart> wire_side(const std::vector<WeightedClipState>& side) {
  std::vector<net::WeightedClipPart> result;
  for (const auto& entry : side) {
    const auto& clip = entry.clip;
    result.push_back({{clip.clip, clip.time, clip.speed, clip.loop, clip.playing}, entry.weight});
  }
  return result;
}

void check_snapshot(const GraphScene& stage) {
  const auto state = stage.player.animation_of(stage.scene, kHero);
  REQUIRE(state.graph.has_value());
  const auto& graph = *state.graph;
  net::AnimationPart part;
  part.graph = net::GraphPart{wire_side(graph.from), wire_side(graph.to), graph.blend, graph.remaining, graph.playing};
  CHECK(net::carriable(part));
  net::PoseUpdate pose;
  pose.key = "Hero";
  pose.animation = part;
  REQUIRE(net::decoded(net::encoded(pose)).has_value());
}

}  // namespace

TEST_CASE("an installed graph blends a character pose and preserves invalid manual requests") {
  GraphScene stage;
  stage.use();
  REQUIRE(stage.player.set_parameter(stage.scene, kHero, "speed", 0.5f));
  stage.advance(0.0f);
  CHECK(stage.position().x == doctest::Approx(kDistance * 0.5f));
  CHECK(stage.position().y == doctest::Approx(kDistance * 0.5f));
  CHECK_FALSE(stage.player.play_on(stage.scene, kHero, "Missing", {}));
  CHECK(stage.player.graph_state(stage.scene, kHero) == "Move");
  REQUIRE(stage.player.play_on(stage.scene, kHero, "Lift", {}));
  CHECK_FALSE(stage.player.graph_state(stage.scene, kHero).has_value());
  stage.advance(0.0f);
  CHECK(stage.position().y == doctest::Approx(kDistance));
}

TEST_CASE("graph parameters and declarations fail without changing the active controller") {
  GraphScene stage;
  stage.use();
  const auto revision = stage.player.revision_of(stage.scene, kHero);
  CHECK(stage.player.set_parameter(stage.scene, kHero, "speed", 0.0f));
  CHECK(stage.player.revision_of(stage.scene, kHero) == revision);
  CHECK_FALSE(stage.player.set_parameter(stage.scene, kHero, "speed", true));
  stage.graph.states.back().points[0].clip = "Missing";
  CHECK_FALSE(stage.player.use_graph(stage.scene, kHero, stage.graph, stage.error));
  CHECK(stage.player.graph_state(stage.scene, kHero) == "Move");
}

TEST_CASE("a graph's evaluated tracks and fading pose agree on another machine") {
  GraphScene owner;
  GraphScene watcher;
  owner.use();
  REQUIRE(owner.player.set_parameter(owner.scene, kHero, "jump", true));
  owner.advance(0.25f);
  const auto state = owner.player.animation_of(owner.scene, kHero);
  REQUIRE(state.graph.has_value());
  watcher.player.take_animation(watcher.scene, kHero, state, 0.25f);
  owner.advance(0.25f);
  watcher.advance(0.0f);
  CHECK(watcher.position().x == doctest::Approx(owner.position().x));
  CHECK(watcher.position().y == doctest::Approx(owner.position().y));
  owner.advance(0.25f);
  watcher.advance(0.25f);
  CHECK(watcher.position().x == doctest::Approx(owner.position().x));
  CHECK(watcher.position().y == doctest::Approx(owner.position().y));
  const auto remote = watcher.player.animation_of(watcher.scene, kHero);
  const auto local = owner.player.animation_of(owner.scene, kHero);
  CHECK(remote.graph->to.front().clip.time == doctest::Approx(local.graph->to.front().clip.time));
}

TEST_CASE("invalid received graph tracks preserve the character's local controller") {
  GraphScene stage;
  stage.use();
  CharacterAnimation state;
  state.graph = GraphAnimation{};
  state.graph->to = {{{"Missing", 0.0f, 1.0f, true, true}, 1.0f}};
  stage.player.take_animation(stage.scene, kHero, state, 0.0f);
  CHECK(stage.player.graph_state(stage.scene, kHero) == "Move");
  state.graph->to.front().clip.clip = "Lift";
  state.graph->to.front().weight = -1.0f;
  stage.player.take_animation(stage.scene, kHero, state, 0.0f);
  CHECK(stage.player.graph_state(stage.scene, kHero) == "Move");
}

TEST_CASE("graph pause freezes the fade on both machines and following the scene leaves the graph") {
  GraphScene owner;
  GraphScene watcher;
  owner.use();
  REQUIRE(owner.player.set_parameter(owner.scene, kHero, "jump", true));
  owner.advance(0.25f);
  owner.player.pause_on(owner.scene, kHero);
  watcher.player.take_animation(watcher.scene, kHero, owner.player.animation_of(owner.scene, kHero), 0.5f);
  watcher.advance(0.5f);
  CHECK(watcher.position().x == doctest::Approx(kDistance * 0.75f));
  owner.player.follow_scene(owner.scene, kHero, 1.0f);
  CHECK_FALSE(owner.player.graph_state(owner.scene, kHero).has_value());
}

TEST_CASE("spawned characters have independent graph poses and preserve their placement") {
  GraphScene stage;
  const auto first = stage.scene.clone(kHero);
  const auto second = stage.scene.clone(kHero);
  REQUIRE(first.size() == 2);
  REQUIRE(second.size() == 2);
  stage.use(first.front());
  stage.use(second.front());
  REQUIRE(stage.player.set_parameter(stage.scene, second.front(), "speed", 1.0f));
  stage.advance(0.0f);
  CHECK(stage.position(first.back()).x == doctest::Approx(kDistance));
  CHECK(stage.position(second.back()).y == doctest::Approx(kDistance));
  CHECK(stage.position(first.front()).x == 0.0f);
}

TEST_CASE("graph pose blending fills missing channels from rest without feedback from earlier frames") {
  GraphScene stage;
  stage.desc.animations[1].channels.clear();
  stage.player.reset(stage.desc.animations, stage.scene);
  stage.use();
  REQUIRE(stage.player.set_parameter(stage.scene, kHero, "speed", 0.5f));
  stage.advance(0.0f);
  CHECK(stage.position().x == doctest::Approx(kDistance * 0.5f));
  REQUIRE(stage.player.set_parameter(stage.scene, kHero, "speed", 1.0f));
  stage.advance(0.0f);
  CHECK(stage.position().x == 0.0f);
  stage.advance(0.25f);
  CHECK(stage.position().x == 0.0f);
}

TEST_CASE("graph rotation blending treats opposite quaternion signs as the same orientation") {
  GraphScene stage;
  for (auto& clip : stage.desc.animations) {
    clip.channels[0].path = AnimationPath::Rotation;
    clip.channels[0].values = {0.0f, 1.0f, 0.0f, 0.0f};
  }
  stage.desc.animations[1].channels[0].values[1] = -1.0f;
  stage.player.reset(stage.desc.animations, stage.scene);
  stage.use();
  REQUIRE(stage.player.set_parameter(stage.scene, kHero, "speed", 0.5f));
  stage.advance(0.0f);
  CHECK(stage.scene.at(kBone).local.rotation.y == doctest::Approx(1.0f));
}

TEST_CASE("nonlooping graph snapshots and received clocks stop together") {
  GraphScene owner;
  GraphScene watcher;
  owner.graph.states.front().loop = false;
  SUBCASE("forward endpoint") {}
  SUBCASE("reverse endpoint") { owner.graph.states.front().speed = -1.0f; }
  SUBCASE("zero duration") {
    owner.desc.animations.front().duration = 0.0f;
    owner.player.reset(owner.desc.animations, owner.scene);
    watcher.desc.animations.front().duration = 0.0f;
    watcher.player.reset(watcher.desc.animations, watcher.scene);
  }
  owner.use();
  watcher.player.take_animation(watcher.scene, kHero, owner.player.animation_of(owner.scene, kHero), 0.0f);
  owner.advance(kDuration);
  watcher.advance(kDuration);
  CHECK_FALSE(owner.player.playing_on(owner.scene, kHero));
  CHECK_FALSE(watcher.player.playing_on(watcher.scene, kHero));
  const auto stopped = owner.player.animation_of(owner.scene, kHero);
  REQUIRE(stopped.graph.has_value());
  CHECK_FALSE(stopped.graph->playing);
  CHECK_FALSE(stopped.graph->to.front().clip.playing);
  watcher.player.take_animation(watcher.scene, kHero, stopped, kDuration);
  CHECK_FALSE(watcher.player.playing_on(watcher.scene, kHero));
  check_snapshot(owner);
}

TEST_CASE("a completed graph destination does not stop a fade before it finishes") {
  GraphScene owner;
  GraphScene watcher;
  owner.graph.states.back().loop = false;
  owner.graph.states.back().speed = 8.0f;  // finish the clip while the one-second fade is still active
  owner.use();
  REQUIRE(owner.player.set_parameter(owner.scene, kHero, "jump", true));
  owner.advance(0.5f);
  const auto snapshot = owner.player.animation_of(owner.scene, kHero);
  REQUIRE(snapshot.graph.has_value());
  CHECK(snapshot.graph->playing);
  CHECK_FALSE(snapshot.graph->to.front().clip.playing);
  watcher.player.take_animation(watcher.scene, kHero, snapshot, 0.0f);
  owner.advance(0.5f);
  watcher.advance(0.5f);
  CHECK_FALSE(owner.player.playing_on(owner.scene, kHero));
  CHECK_FALSE(watcher.player.playing_on(watcher.scene, kHero));
  CHECK(watcher.position().y == doctest::Approx(owner.position().y));
  check_snapshot(watcher);
}

TEST_CASE("unrepresentable synchronized speeds cannot replace an installed graph") {
  GraphScene stage;
  stage.use();
  stage.advance(0.25f);
  const auto revision = stage.player.revision_of(stage.scene, kHero);
  stage.desc.animations.front().duration = 1.0f;
  stage.graph.states.front().speed = 3.0e38f;  // a half blend derives a speed of 4e38 for the longer clip
  stage.graph.parameters["speed"] = 0.5f;
  CHECK_FALSE(stage.player.use_graph(stage.scene, kHero, stage.graph, stage.error));
  CHECK_FALSE(stage.error.empty());
  CHECK(stage.player.revision_of(stage.scene, kHero) == revision);
  CHECK(stage.player.time_on(stage.scene, kHero) == doctest::Approx(0.25f));
  check_snapshot(stage);
}

TEST_CASE("unrepresentable synchronized parameter updates preserve the controller and its revision") {
  GraphScene stage;
  stage.desc.animations.front().duration = 1.0f;
  stage.graph.states.front().speed = 3.0e38f;
  stage.use();
  const auto revision = stage.player.revision_of(stage.scene, kHero);
  CHECK_FALSE(stage.player.set_parameter(stage.scene, kHero, "speed", 0.5f));
  CHECK(stage.player.revision_of(stage.scene, kHero) == revision);
  const auto snapshot = stage.player.animation_of(stage.scene, kHero);
  REQUIRE(snapshot.graph.has_value());
  REQUIRE(snapshot.graph->to.size() == 1);
  CHECK(snapshot.graph->to.front().clip.clip == "Slide");
  CHECK(std::isfinite(snapshot.graph->to.front().clip.speed));
  check_snapshot(stage);
}

TEST_CASE("nearly complete graph fades remain valid network snapshots") {
  GraphScene owner;
  GraphScene watcher;
  owner.use();
  REQUIRE(owner.player.set_parameter(owner.scene, kHero, "jump", true));
  owner.advance(0.5f);
  watcher.player.take_animation(watcher.scene, kHero, owner.player.animation_of(owner.scene, kHero), 0.0f);
  const float almost_half = std::nextafter(0.5f, 0.0f);
  owner.advance(almost_half);
  watcher.advance(almost_half);
  check_snapshot(owner);
  check_snapshot(watcher);
  const auto snapshot = owner.player.animation_of(owner.scene, kHero);
  REQUIRE(snapshot.graph.has_value());
  CHECK(snapshot.graph->remaining > 0.0f);
  CHECK(snapshot.graph->blend < 1.0f);
  owner.advance(0.5f);
  watcher.advance(0.5f);
  check_snapshot(owner);
  check_snapshot(watcher);
}

TEST_CASE("two dimensional graph poses survive snapshots and continue on a watching machine") {
  GraphScene owner;
  owner.desc.animations = {constant_clip("A", {}), constant_clip("B", {kDistance, 0, 0}),
                           constant_clip("C", {0, kDistance, 0}), constant_clip("D", {kDistance, kDistance, 0})};
  owner.player.reset(owner.desc.animations, owner.scene);
  owner.graph.parameters = {{"x", 0.25f}, {"y", 0.75f}};
  owner.graph.states = {{"Move", "x", {{0, "A", 0}, {1, "B", 0}, {0, "C", 1}, {1, "D", 1}}, true, 1, "y"}};
  owner.graph.transitions.clear();
  owner.use();
  owner.advance(0.25f);
  CHECK(owner.position().x == doctest::Approx(kDistance * 0.25f));
  CHECK(owner.position().y == doctest::Approx(kDistance * 0.75f));
  check_snapshot(owner);
  GraphScene watcher;
  watcher.player.reset(owner.desc.animations, watcher.scene);
  watcher.player.take_animation(watcher.scene, kHero, owner.player.animation_of(owner.scene, kHero), 0.25f);
  watcher.advance(0.25f);
  owner.advance(0.5f);
  CHECK(watcher.position().x == doctest::Approx(owner.position().x));
  CHECK(watcher.position().y == doctest::Approx(owner.position().y));
  CHECK(watcher.player.animation_of(watcher.scene, kHero).graph->to.size() == 4);
}
