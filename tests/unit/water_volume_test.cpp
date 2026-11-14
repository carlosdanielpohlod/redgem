#include <doctest/doctest.h>

#include <cmath>

#include "app/scene_water.hpp"
#include "render/underwater.hpp"
#include "scene/water_volume.hpp"

using namespace redgem;

namespace {

constexpr float kPondHeight = 1.0f;
constexpr float kPoolHeight = 3.0f;
constexpr float kWaves = 0.2f;
constexpr Vec3 kRedAbsorption{0.9f, 0.1f, 0.05f};
constexpr int kCrossingSamples = 200;
constexpr float kCrossingReach = 0.5f;  // meters above and below the surface the camera passes
constexpr float kLargestStep = 0.05f;   // of the look, between samples a few millimeters apart

PrimitiveDesc quad(float left, float right, float height, int material) {
  PrimitiveDesc primitive;
  primitive.material = material;
  primitive.positions = {left, height, -5.0f, right, height, -5.0f, right, height, 5.0f, left, height, 5.0f};
  primitive.normals = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  primitive.indices = {0, 1, 2, 0, 2, 3};
  return primitive;
}

// A pond from x -5 to 5 at height 1, a raised pool over its right half at height 3, and a dry slab.
SceneDesc ponds() {
  SceneDesc desc;
  MaterialDesc water;
  water.water.enabled = true;
  water.water.wave_height = kWaves;
  water.water.absorption = kRedAbsorption;
  desc.materials = {water, MaterialDesc{}};
  desc.meshes = {MeshDesc{"Pond", {quad(-5.0f, 5.0f, kPondHeight, 0)}, {}},
                 MeshDesc{"Pool", {quad(0.0f, 5.0f, kPoolHeight, 0)}, {}},
                 MeshDesc{"Slab", {quad(10.0f, 20.0f, 0.0f, 1)}, {}}};
  for (int mesh = 0; mesh < 3; ++mesh) {
    NodeDesc node;
    node.name = desc.meshes[static_cast<std::size_t>(mesh)].name;
    node.mesh = mesh;
    desc.nodes.push_back(node);
  }
  return desc;
}

struct Ponds {
  SceneDesc desc = ponds();
  Scene scene = Scene::from_desc(desc);
  std::vector<WaterSurface> surfaces = water_surfaces(scene, desc);
};

}  // namespace

TEST_CASE("the water over a column is its highest surface, and nothing outside every footprint") {
  Ponds ponds;
  REQUIRE(ponds.surfaces.size() == 2);  // the slab's material is not water
  const auto pond = water_column_at(ponds.surfaces, -2.0f, 0.0f);
  REQUIRE(pond);
  CHECK(pond->height == doctest::Approx(kPondHeight));
  const auto pool = water_column_at(ponds.surfaces, 2.0f, 0.0f);
  REQUIRE(pool);
  CHECK(pool->height == doctest::Approx(kPoolHeight));
  CHECK(pool->surface->water.wave_height == doctest::Approx(kWaves));
  CHECK_FALSE(water_column_at(ponds.surfaces, 15.0f, 0.0f));
  CHECK_FALSE(water_column_at(ponds.surfaces, -2.0f, 8.0f));
}

TEST_CASE("a point is under water below its column's surface, and its depth says how far") {
  Ponds ponds;
  CHECK(water_under(ponds.surfaces, {-2.0f, 0.0f, 0.0f}));
  CHECK_FALSE(water_under(ponds.surfaces, {-2.0f, 2.0f, 0.0f}));
  CHECK(water_under(ponds.surfaces, {2.0f, 2.0f, 0.0f}));  // in the raised pool
  CHECK_FALSE(water_under(ponds.surfaces, {15.0f, -10.0f, 0.0f}));
  CHECK(*water_depth(ponds.surfaces, {-2.0f, -1.5f, 0.0f}) == doctest::Approx(2.5f));
  CHECK(*water_depth(ponds.surfaces, {-2.0f, 1.5f, 0.0f}) == doctest::Approx(-0.5f));
  CHECK_FALSE(water_depth(ponds.surfaces, {NAN, 0.0f, 0.0f}));
}

TEST_CASE("the camera's view of the water carries how deep it is and the surface's look") {
  Ponds ponds;
  const UnderwaterView under = underwater_view(ponds.surfaces, {-2.0f, 0.0f, 0.0f});
  CHECK(under.water);
  CHECK(under.depth == doctest::Approx(kPondHeight));
  CHECK(under.band == doctest::Approx(kWaves));
  CHECK(under.absorption.x == doctest::Approx(kRedAbsorption.x));
  CHECK(underwater_amount(under) == 1.0f);
  CHECK(underwater_amount(underwater_view(ponds.surfaces, {-2.0f, 5.0f, 0.0f})) == 0.0f);
  CHECK_FALSE(underwater_view(ponds.surfaces, {15.0f, 0.0f, 0.0f}).water);
  CHECK(underwater_amount(UnderwaterView{}) == 0.0f);
}

TEST_CASE("crossing the surface fades the underwater look in steadily, with no jump to flicker") {
  UnderwaterView view;
  view.water = true;
  view.band = underwater_band(kWaves);
  float before = 0.0f;
  for (int i = 0; i <= kCrossingSamples; ++i) {
    view.depth = -kCrossingReach + kTwo * kCrossingReach * static_cast<float>(i) / kCrossingSamples;
    const float amount = underwater_amount(view);
    CHECK(amount >= before);
    CHECK(amount - before <= kLargestStep);
    before = amount;
  }
  CHECK(before == 1.0f);
  view.depth = 0.0f;
  CHECK(underwater_amount(view) == doctest::Approx(kHalf));
  CHECK(underwater_band(0.0f) == kUnderwaterMinBand);
  CHECK(underwater_band(NAN) == kUnderwaterMinBand);
}
