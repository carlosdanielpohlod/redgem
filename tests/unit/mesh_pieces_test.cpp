#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "render/mesh_builder.hpp"
#include "render/mesh_pieces.hpp"
#include "scene/scene_config.hpp"

using namespace redgem;

namespace {

constexpr std::size_t kTriangleVertices = 3;
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

TEST_CASE("a primitive has as many vertices as its positions make") {
  CHECK(vertex_count_of(triangle()) == kTriangleVertices);
  CHECK(vertex_count_of(PrimitiveDesc{}) == 0);
}

TEST_CASE("optional vertex data counts only when every vertex has it") {
  const VertexData bare = vertex_data_of(triangle());
  CHECK_FALSE(bare.texcoords);
  CHECK_FALSE(bare.colors);
  CHECK_FALSE(bare.skin);

  const PrimitiveDesc strip = numbered_strip();
  const VertexData marked = vertex_data_of(strip);
  CHECK_FALSE(marked.texcoords);
  CHECK(marked.colors);
  CHECK(marked.skin);

  PrimitiveDesc short_weights = strip;
  short_weights.weights.pop_back();
  CHECK_FALSE(vertex_data_of(short_weights).skin);
  PrimitiveDesc short_colors = strip;
  short_colors.colors.pop_back();
  CHECK_FALSE(vertex_data_of(short_colors).colors);
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

TEST_CASE("a primitive whose indices cannot be trusted is not cut into pieces") {
  constexpr std::size_t kSmallPieces = 2;  // fewer vertices than the triangle has: it would be split
  PrimitiveDesc out_of_range = triangle();
  out_of_range.indices = {0, 1, 4000000};
  const std::vector<PrimitiveDesc> kept = split_primitive(out_of_range, kSmallPieces);
  REQUIRE(kept.size() == 1);
  CHECK(kept[0].indices == out_of_range.indices);
  CHECK(kept[0].positions == out_of_range.positions);
  std::string error;
  CHECK_FALSE(build_mesh(kept[0], error).has_value());

  // Vertices that do not all have a normal are no better: every piece copies one per vertex.
  PrimitiveDesc short_normals = triangle();
  short_normals.normals.resize(3);
  CHECK(primitive_shape_problem(short_normals).find("normal") != std::string::npos);
  CHECK(split_primitive(short_normals, kSmallPieces).size() == 1);
  CHECK_FALSE(build_mesh(short_normals, error).has_value());
  CHECK(primitive_shape_problem(triangle()).empty());
}

TEST_CASE("a mesh's pieces line up with themselves after its vertices move, also where a primitive is split") {
  constexpr float kLift = 5.0f;
  MeshDesc mesh;
  mesh.primitives = {triangle(), numbered_strip(), triangle()};
  const std::vector<PrimitiveDesc> pieces = mesh_pieces(mesh, kSplitMaxVertices);
  const std::size_t strip_pieces = split_primitive(numbered_strip(), kSplitMaxVertices).size();
  REQUIRE(strip_pieces > 1);
  REQUIRE(pieces.size() == strip_pieces + 2);
  // In order: the first primitive whole, the pieces of the second, the third whole.
  CHECK(pieces.front().positions == triangle().positions);
  CHECK(pieces.back().positions == triangle().positions);
  CHECK(pieces[1].positions.size() < numbered_strip().positions.size());

  // The mesh reshaped (every vertex lifted): the same pieces, each with as many vertices as
  // before and every one of them lifted. A piece is never the primitive of the same number.
  MeshDesc lifted = mesh;
  for (PrimitiveDesc& primitive : lifted.primitives) {
    for (std::size_t y = 1; y < primitive.positions.size(); y += config::kFloatsPerPosition) {
      primitive.positions[y] += kLift;
    }
  }
  const std::vector<PrimitiveDesc> moved = mesh_pieces(lifted, kSplitMaxVertices);
  REQUIRE(moved.size() == pieces.size());
  for (std::size_t i = 0; i < pieces.size(); ++i) {
    CAPTURE(i);
    REQUIRE(moved[i].positions.size() == pieces[i].positions.size());
    CHECK(moved[i].indices == pieces[i].indices);
    for (std::size_t y = 1; y < moved[i].positions.size(); y += config::kFloatsPerPosition) {
      CHECK(moved[i].positions[y] == doctest::Approx(pieces[i].positions[y] + kLift));
    }
  }
  CHECK(moved[2].positions.size() != lifted.primitives[2].positions.size());
}
