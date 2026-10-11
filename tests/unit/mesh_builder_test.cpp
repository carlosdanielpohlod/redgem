#include <doctest/doctest.h>

#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "render/mesh_builder.hpp"

using namespace redgem;

namespace {

constexpr std::size_t kTriangleVertices = 3;
constexpr std::uint8_t kRed[] = {200, 10, 10, 255};  // one vertex's color
constexpr std::uint8_t kSecondJoint = 1;
constexpr float kFullWeight = 1.0f;
constexpr std::size_t kTooManyVertices = config::kMaxVerticesPerMesh + 1;
constexpr std::size_t kSplitQuads = 40;  // a strip long enough to need several pieces
constexpr std::size_t kSplitMaxVertices = 12;
constexpr std::size_t kColumnVertices = 2;  // a strip has two vertices per column
constexpr std::uint8_t kColorStep = 3;      // each vertex's color tells which vertex it is

PrimitiveDesc triangle() {
  PrimitiveDesc primitive;
  primitive.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  primitive.normals = {0, 0, 1, 0, 0, 1, 0, 0, 1};
  primitive.indices = {0, 1, 2};
  return primitive;
}

// The triangle with a color, joints and weights on every vertex.
PrimitiveDesc skinned_triangle() {
  PrimitiveDesc primitive = triangle();
  for (std::size_t vertex = 0; vertex < kTriangleVertices; ++vertex) {
    primitive.colors.insert(primitive.colors.end(), std::begin(kRed), std::end(kRed));
    primitive.joints.insert(primitive.joints.end(), {kSecondJoint, 0, 0, 0});
    primitive.weights.insert(primitive.weights.end(), {kFullWeight, 0.0f, 0.0f, 0.0f});
  }
  return primitive;
}

// A strip of quads whose every vertex has a place, color and joint that tell which one it is.
PrimitiveDesc numbered_strip() {
  PrimitiveDesc strip;
  const std::size_t vertices = (kSplitQuads + 1) * kColumnVertices;
  for (std::size_t vertex = 0; vertex < vertices; ++vertex) {
    const auto mark = static_cast<std::uint8_t>(vertex * kColorStep);
    strip.positions.insert(strip.positions.end(), {static_cast<float>(vertex), 0.0f, 0.0f});
    strip.normals.insert(strip.normals.end(), {0.0f, 0.0f, 1.0f});
    strip.colors.insert(strip.colors.end(), {mark, mark, mark, mark});
    strip.joints.insert(strip.joints.end(), {mark, 0, 0, 0});
    strip.weights.insert(strip.weights.end(), {static_cast<float>(vertex), 0.0f, 0.0f, 0.0f});
  }
  for (std::uint32_t quad = 0; quad < kSplitQuads; ++quad) {
    // The quad's corners: a and b in one column, c and d in the next.
    const auto a = static_cast<std::uint32_t>(quad * kColumnVertices);
    const std::uint32_t b = a + 1, c = a + kColumnVertices, d = c + 1;
    strip.indices.insert(strip.indices.end(), {a, c, b, b, c, d});
  }
  return strip;
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

TEST_CASE("a sound primitive has no problem; a broken one says what is wrong with it") {
  CHECK(primitive_problem(triangle()).empty());
  CHECK(primitive_problem(PrimitiveDesc{}).find("no vertices") != std::string::npos);
  PrimitiveDesc half_a_triangle = triangle();
  half_a_triangle.indices = {0, 1};
  CHECK(primitive_problem(half_a_triangle).find("indices") != std::string::npos);
}

TEST_CASE("a primitive with more vertices than a mesh can index is rejected") {
  PrimitiveDesc huge;
  huge.positions.assign(kTooManyVertices * config::kFloatsPerPosition, 0.0f);
  std::string error;
  CHECK(primitive_problem(huge).find("limit") != std::string::npos);
  CHECK_FALSE(build_mesh(huge, error).has_value());
  CHECK(error == primitive_problem(huge));
}

TEST_CASE("vertex colors and skinning are carried into the mesh") {
  std::string error;
  std::optional<Mesh> mesh = build_mesh(skinned_triangle(), error);
  REQUIRE(mesh.has_value());
  CHECK(error.empty());
  CHECK(mesh->colors[0] == kRed[0]);
  CHECK(mesh->colors[1] == kRed[1]);
  REQUIRE(mesh->boneIds != nullptr);
  REQUIRE(mesh->boneWeights != nullptr);
  CHECK(mesh->boneIds[0] == kSecondJoint);
  CHECK(mesh->boneWeights[0] == doctest::Approx(kFullWeight));
  free_cpu_mesh(*mesh);
}

TEST_CASE("a mesh without skinning, or with skinning of the wrong length, is rigid") {
  std::string error;
  std::optional<Mesh> rigid = build_mesh(triangle(), error);
  REQUIRE(rigid.has_value());
  CHECK(rigid->boneIds == nullptr);
  CHECK(rigid->boneWeights == nullptr);
  free_cpu_mesh(*rigid);

  PrimitiveDesc broken = skinned_triangle();
  broken.weights.pop_back();
  std::optional<Mesh> ignored = build_mesh(broken, error);
  REQUIRE(ignored.has_value());
  CHECK(ignored->boneIds == nullptr);
  free_cpu_mesh(*ignored);
}

TEST_CASE("the pieces of a split primitive keep every vertex's own color and skinning") {
  const PrimitiveDesc strip = numbered_strip();
  const std::vector<PrimitiveDesc> pieces = split_primitive(strip, kSplitMaxVertices);
  REQUIRE(pieces.size() > 1);
  for (const PrimitiveDesc& piece : pieces) {
    const std::size_t vertices = piece.positions.size() / config::kFloatsPerPosition;
    REQUIRE(piece.colors.size() == vertices * config::kChannelsPerColor);
    REQUIRE(piece.joints.size() == vertices * config::kJointsPerVertex);
    REQUIRE(piece.weights.size() == vertices * config::kJointsPerVertex);
    for (std::size_t vertex = 0; vertex < vertices; ++vertex) {
      // The x of each vertex says which vertex of the strip it was.
      const auto original = static_cast<std::size_t>(piece.positions[vertex * config::kFloatsPerPosition]);
      const auto mark = static_cast<std::uint8_t>(original * kColorStep);
      CHECK(piece.colors[vertex * config::kChannelsPerColor] == mark);
      CHECK(piece.joints[vertex * config::kJointsPerVertex] == mark);
      CHECK(piece.weights[vertex * config::kJointsPerVertex] == doctest::Approx(static_cast<float>(original)));
    }
  }
}

TEST_CASE("the triangles of a split primitive still join the same points, in the same order") {
  const PrimitiveDesc strip = numbered_strip();
  std::vector<std::uint32_t> rebuilt;
  for (const PrimitiveDesc& piece : split_primitive(strip, kSplitMaxVertices)) {
    for (const std::uint32_t index : piece.indices) {
      rebuilt.push_back(static_cast<std::uint32_t>(piece.positions[index * config::kFloatsPerPosition]));
    }
  }
  CHECK(rebuilt == strip.indices);
}

TEST_CASE("a split primitive without optional data gets none made up") {
  PrimitiveDesc strip = numbered_strip();
  strip.colors.clear();
  strip.joints.clear();
  strip.weights.clear();
  for (const PrimitiveDesc& piece : split_primitive(strip, kSplitMaxVertices)) {
    CHECK(piece.colors.empty());
    CHECK(piece.texcoords.empty());
    CHECK(piece.joints.empty());
    CHECK(piece.weights.empty());
  }
}
