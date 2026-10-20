// System test: what makes an outdoor scene look real, drawn in a real window: a sky picture that
// lights the scene, scattered copies that fade out instead of popping, and soft sun shadows.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "render_system/render_stage.hpp"
#include "scene/shapes/primitives.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr int kSkyWide = 64;  // pixels of the test's sky picture
constexpr int kSkyHigh = 32;
constexpr std::uint8_t kBright = 128;       // a mantissa of 128 ...
constexpr std::uint8_t kOneExponent = 129;  // ... under this exponent is 1.0
constexpr Vec3 kCameraAt{0.0f, 0.0f, 10.0f};
constexpr float kTop = 0.05f;  // of the picture, down from its top: the sky behind everything
constexpr float kMiddle = 0.5f;
constexpr double kHalfTurn = 180.0;  // degrees
constexpr int kFadeCopies = 40;
constexpr float kFadeEnd = 12.0f;     // meters from the camera the copies are drawn to
constexpr float kFade = 6.0f;         // meters they fade over before that
constexpr float kRowDistance = 9.0f;  // meters from the camera: half faded
constexpr float kRowSpacing = 0.3f;
constexpr float kFewest = 0.25f;  // share of the copies' pixels a half faded row keeps, at least ...
constexpr float kMost = 0.75f;    // ... and at most
constexpr Vec3 kAboveTheGround{0.0f, 12.0f, 1.0f};
constexpr Vec3 kPole{2.0f, 0.0f, 0.0f};
constexpr Vec3 kPoleSize{2.0f, 3.0f, 0.4f};  // a wall: wide enough to keep a dark middle under a wide sun
constexpr double kSoftSun = 6.0;             // degrees across
constexpr double kCascades = 4.0;
constexpr int kDark = 250;  // the sum of a pixel's channels in full shadow is below this ...
constexpr int kLit = 550;   // ... and in full sun above this
constexpr float kWiderEdges = 1.5f;
constexpr std::size_t kSomeShadow = 100;

bool reddish(::Color c) { return c.r > c.b; }
// How green a pixel is over the bluish grey behind the copies.
float greenness(::Color c) { return std::max(static_cast<float>(c.g) - static_cast<float>(c.b), 0.0f); }

float total_greenness(const Picture& picture) {
  float total = 0.0f;
  for (const ::Color c : picture.pixels) total += greenness(c);
  return total;
}
int luma(::Color c) { return c.r + c.g + c.b; }
bool in_shadow(::Color c) { return luma(c) < kDark; }
bool on_an_edge(::Color c) { return luma(c) >= kDark && luma(c) <= kLit; }

// A Radiance picture whose left half is red and right half blue, flat (not run-length encoded).
void write_sky(const std::filesystem::path& file) {
  std::ofstream out(file, std::ios::binary);
  out << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y " << kSkyHigh << " +X " << kSkyWide << "\n";
  for (int y = 0; y < kSkyHigh; ++y) {
    for (int x = 0; x < kSkyWide; ++x) {
      const bool left = x < kSkyWide / 2;
      const char pixel[] = {static_cast<char>(left ? kBright : 0), 0, static_cast<char>(left ? 0 : kBright),
                            static_cast<char>(kOneExponent)};
      out.write(pixel, sizeof(pixel));
    }
  }
}

MeshDesc mesh_of(PrimitiveShape shape, int material) {
  MeshDesc mesh{"Shape", {make_primitive(shape)}, {}};
  mesh.primitives[0].material = material;
  for (std::size_t i = 0; i < mesh.primitives[0].positions.size(); i += 3) {
    mesh.bounds.include(vec3_from(&mesh.primitives[0].positions[i]));
  }
  return mesh;
}

NodeDesc environment_of(PropertyMap props) {
  NodeDesc node;
  node.name = "Environment";
  node.props = std::move(props);
  node.props["environment"] = true;
  return node;
}

// A white cube in front of the camera, a sky picture around it, no sun: only the sky lights it.
SceneDesc cube_under_a_picture(double rotation) {
  SceneDesc desc;
  desc.materials = {MaterialDesc{}};
  desc.meshes = {mesh_of(PrimitiveShape::Cube, 0)};
  NodeDesc cube;
  cube.name = "Cube";
  cube.mesh = 0;
  NodeDesc dark_sun;
  dark_sun.name = "Sun";
  dark_sun.light = LightDesc{};
  desc.nodes = {test::camera_at(kCameraAt), cube, dark_sun,
                environment_of({{"sky_image", std::string("sky")}, {"sky_rotation", rotation}, {"ambient", 1.0}})};
  return desc;
}

