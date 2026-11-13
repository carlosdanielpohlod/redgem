// System test: shape keys (glTF morph targets) drawn by the vertex shader, in a real window. The
// square of tests/fixtures/morph.glb grows up by half its height with Smile whole and to the
// right by half its width with Wide whole; a copy of the mesh shows its own weights.
#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

#include "render_system/render_stage.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr Vec3 kCamera{0.0f, 0.0f, 3.0f};
constexpr redgem::Color kGreen{0.1f, 1.0f, 0.1f, 1.0f};
constexpr float kGlow = 2.0f;
constexpr int kGreener = 30;        // a pixel of the square is this much greener than red or blue, at least
constexpr std::size_t kSeen = 100;  // pixels of the square, at least
constexpr float kGrown = 1.5f;      // either key whole makes the square this many times as big
constexpr float kNearly = 0.1f;     // share either way a count may be off by
constexpr float kMiddle = 0.5f;     // of the picture across
constexpr float kAside = 1.5f;      // meters between the two copies of the last test
constexpr std::size_t kFace = 1;    // the square's node, after the camera
constexpr std::size_t kCopy = 2;

// The square's green, paled by the sky's light around it, and not the sky's own grey-blue.
bool green(::Color c) { return c.g > c.r + kGreener && c.g > c.b + kGreener; }

bool about(float value, float expected) { return std::fabs(value - expected) <= expected * kNearly; }

// The fixture's square, glowing green, in front of the camera; `copies` of it side by side.
SceneDesc glowing_faces(std::size_t copies) {
  const LoadResult loaded = load_gltf_scene(redgem::test::fixture("morph.glb"));
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  SceneDesc desc;
  desc.meshes = loaded.scene.meshes;
  desc.meshes.front().primitives.front().material = 0;
  desc.materials = {test::glowing_material(kGreen, kGlow)};
  desc.nodes = {test::camera_at(kCamera)};
  for (std::size_t i = 0; i < copies; ++i) {
    NodeDesc face;
    face.name = "Face" + std::to_string(i);
    face.mesh = 0;
    face.local.position.x = (static_cast<float>(i) - static_cast<float>(copies - 1) * kHalf) * kAside;
    desc.nodes.push_back(face);
  }
  return desc;
}

Picture draw_weighted(RenderStage& stage, const SceneDesc& desc, const std::vector<std::vector<float>>& weights,
                      const std::string& name) {
  Scene scene = Scene::from_desc(desc);
  for (std::size_t i = 0; i < weights.size(); ++i)
    scene.at(static_cast<ObjectId>(kFace + i)).morph_weights = weights[i];
  return stage.draw(scene, name);
}

}  // namespace

TEST_CASE("a mesh's shape keys move its vertices by their weights") {
  RenderStage stage;
  const SceneDesc desc = glowing_faces(1);
  stage.upload(desc);
  const Picture::Found plain = draw_weighted(stage, desc, {{0.0f, 0.0f}}, "morph_plain").find(green);
  const Picture::Found smile = draw_weighted(stage, desc, {{1.0f, 0.0f}}, "morph_smile").find(green);
  const Picture::Found wide = draw_weighted(stage, desc, {{0.0f, 1.0f}}, "morph_wide").find(green);
  const Picture::Found half = draw_weighted(stage, desc, {{0.5f, 0.0f}}, "morph_half_smile").find(green);
  REQUIRE(plain.count > kSeen);
  CHECK(about(static_cast<float>(smile.count) / static_cast<float>(plain.count), kGrown));
  CHECK(about(static_cast<float>(wide.count) / static_cast<float>(plain.count), kGrown));
  CHECK(about(static_cast<float>(half.count) / static_cast<float>(plain.count), (1.0f + kGrown) * kHalf));
  CHECK(std::fabs(smile.across - plain.across) < kNearly * kHalf);  // grown up, not sideways
  CHECK(wide.across > plain.across);                                // grown to the right
}

TEST_CASE("copies of a mesh drawn in one call each show their own shape keys") {
  RenderStage stage;
  const SceneDesc desc = glowing_faces(kCopy);
  stage.upload(desc);
  const Picture plain = draw_weighted(stage, desc, {{0.0f, 0.0f}, {0.0f, 0.0f}}, "morph_copies_plain");
  const Picture one_smiles = draw_weighted(stage, desc, {{0.0f, 0.0f}, {1.0f, 0.0f}}, "morph_copies_one_smiles");
  const Picture::Found before = plain.find(green);
  const Picture::Found after = one_smiles.find(green);
  REQUIRE(before.count > kSeen);
  CHECK(before.across == doctest::Approx(kMiddle).epsilon(kNearly));
  // Only the copy on the right grew: the green moved right, and there is half a square more of it.
  CHECK(after.across > before.across);
  CHECK(about(static_cast<float>(after.count) / static_cast<float>(before.count), (1.0f + kGrown) * kHalf));
}
