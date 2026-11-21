#include <doctest/doctest.h>

#include <array>
#include <vector>

#include "decal_meshes.hpp"
#include "scene/decal_clip.hpp"
#include "scene/decals.hpp"
#include "scene/scene.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

namespace {

constexpr float kSize = 0.4f;
constexpr float kDepth = 0.2f;
constexpr float kFloorSide = 2.0f;
constexpr float kQuarterTurn = kPi * kHalf;

// The area the triangles of a list of corners cover.
float area_of(const std::vector<DecalCorner>& corners) {
  float area = 0.0f;
  for (std::size_t i = 0; i + 2 < corners.size(); i += 3) {
    area += length(cross(corners[i + 1].position - corners[i].position, corners[i + 2].position - corners[i].position));
  }
  return area * kHalf;
}

bool on_the_picture(const DecalCorner& corner) {
  const bool across = corner.u >= -kTolerance && corner.u <= 1.0f + kTolerance;
  return across && corner.v >= -kTolerance && corner.v <= 1.0f + kTolerance;
}

// A wall's big triangle in the z = 0 plane, facing +Z, around the origin.
constexpr std::array<Vec3, 3> kWall{Vec3{-5, -5, 0}, Vec3{5, -5, 0}, Vec3{0, 5, 0}};

// A scene of a floor: a mesh, facing up.
SceneDesc floor_desc() {
  SceneDesc desc;
  desc.meshes = {test::floor_quad(kFloorSide)};
  NodeDesc floor;
  floor.name = "Floor";
  floor.mesh = 0;
  desc.nodes = {floor};
  return desc;
}

DecalPlacement on_the_floor(Vec3 point, Color color = {}) {
  DecalPlacement placement;
  placement.point = point;
  placement.normal = Vec3::up();
  placement.size = kSize;
  placement.depth = kDepth;
  placement.color = color;
  return placement;
}

}  // namespace

TEST_CASE("a decal's box is upright on a wall, and faces forward on a floor") {
  const DecalBox wall = decal_box(Vec3::zero(), {0, 0, 1}, kSize, kDepth, 0.0f);
  CHECK(approx_equal(wall.right, Vec3::right(), kTolerance));
  CHECK(approx_equal(wall.up, Vec3::up(), kTolerance));
  CHECK(wall.half_size == doctest::Approx(kSize * kHalf));
  CHECK(wall.half_depth == doctest::Approx(kDepth * kHalf));
  const DecalBox floor = decal_box(Vec3::zero(), Vec3::up(), kSize, kDepth, 0.0f);
  CHECK(approx_equal(floor.right, Vec3::right(), kTolerance));
  CHECK(approx_equal(floor.up, Vec3::forward(), kTolerance));
  const DecalBox turned = decal_box(Vec3::zero(), {0, 0, 1}, kSize, kDepth, kQuarterTurn);
  CHECK(approx_equal(turned.right, Vec3::up(), kTolerance));  // turned a quarter about the normal
}

TEST_CASE("a surface larger than a decal is cut to the decal's square, its picture spread over it") {
  const DecalBox box = decal_box(Vec3::zero(), {0, 0, 1}, kSize, kDepth, 0.0f);
  std::vector<DecalCorner> corners;
  clip_to_decal(box, kWall, corners);
  REQUIRE(corners.size() % 3 == 0);
  CHECK(area_of(corners) == doctest::Approx(kSize * kSize));
  for (const DecalCorner& corner : corners) CHECK(on_the_picture(corner));
}

TEST_CASE("a decal leaves out a surface turned away from it, and one outside its box") {
  const DecalBox box = decal_box(Vec3::zero(), {0, 0, 1}, kSize, kDepth, 0.0f);
  std::vector<DecalCorner> corners;
  clip_to_decal(box, {kWall[0], kWall[2], kWall[1]}, corners);  // its back
  const Vec3 away{0, 0, kDepth};                                   // past the box's front
  clip_to_decal(box, {kWall[0] + away, kWall[1] + away, kWall[2] + away}, corners);
  CHECK(corners.empty());
}

TEST_CASE("a decal falls on the mesh under it, and on nothing in empty space") {
  const SceneDesc desc = floor_desc();
  const Scene scene = Scene::from_desc(desc);
  DecalSystem decals;
  CHECK_FALSE(decals.place(scene, desc, on_the_floor({0, 5, 0})));
  CHECK(decals.decals().empty());
  REQUIRE(decals.place(scene, desc, on_the_floor(Vec3::zero())));
  REQUIRE(decals.decals().size() == 1);
  const Decal& decal = decals.decals().front();
  REQUIRE(decal.pieces.size() == 1);
  CHECK(scene.resolve(decal.pieces.front().host) == 0);
  CHECK(area_of(decal.pieces.front().corners) == doctest::Approx(kSize * kSize));
}