MaterialDesc glowing_green() {
  MaterialDesc material;
  material.base_color = {0.0f, 1.0f, 0.0f, 1.0f};
  material.emissive = Vec3{0.0f, 2.0f, 0.0f};
  return material;
}

// A row of glowing cubes across the view, kRowDistance from the camera, scattered with `fade`.
Picture fading_row(RenderStage& stage, float fade, const std::string& name) {
  SceneDesc desc;
  desc.materials = {glowing_green()};
  desc.meshes = {mesh_of(PrimitiveShape::Cube, 0)};
  desc.nodes = {test::camera_at(kCameraAt), environment_of({{"fog_density", 0.0}})};
  stage.upload(desc);
  Scene scene = Scene::from_desc(desc);
  ScatterSet set;
  set.mesh = 0;
  set.visible_to = kFadeEnd;
  set.fade = fade;
  for (int i = 0; i < kFadeCopies; ++i) {
    const float x = (static_cast<float>(i) - static_cast<float>(kFadeCopies) * kHalf) * kRowSpacing;
    set.places.push_back(redgem::Transform{{x, 0.0f, kCameraAt.z - kRowDistance}, {}, Vec3::one() * kRowSpacing});
  }
  scene.scattered().add(set);
  return stage.draw(scene, name);
}

// A white ground seen from above, a pole standing on it, the sun casting its shadow.
SceneDesc pole_in_the_sun(PropertyMap environment) {
  SceneDesc desc;
  desc.materials = {MaterialDesc{}};
  desc.meshes = {mesh_of(PrimitiveShape::Plane, 0), mesh_of(PrimitiveShape::Cube, 0)};
  NodeDesc ground;
  ground.name = "Ground";
  ground.mesh = 0;
  NodeDesc pole;
  pole.name = "Pole";
  pole.mesh = 1;
  pole.local.position = kPole + Vec3{0.0f, kPoleSize.y * kHalf, 0.0f};
  pole.local.scale = kPoleSize * kHalf;
  environment["ambient"] = 0.0;
  environment["ambient_occlusion"] = 0.0;
  desc.nodes = {test::camera_at(kAboveTheGround), ground, pole, environment_of(environment)};
  return desc;
}

Picture shadow_of(RenderStage& stage, PropertyMap environment, const std::string& name) {
  const SceneDesc desc = pole_in_the_sun(std::move(environment));
  stage.upload(desc);
  return stage.draw(Scene::from_desc(desc), name);
}

}  // namespace

TEST_CASE("a sky picture is drawn behind everything, turned as asked, and lights what it surrounds") {
  const std::filesystem::path images = std::filesystem::temp_directory_path() / "redgem_render_sky";
  std::filesystem::create_directories(images);
  write_sky(images / "sky.hdr");
  RenderStage stage;
  stage.set_images_dir(images);
  const SceneDesc facing_red = cube_under_a_picture(0.0);
  stage.upload(facing_red);
  const Picture red_side = stage.draw(Scene::from_desc(facing_red), "sky_picture");
  // The camera looks along -Z: a quarter of the way across the picture, its red half.
  CHECK(reddish(red_side.at(kMiddle, kTop)));
  const SceneDesc turned = cube_under_a_picture(kHalfTurn);
  stage.upload(turned);
  const Picture blue_side = stage.draw(Scene::from_desc(turned), "sky_picture_turned");
  CHECK_FALSE(reddish(blue_side.at(kMiddle, kTop)));
  // The cube's face toward the camera is lit by the half of the sky behind the camera.
  CHECK_FALSE(reddish(red_side.at(kMiddle, kMiddle)));
  CHECK(reddish(blue_side.at(kMiddle, kMiddle)));
}

TEST_CASE("scattered copies near the end of their distances fade out a share of their pixels instead of popping") {
  RenderStage stage;
  const float whole = total_greenness(fading_row(stage, 0.0f, "copies_unfaded"));
  const float faded = total_greenness(fading_row(stage, kFade, "copies_half_faded"));
  REQUIRE(whole > 0.0f);
  const float kept = faded / whole;
  CHECK(kept > kFewest);
  CHECK(kept < kMost);
}

