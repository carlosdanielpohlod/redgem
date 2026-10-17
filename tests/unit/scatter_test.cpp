#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "render/render_math.hpp"
#include "render/scatter_chunks.hpp"
#include "scene/scatter.hpp"
#include "scene/scatter_looks.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::render_math;
using redgem::test::kTolerance;

namespace {

constexpr std::size_t kTries = 200;
constexpr std::uint32_t kSeed = 7;
constexpr std::uint32_t kOtherSeed = 8;
constexpr Vec3 kLow{-20.0f, 0.0f, -10.0f};
constexpr Vec3 kHigh{20.0f, 30.0f, 10.0f};
constexpr float kSmallest = 0.5f;
constexpr float kLargest = 2.0f;
constexpr float kGroundHeight = 4.0f;
constexpr Vec3 kSlope{0.0f, 0.8f, 0.6f};  // a hillside facing +z, normalized
constexpr std::size_t kTooMany = kMaxScatteredCopies + 1;
// Two copies in one cell, a third in the cell beside it.
constexpr Vec3 kFirstInCell{1.0f, 0.0f, 1.0f};
constexpr Vec3 kSecondInCell{3.0f, 0.0f, 2.0f};
constexpr Vec3 kNextCell{kScatterCell + 1.0f, 0.0f, 1.0f};
constexpr Vec3 kUnitBoxMin{-1.0f, -1.0f, -1.0f};
constexpr Vec3 kUnitBoxMax{1.0f, 1.0f, 1.0f};
constexpr float kIdentity[kMatrixFloats] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
constexpr float kMinAngle = 0.01f;
constexpr Vec3 kNearViewer{0.0f, 0.0f, 3.0f};
constexpr Vec3 kFarViewer{0.0f, 0.0f, 900.0f};
constexpr float kSmallCopy = 0.25f;
constexpr float kChunkRadius = 0.9f;
constexpr float kNear = 1.0f;  // meters: a level of detail for what is at most this far
const float kForever = std::numeric_limits<float>::infinity();
constexpr float kLodBoundary = 3.0f;         // meters: where one level of detail hands over to the next
constexpr Vec3 kNearCopy{0.0f, 0.0f, 0.4f};  // 2.6 m from the near viewer
constexpr Vec3 kFarCopy{0.0f, 0.0f, -0.4f};  // 3.4 m from it
constexpr int kNearMesh = 0;
constexpr int kFarMesh = 1;

ScatterRequest request(std::uint32_t seed = kSeed) {
  ScatterRequest asked;
  asked.count = kTries;
  asked.low = kLow;
  asked.high = kHigh;
  asked.seed = seed;
  asked.smallest = kSmallest;
  asked.largest = kLargest;
  return asked;
}

// Ground at a height of its own, sloping as `normal` says.
GroundProbe flat_ground(Vec3 normal = Vec3::up()) {
  return [normal](Vec3 above, float) {
    return std::optional<GroundHit>(GroundHit{{above.x, kGroundHeight, above.z}, normal});
  };
}

// Ground only where x is positive: copies elsewhere are not placed.
GroundProbe half_ground() {
  return [](Vec3 above, float) {
    return above.x > 0.0f ? std::optional<GroundHit>(GroundHit{{above.x, kGroundHeight, above.z}, Vec3::up()})
                          : std::nullopt;
  };
}

bool same_places(const std::vector<redgem::Transform>& a, const std::vector<redgem::Transform>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!approx_equal(a[i].position, b[i].position, kTolerance)) return false;
  }
  return true;
}

redgem::Transform at(Vec3 position) {
  redgem::Transform place;
  place.position = position;
  return place;
}

Bounds unit_box() {
  Bounds bounds;
  bounds.include(kUnitBoxMin);
  bounds.include(kUnitBoxMax);
  return bounds;
}

View identity_view(Vec3 viewer) { return {frustum_from_matrix(kIdentity), viewer, kMinAngle}; }

ScatterChunk small_chunk_in_the_middle() { return {0, 1, {Vec3::zero(), kChunkRadius}, kSmallCopy}; }

}  // namespace

TEST_CASE("the same seed scatters copies in the same places, another seed elsewhere") {
  const std::vector<redgem::Transform> first = scatter_places(request(), flat_ground());
  CHECK(first.size() == kTries);
  CHECK(same_places(first, scatter_places(request(), flat_ground())));
  CHECK_FALSE(same_places(first, scatter_places(request(kOtherSeed), flat_ground())));
}

