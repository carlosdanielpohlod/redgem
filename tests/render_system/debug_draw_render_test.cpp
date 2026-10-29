#include <doctest/doctest.h>
#include <algorithm>

#include "render/debug_draw.hpp"
#include "render_system/render_stage.hpp"

using namespace redgem;
namespace {
constexpr Vec3 kCamera{0.0f, 0.0f, 5.0f};
constexpr Vec3 kWallSize{2.0f, 2.0f, 2.0f};
constexpr Vec3 kBehind{0.0f, 0.0f, -2.0f};
constexpr float kHalfLine = 0.5f;
constexpr unsigned char kBrightGreen = 200;
constexpr unsigned char kOtherChannel = 32;
constexpr redgem::Color kRed{1.0f, 0.0f, 0.0f, 1.0f};
constexpr redgem::Color kGreen{0.0f, 1.0f, 0.0f, 1.0f};
bool green(::Color pixel) { return pixel.g > kBrightGreen && pixel.r < kOtherChannel && pixel.b < kOtherChannel; }
bool yellow(::Color pixel) { return pixel.r > kBrightGreen && pixel.g > kBrightGreen && pixel.b < kOtherChannel; }
bool same_pixel(::Color a, ::Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
SceneDesc wall_scene() {
  SceneDesc desc;
  desc.materials = {test::glowing_material(kRed, 1.0f)};
  desc.meshes = {test::cube_mesh(0)};
  NodeDesc wall;
  wall.mesh = 0;
  wall.local.scale = kWallSize;
  NodeDesc environment;
  environment.props["environment"] = true;
  environment.props["antialias"] = std::string("fxaa");
  desc.nodes = {test::camera_at(kCamera), wall, environment};
  return desc;
}
}  // namespace
TEST_CASE("debug lines and boxes obey scene depth and explicitly support x-ray") {
  DebugDrawItem command;
  command.color = kGreen;
  std::string name;
  SUBCASE("line") {
    name = "line";
    command.first = kBehind - Vec3{kHalfLine, 0.0f, 0.0f};
    command.second = kBehind + Vec3{kHalfLine, 0.0f, 0.0f};
  }
  SUBCASE("box") {
    name = "box";
    command.kind = DebugDrawItem::Kind::Box;
    command.first = kBehind;
    command.second = {1.0f, 1.0f, 1.0f};
  }
  test::RenderStage stage;
  stage.set_clock(0.0);
  const auto desc = wall_scene();
  stage.upload(desc);
  const auto scene = Scene::from_desc(desc);
  DebugDrawBuffer commands;
  REQUIRE(commands.append(command));
  CHECK(stage.draw_debug(scene, commands, "debug_" + name + "_hidden").find(green).count == 0);
  commands.clear();
  command.depth_test = false;
  REQUIRE(commands.append(command));
  CHECK(stage.draw_debug(scene, commands, "debug_" + name + "_xray").find(green).count > 0);
  commands.clear();
  auto tested = command;
  tested.depth_test = true;
  auto xray = command;
  xray.color = {1.0f, 1.0f, 0.0f, 1.0f};
  constexpr float kSeparation = 0.25f;  // distinguish the two depth batches in pixel readback
  tested.first.y += kSeparation;
  xray.first.y -= kSeparation;
  if (command.kind == DebugDrawItem::Kind::Line) {
    tested.second.y += kSeparation;
    xray.second.y -= kSeparation;
  }
  REQUIRE(commands.append(tested));
  REQUIRE(commands.append(xray));
  const auto mixed = stage.draw_debug(scene, commands, "debug_" + name + "_mixed");
  CHECK(mixed.find(green).count == 0);
  CHECK(mixed.find(yellow).count > 0);
  commands.clear();
  command.depth_test = true;
  command.first.z = 2.0f;
  if (command.kind == DebugDrawItem::Kind::Line) command.second.z = 2.0f;
  REQUIRE(commands.append(command));
  CHECK(stage.draw_debug(scene, commands, "debug_" + name + "_front").find(green).count > 0);
  commands.clear();
  constexpr int kReflectionWarmup = 12;  // two full cycles of the progressive six-face panorama
  for (int frame = 0; frame < kReflectionWarmup; ++frame) stage.draw(scene, "debug_reflection_warmup");
  const auto clean = stage.draw(scene, "debug_" + name + "_clean");
  const auto empty = stage.draw_debug(scene, commands, "debug_" + name + "_empty");
  CHECK(clean.pixels.size() == empty.pixels.size());
  CHECK(std::equal(clean.pixels.begin(), clean.pixels.end(), empty.pixels.begin(), same_pixel));
}