TEST_CASE("cascaded shadows of a sun as wide as asked have softer edges than a sharp sun's") {
  RenderStage stage;
  const Picture sharp = shadow_of(stage, {{"shadow_cascades", kCascades}}, "shadow_sharp");
  const Picture soft = shadow_of(stage, {{"shadow_cascades", kCascades}, {"shadow_softness", kSoftSun}}, "shadow_soft");
  REQUIRE(sharp.find(in_shadow).count > kSomeShadow);
  REQUIRE(soft.find(in_shadow).count > kSomeShadow);
  CHECK(static_cast<float>(soft.find(on_an_edge).count) >
        static_cast<float>(sharp.find(on_an_edge).count) * kWiderEdges);
}

namespace {

constexpr float kCoatRoughness = 0.12f;  // a polished coating over a rough red base
constexpr float kBaseRoughness = 0.8f;
constexpr int kCoatHighlightGain = 20;  // visible green-channel increase in the white highlight
constexpr float kCoatGlow = 0.4f;

MaterialDesc coat_material(float weight, float roughness) {
  constexpr redgem::Color kRedPaint{0.6f, 0.02f, 0.01f, 1.0f};
  MaterialDesc material;
  material.base_color = kRedPaint;
  material.roughness = kBaseRoughness;
  material.clearcoat.factor = weight;
  material.clearcoat.roughness = roughness;
  return material;
}

NodeDesc coat_sun() {
  NodeDesc sun;
  sun.name = "Sun";
  sun.light = LightDesc{};
  sun.light->intensity = 1.0f;
  return sun;
}

SceneDesc coated_cube(float weight, float roughness = kCoatRoughness) {
  SceneDesc desc;
  desc.materials = {coat_material(weight, roughness)};
  desc.meshes = {mesh_of(PrimitiveShape::Cube, 0)};
  NodeDesc cube;
  cube.name = "CoatedCube";
  cube.mesh = 0;
  desc.nodes = {test::camera_at(kCameraAt), cube, coat_sun(), environment_of({{"ambient", 0.0}, {"fog_density", 0.0}})};
  return desc;
}

Picture coat_picture(RenderStage& stage, const SceneDesc& desc, const std::string& name) {
  stage.upload(desc);
  return stage.draw(Scene::from_desc(desc), name);
}

bool different_rgb(::Color a, ::Color b) { return a.r != b.r || a.g != b.g || a.b != b.b; }

bool same_pixels(const Picture& a, const Picture& b) {
  if (a.pixels.size() != b.pixels.size()) return false;
  for (std::size_t i = 0; i < a.pixels.size(); ++i) {
    const auto x = a.pixels[i];
    const auto y = b.pixels[i];
    if (different_rgb(x, y)) return false;
  }
  return true;
}

}  // namespace

TEST_CASE("a polished clearcoat adds a white highlight above a rough colored base") {
  RenderStage stage;
  const auto plain = coat_picture(stage, coated_cube(0.0f), "coat_plain");
  const auto coated = coat_picture(stage, coated_cube(1.0f), "coat_polished");
  CHECK(coated.at(kMiddle, kMiddle).g > plain.at(kMiddle, kMiddle).g + kCoatHighlightGain);
}

TEST_CASE("a zero coat factor keeps the original pixels whatever the coating roughness") {
  RenderStage stage;
  const auto polished = coat_picture(stage, coated_cube(0.0f), "coat_disabled_polished");
  const auto rough = coat_picture(stage, coated_cube(0.0f, 1.0f), "coat_disabled_rough");
  CHECK(same_pixels(polished, rough));
}

TEST_CASE("clearcoat attenuates emission rather than adding energy over it") {
  RenderStage stage;
  SceneDesc desc = coated_cube(0.0f);
  desc.nodes[2].light->intensity = 0.0f;
  desc.materials[0].emissive = Vec3::one() * kCoatGlow;
  const auto plain = coat_picture(stage, desc, "coat_emission_plain");
  desc.materials[0].clearcoat.factor = 1.0f;
  const auto coated = coat_picture(stage, desc, "coat_emission_attenuated");
  CHECK(luma(coated.at(kMiddle, kMiddle)) < luma(plain.at(kMiddle, kMiddle)));
}

