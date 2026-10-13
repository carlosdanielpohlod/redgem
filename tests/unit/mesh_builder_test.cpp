#include <doctest/doctest.h>

#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "render/render_config.hpp"
#include "scene/scene_config.hpp"
#include "render/mesh_builder.hpp"

using namespace redgem;

namespace {

constexpr std::size_t kTriangleVertices = 3;
constexpr std::uint8_t kRed[] = {200, 10, 10, 255};  // one vertex's color
constexpr std::uint8_t kSecondJoint = 1;
constexpr float kFullWeight = 1.0f;
constexpr std::size_t kTooManyVertices = config::kMaxVerticesPerMesh + 1;

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