TEST_CASE("scattered copies stand on the ground within the box, upright, turned and scaled as asked") {
  for (const redgem::Transform& place : scatter_places(request(), flat_ground())) {
    CHECK(place.position.x >= kLow.x);
    CHECK(place.position.x <= kHigh.x);
    CHECK(place.position.z >= kLow.z);
    CHECK(place.position.z <= kHigh.z);
    CHECK(place.position.y == doctest::Approx(kGroundHeight));
    CHECK(place.scale.x >= kSmallest);
    CHECK(place.scale.x <= kLargest);
    CHECK(approx_equal(place.up(), Vec3::up(), kTolerance));
  }
}

TEST_CASE("without a ground, copies stand on the bottom of the box") {
  for (const redgem::Transform& place : scatter_places(request(), GroundProbe{})) CHECK(place.position.y == kLow.y);
}

TEST_CASE("aligned copies lean with the slope they stand on") {
  ScatterRequest aligned = request();
  aligned.align = true;
  for (const redgem::Transform& place : scatter_places(aligned, flat_ground(kSlope))) {
    CHECK(approx_equal(place.up(), kSlope, kTolerance));
  }
}

TEST_CASE("where the ground is missing nothing is placed, and the others stay where they were") {
  const std::vector<redgem::Transform> everywhere = scatter_places(request(), flat_ground());
  const std::vector<redgem::Transform> some = scatter_places(request(), half_ground());
  CHECK(some.size() < everywhere.size());
  CHECK_FALSE(some.empty());
  std::vector<redgem::Transform> kept;
  for (const redgem::Transform& place : everywhere) {
    if (place.position.x > 0.0f) kept.push_back(place);
  }
  CHECK(same_places(some, kept));
}

TEST_CASE("a scene's sets are added and removed by id, and every change is a version of its own") {
  Scattered scattered;
  CHECK(scattered.version() == 0);
  ScatterSet set;
  set.places = {at(kFirstInCell), at(kSecondInCell)};
  const int id = scattered.add(set);
  CHECK(id != 0);
  CHECK(scattered.copies() == 2);
  const std::uint64_t added = scattered.version();
  Scattered another;
  another.add(set);
  CHECK(another.version() != added);  // even in another scene
  CHECK(scattered.remove(id));
  CHECK_FALSE(scattered.remove(id));
  CHECK(scattered.version() != added);
  CHECK(scattered.sets().empty());
}

TEST_CASE("a scene holds only so many scattered copies") {
  Scattered scattered;
  ScatterSet huge;
  huge.places.resize(kTooMany);
  CHECK(scattered.add(huge) == 0);
  CHECK(scattered.sets().empty());
}

TEST_CASE("scattered copies are cut into cells of the ground, each with a sphere around its copies") {
  std::vector<std::size_t> order;
  const std::vector<ScatterChunk> chunks =
      chunk_copies({at(kFirstInCell), at(kNextCell), at(kSecondInCell)}, unit_box(), order);
  REQUIRE(chunks.size() == 2);
  CHECK(order == std::vector<std::size_t>{0, 2, 1});  // the two of the first cell together
  CHECK(chunks[0].first == 0);
  CHECK(chunks[0].count == 2);
  CHECK(chunks[1].first == 2);
  CHECK(chunks[1].count == 1);
  CHECK(chunks[0].largest_copy == doctest::Approx(std::sqrt(3.0f)));  // half a unit box's diagonal
  for (const Vec3 copy : {kFirstInCell, kSecondInCell}) {
    CHECK(length(copy - chunks[0].sphere.center) + std::sqrt(3.0f) <= chunks[0].sphere.radius + kTolerance);
  }
}

TEST_CASE("a cell is drawn when in view and near enough for its biggest copy to show") {
  const ScatterChunk chunk = small_chunk_in_the_middle();
  CHECK(chunk_worth_drawing(identity_view(kNearViewer), chunk));
  CHECK_FALSE(chunk_worth_drawing(identity_view(kFarViewer), chunk));  // too small from there
  ScatterChunk aside = chunk;
  aside.sphere.center = kNextCell;
  CHECK_FALSE(chunk_worth_drawing(identity_view(kNearViewer), aside));  // out of the view
}

TEST_CASE("a cell is within its distances, beyond them, or across them") {
  const ScatterChunk chunk = small_chunk_in_the_middle();  // 3 m from the viewer, 0.9 m across
  CHECK(reach_of(identity_view(kNearViewer), chunk, 0.0f, kForever) == Reach::All);
  CHECK(reach_of(identity_view(kNearViewer), chunk, 0.0f, kNear) == Reach::None);  // all of it farther
  CHECK(reach_of(identity_view(kNearViewer), chunk, 0.0f, kLodBoundary) == Reach::Some);
}

