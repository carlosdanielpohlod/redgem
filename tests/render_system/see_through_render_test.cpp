// System test: see-through materials as the renderer draws them, read back as pixels: blended
// cards mixed the farthest first whatever order the scene lists them in, a nearly opaque one
// hiding what is behind it, and a dithered one leaving out a share of its pixels.
#include <doctest/doctest.h>

#include <string>

#include "render_system/render_stage.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr Vec3 kCameraAt{0.0f, 0.0f, 10.0f};
constexpr float kMiddle = 0.5f;
constexpr float kGlow = 1.0f;
constexpr float kHalfSeen = 0.5f;             // alpha of a card half see-through
constexpr Vec3 kCardSize{4.0f, 4.0f, 0.02f};  // meters: thin, facing the camera
constexpr float kNear = 2.0f;                 // meters along z
constexpr float kFar = 0.0f;
constexpr float kRegion = 0.1f;       // of the picture, around its middle, where pixels are counted
constexpr float kFewestShare = 0.2f;  // of the region: each color a dithered card shows at least this

bool greener_than_red(::Color c) { return c.g > c.r; }
bool redder_than_green(::Color c) { return c.r > c.g; }

MaterialDesc card_material(redgem::Color color, AlphaMode mode) {
  MaterialDesc material = test::glowing_material(color, kGlow);
  material.alpha_mode = mode;
  return material;
}

NodeDesc card(const std::string& name, int mesh, float z) {
  NodeDesc node;
  node.name = name;
  node.mesh = mesh;
  node.local.position = {0.0f, 0.0f, z};
  node.local.scale = kCardSize;
  return node;
}

// A camera and two cards, one behind the other, each of its own material; the near one is listed first.
SceneDesc two_cards(const MaterialDesc& near, const MaterialDesc& far) {
  SceneDesc desc;
  desc.materials = {near, far};
  desc.meshes = {test::cube_mesh(0), test::cube_mesh(1)};
  desc.nodes = {test::camera_at(kCameraAt), card("Near", 0, kNear), card("Far", 1, kFar)};
  return desc;
}

::Color middle_of(const SceneDesc& desc, const std::string& name) {
  RenderStage stage;
  stage.upload(desc);
  return stage.draw(Scene::from_desc(desc), name).at(kMiddle, kMiddle);
}

// The share of the pixels around the middle of a picture that `wanted` picks.
float share_around_middle(const Picture& picture, bool (*wanted)(::Color)) {
  int picked = 0;
  int all = 0;
  for (float across = kMiddle - kRegion; across < kMiddle + kRegion; across += 1.0f / picture.width) {
    for (float down = kMiddle - kRegion; down < kMiddle + kRegion; down += 1.0f / picture.height) {
      picked += wanted(picture.at(across, down)) ? 1 : 0;
      ++all;
    }
  }
  return static_cast<float>(picked) / static_cast<float>(all);
}

}  // namespace

TEST_CASE("blended cards are mixed the farthest first, whatever order the scene lists them in") {
  const SceneDesc desc = two_cards(card_material({0.0f, 1.0f, 0.0f, kHalfSeen}, AlphaMode::Blend),
                                   card_material({1.0f, 0.0f, 0.0f, kHalfSeen}, AlphaMode::Blend));
  const ::Color seen = middle_of(desc, "see_through_cards_sorted");
  CHECK(greener_than_red(seen));  // the near green card was mixed over the far red one
  CHECK(seen.r > 0);              // ... which shows through it
}

TEST_CASE("the nearly opaque part of a blended material hides what is behind it") {
  const SceneDesc desc = two_cards(card_material({0.0f, 1.0f, 0.0f, 1.0f}, AlphaMode::Blend),
                                   card_material({1.0f, 0.0f, 0.0f, 1.0f}, AlphaMode::Opaque));
  const ::Color seen = middle_of(desc, "see_through_core_hides");
  CHECK(greener_than_red(seen));
  CHECK(seen.r < seen.g / 4);
}

TEST_CASE("a dithered card leaves out a share of its pixels instead of mixing them") {
  MaterialDesc fur = card_material({0.0f, 1.0f, 0.0f, kHalfSeen}, AlphaMode::Blend);
  fur.dithered = true;
  const SceneDesc desc = two_cards(fur, card_material({1.0f, 0.0f, 0.0f, 1.0f}, AlphaMode::Opaque));
  RenderStage stage;
  stage.upload(desc);
  const Picture picture = stage.draw(Scene::from_desc(desc), "see_through_dithered");
  CHECK(share_around_middle(picture, greener_than_red) > kFewestShare);
  CHECK(share_around_middle(picture, redder_than_green) > kFewestShare);
}
