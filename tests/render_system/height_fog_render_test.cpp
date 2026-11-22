// System test: the air between the eye and a wall, drawn in a real window and read back as pixels.
// Fog that lies low hides a wall in it more than one above it; fog the sun shines through toward
// the eye is brighter than fog it does not; haze takes blue out of the light first.
#include <doctest/doctest.h>

#include <string>
#include <utility>

#include "render_system/render_stage.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::RenderStage;

namespace {

constexpr Vec3 kCameraAt{0.0f, 0.0f, 30.0f};  // level with the wall, 30 m away
constexpr float kMiddle = 0.5f;
constexpr Vec3 kWallSize{120.0f, 120.0f, 0.1f};  // fills the view
constexpr redgem::Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};
constexpr float kDark = 0.0f;  // the wall's glow: black
constexpr float kLit = 0.5f;   // a white wall's glow
constexpr double kNone = 0.0;
constexpr double kThickFog = 0.05;
constexpr double kLowFog = 0.5;         // per meter up: gone a few meters above its height
constexpr double kFogFarBelow = -20.0;  // meters: the camera and the wall are well above it
constexpr double kSunScatter = 1.0;
constexpr double kThickHaze = 0.03;
constexpr float kSunStrength = 3.0f;
constexpr Vec3 kSunFacingTheCamera{0.0f, 180.0f, 0.0f};  // degrees: it shines from behind the wall
constexpr int kNoticeably = 20;                          // brighter by more than this, of 765 for white

int luma(::Color c) { return c.r + c.g + c.b; }

// An environment of no light of its own but its air, `air` the settings of it.
NodeDesc environment_of(PropertyMap air) {
  NodeDesc node;
  node.name = "Environment";
  node.props = std::move(air);
  node.props["environment"] = true;
  for (const char* off : {"ambient", "bloom", "vignette", "sun_glow", "grain"}) node.props[off] = kNone;
  return node;
}

// A wall glowing white `glow` times over in front of the camera, a sun behind it shining toward the
// eye, and air between.
SceneDesc wall_through(float glow, PropertyMap air) {
  SceneDesc desc;
  desc.materials = {test::glowing_material(kWhite, glow)};
  desc.meshes = {test::cube_mesh(0)};
  NodeDesc wall;
  wall.name = "Wall";
  wall.mesh = 0;
  wall.local.scale = kWallSize;
  NodeDesc sun;
  sun.name = "Sun";
  sun.light = LightDesc{};
  sun.light->intensity = kSunStrength;
  sun.local.rotation = quat_from_euler_degrees(kSunFacingTheCamera);
  desc.nodes = {test::camera_at(kCameraAt), wall, sun, environment_of(std::move(air))};
  return desc;
}

::Color middle_of(RenderStage& stage, const SceneDesc& desc, const std::string& name) {
  stage.upload(desc);
  return stage.draw(Scene::from_desc(desc), name).at(kMiddle, kMiddle);
}

}  // namespace

TEST_CASE("fog that lies low hides what is in it, and not what stands above it") {
  RenderStage stage;
  const PropertyMap in_it = {
      {"fog_density", kThickFog}, {"fog_color", std::string("#ffffff")}, {"fog_falloff", kLowFog}};
  PropertyMap above_it = in_it;
  above_it["fog_height"] = kFogFarBelow;
  const ::Color hidden = middle_of(stage, wall_through(kDark, in_it), "height_fog_in_it");
  const ::Color clear = middle_of(stage, wall_through(kDark, above_it), "height_fog_above_it");
  CHECK(luma(hidden) > luma(clear) + kNoticeably);
}

TEST_CASE("fog with no falloff is as thick at any height") {
  RenderStage stage;
  const PropertyMap here = {{"fog_density", kThickFog}, {"fog_color", std::string("#ffffff")}};
  PropertyMap far_below = here;
  far_below["fog_height"] = kFogFarBelow;
  const ::Color level = middle_of(stage, wall_through(kDark, here), "uniform_fog_here");
  const ::Color lowered = middle_of(stage, wall_through(kDark, far_below), "uniform_fog_height_ignored");
  CHECK(luma(level) == luma(lowered));
}

TEST_CASE("fog the sun shines through toward the eye glows") {
  RenderStage stage;
  const PropertyMap dark_fog = {{"fog_density", kThickFog}, {"fog_color", std::string("#000000")}};
  PropertyMap sunlit_fog = dark_fog;
  sunlit_fog["fog_sun_scatter"] = kSunScatter;
  const ::Color unlit = middle_of(stage, wall_through(kDark, dark_fog), "fog_without_sun");
  const ::Color glowing = middle_of(stage, wall_through(kDark, sunlit_fog), "fog_toward_the_sun");
  CHECK(luma(glowing) > luma(unlit) + kNoticeably);
}

TEST_CASE("haze takes blue out of the light before red") {
  RenderStage stage;
  const PropertyMap haze = {{"fog_density", kNone}, {"fog_color", std::string("#000000")}, {"haze", kThickHaze}};
  const ::Color seen = middle_of(stage, wall_through(kLit, haze), "haze_over_a_white_wall");
  CHECK(seen.r > seen.b + kNoticeably);
}
