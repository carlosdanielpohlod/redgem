#include <doctest/doctest.h>

#include <cmath>

#include "scene/terrain.hpp"

using namespace redgem;

namespace {

constexpr TerrainGrid kGrid{40.0f, 20};  // 2 m cells
constexpr float kBrushRadius = 6.0f;
constexpr float kLift = 3.0f;
constexpr float kTolerance = 1e-3f;
constexpr float kHitTolerance = 0.05f;

PrimitiveDesc hill() {
  PrimitiveDesc mesh = make_terrain(kGrid);
  REQUIRE(sculpt_terrain(mesh, kGrid, {0.0f, 0.0f, kBrushRadius, kLift, SculptMode::Raise}));
  return mesh;
}

}  // namespace

TEST_CASE("a new terrain is a flat grid facing up") {
  const PrimitiveDesc mesh = make_terrain(kGrid);
  CHECK(kGrid.fits(mesh));
  CHECK(mesh.positions.size() == 21u * 21u * 3u);
  CHECK(mesh.indices.size() == 20u * 20u * 6u);
  CHECK(mesh.texcoords.size() == 21u * 21u * 2u);
  CHECK(terrain_height(mesh, kGrid, 3.3f, -7.1f) == doctest::Approx(0.0f));
  CHECK(mesh.normals[1] == doctest::Approx(1.0f));
  const Bounds bounds = mesh_bounds_of(mesh);
  CHECK(bounds.min.x == doctest::Approx(-20.0f));
  CHECK(bounds.max.z == doctest::Approx(20.0f));
  CHECK_FALSE(TerrainGrid{40.0f, 10}.fits(mesh));  // another shape of grid
}

TEST_CASE("raising lifts the ground most at the brush's center and not at all beyond its radius") {
  const PrimitiveDesc mesh = hill();
  CHECK(terrain_height(mesh, kGrid, 0.0f, 0.0f) == doctest::Approx(kLift));
  const float halfway = terrain_height(mesh, kGrid, 2.0f, 0.0f);
  CHECK(halfway > 0.0f);
  CHECK(halfway < kLift);
  CHECK(terrain_height(mesh, kGrid, 8.0f, 0.0f) == doctest::Approx(0.0f));
  CHECK(terrain_height(mesh, kGrid, 100.0f, 0.0f) == doctest::Approx(0.0f));  // off the terrain
  // The slope leans the normals away from the top.
  const std::size_t east_of_top = (10u * 21u + 11u) * 3u;
  CHECK(mesh.normals[east_of_top] > 0.0f);
  CHECK(mesh.normals[east_of_top + 1] < 1.0f);
}

TEST_CASE("lowering digs, flattening levels to the center's height, smoothing softens a peak") {
  PrimitiveDesc dug = make_terrain(kGrid);
  sculpt_terrain(dug, kGrid, {0.0f, 0.0f, kBrushRadius, kLift, SculptMode::Lower});
  CHECK(terrain_height(dug, kGrid, 0.0f, 0.0f) == doctest::Approx(-kLift));

  PrimitiveDesc level = hill();
  sculpt_terrain(level, kGrid, {0.0f, 0.0f, kBrushRadius, 1.0f, SculptMode::Flatten});
  CHECK(terrain_height(level, kGrid, 2.0f, 0.0f) > terrain_height(hill(), kGrid, 2.0f, 0.0f));  // pulled up toward the top

  PrimitiveDesc soft = hill();
  sculpt_terrain(soft, kGrid, {0.0f, 0.0f, kBrushRadius, 1.0f, SculptMode::Smooth});
  CHECK(terrain_height(soft, kGrid, 0.0f, 0.0f) < kLift);
  CHECK(terrain_height(soft, kGrid, 0.0f, 0.0f) > 0.0f);
}

TEST_CASE("a brush off the terrain or without a radius changes nothing") {
  PrimitiveDesc mesh = make_terrain(kGrid);
  CHECK_FALSE(sculpt_terrain(mesh, kGrid, {500.0f, 0.0f, kBrushRadius, kLift, SculptMode::Raise}));
  CHECK_FALSE(sculpt_terrain(mesh, kGrid, {0.0f, 0.0f, 0.0f, kLift, SculptMode::Raise}));
  CHECK(terrain_height(mesh, kGrid, 0.0f, 0.0f) == doctest::Approx(0.0f));
}

TEST_CASE("a ray finds where it meets the ground") {
  const PrimitiveDesc mesh = hill();
  // Straight down on the hill's top.
  const auto top = terrain_raycast(mesh, kGrid, {0.0f, 20.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 100.0f);
  REQUIRE(top.has_value());
  CHECK(top->y == doctest::Approx(kLift).epsilon(kHitTolerance));
  // At an angle onto flat ground.
  const Vec3 slanted = normalize(Vec3{1.0f, -1.0f, 0.0f});
  const auto flat = terrain_raycast(mesh, kGrid, {5.0f, 5.0f, 12.0f}, slanted, 100.0f);
  REQUIRE(flat.has_value());
  CHECK(flat->x == doctest::Approx(10.0f).epsilon(kHitTolerance));
  CHECK(std::fabs(flat->y) < kTolerance);
  // Up into the sky, and from under the ground: nothing.
  CHECK_FALSE(terrain_raycast(mesh, kGrid, {0.0f, 20.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 100.0f).has_value());
  CHECK_FALSE(terrain_raycast(mesh, kGrid, {0.0f, -5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 100.0f).has_value());
}
