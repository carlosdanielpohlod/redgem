// Distance replacements use the same dither in every pass and keep the original copy's position.
#include <doctest/doctest.h>

#include <string>

#include "render_system/render_stage.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr Vec3 kCamera{0.0f, 0.0f, 10.0f};
constexpr float kTakeOver = 8.0f;  // meters where the far mesh begins appearing
constexpr float kTransition = 4.0f;
constexpr float kNearView = 6.0f;  // distances of the copy from the camera
constexpr float kMiddleView = 10.0f;
constexpr float kFarView = 14.0f;
constexpr int kNearMesh = 0;
constexpr int kFarMesh = 1;
constexpr float kGlow = 2.0f;
constexpr float kMostly = 0.9f;  // how much of an unfaded copy a cross-fade must still cover
constexpr float kShare = 0.25f;  // each level supplies at least this share midway through
constexpr float kCenter = 0.5f;

bool red(::Color c) { return c.r > c.g && c.r > c.b; }
bool green(::Color c) { return c.g > c.r && c.g > c.b; }
bool colored(::Color c) { return red(c) || green(c); }

SceneDesc stage_scene(bool same_color) {
  SceneDesc desc;
  const redgem::Color near_color{1.0f, 0.0f, 0.0f, 1.0f};
  const redgem::Color far_color{0.0f, 1.0f, 0.0f, 1.0f};
  desc.materials = {test::glowing_material(near_color, kGlow),
                    test::glowing_material(same_color ? near_color : far_color, kGlow)};
  desc.meshes = {test::cube_mesh(kNearMesh), test::cube_mesh(kFarMesh)};
  desc.nodes = {test::camera_at(kCamera)};
  return desc;
}

Picture draw_copy(RenderStage& stage, float distance, bool lods, bool same_color, const std::string& name) {
  const SceneDesc desc = stage_scene(same_color);
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  ScatterSet set;
  set.mesh = kNearMesh;
  set.fade = kTransition;
  if (lods) set.lods = {{kFarMesh, kTakeOver}};
  set.places = {redgem::Transform{{0.0f, 0.0f, kCamera.z - distance}, {}, Vec3::one()}};
  scene.scattered().add(set);
  return stage.draw(scene, name);
}

}  // namespace

TEST_CASE("scatter LODs draw the near mesh, cross-fade, then draw only the far mesh") {
  RenderStage stage;
  const Picture near = draw_copy(stage, kNearView, true, false, "scatter_lod_near");
  CHECK(red(near.at(kCenter, kCenter)));
  CHECK(near.find(green).count == 0);
  const Picture middle = draw_copy(stage, kMiddleView, true, false, "scatter_lod_transition");
  const auto all = static_cast<float>(middle.find(colored).count);
  REQUIRE(all > 0.0f);
  CHECK(static_cast<float>(middle.find(red).count) > all * kShare);
  CHECK(static_cast<float>(middle.find(green).count) > all * kShare);
  const Picture far = draw_copy(stage, kFarView, true, false, "scatter_lod_far");
  CHECK(green(far.at(kCenter, kCenter)));
  CHECK(far.find(red).count == 0);
}

TEST_CASE("scatter LOD cross-fades keep the same coverage as a single representation") {
  RenderStage stage;
  const Picture original = draw_copy(stage, kMiddleView, false, true, "scatter_lod_original");
  const Picture transition = draw_copy(stage, kMiddleView, true, true, "scatter_lod_coverage");
  const auto whole = static_cast<float>(original.find(red).count);
  REQUIRE(whole > 0.0f);
  CHECK(static_cast<float>(transition.find(red).count) >= whole * kMostly);
  REQUIRE(transition.pixels.size() == original.pixels.size());
  for (std::size_t i = 0; i < original.pixels.size(); ++i) {
    CHECK(transition.pixels[i].r == original.pixels[i].r);
    CHECK(transition.pixels[i].g == original.pixels[i].g);
    CHECK(transition.pixels[i].b == original.pixels[i].b);
  }
}
