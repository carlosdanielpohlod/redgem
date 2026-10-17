// System test: see-through materials as the renderer draws them, read back as pixels: blended
// cards mixed the farthest first whatever order the scene lists them in, scattered ones among
// them, a nearly opaque one hiding what is behind it, a dithered one leaving out a share of its
// pixels, and a card seen in a mirror.
#include <doctest/doctest.h>

#include <string>

#include "render_system/render_stage.hpp"
#include "scene/scatter.hpp"

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
constexpr float kRegion = 0.1f;         // of the picture, around its middle, where pixels are counted
constexpr float kFewestShare = 0.2f;    // of the region: each color a dithered card shows at least this
constexpr int kNearCardNode = 1;        // of two_cards: after the camera
constexpr float kBehindCamera = 14.0f;  // meters along z: what only a mirror shows
constexpr int kFullTurn = 7;            // frames: reflections capture one cube face a frame, six in all

bool greener_than_red(::Color c) { return c.g > c.r; }
bool redder_than_green(::Color c) { return c.r > c.g; }
bool greener_than_blue(::Color c) { return c.g > c.b; }

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

// A camera facing a mirror; behind the camera, a half see-through green card when `with_card`.
SceneDesc mirror_room(bool with_card) {
  MaterialDesc mirror;
  mirror.metallic = 1.0f;
  mirror.roughness = 0.0f;
  SceneDesc desc;
  desc.materials = {mirror, card_material({0.0f, 1.0f, 0.0f, kHalfSeen}, AlphaMode::Blend)};
  desc.meshes = {test::cube_mesh(0), test::cube_mesh(1)};
  desc.nodes = {test::camera_at(kCameraAt), card("Mirror", 0, kFar)};
  if (with_card) desc.nodes.push_back(card("Behind", 1, kBehindCamera));
  return desc;
}

// The middle of the mirror once its reflections have gone all the way round.
::Color mirrored(const SceneDesc& desc, const std::string& name) {
  RenderStage stage;
  stage.upload(desc);
  const Scene scene = Scene::from_desc(desc);
  for (int frame = 1; frame < kFullTurn; ++frame) stage.draw(scene, name);
  return stage.draw(scene, name).at(kMiddle, kMiddle);
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

TEST_CASE("a see-through card shows in a mirror's reflections") {
  const ::Color without = mirrored(mirror_room(false), "see_through_mirror_empty");
  const ::Color with = mirrored(mirror_room(true), "see_through_mirror_card");
  CHECK_FALSE(greener_than_blue(without));  // the sky, as a mirror shows it
  CHECK(greener_than_blue(with));
}

TEST_CASE("scattered see-through cards are mixed among the objects' the farthest first") {
  SceneDesc desc = two_cards(card_material({0.0f, 1.0f, 0.0f, kHalfSeen}, AlphaMode::Blend),
                             card_material({1.0f, 0.0f, 0.0f, kHalfSeen}, AlphaMode::Blend));
  desc.nodes.erase(desc.nodes.begin() + kNearCardNode);  // the near green card is scattered instead
  ScatterSet near_cards;
  near_cards.mesh = 0;
  near_cards.places = {card("Near", 0, kNear).local};
  RenderStage stage;
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  scene.scattered().add(near_cards);
  const ::Color seen = stage.draw(scene, "see_through_scattered_sorted").at(kMiddle, kMiddle);
  CHECK(greener_than_red(seen));  // the near green copy was mixed over the far red object
  CHECK(seen.r > 0);
}
