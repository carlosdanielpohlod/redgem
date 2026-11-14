// System test: auto exposure and leaves lit from behind, drawn in a real window and read back as
// pixels. A dim wall is brightened toward middle grey; a wall that suddenly glows brighter is too
// bright for a moment, then settles; a leaf between the camera and the sun glows green.
#include <doctest/doctest.h>

#include <string>
#include <utility>

#include "core/image_header.hpp"
#include "core/safe_path.hpp"
#include "render_system/render_stage.hpp"
#include "scene/shapes/primitives.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr Vec3 kCameraAt{0.0f, 0.0f, 10.0f};
constexpr float kMiddle = 0.5f;
constexpr redgem::Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};
constexpr float kDimGlow = 0.02f;              // far darker than middle grey
constexpr float kBrighterGlow = 0.32f;         // sixteen times that
constexpr Vec3 kWallSize{40.0f, 40.0f, 0.1f};  // fills the view
constexpr double kStill = 0.0;
constexpr float kFrame = 1.0f / 60.0f;        // seconds
constexpr float kSettleStep = 0.25f;          // seconds a frame while it settles
constexpr int kSettleFrames = 40;             // ten seconds: settled at any speed it has
constexpr int kNoticeably = 20;               // brighter by more than this, of 765 for white
constexpr Vec3 kLeafTurn{90.0f, 0.0f, 0.0f};  // degrees: a plane, turned to face the camera
constexpr Vec3 kLeafSize{4.0f, 1.0f, 4.0f};
constexpr Vec3 kSunFacingTheCamera{0.0f, 180.0f, 0.0f};  // degrees: it shines from behind the leaf
constexpr float kSunStrength = 3.0f;

int luma(::Color c) { return c.r + c.g + c.b; }

NodeDesc environment_of(PropertyMap props) {
  NodeDesc node;
  node.name = "Environment";
  node.props = std::move(props);
  node.props["environment"] = true;
  return node;
}

// A wall glowing white, `glow` times over, in front of the camera; nothing else lights it (a sun of
// no strength stands in for the default one).
SceneDesc glowing_wall(float glow, bool auto_exposure) {
  SceneDesc desc;
  desc.materials = {test::glowing_material(kWhite, glow)};
  desc.meshes = {test::cube_mesh(0)};
  NodeDesc wall;
  wall.name = "Wall";
  wall.mesh = 0;
  wall.local.scale = kWallSize;
  const PropertyMap look = {{"auto_exposure", auto_exposure},
                            {"fog_density", kStill},
                            {"bloom", kStill},
                            {"vignette", kStill},
                            {"ambient", kStill},
                            {"grain", kStill}};
  NodeDesc dark_sun;
  dark_sun.name = "Sun";
  dark_sun.light = LightDesc{};
  desc.nodes = {test::camera_at(kCameraAt), wall, dark_sun, environment_of(look)};
  return desc;
}

::Color middle_of(RenderStage& stage, const SceneDesc& desc, const std::string& name) {
  stage.upload(desc);
  return stage.draw(Scene::from_desc(desc), name).at(kMiddle, kMiddle);
}

ImageData picture(const char* file, const char* extension) {
  const auto bytes = read_file_up_to(test::fixture("images") + "/" + file, kMaxEncodedImageBytes);
  REQUIRE(bytes.has_value());
  return ImageData{*bytes, extension};
}

// A green cut-out leaf (foliage) between the camera and the sun, lit by nothing else.
SceneDesc leaf_against_the_sun(double translucency) {
  SceneDesc desc;
  desc.images = {picture("sniff.png", ".png")};  // all green
  MaterialDesc leaf;
  leaf.base_color_image = 0;
  leaf.alpha_mode = AlphaMode::Mask;
  desc.materials = {leaf};
  MeshDesc plane{"Leaf", {make_primitive(PrimitiveShape::Plane)}, {}};
  plane.primitives[0].material = 0;
  for (std::size_t i = 0; i < plane.primitives[0].positions.size(); i += 3) {
    plane.bounds.include(vec3_from(&plane.primitives[0].positions[i]));
  }
  desc.meshes = {plane};
  NodeDesc card;
  card.name = "Leaf";
  card.mesh = 0;
  card.local.rotation = quat_from_euler_degrees(kLeafTurn);
  card.local.scale = kLeafSize;
  NodeDesc sun;
  sun.name = "Sun";
  sun.light = LightDesc{};
  sun.light->intensity = kSunStrength;
  sun.local.rotation = quat_from_euler_degrees(kSunFacingTheCamera);
  const PropertyMap look = {{"foliage_translucency", translucency},
                            {"ambient", kStill},
                            {"fog_density", kStill},
                            {"bloom", kStill},
                            {"sun_glow", kStill},
                            {"vignette", kStill}};
  desc.nodes = {test::camera_at(kCameraAt), card, sun, environment_of(look)};
  return desc;
}

}  // namespace

TEST_CASE("auto exposure brightens a dim scene toward middle grey") {
  RenderStage stage;
  const ::Color fixed = middle_of(stage, glowing_wall(kDimGlow, false), "exposure_fixed");
  const ::Color adapted = middle_of(stage, glowing_wall(kDimGlow, true), "exposure_adapted");
  CHECK(luma(adapted) > luma(fixed) + kNoticeably);
}

TEST_CASE("a scene that gets brighter is too bright for a moment, then the exposure settles") {
  RenderStage stage;
  SceneDesc desc = glowing_wall(kDimGlow, true);
  const Scene scene = Scene::from_desc(desc);
  stage.set_clock(0.0);
  stage.upload(desc);
  stage.draw(scene, "exposure_dim_settled");
  desc.materials[0] = test::glowing_material(kWhite, kBrighterGlow);
  stage.sync(desc);  // the same scene, through the same camera: it adapts instead of starting anew
  stage.set_clock(kFrame);
  const ::Color dazzled = stage.draw(scene, "exposure_dazzled").at(kMiddle, kMiddle);
  double clock = kFrame;
  for (int i = 0; i < kSettleFrames; ++i) {
    clock += kSettleStep;
    stage.set_clock(clock);
    stage.draw(scene, "exposure_settling");
  }
  const ::Color settled = stage.draw(scene, "exposure_bright_settled").at(kMiddle, kMiddle);
  CHECK(luma(dazzled) > luma(settled) + kNoticeably);
}

TEST_CASE("sunlight comes through a leaf between the camera and the sun") {
  RenderStage stage;
  const ::Color opaque = middle_of(stage, leaf_against_the_sun(0.0), "foliage_opaque");
  const ::Color translucent = middle_of(stage, leaf_against_the_sun(1.0), "foliage_translucent");
  CHECK(translucent.g > opaque.g + kNoticeably);
  CHECK(translucent.g > translucent.r);  // the leaf's own color
}
