#include <doctest/doctest.h>

#include <vector>

#include "render/instance_batches.hpp"
#include "render/instance_records.hpp"

#include <raymath.h>  // after raylib.h (through instance_records.hpp): it relies on raylib's own types

using namespace redgem;
using namespace redgem::render_math;

namespace {

constexpr std::size_t kThirdInstance = 2;
constexpr float kMoveX = 4.0f;
constexpr float kMoveY = 1.0f;
constexpr float kMoveZ = -3.0f;
constexpr float kStretch = 2.0f;  // along x only: normals must not stretch with it
constexpr Vec3 kReddish{1.0f, 0.4f, 0.2f};
constexpr float kFirstBone = 640.0f;
constexpr int kTranslationColumn = 3;
constexpr int kMeshes = 4;
constexpr int kTree = 1;
constexpr int kGrass = 3;
constexpr std::size_t kGrassRunStart = 100;
constexpr std::size_t kGrassRunLength = 3;
constexpr std::size_t kNoCopies = 0;

const float* texel(const TexelRows& rows, std::size_t index) {
  const TexelPlace place = texel_place(index);
  return rows.row(place.y) + static_cast<std::size_t>(place.x) * kFloatsPerTexel;
}

bool same(const std::vector<float>& numbers, const std::vector<float>& expected) { return numbers == expected; }

}  // namespace

TEST_CASE("an instance's record is its model matrix, its normal matrix, then its tint and first bone") {
  TexelRows rows;
  const Matrix model = MatrixMultiply(MatrixScale(kStretch, 1.0f, 1.0f), MatrixTranslate(kMoveX, kMoveY, kMoveZ));
  REQUIRE(write_instance(rows, kThirdInstance, model, {kReddish, kFirstBone}));
  const std::size_t first = record_texel(kThirdInstance);
  CHECK(first == kThirdInstance * kTexelsPerInstance);
  const float* translation = texel(rows, first + kTranslationColumn);
  CHECK(translation[0] == kMoveX);
  CHECK(translation[1] == kMoveY);
  CHECK(translation[2] == kMoveZ);
  // The normal matrix of a stretch along x shrinks x instead: (1/2, 1, 1) down its diagonal.
  const Matrix normals = MatrixTranspose(MatrixInvert(model));
  CHECK(texel(rows, first + kNormalTexel)[0] == doctest::Approx(normals.m0));
  CHECK(texel(rows, first + kNormalTexel)[0] == doctest::Approx(1.0f / kStretch));
  CHECK(texel(rows, first + kNormalTexel + 1)[1] == doctest::Approx(1.0f));
  const float* look = texel(rows, first + kLookTexel);
  CHECK(look[0] == kReddish.x);
  CHECK(look[1] == kReddish.y);
  CHECK(look[2] == kReddish.z);
  CHECK(look[3] == kFirstBone);
}

TEST_CASE("a copy without a skeleton says so where its first bone would be") {
  TexelRows rows;
  REQUIRE(write_instance(rows, 0, MatrixIdentity(), InstanceLook{}));
  CHECK(texel(rows, kLookTexel)[3] == kNoBones);
  CHECK(texel(rows, kLookTexel)[0] == 1.0f);  // untinted: white
}

TEST_CASE("copies are batched by mesh, each mesh in the order its first copy came in") {
  InstanceBatches batches;
  batches.start(kMeshes);
  batches.add(kTree, 0);
  batches.add(kGrass, 1);
  batches.add(kTree, 2);
  batches.add_run(kGrass, kGrassRunStart, kGrassRunLength);
  batches.lay_out();
  REQUIRE(batches.batches().size() == 2);
  CHECK(batches.batches()[0].mesh == kTree);
  CHECK(batches.batches()[0].first == 0);
  CHECK(batches.batches()[0].count == 2);
  CHECK(batches.batches()[1].mesh == kGrass);
  CHECK(batches.batches()[1].first == 2);
  CHECK(batches.batches()[1].count == 1 + kGrassRunLength);
  CHECK(same(batches.slots(), {0.0f, 2.0f, 1.0f, 100.0f, 101.0f, 102.0f}));
}

TEST_CASE("a new pass starts with no copies, and an empty run adds no batch") {
  InstanceBatches batches;
  batches.start(kMeshes);
  batches.add(kTree, 0);
  batches.lay_out();
  batches.start(kMeshes);
  batches.add_run(kGrass, kGrassRunStart, kNoCopies);
  batches.add(kTree, 1);
  batches.lay_out();
  REQUIRE(batches.batches().size() == 1);
  CHECK(batches.batches()[0].mesh == kTree);
  CHECK(same(batches.slots(), {1.0f}));
}
