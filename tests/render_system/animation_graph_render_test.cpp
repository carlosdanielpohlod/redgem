#include <doctest/doctest.h>

#include "render_system/render_stage.hpp"
#include "scene/animation/animation_player.hpp"

using namespace redgem;
using namespace redgem::test;

namespace {

constexpr ObjectId kCube = 1;
constexpr Vec3 kView{0.0f, 0.0f, 8.0f};
constexpr float kGlow = 2.0f;

AnimationDesc location(const std::string& name, float x) {
  AnimationChannel channel;
  channel.node = kCube;
  channel.times = {0.0f};
  channel.values = {x, 0.0f, 0.0f};
  return {name, 1.0f, {channel}};
}

SceneDesc graph_scene() {
  SceneDesc desc;
  desc.materials = {glowing_material(redgem::Color{1.0f, 0.0f, 0.0f, 1.0f}, kGlow)};
  desc.meshes = {cube_mesh(0)};
  NodeDesc cube;
  cube.mesh = 0;
  desc.nodes = {camera_at(kView), cube};
  desc.animations = {location("Left", -1.0f), location("Right", 1.0f)};
  return desc;
}

bool red(::Color color) { return color.r > color.g && color.r > color.b; }

}  // namespace

TEST_CASE("a graph blends rendered animation poses and matches a manually positioned reference") {
  RenderStage stage;
  const auto desc = graph_scene();
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  const auto reference = stage.draw(scene, "animation_graph_reference");
  AnimationPlayer player;
  player.reset(desc.animations, scene);
  AnimationGraphDefinition graph;
  graph.initial = "Move";
  graph.parameters = {{"speed", 0.5f}};
  graph.states = {{"Move", "speed", {{0.0f, "Left"}, {1.0f, "Right"}}}};
  std::string error;
  REQUIRE(player.use_graph(scene, kCube, graph, error));
  player.advance(0.0f, scene);
  const auto middle = stage.draw(scene, "animation_graph_middle");
  REQUIRE(reference.pixels.size() == middle.pixels.size());
  for (std::size_t pixel = 0; pixel < reference.pixels.size(); ++pixel) {
    CHECK(reference.pixels[pixel].r == middle.pixels[pixel].r);
    CHECK(reference.pixels[pixel].g == middle.pixels[pixel].g);
    CHECK(reference.pixels[pixel].b == middle.pixels[pixel].b);
  }
  REQUIRE(player.set_parameter(scene, kCube, "speed", 0.0f));
  player.advance(0.0f, scene);
  const auto left = stage.draw(scene, "animation_graph_left");
  REQUIRE(player.set_parameter(scene, kCube, "speed", 1.0f));
  player.advance(0.0f, scene);
  const auto right = stage.draw(scene, "animation_graph_right");
  CHECK(left.find(red).across < middle.find(red).across);
  CHECK(right.find(red).across > middle.find(red).across);
}
