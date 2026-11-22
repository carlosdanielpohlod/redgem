#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <vector>

#include "scene/shapes/terrain.hpp"
#include "scene/triangle_index.hpp"

using namespace redgem;

namespace {

constexpr float kTerrainSize = 64.0f;
constexpr int kDenseCells = 128;  // 32 768 triangles
constexpr int kSparseCells = 8;   // 128 triangles: walked whole
constexpr std::size_t kCorners = 3;
constexpr std::size_t kFloats = 3;
constexpr float kReach = 0.5f;  // meters around a point a box goes, every way

Vec3 corner_at(const PrimitiveDesc& primitive, std::size_t listed) {
  return vec3_from(&primitive.positions[primitive.indices[listed] * kFloats]);
}

// The triangles whose boxes overlap `box`, found by walking them all.
std::vector<std::size_t> overlapping(const PrimitiveDesc& primitive, const Bounds& box) {
  std::vector<std::size_t> found;
  for (std::size_t first = 0; first + kCorners <= primitive.indices.size(); first += kCorners) {
    Bounds around;
    for (std::size_t corner = 0; corner < kCorners; ++corner) around.include(corner_at(primitive, first + corner));
    if (boxes_overlap(around, box)) found.push_back(first);
  }
  return found;
}

Bounds box_around(Vec3 point) {
  Bounds box;
  box.include(point - Vec3{kReach, kReach, kReach});
  box.include(point + Vec3{kReach, kReach, kReach});
  return box;
}

}  // namespace

TEST_CASE("a triangle index finds every triangle a box overlaps, and few others") {
  const PrimitiveDesc terrain = make_terrain({kTerrainSize, kDenseCells});
  const TriangleIndex index(terrain);
  CHECK(index.triangles() == terrain.indices.size() / kCorners);
  for (const Vec3 point : {Vec3{0, 0, 0}, Vec3{-31.9f, 0, 31.9f}, Vec3{12.3f, 0, -7.7f}}) {
    const std::vector<std::size_t> wanted = overlapping(terrain, box_around(point));
    std::vector<std::size_t> found;
    index.find(box_around(point), found);
    for (const std::size_t first : wanted) CHECK(std::find(found.begin(), found.end(), first) != found.end());
    CHECK(found.size() < wanted.size() * 4);  // the leaves around it, not the whole terrain
  }
  std::vector<std::size_t> found;
  index.find(box_around({100, 0, 0}), found);
  CHECK(found.empty());
}

TEST_CASE("a triangle index cache sorts a dense primitive once, and a small one never") {
  PrimitiveDesc dense = make_terrain({kTerrainSize, kDenseCells});
  const PrimitiveDesc sparse = make_terrain({kTerrainSize, kSparseCells});
  TriangleIndexCache cache;
  CHECK(cache.of(sparse) == nullptr);
  const TriangleIndex* first = cache.of(dense);
  REQUIRE(first != nullptr);
  CHECK(cache.of(dense) == first);
  dense = make_terrain({kTerrainSize, kDenseCells / 2});  // its lists replaced: sorted anew
  REQUIRE(cache.of(dense) != nullptr);
  CHECK(cache.of(dense)->triangles() == dense.indices.size() / kCorners);
}
