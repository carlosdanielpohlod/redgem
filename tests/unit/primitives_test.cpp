#include <doctest/doctest.h>

#include "core/config.hpp"
#include "scene/primitives.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr float kTolerance = 1e-3f;

Bounds bounds_of(const PrimitiveDesc& p) {
  Bounds bounds;
  for (std::size_t i = 0; i < p.positions.size(); i += config::kFloatsPerPosition) {
    bounds.include({p.positions[i], p.positions[i + 1], p.positions[i + 2]});
  }
  return bounds;
}

Vec3 at(const std::vector<float>& data, std::uint32_t index) {
  return {data[index * 3], data[index * 3 + 1], data[index * 3 + 2]};
}

}  // namespace

TEST_CASE("every primitive is a valid closed-or-flat triangle mesh with unit normals and uvs") {
  for (const PrimitiveShape shape : all_primitives()) {
    CAPTURE(primitive_name(shape));
    const PrimitiveDesc p = make_primitive(shape);
    const std::size_t vertices = p.positions.size() / config::kFloatsPerPosition;
    REQUIRE(vertices > 0);
    CHECK(p.normals.size() == p.positions.size());
    CHECK(p.texcoords.size() == vertices * config::kFloatsPerTexcoord);
    REQUIRE(p.indices.size() % config::kVerticesPerTriangle == 0);
    for (const std::uint32_t index : p.indices) REQUIRE(index < vertices);
    for (std::uint32_t v = 0; v < vertices; ++v) CHECK(length(at(p.normals, v)) == doctest::Approx(1.0f));
    // Triangles face their normals (counter-clockwise from outside), so back-face culling works.
    for (std::size_t i = 0; i < p.indices.size(); i += config::kVerticesPerTriangle) {
      const Vec3 a = at(p.positions, p.indices[i]);
      const Vec3 b = at(p.positions, p.indices[i + 1]);
      const Vec3 c = at(p.positions, p.indices[i + 2]);
      const Vec3 face = cross(b - a, c - a);
      if (length(face) < kTolerance * kTolerance) continue;  // degenerate pole triangles
      const Vec3 normal =
          at(p.normals, p.indices[i]) + at(p.normals, p.indices[i + 1]) + at(p.normals, p.indices[i + 2]);
      CHECK(dot(face, normal) > 0.0f);
    }
  }
}

TEST_CASE("primitives have the expected sizes") {
  constexpr Vec3 kUnit{0.5f, 0.5f, 0.5f};
  constexpr Vec3 kPlaneHalf{5.0f, 0.0f, 5.0f};
  constexpr Vec3 kCapsuleHalf{0.5f, 1.0f, 0.5f};
  CHECK(approx_equal(bounds_of(make_primitive(PrimitiveShape::Cube)).half_extents(), kUnit, kTolerance));
  CHECK(approx_equal(bounds_of(make_primitive(PrimitiveShape::Sphere)).half_extents(), kUnit, kTolerance));
  CHECK(approx_equal(bounds_of(make_primitive(PrimitiveShape::Cylinder)).half_extents(), kUnit, kTolerance));
  CHECK(approx_equal(bounds_of(make_primitive(PrimitiveShape::Plane)).half_extents(), kPlaneHalf, kTolerance));
  CHECK(approx_equal(bounds_of(make_primitive(PrimitiveShape::Capsule)).half_extents(), kCapsuleHalf, kTolerance));
}

TEST_CASE("primitive names and colliders") {
  for (const PrimitiveShape shape : all_primitives()) CHECK(primitive_from_name(primitive_name(shape)) == shape);
  CHECK_FALSE(primitive_from_name("Teapot").has_value());
  CHECK(std::string(primitive_collider(PrimitiveShape::Sphere)) == "sphere");
  CHECK(std::string(primitive_collider(PrimitiveShape::Capsule)) == "capsule");
  CHECK(std::string(primitive_collider(PrimitiveShape::Cube)) == "box");
}