namespace {

constexpr const char* kPng = ".png";
constexpr unsigned char kFullChannel = 255;
constexpr unsigned char kRoughnessChannel = 64;
constexpr ::Color kNoRed{0, kFullChannel, kFullChannel, kFullChannel};
constexpr ::Color kOnlyGreen{0, kRoughnessChannel, 0, kFullChannel};
constexpr ::Color kSidewaysNormal{kFullChannel, 128, 128, kFullChannel};
constexpr float kMapRoughness = static_cast<float>(kRoughnessChannel) / kFullChannel;

ImageData solid_picture(::Color color) {
  Image image = GenImageColor(1, 1, color);
  int count = 0;
  unsigned char* bytes = ExportImageToMemory(image, kPng, &count);
  ImageData data{{bytes, bytes + count}, kPng};
  MemFree(bytes);
  UnloadImage(image);
  return data;
}

}  // namespace

TEST_CASE("the clearcoat factor uses the linear red channel and zero keeps every base pixel") {
  RenderStage stage;
  SceneDesc desc = coated_cube(0.0f);
  const auto plain = coat_picture(stage, desc, "coat_factor_plain");
  desc.materials[0].clearcoat.factor = 1.0f;
  desc.materials[0].clearcoat.factor_image = 0;
  desc.images = {solid_picture(kNoRed)};
  const auto mapped = coat_picture(stage, desc, "coat_factor_red_zero");
  CHECK(same_pixels(plain, mapped));
}

TEST_CASE("the clearcoat roughness uses its linear green channel") {
  RenderStage stage;
  SceneDesc desc = coated_cube(1.0f, kMapRoughness);
  const auto factor = coat_picture(stage, desc, "coat_roughness_factor");
  desc.materials[0].clearcoat.roughness = 1.0f;
  desc.materials[0].clearcoat.roughness_image = 0;
  desc.images = {solid_picture(kOnlyGreen)};
  const auto mapped = coat_picture(stage, desc, "coat_roughness_green");
  CHECK(same_pixels(factor, mapped));
}

TEST_CASE("the coating normal can turn its highlight without changing the base normal") {
  RenderStage stage;
  SceneDesc desc = coated_cube(1.0f);
  const auto geometric = coat_picture(stage, desc, "coat_normal_geometric");
  desc.materials[0].clearcoat.normal_image = 0;
  desc.images = {solid_picture(kSidewaysNormal)};
  const auto sideways = coat_picture(stage, desc, "coat_normal_sideways");
  CHECK(sideways.at(kMiddle, kMiddle).g < geometric.at(kMiddle, kMiddle).g);
  desc.materials[0].clearcoat.factor = 0.0f;
  const auto disabled = coat_picture(stage, desc, "coat_normal_disabled");
  desc.materials[0].clearcoat.normal_image = kNoIndex;
  const auto absent = coat_picture(stage, desc, "coat_normal_absent");
  CHECK(same_pixels(disabled, absent));
}

TEST_CASE("clearcoat reflects the ambient sky even without a direct light") {
  RenderStage stage;
  SceneDesc desc = coated_cube(0.0f);
  desc.nodes[2].light->intensity = 0.0f;
  desc.nodes[3].props["ambient"] = 1.0;
  desc.materials[0].base_color = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.materials[0].roughness = 1.0f;
  const auto plain = coat_picture(stage, desc, "coat_ambient_plain");
  desc.materials[0].clearcoat.factor = 1.0f;
  const auto coated = coat_picture(stage, desc, "coat_ambient_reflection");
  CHECK(luma(coated.at(kMiddle, kMiddle)) > luma(plain.at(kMiddle, kMiddle)));
}

TEST_CASE("clearcoat also receives highlights from a point light") {
  constexpr float kLampWatts = 100.0f;
  RenderStage stage;
  SceneDesc desc = coated_cube(0.0f);
  constexpr float kLampRange = 20.0f;  // the cube is nine meters from the lamp
  desc.nodes[2].light->intensity = 0.0f;
  NodeDesc lamp;
  lamp.name = "CoatLamp";
  lamp.light = LightDesc{LightType::Point, {}, kLampWatts, kLampRange};
  lamp.local.position = kCameraAt;
  desc.nodes.push_back(lamp);
  const auto plain = coat_picture(stage, desc, "coat_point_plain");
  desc.materials[0].clearcoat.factor = 1.0f;
  const auto coated = coat_picture(stage, desc, "coat_point_reflection");
  CHECK(coated.at(kMiddle, kMiddle).g > plain.at(kMiddle, kMiddle).g);
}
