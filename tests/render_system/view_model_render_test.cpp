// System test: the view model (a first-person weapon) in a real window. Pressed against a wall,
// the weapon is drawn over it instead of going into it; its field of view is its own, apart from
// the world's; a muzzle under it is drawn with it; it is lit in a dark room; and it casts no
// shadow on the world.
#include <doctest/doctest.h>

#include <cmath>
#include <cstdlib>
#include <string>

#include "render_system/render_stage.hpp"
#include "scene/props.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr float kHalfTurn = 3.14159265f;
constexpr float kRadiansPerDegree = kHalfTurn / 180.0f;
constexpr float kGlow = 2.0f;
constexpr int kStrongly = 60;       // a channel is this much over the others, at least, in a colored pixel
constexpr std::size_t kSeen = 50;   // pixels of a part, at least
constexpr std::size_t kNone = 5;    // pixels of a part that is hidden, at most
constexpr float kSameSize = 0.1f;   // a part this share bigger or smaller is the same size
constexpr float kSameSpot = 0.02f;  // of the picture across

constexpr redgem::Color kRed{1.0f, 0.1f, 0.1f, 1.0f};
constexpr redgem::Color kGreen{0.1f, 1.0f, 0.1f, 1.0f};
constexpr redgem::Color kBlue{0.1f, 0.1f, 1.0f, 1.0f};
constexpr redgem::Color kYellow{1.0f, 1.0f, 0.1f, 1.0f};
enum Materials : int {
  kWallMaterial,
  kBarrelMaterial,
  kTargetMaterial,
  kMuzzleMaterial,
  kPlainMaterial,
  kInsideMaterial,  // plain, seen from inside a box
};

// The camera at the origin looks down -z. The weapon, an empty, hangs to its right and below,
// with a barrel along it and a muzzle at its end. The wall's face is nearer than the weapon.
constexpr Vec3 kWeaponAt{0.15f, -0.12f, -0.55f};
constexpr Vec3 kBarrelSize{0.08f, 0.08f, 0.4f};
constexpr Vec3 kMuzzleAt{0.0f, 0.0f, -0.22f};
constexpr float kMuzzleSize = 0.06f;
constexpr Vec3 kWallAt{0.0f, 0.0f, -0.4f};
constexpr Vec3 kWallSize{4.0f, 4.0f, 0.2f};  // its face 0.3 m in front of the camera
constexpr Vec3 kTargetAt{1.5f, 0.0f, -6.0f};
constexpr float kTargetSize = 0.5f;
constexpr float kWideFov = 80.0f;  // degrees

// The dark room: a closed box around the camera, no ambient light.
constexpr float kRoomSize = 6.0f;
constexpr int kReadable = 120;  // the sum of a pixel's channels on a lit weapon, at least

// The shadow test: the camera high above a white ground, a plate under it in the sun's way.
constexpr Vec3 kHighUp{0.0f, 12.0f, 1.0f};
constexpr float kGroundScale = 30.0f;
constexpr Vec3 kPlateAt{2.0f, 1.5f, 0.0f};
constexpr float kPlateSize = 2.0f;
constexpr int kInShadow = 250;  // the sum of a pixel's channels in the sun's shadow is below this
constexpr std::size_t kSomeShadow = 100;
constexpr float kMiddle = 0.5f;

int luma(::Color c) { return c.r + c.g + c.b; }
bool over(int a, int b, int c) { return a > b + kStrongly && a > c + kStrongly; }
bool red(::Color c) { return over(c.r, c.g, c.b); }
bool green(::Color c) { return over(c.g, c.r, c.b); }
bool blue(::Color c) { return over(c.b, c.r, c.g); }
bool yellow(::Color c) { return c.r > c.b + kStrongly && c.g > c.b + kStrongly; }
bool in_sun_shadow(::Color c) { return luma(c) < kInShadow; }
// The barrel, lit: bright, and not the muzzle's glow.
bool lit_barrel(::Color c) { return luma(c) >= kReadable && !yellow(c); }

