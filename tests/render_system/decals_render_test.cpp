// System test: decals in a real window. A mark placed on a wall (a mesh) and on a sculpted terrain
// darkens what is under it there, and only there; its color tints it; and past the limit the
// oldest mark is gone from the picture.

#include <doctest/doctest.h>

#include <string>

#include "render_system/render_stage.hpp"
#include "scene/decals.hpp"
#include "scene/props.hpp"
#include "scene/shapes/terrain.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr float kGlow = 0.5f;
constexpr redgem::Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};
constexpr redgem::Color kBlack{0.0f, 0.0f, 0.0f, 1.0f};
constexpr redgem::Color kRed{1.0f, 0.0f, 0.0f, 1.0f};
enum Materials : int { kPlainMaterial };

// The wall: a box whose face (z = kWallFace) the camera looks at straight on.
constexpr Vec3 kWallSize{6.0f, 6.0f, 0.2f};
constexpr float kWallFace = 0.1f;
constexpr Vec3 kFacingCamera{0.0f, 0.0f, 1.0f};
constexpr Vec3 kCameraBack{0.0f, 0.0f, 4.0f};
constexpr float kMarkSize = 0.8f;
constexpr float kMarkDepth = 0.4f;
constexpr float kAside = 1.2f;  // meters between marks side by side

// The terrain, seen from above: a hill raised in its middle.
constexpr float kTerrainSize = 8.0f;
constexpr int kTerrainCells = 32;
constexpr float kHillRadius = 3.0f;
constexpr float kHillHeight = 1.0f;
constexpr Vec3 kCameraAbove{0.0f, 7.0f, 3.0f};

constexpr float kMiddle = 0.5f;
constexpr int kDark = 150;         // the sum of a marked pixel's channels is below this
constexpr int kBright = 450;       // and an unmarked one's above this
constexpr int kStrongly = 60;      // a tinted channel is this much over the others, at least
constexpr std::size_t kSeen = 200;  // pixels of a mark, at least
constexpr float kLeftOfMiddle = 0.45f;
constexpr float kRightOfMiddle = 0.55f;

int luma(::Color c) { return c.r + c.g + c.b; }
bool dark(::Color c) { return luma(c) < kDark; }
bool reddish(::Color c) { return c.r > c.g + kStrongly && c.r > c.b + kStrongly; }

// No bloom: the bright surfaces would bleed their glow over the marks.
NodeDesc environment() {
  NodeDesc node;
  node.name = "Environment";
  node.props = {{props::environment::kBloom, 0.0},
                {props::environment::kFogDensity, 0.0},
                {props::environment::kTemporalAntialias, false},
                {props::environment::kMsaa, 0.0},
                {props::environment::kAmbientOcclusion, 0.0},
                {props::environment::kAutoExposure, false},
                {"environment", true}};
  return node;
}

SceneDesc plain_scene() {
  SceneDesc scene;
  scene.materials = {test::glowing_material(kWhite, kGlow)};
  scene.nodes = {environment()};
  return scene;
}

SceneDesc wall_scene() {
  SceneDesc scene = plain_scene();
  scene.meshes = {test::cube_mesh(kPlainMaterial)};
  scene.nodes.push_back(test::camera_at(kCameraBack));
  NodeDesc wall;
  wall.name = "Wall";
  wall.mesh = 0;
  wall.local.scale = kWallSize;
  scene.nodes.push_back(wall);
  return scene;
}

SceneDesc terrain_scene() {
  SceneDesc scene = plain_scene();
  const TerrainGrid grid{kTerrainSize, kTerrainCells};
  PrimitiveDesc ground = make_terrain(grid);
  sculpt_terrain(ground, grid, TerrainBrush{0.0f, 0.0f, kHillRadius, kHillHeight, SculptMode::Raise});
  ground.material = kPlainMaterial;
  MeshDesc mesh;
  mesh.name = "Terrain";
  mesh.primitives = {ground};
  for (std::size_t i = 0; i < ground.positions.size(); i += 3) mesh.bounds.include(vec3_from(&ground.positions[i]));
  scene.meshes = {mesh};
  scene.nodes.push_back(test::camera_at(kCameraAbove));
  NodeDesc terrain;
  terrain.name = "Terrain";
  terrain.mesh = 0;
  scene.nodes.push_back(terrain);
  return scene;
}

