// System test: what the renderer draws, in a real window, read back as pixels. Scenes are made in
// code: a skeleton of more bones than shader uniforms ever held, copies of one mesh drawn
// together, and scattered copies.
#include <doctest/doctest.h>

#include <array>
#include <string>
#include <vector>

#include "render_system/render_stage.hpp"
#include "scene/animation/animation_player.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr Vec3 kCameraAt{0.0f, 0.0f, 10.0f};
constexpr int kBones = 70;  // more than the 64 a computer's shader held, and the 48 of a phone's
constexpr int kLastBone = kBones - 1;
constexpr std::size_t kFirstBoneNode = 1;  // after the camera
constexpr float kSwing = 3.0f;             // meters the last bone moves the box along x
constexpr float kSwingSeconds = 1.0f;
constexpr const char* kSwingClip = "Swing";
constexpr float kMiddle = 0.5f;
constexpr float kNearTheMiddle = 0.05f;  // of the picture's width
constexpr float kFarRight = 0.75f;
constexpr std::size_t kEnoughPixels = 50;
constexpr std::uint8_t kStrong = 150;  // a channel this bright ...
constexpr std::uint8_t kWeak = 100;    // ... over the others below this is that channel's color
constexpr int kNoMaterial = kNoIndex;
constexpr float kSpacing = 3.0f;  // meters between the copies of a row
constexpr std::array<Vec3, 3> kTints = {Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}};
constexpr std::array<float, 3> kAcross = {0.13f, 0.5f, 0.87f};  // where the row's copies are, in the picture
constexpr float kAtEyeHeight = 0.5f;
constexpr float kGlow = 2.0f;  // how brightly the glowing material shines, over its color
constexpr int kBrightRed = 0;
constexpr int kBrightGreen = 1;
constexpr int kBrightBlue = 2;

bool red(::Color c) { return c.r > kStrong && c.g < kWeak && c.b < kWeak; }
bool green(::Color c) { return c.g > kStrong && c.r < kWeak && c.b < kWeak; }

// Which channel of a pixel is brightest.
int brightest(::Color c) {
  if (c.r > c.g && c.r > c.b) return kBrightRed;
  return c.g > c.b ? kBrightGreen : kBrightBlue;
}

MaterialDesc glowing(redgem::Color color) { return test::glowing_material(color, kGlow); }
MeshDesc cube(int material) { return test::cube_mesh(material); }

// A cube every vertex of which follows the skeleton's last bone, and nothing else.
MeshDesc box_on_the_last_bone() {
  MeshDesc mesh = cube(0);
  PrimitiveDesc& box = mesh.primitives[0];
  for (std::size_t vertex = 0; vertex < box.positions.size() / 3; ++vertex) {
    box.joints.insert(box.joints.end(), {kLastBone, 0, 0, 0});
    box.weights.insert(box.weights.end(), {1.0f, 0.0f, 0.0f, 0.0f});
  }
  return mesh;
}

// A camera, kBones bones (all at the origin), and a box skinned to the last one, which the clip
// "Swing" moves to the right.
SceneDesc skeleton_of_many_bones() {
  SceneDesc desc;
  desc.materials = {glowing({1.0f, 0.0f, 0.0f, 1.0f})};
  desc.meshes = {box_on_the_last_bone()};
  desc.nodes = {test::camera_at(kCameraAt)};
  SkinDesc skin;
  for (int bone = 0; bone < kBones; ++bone) {
    NodeDesc node;
    node.name = "Bone" + std::to_string(bone);
    desc.nodes.push_back(node);
    skin.joints.push_back(static_cast<int>(kFirstBoneNode) + bone);
    skin.inverse_bind.push_back({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
  }
  desc.skins = {skin};
  NodeDesc body;
  body.name = "Body";
  body.mesh = 0;
  body.skin = 0;
  desc.nodes.push_back(body);
  AnimationChannel swing{static_cast<int>(kFirstBoneNode) + kLastBone,
                         AnimationPath::Translation,
                         {0.0f, kSwingSeconds},
                         {0, 0, 0, kSwing, 0, 0}};
  desc.animations = {AnimationDesc{kSwingClip, kSwingSeconds, {swing}}};
  return desc;
}

// A camera and a row of three objects sharing one mesh, each tinted its own color.
SceneDesc row_of_copies() {
  SceneDesc desc;
  desc.meshes = {cube(kNoMaterial)};
  desc.nodes = {test::camera_at(kCameraAt)};
  for (std::size_t i = 0; i < kTints.size(); ++i) {
    NodeDesc copy;
    copy.name = "Copy" + std::to_string(i);
    copy.mesh = 0;
    copy.local.position = {(static_cast<float>(i) - 1.0f) * kSpacing, 0.0f, 0.0f};
    desc.nodes.push_back(copy);
  }
  return desc;
}

Picture posed_at(RenderStage& stage, const SceneDesc& desc, float seconds, const std::string& name) {
  Scene scene = Scene::from_desc(desc);
  AnimationPlayer player;
  player.reset(desc.animations, scene);
  player.play(kSwingClip, false, 1.0f, false);  // once: its end holds instead of wrapping to its start
  player.seek(kSwingClip, seconds);
  player.advance(0.0f, scene);
  return stage.draw(scene, name);
}

}  // namespace

TEST_CASE("a skeleton of more bones than shader uniforms ever held is posed by every one of them") {
  RenderStage stage;
  const SceneDesc desc = skeleton_of_many_bones();
  stage.upload(desc);
  const Picture::Found at_rest = posed_at(stage, desc, 0.0f, "many_bones_at_rest").find(red);
  const Picture::Found swung = posed_at(stage, desc, kSwingSeconds, "many_bones_swung").find(red);
  REQUIRE(at_rest.count > kEnoughPixels);
  REQUIRE(swung.count > kEnoughPixels);
  CHECK(at_rest.across == doctest::Approx(kMiddle).epsilon(kNearTheMiddle));
  CHECK(swung.across > kFarRight);  // bone 70 carried the box over to the right
}

TEST_CASE("copies of one mesh, drawn together, are each where their object is and in its own tint") {
  RenderStage stage;
  const SceneDesc desc = row_of_copies();
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  for (std::size_t i = 0; i < kTints.size(); ++i) scene.at(static_cast<ObjectId>(i + 1)).tint = kTints[i];
  const Picture picture = stage.draw(scene, "tinted_copies");
  CHECK(brightest(picture.at(kAcross[0], kAtEyeHeight)) == kBrightRed);
  CHECK(brightest(picture.at(kAcross[1], kAtEyeHeight)) == kBrightGreen);
  CHECK(brightest(picture.at(kAcross[2], kAtEyeHeight)) == kBrightBlue);
}

TEST_CASE("scattered copies are drawn where they were placed, and are gone once removed") {
  RenderStage stage;
  SceneDesc desc;
  desc.materials = {glowing({0.0f, 1.0f, 0.0f, 1.0f})};
  desc.meshes = {cube(0)};
  desc.nodes = {test::camera_at(kCameraAt)};
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  ScatterSet set;
  set.mesh = 0;
  for (const float x : {kSpacing * kHalf, kSpacing})
    set.places.push_back(redgem::Transform{{x, 0.0f, 0.0f}, {}, Vec3::one()});
  const int id = scene.scattered().add(set);
  const Picture::Found drawn = stage.draw(scene, "scattered_copies").find(green);
  CHECK(drawn.count > kEnoughPixels);
  CHECK(drawn.across > kMiddle);  // both stand right of the middle
  scene.scattered().remove(id);
  CHECK(stage.draw(scene, "scattered_copies_removed").find(green).count == 0);
}
