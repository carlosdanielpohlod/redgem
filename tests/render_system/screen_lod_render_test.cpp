// Levels of detail chosen by size on screen (props screen_size_from / screen_size_to): the full
// mesh up close, a cross-fade near the switch that leaves no pixel uncovered, then the light one.
#include <doctest/doctest.h>

#include <cmath>
#include <string>

#include "render_system/render_stage.hpp"
#include "scene/props.hpp"
#include "scene/scene_config.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr float kSwitchAt = 10.0f;  // meters where the light level takes over
constexpr float kNearView = 6.0f;   // distances of the camera from the levels
constexpr float kMiddleView = 9.0f;
constexpr float kFarView = 14.0f;
constexpr float kLodFade = 1.0f;
constexpr int kNearMaterial = 0;
constexpr int kFarMaterial = 1;
constexpr float kGlow = 2.0f;
constexpr float kShare = 0.2f;   // each level supplies at least this share midway through the fade
constexpr float kMostly = 0.9f;  // how much of an unfaded cube a cross-fade must still cover
constexpr float kCenter = 0.5f;
constexpr float kHalf = 0.5f;

bool red(::Color c) { return c.r > c.g && c.r > c.b; }
bool green(::Color c) { return c.g > c.r && c.g > c.b; }
bool colored(::Color c) { return red(c) || green(c); }

float tan_half_fov() { return std::tan(degrees_to_radians(config::kDefaultFovYDegrees) * kHalf); }

// The cube's bounding sphere, and the share of the view's height it covers at the switch.
float cube_radius(const MeshDesc& cube) { return length(cube.bounds.max - cube.bounds.min) * kHalf; }
float switch_size(float radius) { return radius / (kSwitchAt * tan_half_fov()); }

NodeDesc level(const std::string& name, int mesh, PropertyMap props) {
  NodeDesc node;
  node.name = name;
  node.mesh = mesh;
  node.props = std::move(props);
  return node;
}

SceneDesc stage_scene(float distance, bool levels, bool same_color) {
  SceneDesc desc;
  const redgem::Color near_color{1.0f, 0.0f, 0.0f, 1.0f};
  const redgem::Color far_color{0.0f, 1.0f, 0.0f, 1.0f};
  desc.materials = {test::glowing_material(near_color, kGlow),
                    test::glowing_material(same_color ? near_color : far_color, kGlow)};
  desc.meshes = {test::cube_mesh(kNearMaterial), test::cube_mesh(kFarMaterial)};
  const auto radius = static_cast<double>(cube_radius(desc.meshes[0]));
  const auto size = static_cast<double>(switch_size(cube_radius(desc.meshes[0])));
  const PropertyMap shared{{props::kLodRadius, radius}, {props::kLodFade, static_cast<double>(kLodFade)}};
  PropertyMap near_props = shared;
  PropertyMap far_props = shared;
  near_props[props::kScreenSizeFrom] = size;
  far_props[props::kScreenSizeTo] = size;
  desc.nodes = {test::camera_at(Vec3{0.0f, 0.0f, distance}), level("Near", 0, levels ? near_props : PropertyMap{})};
  if (levels) desc.nodes.push_back(level("Far", 1, far_props));
  return desc;
}

Picture draw_levels(RenderStage& stage, float distance, bool levels, bool same_color, const std::string& name) {
  const SceneDesc desc = stage_scene(distance, levels, same_color);
  stage.upload(desc);
  return stage.draw(Scene::from_desc(desc), name);
}

}  // namespace

TEST_CASE("screen-size levels draw the full mesh up close, cross-fade, then only the light one") {
  RenderStage stage;
  const Picture near = draw_levels(stage, kNearView, true, false, "screen_lod_near");
  CHECK(red(near.at(kCenter, kCenter)));
  CHECK(near.find(green).count == 0);
  const Picture middle = draw_levels(stage, kMiddleView, true, false, "screen_lod_transition");
  const auto all = static_cast<float>(middle.find(colored).count);
  REQUIRE(all > 0.0f);
  CHECK(static_cast<float>(middle.find(red).count) > all * kShare);
  CHECK(static_cast<float>(middle.find(green).count) > all * kShare);
  const Picture far = draw_levels(stage, kFarView, true, false, "screen_lod_far");
  CHECK(green(far.at(kCenter, kCenter)));
  CHECK(far.find(red).count == 0);
}

TEST_CASE("a screen-size cross-fade covers every pixel a single mesh covers") {
  RenderStage stage;
  const Picture original = draw_levels(stage, kMiddleView, false, true, "screen_lod_original");
  const Picture transition = draw_levels(stage, kMiddleView, true, true, "screen_lod_coverage");
  const auto whole = static_cast<float>(original.find(red).count);
  REQUIRE(whole > 0.0f);
  CHECK(static_cast<float>(transition.find(red).count) >= whole * kMostly);
  REQUIRE(transition.pixels.size() == original.pixels.size());
  std::size_t differing = 0;
  for (std::size_t i = 0; i < original.pixels.size(); ++i) {
    const bool same = transition.pixels[i].r == original.pixels[i].r &&
                      transition.pixels[i].g == original.pixels[i].g && transition.pixels[i].b == original.pixels[i].b;
    if (!same) ++differing;
  }
  CHECK(differing == 0);
}
