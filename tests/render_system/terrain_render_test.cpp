// System test: terrains drawn in a real window. A terrain without ground layers is drawn as it
// always was (a picture kept from before the layers were added).

#include <doctest/doctest.h>

#include <cstdlib>
#include <string>

#include "render_system/render_stage.hpp"
#include "scene/props.hpp"
#include "scene/shapes/terrain.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr TerrainGrid kGrid{24.0f, 48};
constexpr Vec3 kCamera{0.0f, 11.0f, 13.0f};
constexpr redgem::Color kGrass{0.33f, 0.46f, 0.25f, 1.0f};
constexpr redgem::Color kSand{0.85f, 0.75f, 0.55f, 1.0f};
constexpr float kGroundRoughness = 0.9f;
constexpr TerrainBrush kHill{-4.0f, -2.0f, 7.0f, 3.0f, SculptMode::Raise};
constexpr TerrainBrush kBeach{5.0f, 3.0f, 6.0f, 1.0f, SculptMode::Raise};
constexpr const char* kBaseline = "images/terrain_tinted_baseline.png";
// A picture drawn again on the same card differs from its baseline by rounding at most: on
// average a fraction of a level, and no pixel by more than a few.
constexpr double kMostMeanDifference = 0.5;
constexpr int kMostPixelDifference = 6;

MaterialDesc ground_material() {
  MaterialDesc material;
  material.name = "Ground";
  material.base_color = kGrass;
  material.roughness = kGroundRoughness;
  return material;
}

// A sculpted terrain, one hill, with a sandy patch painted into its vertex colors.
MeshDesc tinted_ground() {
  PrimitiveDesc ground = make_terrain(kGrid);
  sculpt_terrain(ground, kGrid, kHill);
  paint_terrain(ground, kGrid, kBeach, kSand);
  ground.material = 0;
  MeshDesc mesh{"Terrain", {ground}, mesh_bounds_of(ground), {}, {}};
  return mesh;
}

NodeDesc terrain_node() {
  NodeDesc node;
  node.name = "Terrain";
  node.mesh = 0;
  node.props[props::kTerrain] = true;
  node.props[props::kTerrainSize] = static_cast<double>(kGrid.size);
  node.props[props::kTerrainCells] = static_cast<double>(kGrid.cells);
  return node;
}

SceneDesc tinted_terrain() {
  SceneDesc desc;
  desc.materials = {ground_material()};
  desc.meshes = {tinted_ground()};
  desc.nodes = {test::camera_at(kCamera), terrain_node()};
  return desc;
}

struct Difference {
  double mean = 0.0;
  int most = 0;
};

Difference difference(const Picture& drawn, const Image& baseline) {
  const auto* expected = static_cast<const ::Color*>(baseline.data);
  Difference found;
  double total = 0.0;
  for (std::size_t i = 0; i < drawn.pixels.size(); ++i) {
    for (const int gap :
         {drawn.pixels[i].r - expected[i].r, drawn.pixels[i].g - expected[i].g, drawn.pixels[i].b - expected[i].b}) {
      total += std::abs(gap);
      found.most = std::max(found.most, std::abs(gap));
    }
  }
  found.mean = total / static_cast<double>(drawn.pixels.size() * 3);
  return found;
}

}  // namespace

TEST_CASE("a terrain without ground layers is drawn as it was before layers existed") {
  RenderStage stage;
  const SceneDesc desc = tinted_terrain();
  stage.upload(desc);
  const Picture drawn = stage.draw(Scene::from_desc(desc), "terrain_tinted");
  Image baseline = LoadImage(test::fixture(kBaseline).c_str());
  REQUIRE(IsImageValid(baseline));
  ImageFormat(&baseline, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  REQUIRE(baseline.width == drawn.width);
  REQUIRE(baseline.height == drawn.height);
  const Difference found = difference(drawn, baseline);
  UnloadImage(baseline);
  CHECK(found.mean <= kMostMeanDifference);
  CHECK(found.most <= kMostPixelDifference);
}