bool about_the_same(std::size_t a, std::size_t b) {
  return std::fabs(static_cast<float>(a) - static_cast<float>(b)) <= static_cast<float>(b) * kSameSize;
}

NodeDesc node(const std::string& name, int mesh, Vec3 at, Vec3 size, int parent = kNoIndex) {
  NodeDesc desc;
  desc.name = name;
  desc.mesh = mesh;
  desc.local.position = at;
  desc.local.scale = size;
  desc.parent = parent;
  return desc;
}

// Puts a node in a scene, under its parent (if it has one); returns its index.
int add(SceneDesc& scene, NodeDesc desc) {
  const int index = static_cast<int>(scene.nodes.size());
  if (desc.parent != kNoIndex) scene.nodes[static_cast<std::size_t>(desc.parent)].children.push_back(index);
  scene.nodes.push_back(std::move(desc));
  return index;
}

NodeDesc environment(double ambient) {
  NodeDesc node;
  node.name = "Environment";
  node.props = {{props::environment::kAmbient, ambient},
                {props::environment::kFogDensity, 0.0},
                {props::environment::kAmbientOcclusion, 0.0},
                {props::environment::kBloom, 0.0},
                {"environment", true}};
  return node;
}

// A mesh for each material (mesh i is drawn with material i), and nothing else.
SceneDesc materials_scene() {
  SceneDesc scene;
  MaterialDesc inside;
  inside.double_sided = true;
  scene.materials = {test::glowing_material(kRed, kGlow),
                     test::glowing_material(kGreen, kGlow),
                     test::glowing_material(kBlue, kGlow),
                     test::glowing_material(kYellow, kGlow),
                     MaterialDesc{},
                     inside};
  for (int material = kWallMaterial; material <= kInsideMaterial; ++material)
    scene.meshes.push_back(test::cube_mesh(material));
  return scene;
}

// A camera looking down -z, holding the weapon.
SceneDesc stage_scene(bool view_model) {
  SceneDesc scene = materials_scene();
  NodeDesc camera = node("Camera", kNoIndex, Vec3::zero(), Vec3::one());
  camera.camera = CameraDesc{};
  const int eye = add(scene, camera);
  NodeDesc weapon = node("Weapon", kNoIndex, kWeaponAt, Vec3::one(), eye);
  weapon.props[props::kViewModel] = view_model;
  const int held = add(scene, weapon);
  add(scene, node("Barrel", kBarrelMaterial, Vec3::zero(), kBarrelSize, held));
  add(scene, node("Muzzle", kMuzzleMaterial, kMuzzleAt, Vec3::one() * kMuzzleSize, held));
  return scene;
}

SceneDesc against_a_wall(bool view_model) {
  SceneDesc scene = stage_scene(view_model);
  add(scene, node("Wall", kWallMaterial, kWallAt, kWallSize));
  return scene;
}

SceneDesc with_a_target() {
  SceneDesc scene = stage_scene(true);
  add(scene, node("Target", kTargetMaterial, kTargetAt, Vec3::one() * kTargetSize));
  return scene;
}

NodeDesc dark_sun() {
  NodeDesc sun;
  sun.name = "Sun";
  sun.light = LightDesc{};  // no intensity: it lights nothing (and keeps the default sun away)
  return sun;
}

// The weapon made of a plain material (it does not glow), in a closed room with no light at all:
// only the view model's fill can light it.
SceneDesc in_a_dark_room(bool view_model) {
  SceneDesc scene = stage_scene(view_model);
  scene.nodes[2].mesh = kPlainMaterial;  // the barrel
  add(scene, node("Room", kInsideMaterial, Vec3::zero(), Vec3::one() * kRoomSize));
  add(scene, dark_sun());
  add(scene, environment(0.0));
  return scene;
}