TEST_CASE("a decal across the edge of a surface keeps only what is on it") {
  const SceneDesc desc = floor_desc();
  const Scene scene = Scene::from_desc(desc);
  DecalSystem decals;
  REQUIRE(decals.place(scene, desc, on_the_floor({kFloorSide * kHalf, 0, 0})));
  CHECK(area_of(decals.decals().front().pieces.front().corners) == doctest::Approx(kSize * kSize * kHalf));
}

TEST_CASE("decals are kept up to their limit: past it, the oldest goes") {
  const SceneDesc desc = floor_desc();
  const Scene scene = Scene::from_desc(desc);
  DecalSystem decals;
  CHECK(decals.limit() == kDefaultDecalLimit);
  decals.set_limit(2);
  for (const float red : {0.1f, 0.2f, 0.3f}) decals.place(scene, desc, on_the_floor(Vec3::zero(), {red, 0, 0, 1}));
  REQUIRE(decals.decals().size() == 2);
  CHECK(decals.decals().front().color.r == doctest::Approx(0.2f));
  CHECK(decals.decals().back().color.r == doctest::Approx(0.3f));
  decals.set_limit(1);
  REQUIRE(decals.decals().size() == 1);
  CHECK(decals.decals().front().color.r == doctest::Approx(0.3f));
  decals.set_limit(0);
  CHECK(decals.limit() == kMinDecalLimit);
  decals.set_limit(kMaxDecalLimit * 2);
  CHECK(decals.limit() == kMaxDecalLimit);
  decals.clear();
  CHECK(decals.decals().empty());
}

TEST_CASE("a decal stays where it was placed on its object as the object moves") {
  const SceneDesc desc = floor_desc();
  Scene scene = Scene::from_desc(desc);
  DecalSystem decals;
  const Vec3 mark{0.5f, 0, 0.5f};
  REQUIRE(decals.place(scene, desc, on_the_floor(mark)));
  const Vec3 moved{3, 1, -2};
  scene.set_world_position(0, moved);
  const std::vector<DecalCorner> corners =
      world_corners(decals.decals().front().pieces.front(), scene.world_transform(0));
  Bounds around;
  for (const DecalCorner& corner : corners) around.include(corner.position);
  CHECK(around.center().x == doctest::Approx(mark.x + moved.x));
  CHECK(around.center().z == doctest::Approx(mark.z + moved.z));
  CHECK(around.min.y > moved.y);  // raised a hair off the surface
}

TEST_CASE("a mirrored object takes decals on the side that faces out") {
  SceneDesc desc = floor_desc();
  desc.nodes[0].local.scale = {-1, 1, 1};
  const Scene scene = Scene::from_desc(desc);
  DecalSystem decals;
  CHECK(decals.place(scene, desc, on_the_floor(Vec3::zero())));
}

TEST_CASE("hidden objects, prefabs, the view model and skinned or shape-keyed meshes take no decals") {
  SceneDesc desc = floor_desc();
  const Scene plain = Scene::from_desc(desc);
  const GameObject& floor = plain.at(0);
  CHECK(takes_decals(floor, desc));
  GameObject other = floor;
  other.visible = false;
  CHECK_FALSE(takes_decals(other, desc));
  other = floor;
  other.prefab = true;
  CHECK_FALSE(takes_decals(other, desc));
  other = floor;
  other.view_model = true;
  CHECK_FALSE(takes_decals(other, desc));
  other = floor;
  other.skin = 0;
  CHECK_FALSE(takes_decals(other, desc));
  desc.meshes[0].target_names = {"Bend"};
  CHECK_FALSE(takes_decals(floor, desc));
}

TEST_CASE("a decal does not fall on a mesh under the view model") {
  SceneDesc desc = floor_desc();
  NodeDesc weapon;
  weapon.name = "Weapon";
  weapon.props["view_model"] = true;
  weapon.children = {0};
  desc.nodes[0].parent = 1;
  desc.nodes.push_back(weapon);
  const Scene scene = Scene::from_desc(desc);
  DecalSystem decals;
  CHECK_FALSE(decals.place(scene, desc, on_the_floor(Vec3::zero())));
}
