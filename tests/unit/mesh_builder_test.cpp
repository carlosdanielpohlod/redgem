#include <doctest/doctest.h>

#include "core/config.hpp"
#include "render/mesh_builder.hpp"

using namespace redgem;

namespace {

PrimitiveDesc triangle() {
  PrimitiveDesc primitive;
  primitive.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  primitive.normals = {0, 0, 1, 0, 0, 1, 0, 0, 1};
  primitive.indices = {0, 1, 2};
  return primitive;
}

}  // namespace

TEST_CASE("builds a raylib mesh with default uvs and white vertex colors") {
  std::string error;
  std::optional<Mesh> mesh = build_mesh(triangle(), error);
  REQUIRE(mesh.has_value());
  CHECK(mesh->vertexCount == 3);
  CHECK(mesh->triangleCount == 1);
  CHECK(mesh->vertices[3] == doctest::Approx(1.0f));
  CHECK(mesh->texcoords[0] == doctest::Approx(0.0f));
  CHECK(mesh->colors[0] == 255);
  CHECK(mesh->indices[2] == 2);
  free_cpu_mesh(*mesh);
}

TEST_CASE("rejects broken primitives") {
  std::string error;
  PrimitiveDesc out_of_range = triangle();
  out_of_range.indices = {0, 1, 9};
  CHECK_FALSE(build_mesh(out_of_range, error).has_value());
  CHECK(error.find("indices") != std::string::npos);

  CHECK_FALSE(build_mesh(PrimitiveDesc{}, error).has_value());
  CHECK(error.find("no vertices") != std::string::npos);
}

TEST_CASE("large primitives split into pieces that fit 16-bit indices") {
  // A strip of quads: every triangle shares vertices with its neighbours.
  constexpr std::size_t kQuads = 100;
  constexpr std::size_t kMaxVertices = 30;
  constexpr std::size_t kVerticesPerColumn = 2;
  constexpr int kAnyMaterial = 7;
  constexpr std::uint32_t kVerticesPerQuadStep = 2;
  PrimitiveDesc strip;
  for (std::size_t q = 0; q <= kQuads; ++q) {
    for (std::size_t row = 0; row < kVerticesPerColumn; ++row) {
      strip.positions.insert(strip.positions.end(), {static_cast<float>(q), static_cast<float>(row), 0.0f});
      strip.normals.insert(strip.normals.end(), {0.0f, 0.0f, 1.0f});
      strip.texcoords.insert(strip.texcoords.end(), {static_cast<float>(q), static_cast<float>(row)});
    }
  }
  for (std::uint32_t q = 0; q < kQuads; ++q) {
    const std::uint32_t a = q * kVerticesPerQuadStep, b = a + 1, c = a + 2, d = a + 3;
    strip.indices.insert(strip.indices.end(), {a, c, b, b, c, d});
  }
  strip.material = kAnyMaterial;

  const std::vector<PrimitiveDesc> pieces = split_primitive(strip, kMaxVertices);
  REQUIRE(pieces.size() > 1);
  std::size_t triangles = 0;
  for (const PrimitiveDesc& piece : pieces) {
    const std::size_t vertices = piece.positions.size() / config::kFloatsPerPosition;
    CHECK(vertices <= kMaxVertices);
    CHECK(piece.normals.size() == piece.positions.size());
    CHECK(piece.texcoords.size() == vertices * config::kFloatsPerTexcoord);
    CHECK(piece.material == strip.material);
    for (const std::uint32_t index : piece.indices) CHECK(index < vertices);
    triangles += piece.indices.size() / config::kVerticesPerTriangle;
  }
  CHECK(triangles == strip.indices.size() / config::kVerticesPerTriangle);  // nothing lost
  CHECK(split_primitive(triangle(), kMaxVertices).size() == 1);
}