SceneDesc plate_in_the_sun(bool view_model) {
  SceneDesc scene = materials_scene();
  add(scene, test::camera_at(kHighUp));
  add(scene, node("Ground", kPlainMaterial, Vec3{0.0f, -kMiddle, 0.0f}, Vec3{kGroundScale, 1.0f, kGroundScale}));
  NodeDesc plate = node("Plate", kPlainMaterial, kPlateAt, Vec3{kPlateSize, kMuzzleSize, kPlateSize});
  plate.props[props::kViewModel] = view_model;
  add(scene, plate);
  add(scene, environment(0.0));
  return scene;
}

Picture draw(RenderStage& stage, const SceneDesc& desc, const std::string& name) {
  stage.upload(desc);
  return stage.draw(Scene::from_desc(desc), name);
}

Picture draw_with_fovs(RenderStage& stage, float world_degrees, float view_model_degrees, const std::string& name) {
  const SceneDesc desc = with_a_target();
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  CameraDesc& lens = *scene.at(scene.main_camera()).camera;
  lens.fov_y_radians = world_degrees * kRadiansPerDegree;
  lens.view_model_fov_y_radians = view_model_degrees * kRadiansPerDegree;
  return stage.draw(scene, name);
}

}  // namespace

TEST_CASE("a weapon pressed against a wall goes into it, unless it is the view model") {
  RenderStage stage;
  const Picture world = draw(stage, against_a_wall(false), "view_model_off_in_wall");
  const Picture held = draw(stage, against_a_wall(true), "view_model_over_wall");
  REQUIRE(world.find(red).count > kSeen);
  CHECK(world.find(green).count <= kNone);  // the barrel is behind the wall's face
  CHECK(world.find(yellow).count <= kNone);
  CHECK(held.find(green).count > kSeen);
  CHECK(held.find(yellow).count > kSeen);  // the muzzle under it is drawn with it
  CHECK(held.find(red).count > kSeen);     // and the wall is still there around it
}

TEST_CASE("the view model's field of view is its own, apart from the world's") {
  RenderStage stage;
  const Picture normal =
      draw_with_fovs(stage, config::kDefaultFovYDegrees, config::kDefaultViewModelFovYDegrees, "view_model_fov_normal");
  const Picture wide_world =
      draw_with_fovs(stage, kWideFov, config::kDefaultViewModelFovYDegrees, "view_model_fov_wide_world");
  const Picture wide_weapon = draw_with_fovs(stage, config::kDefaultFovYDegrees, kWideFov, "view_model_fov_wide");
  const Picture::Found barrel = normal.find(green);
  const Picture::Found target = normal.find(blue);
  REQUIRE(barrel.count > kSeen);
  REQUIRE(target.count > kSeen);
  // A wider world shows the target smaller and nearer the middle; the weapon stays as it was.
  CHECK(wide_world.find(blue).count < target.count);
  CHECK(wide_world.find(blue).across < target.across);
  CHECK(about_the_same(wide_world.find(green).count, barrel.count));
  CHECK(std::fabs(wide_world.find(green).across - barrel.across) < kSameSpot);
  // A wider view model shows the weapon smaller; the target stays as it was.
  CHECK(wide_weapon.find(green).count < barrel.count);
  CHECK(about_the_same(wide_weapon.find(blue).count, target.count));
}

TEST_CASE("the view model has a light of its own: it is seen in a room with no light at all") {
  RenderStage stage;
  const Picture world = draw(stage, in_a_dark_room(false), "view_model_off_dark_room");
  const Picture held = draw(stage, in_a_dark_room(true), "view_model_dark_room");
  REQUIRE(world.find(yellow).count > kSeen);  // the muzzle glows: the barrel is there
  CHECK(world.find(lit_barrel).count <= kNone);
  CHECK(held.find(lit_barrel).count > kSeen);
}

TEST_CASE("the view model casts no shadow on the world") {
  RenderStage stage;
  const Picture world = draw(stage, plate_in_the_sun(false), "view_model_off_shadow");
  const Picture held = draw(stage, plate_in_the_sun(true), "view_model_no_shadow");
  REQUIRE(world.find(in_sun_shadow).count > kSomeShadow);
  CHECK(held.find(in_sun_shadow).count <= kNone);
}