DecalPlacement mark(Vec3 point, Vec3 normal, redgem::Color color = kBlack) {
  DecalPlacement placement;
  placement.point = point;
  placement.normal = normal;
  placement.size = kMarkSize;
  placement.depth = kMarkDepth;
  placement.color = color;
  return placement;
}

// (The scene the decals were placed on: they keep its objects, which another made of the same
// description would not have.)
Picture draw(RenderStage& stage, const SceneDesc& desc, const Scene& scene, const DecalSystem& decals,
             const std::string& name) {
  stage.upload(desc);
  stage.set_decals(&decals);
  const Picture picture = stage.draw(scene, name);
  stage.set_decals(nullptr);
  return picture;
}

}  // namespace

TEST_CASE("a decal darkens the wall it is placed on, there and nowhere else") {
  RenderStage stage;
  const SceneDesc desc = wall_scene();
  const Scene scene = Scene::from_desc(desc);
  DecalSystem decals;
  const Picture bare = draw(stage, desc, scene, decals, "decals_bare_wall");
  REQUIRE(decals.place(scene, desc, mark({0.0f, 0.0f, kWallFace}, kFacingCamera)));
  const Picture marked = draw(stage, desc, scene, decals, "decals_on_wall");
  REQUIRE(luma(bare.at(kMiddle, kMiddle)) > kBright);
  CHECK(bare.find(dark).count == 0);
  CHECK(dark(marked.at(kMiddle, kMiddle)));
  const Picture::Found spot = marked.find(dark);
  CHECK(spot.count > kSeen);
  CHECK(spot.across == doctest::Approx(kMiddle).epsilon(0.05));
  CHECK(luma(marked.at(0.1f, 0.1f)) > kBright);  // the wall away from it is as it was
}

TEST_CASE("a decal falls on a sculpted terrain, following its hill") {
  RenderStage stage;
  const SceneDesc desc = terrain_scene();
  const Scene scene = Scene::from_desc(desc);
  DecalSystem decals;
  const TerrainGrid grid{kTerrainSize, kTerrainCells};
  const Vec3 top{0.0f, terrain_height(desc.meshes[0].primitives[0], grid, 0.0f, 0.0f), 0.0f};
  REQUIRE(top.y > kHillHeight * kHalf);
  REQUIRE(decals.place(scene, desc, mark(top, Vec3::up())));
  const Picture marked = draw(stage, desc, scene, decals, "decals_on_terrain");
  const Picture::Found spot = marked.find(dark);
  CHECK(spot.count > kSeen);
  CHECK(spot.across == doctest::Approx(kMiddle).epsilon(0.05));
}

TEST_CASE("a decal's color tints the mark") {
  RenderStage stage;
  const SceneDesc desc = wall_scene();
  const Scene scene = Scene::from_desc(desc);
  DecalSystem decals;
  REQUIRE(decals.place(scene, desc, mark({0.0f, 0.0f, kWallFace}, kFacingCamera, kRed)));
  const Picture marked = draw(stage, desc, scene, decals, "decals_red_on_wall");
  CHECK(reddish(marked.at(kMiddle, kMiddle)));
  CHECK(marked.find(reddish).count > kSeen);
}

TEST_CASE("past their limit, the oldest decal is gone from the picture") {
  RenderStage stage;
  const SceneDesc desc = wall_scene();
  const Scene scene = Scene::from_desc(desc);
  DecalSystem decals;
  for (const float x : {-kAside, 0.0f, kAside}) decals.place(scene, desc, mark({x, 0.0f, kWallFace}, kFacingCamera));
  const Picture three = draw(stage, desc, scene, decals, "decals_three");
  decals.set_limit(2);
  const Picture two = draw(stage, desc, scene, decals, "decals_newest_two");
  const Picture::Found all = three.find(dark);
  const Picture::Found kept = two.find(dark);
  REQUIRE(all.count > kSeen);
  CHECK(all.across > kLeftOfMiddle);
  CHECK(all.across < kRightOfMiddle);
  CHECK(kept.count < all.count);
  CHECK(kept.across > kRightOfMiddle);  // the left one, the first placed, is gone
}