TEST_CASE("two levels of detail meeting across a cell draw each copy once, by where each copy is") {
  ScatterLayout near_set{kNearMesh, 0.0f, kLodBoundary, true, 0, {small_chunk_in_the_middle()}, {kNearCopy, kFarCopy}};
  near_set.chunks[0].count = 2;
  ScatterLayout far_set = near_set;
  far_set.mesh = kFarMesh;
  far_set.visible_from = kLodBoundary;
  far_set.visible_to = kForever;
  InstanceBatches batches;
  batches.start();
  gather_set(batches, identity_view(kNearViewer), near_set);
  gather_set(batches, identity_view(kNearViewer), far_set);
  REQUIRE(batches.batches().size() == 2);
  CHECK(batches.batches()[0].mesh == kNearMesh);
  CHECK(batches.batches()[1].mesh == kFarMesh);
  CHECK(batches.slots() == std::vector<float>{0.0f, 1.0f});  // the near copy by the near set, the far by the far
}

TEST_CASE("copies take tints along the list asked for, the same for the same seed") {
  ScatterLooks looks;
  looks.tints = {Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}};
  const std::vector<Vec3> tints = scatter_tints(looks, kSeed, kTries);
  REQUIRE(tints.size() == kTries);
  const std::vector<Vec3> again = scatter_tints(looks, kSeed, kTries);
  const std::vector<Vec3> other = scatter_tints(looks, kOtherSeed, kTries);
  CHECK(approx_equal(tints[kTries / 2], again[kTries / 2], 0.0f));
  CHECK_FALSE(approx_equal(tints[kTries / 2], other[kTries / 2], kTolerance));
  for (const Vec3 tint : tints) {
    CHECK(tint.y == doctest::Approx(0.0f));
    CHECK(tint.x + tint.z == doctest::Approx(1.0f));  // a blend of the two
  }
}

TEST_CASE("copies vary in brightness only as much as asked, and not at all unless asked") {
  ScatterLooks looks;
  CHECK(scatter_tints(looks, kSeed, kTries).empty());
  constexpr float kVariation = 0.2f;
  looks.tint_variation = kVariation;
  float darkest = 1.0f;
  float brightest = 1.0f;
  for (const Vec3 tint : scatter_tints(looks, kSeed, kTries)) {
    CHECK(tint.x == doctest::Approx(tint.y));  // no hue: white made brighter or darker
    darkest = std::min(darkest, tint.x);
    brightest = std::max(brightest, tint.x);
  }
  CHECK(darkest >= 1.0f - kVariation);
  CHECK(brightest <= 1.0f + kVariation);
  CHECK(brightest - darkest > kVariation);  // they do vary
}

TEST_CASE("copies are scaled by their template's own scale, on top of their own") {
  std::vector<redgem::Transform> places = scatter_places(request(), {});
  const float own = places.front().scale.x;
  scale_places(places, Vec3{2.0f, 3.0f, 2.0f});
  CHECK(places.front().scale.x == doctest::Approx(own * 2.0f));
  CHECK(places.front().scale.y == doctest::Approx(own * 3.0f));
}

TEST_CASE("scattered see-through copies are drawn cell by cell, sorted by the distance of their cell") {
  constexpr Vec3 kNearCell{0.0f, 0.0f, 2.0f};
  constexpr Vec3 kFarCell{0.0f, 0.0f, 40.0f};
  constexpr std::size_t kObjectRecord = 7;  // after every scattered copy's
  constexpr std::size_t kPassFirst = 10;    // where the pass's record numbers start among the frame's
  const ScatterLayout layout{
      kNearMesh,
      0.0f,
      kForever,
      true,
      0,
      {{0, 1, {kNearCell, kChunkRadius}, kSmallCopy}, {1, 2, {kFarCell, kChunkRadius}, kSmallCopy}},
      {kNearCell, kFarCell, kFarCell}};
  const std::vector<float> records{static_cast<float>(kObjectRecord), 0.0f, 1.0f, 2.0f};  // an object, then the copies
  std::vector<SeeThroughDraw> draws;
  add_see_through_cells(draws, records, {layout}, GatheredRun{kNearMesh, 1, 3, kPassFirst}, Vec3::zero());
  sort_back_to_front(draws);
  REQUIRE(draws.size() == 2);
  CHECK(draws[0].first == kPassFirst + 2);  // the far cell's two copies first
  CHECK(draws[0].count == 2);
  CHECK(draws[0].distance == doctest::Approx(dot(kFarCell, kFarCell)));
  CHECK(draws[1].first == kPassFirst + 1);
  CHECK(draws[1].count == 1);
  CHECK(draws[1].mesh == kNearMesh);
}
