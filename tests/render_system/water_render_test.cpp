// Water must move, transmit the actual bottom, absorb by depth and respect opaque occlusion.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "render_system/render_stage.hpp"
#include "scene/shapes/primitives.hpp"
#include "render/water_ripple_packet.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;
namespace {
constexpr Vec3 kViewer{0.0f, 4.0f, 6.0f};
constexpr float kWaterSize = 8.0f;
constexpr float kBottomDepth = -0.4f;
constexpr float kMiddle = 0.5f;
constexpr double kLater = 0.7;
constexpr redgem::Color kWaterColor{0.015f, 0.08f, 0.1f, 1.0f};
constexpr float kWaterRoughness = 0.08f;
constexpr int kEnvironmentNode = 3;
constexpr int kRockAsset = 2;
constexpr std::size_t kCoordinates = 3;
constexpr std::size_t kMinimumChangingPixels = 100;
constexpr redgem::Color kBlackMaterial{0.0f, 0.0f, 0.0f, 1.0f};
constexpr const char* kBlackSky = "#000000";
constexpr int kSettlingFrames = 12;  // enough frames to exercise both temporal history targets
constexpr double kFrameSeconds = 1.0 / 60.0;
constexpr int kDarkChannel = 8;  // a silhouette mask, independent of small reflection differences
constexpr float kReflectionFov = 50.0f;
constexpr double kReflectionFog = 0.12;
constexpr float kReflectionRegion = 0.57f;  // below the post's direct silhouette
constexpr BoundingBox kReflectionPostBounds{{-0.5f, 0.5f, -1.5f}, {0.5f, 1.5f, -0.5f}};
constexpr float kSlopedBottomDepth = 0.9f;
constexpr float kBottomSlope = 0.5f;  // bottom height is slope * world Z minus depth
constexpr float kBandNear = -0.9f;
constexpr float kBandFar = -1.5f;

struct ReflectionProbe {
  std::size_t pixel = 0;
  float path = 0.0f;
  int red = 0;
};

NodeDesc downward_sun(float intensity) {
  NodeDesc sun;
  sun.name = "Sun";
  sun.light = LightDesc{};
  sun.light->intensity = intensity;
  sun.local.rotation = quat_look_rotation({0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
  return sun;
}

void darken(SceneDesc& desc) {
  auto& environment = desc.nodes[kEnvironmentNode].props;
  for (const char* color : {"sky_top", "sky_horizon", "ground_color", "fog_color"})
    environment[color] = std::string(kBlackSky);
  for (const char* off : {"ambient", "sun_glow", "grain", "ambient_occlusion"}) environment[off] = 0.0;
  desc.nodes.push_back(downward_sun(0.0f));
}

bool dark_pixel(::Color pixel) { return pixel.r < kDarkChannel && pixel.g < kDarkChannel && pixel.b < kDarkChannel; }

std::size_t changed_silhouette(const Picture& a, const Picture& b) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < a.pixels.size(); ++i)
    if (dark_pixel(a.pixels[i]) != dark_pixel(b.pixels[i])) ++count;
  return count;
}

NodeDesc plane_node(const char* name, int mesh, float height) {
  NodeDesc node;
  node.name = name;
  node.mesh = mesh;
  node.local.position.y = height;
  node.local.scale = {kWaterSize, 1.0f, kWaterSize};
  return node;
}
MeshDesc plane_mesh(int material) {
  MeshDesc mesh{"Water plane", {make_primitive(PrimitiveShape::Plane)}, {}};
  mesh.primitives.front().material = material;
  for (std::size_t i = 0; i < mesh.primitives.front().positions.size(); i += kCoordinates)
    mesh.bounds.include(vec3_from(&mesh.primitives.front().positions[i]));
  return mesh;
}
MaterialDesc pond_material() {
  MaterialDesc water;
  water.base_color = kWaterColor;
  water.roughness = kWaterRoughness;
  water.water.enabled = true;
  water.water.foam_width = 0.0f;
  return water;
}
NodeDesc pond_environment() {
  NodeDesc environment;
  environment.name = "Environment";
  environment.props = {{"environment", true},
                       {"ambient", 1.0},
                       {"shadows", 0.0},
                       {"bloom", 0.0},
                       {"vignette", 0.0},
                       {"fog_density", 0.0},
                       {"sky_top", std::string("#245ba0")},
                       {"sky_horizon", std::string("#ffffff")}};
  return environment;
}
SceneDesc pond() {
  SceneDesc desc;
  desc.materials = {pond_material(), redgem::test::glowing_material({1.0f, 1.0f, 1.0f, 1.0f}, 1.0f)};
  desc.meshes = {plane_mesh(0), plane_mesh(1)};
  desc.nodes = {plane_node("Water", 0, 0.0f), plane_node("Bottom", 1, kBottomDepth), redgem::test::camera_at(kViewer),
                pond_environment()};
  return desc;
}
Picture draw(RenderStage& stage, const SceneDesc& desc, const char* name, double clock = 0.0) {
  stage.set_clock(clock);
  stage.upload(desc);
  return stage.draw(Scene::from_desc(desc), name);
}
std::size_t changed(const Picture& a, const Picture& b) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < a.pixels.size(); ++i) {
    const auto x = a.pixels[i];
    const auto y = b.pixels[i];
    if (x.r != y.r || x.g != y.g || x.b != y.b) ++count;
  }
  return count;
}
ImageData patterned_bottom() {
  constexpr int kSide = 8;
  constexpr int kCheckSize = 2;
  constexpr ::Color kRed{255, 0, 0, 255};
  constexpr ::Color kBlue{0, 0, 255, 255};
  constexpr const char* kPng = ".png";
  Image image = GenImageChecked(kSide, kSide, kCheckSize, kCheckSize, kRed, kBlue);
  int bytes = 0;
  unsigned char* encoded = ExportImageToMemory(image, kPng, &bytes);
  ImageData out{{encoded, encoded + bytes}, kPng};
  MemFree(encoded);
  UnloadImage(image);
  return out;
}

void add_foam_roof(SceneDesc& desc) {
  desc.materials.push_back(redgem::test::glowing_material(kBlackMaterial, 0.0f));
  desc.meshes.push_back(redgem::test::cube_mesh(kRockAsset));
  NodeDesc roof;
  roof.name = "Foam shade";
  roof.mesh = kRockAsset;
  constexpr Vec3 kRoofPosition{0.0f, 2.0f, 0.0f};
  constexpr Vec3 kRoofSize{2.0f, 0.2f, 2.0f};  // clear of the camera's ray to the origin
  roof.local.position = kRoofPosition;
  roof.local.scale = kRoofSize;
  desc.nodes.push_back(roof);
}
SceneDesc foam_shadow_pond() {
  SceneDesc desc = pond();
  desc.materials[0].water.wave_height = 0.0f;
  constexpr float kShallowBottom = -0.01f;
  constexpr float kFoamReach = 10.0f;
  desc.nodes[1].local.position.y = kShallowBottom;
  desc.materials[0].water.foam_width = kFoamReach;
  desc.nodes[kEnvironmentNode].props["ambient"] = 0.0;
  desc.nodes[kEnvironmentNode].props["shadows"] = 1.0;
  desc.nodes.push_back(downward_sun(1.0f));
  add_foam_roof(desc);
  return desc;
}
SceneDesc clear_dark_pond() {
  SceneDesc desc = pond();
  darken(desc);
  desc.materials[0].base_color = kBlackMaterial;
  desc.materials[0].water.wave_height = 0.0f;
  desc.materials[0].water.absorption = Vec3::zero();
  desc.materials[0].water.refraction = 0.0f;
  return desc;
}
SceneDesc foggy_pond() {
  SceneDesc desc = clear_dark_pond();
  desc.materials[1] = redgem::test::glowing_material(kBlackMaterial, 0.0f);
  constexpr double kVisibleFog = 0.15;
  desc.nodes[kEnvironmentNode].props["fog_density"] = kVisibleFog;
  desc.nodes[kEnvironmentNode].props["fog_color"] = std::string("#ff0000");
  return desc;
}
SceneDesc silhouette_pond() {
  SceneDesc desc = clear_dark_pond();
  constexpr float kSilhouetteHeight = 0.4f;
  constexpr float kLongWaves = 40.0f;  // gentle slopes reflect above the emissive bottom
  constexpr float kOpaqueAbsorption = 10.0f;
  constexpr float kSmallPatch = 0.2f;  // a two-meter patch with its edges inside the view
  constexpr float kDeepBottom = -3.0f;
  desc.materials[0].water.wave_height = kSilhouetteHeight;
  desc.materials[0].water.wavelength = kLongWaves;
  desc.materials[0].water.absorption = Vec3::one() * kOpaqueAbsorption;
  desc.nodes[0].local.scale = {kSmallPatch, 1.0f, kSmallPatch};
  desc.nodes[1].local.position.y = kDeepBottom;
  return desc;
}
Picture settled_water(RenderStage& stage, const SceneDesc& desc) {
  stage.upload(desc);
  const Scene scene = Scene::from_desc(desc);
  Picture picture;
  for (int frame = 0; frame < kSettlingFrames; ++frame) {
    stage.set_clock(static_cast<double>(frame) * kFrameSeconds);
    picture = stage.draw(scene, "water_sampled_temporal");
  }
  return picture;
}

SceneDesc fog_reflection_pond() {
  SceneDesc desc = clear_dark_pond();
  desc.materials[1] = redgem::test::glowing_material(kBlackMaterial, 0.0f);
  desc.materials.push_back(redgem::test::glowing_material({1.0f, 0.0f, 0.0f, 1.0f}, 1.0f));
  desc.meshes.push_back(redgem::test::cube_mesh(kRockAsset));
  NodeDesc post;
  post.name = "Fog reflection post";
  post.mesh = kRockAsset;
  constexpr Vec3 kPostPosition{0.0f, 1.0f, -1.0f};
  post.local.position = kPostPosition;
  desc.nodes.push_back(post);
  desc.nodes[2].camera->fov_y_radians = degrees_to_radians(kReflectionFov);
  return desc;
}

// Geometric optics gives the full camera-to-water-to-post path, independently of the SSR march.
float reflected_path(const Picture& picture, std::size_t pixel) {
  const Camera3D camera{{kViewer.x, kViewer.y, kViewer.z}, {}, {0.0f, 1.0f, 0.0f}, kReflectionFov, CAMERA_PERSPECTIVE};
  const auto x = static_cast<int>(pixel % static_cast<std::size_t>(picture.width));
  const auto y = static_cast<int>(pixel / static_cast<std::size_t>(picture.width));
  const Vector2 center{static_cast<float>(x) + kMiddle, static_cast<float>(y) + kMiddle};
  const Ray ray = GetScreenToWorldRayEx(center, camera, picture.width, picture.height);
  if (ray.direction.y >= 0.0f) return 0.0f;
  const float air_path = -ray.position.y / ray.direction.y;
  const Ray reflected{{ray.position.x + ray.direction.x * air_path, 0.0f, ray.position.z + ray.direction.z * air_path},
                      {ray.direction.x, -ray.direction.y, ray.direction.z}};
  const RayCollision hit = GetRayCollisionBox(reflected, kReflectionPostBounds);
  return hit.hit ? air_path + hit.distance : 0.0f;
}

ReflectionProbe reflection_probe(const Picture& picture) {
  ReflectionProbe best;
  const auto first_row = static_cast<std::size_t>(static_cast<float>(picture.height) * kReflectionRegion);
  for (std::size_t pixel = first_row * static_cast<std::size_t>(picture.width); pixel < picture.pixels.size();
       ++pixel) {
    const int red = picture.pixels[pixel].r;
    if (red <= best.red) continue;
    const float path = reflected_path(picture, pixel);
    if (path > 0.0f) best = {pixel, path, red};
  }
  return best;
}

Picture attenuated_reflection(RenderStage& stage, SceneDesc desc, float path) {
  // A dimmer emitter without fog is an independent radiance reference for the known optical path.
  const float transmission = std::exp(-static_cast<float>(kReflectionFog) * path);
  desc.materials[kRockAsset].emissive = desc.materials[kRockAsset].emissive * transmission;
  return draw(stage, desc, "water_reflection_path_reference");
}

MeshDesc sloped_bottom(int material, float far, float near, float lift = 0.0f) {
  MeshDesc mesh = plane_mesh(material);
  mesh.bounds = Bounds{};
  auto& positions = mesh.primitives.front().positions;
  for (std::size_t i = 0; i < positions.size(); i += kCoordinates) {
    const float z = positions[i + 2] > 0.0f ? near : far;
    positions[i + 1] = kBottomSlope * z - kSlopedBottomDepth + lift;
    positions[i + 2] = z;
    mesh.bounds.include(vec3_from(&positions[i]));
  }
  return mesh;
}

SceneDesc tilted_refraction_pond() {
  SceneDesc desc = clear_dark_pond();
  desc.materials[1] = redgem::test::glowing_material({1.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
  desc.materials.push_back(redgem::test::glowing_material({0.0f, 1.0f, 0.0f, 1.0f}, 1.0f));
  constexpr float kFloorExtent = 12.0f;  // includes the much farther unrefracted intersection
  constexpr float kBandLift = 0.003f;    // avoid coincident depth with the red floor
  desc.meshes[1] = sloped_bottom(1, -kFloorExtent, kFloorExtent);
  desc.meshes.push_back(sloped_bottom(kRockAsset, kBandFar, kBandNear, kBandLift));
  desc.nodes[1].local = redgem::Transform{};
  NodeDesc band;
  band.name = "Snell intersection band";
  band.mesh = kRockAsset;
  desc.nodes.push_back(band);
  return desc;
}

float central_snell_hit() {
  constexpr float kWaterIor = 1.333f;
  const Vec3 incident = normalize(Vec3::zero() - kViewer);
  const float along_z = incident.z / kWaterIor;
  const float down = -std::sqrt(1.0f - along_z * along_z);
  const float distance = kSlopedBottomDepth / (kBottomSlope * along_z - down);
  return along_z * distance;
}
constexpr float kPrimitivePlaneSide = 10.0f;  // built-in Plane spans ten meters before its node scale
constexpr float kRippleTestAge = 0.25f;
constexpr float kRippleTestWidth = 0.4f;
constexpr float kRippleTestStrength = 0.12f;

WaterRippleSnapshot ripple_snapshot(const Scene& scene, ObjectId water = 0) {
  WaterRippleSnapshot snapshot;
  snapshot.count = 1;
  snapshot.time = kRippleTestAge;
  snapshot.ripples[0] = {1,
                         scene.handle(water),
                         0,
                         scene.world_transform(water).position,
                         kRippleTestAge,
                         kRippleTestStrength,
                         kRippleTestWidth};
  return snapshot;
}

void normalize_ripple_water(MeshDesc& mesh) {
  mesh.bounds = Bounds{};
  for (auto& primitive : mesh.primitives) {
    for (float& coordinate : primitive.positions) coordinate /= kPrimitivePlaneSide;
    for (std::size_t index = 0; index < primitive.positions.size(); index += kCoordinates) {
      mesh.bounds.include(vec3_from(primitive.positions.data() + index));
    }
  }
}

SceneDesc ripple_pond() {
  SceneDesc desc = pond();
  normalize_ripple_water(desc.meshes[0]);
  desc.materials[0].water.wave_height = 0.0f;
  desc.nodes[kEnvironmentNode].props["grain"] = 0.0;
  return desc;
}

std::size_t changed_right(const Picture& left, const Picture& right) {
  constexpr float kOtherPondRegion = 0.65f;  // right of the contacted lake and its shoreline
  const int first = static_cast<int>(left.width * kOtherPondRegion);
  std::size_t count = 0;
  for (int y = 0; y < left.height; ++y) {
    for (int x = first; x < left.width; ++x) {
      const auto index = static_cast<std::size_t>(y * left.width + x);
      const auto a = left.pixels[index];
      const auto b = right.pixels[index];
      if (a.r != b.r || a.g != b.g || a.b != b.b) ++count;
    }
  }
  return count;
}

constexpr int kRippleGridSide = 24;          // vertices resolve the moving ring in the motion pass
constexpr double kRippleHistoryError = 2.0;  // RGB levels: subpixel jitter may differ at the shoreline

void append_water_tile(PrimitiveDesc& grid, int across, int along) {
  const auto tile = make_primitive(PrimitiveShape::Plane);
  const auto first = static_cast<std::uint32_t>(grid.positions.size() / kCoordinates);
  for (std::size_t index = 0; index < tile.positions.size(); index += kCoordinates) {
    grid.positions.push_back((tile.positions[index] / kPrimitivePlaneSide + across + kMiddle) / kRippleGridSide -
                             kMiddle);
    grid.positions.push_back(0.0f);
    grid.positions.push_back((tile.positions[index + 2] / kPrimitivePlaneSide + along + kMiddle) / kRippleGridSide -
                             kMiddle);
  }
  grid.normals.insert(grid.normals.end(), tile.normals.begin(), tile.normals.end());
  for (const auto index : tile.indices) grid.indices.push_back(first + index);
}

PrimitiveDesc ripple_grid() {
  PrimitiveDesc grid;
  grid.material = 0;
  for (int along = 0; along < kRippleGridSide; ++along) {
    for (int across = 0; across < kRippleGridSide; ++across) append_water_tile(grid, across, along);
  }
  return grid;
}

Picture settled_ripples(RenderStage& stage, const Scene& scene, const WaterRippleSnapshot* snapshot, const char* name,
                        double render_clock) {
  Picture picture;
  for (int frame = 0; frame < kSettlingFrames; ++frame) {
    stage.set_clock(render_clock + static_cast<double>(frame) * kFrameSeconds);
    picture = stage.draw(scene, name, nullptr, snapshot);
  }
  return picture;
}

double ripple_picture_error(const Picture& a, const Picture& b) {
  double error = 0.0;
  for (std::size_t index = 0; index < a.pixels.size(); ++index) {
    error += std::abs(static_cast<int>(a.pixels[index].r) - b.pixels[index].r);
    error += std::abs(static_cast<int>(a.pixels[index].g) - b.pixels[index].g);
    error += std::abs(static_cast<int>(a.pixels[index].b) - b.pixels[index].b);
  }
  return error / static_cast<double>(a.pixels.size() * kCoordinates);
}

}  // namespace

TEST_CASE("water ripples change reflections with time while a still surface is stable") {
  RenderStage stage;
  SceneDesc desc = pond();
  const auto first = draw(stage, desc, "water_waves_start");
  const auto later = draw(stage, desc, "water_waves_later", kLater);
  CHECK(changed(first, later) > kMinimumChangingPixels);
  desc.materials[0].water.wave_height = 0.0f;
  const auto still = draw(stage, desc, "water_still_start");
  const auto still_later = draw(stage, desc, "water_still_later", kLater);
  CHECK(changed(still, still_later) == 0);
}
TEST_CASE("water absorption removes more red light through a deeper column") {
  RenderStage stage;
  SceneDesc desc = pond();
  desc.materials[0].water.wave_height = 0.0f;
  desc.materials[0].water.absorption = {1.0f, 0.0f, 0.0f};
  const auto shallow = draw(stage, desc, "water_shallow");
  constexpr float kDeepBottom = -3.0f;
  desc.nodes[1].local.position.y = kDeepBottom;
  const auto deep = draw(stage, desc, "water_deep");
  CHECK(deep.at(kMiddle, kMiddle).r < shallow.at(kMiddle, kMiddle).r);
  CHECK(deep.at(kMiddle, kMiddle).g == shallow.at(kMiddle, kMiddle).g);
}
TEST_CASE("water shore foam brightens a shallow edge and remains optional") {
  RenderStage stage;
  SceneDesc desc = pond();
  desc.materials[0].water.wave_height = 0.0f;
  desc.materials[1] = redgem::test::glowing_material({0.0f, 0.0f, 0.0f, 1.0f}, 0.0f);
  const auto plain = draw(stage, desc, "water_without_foam");
  constexpr float kFoamReach = 2.0f;
  desc.materials[0].water.foam_width = kFoamReach;
  const auto foam = draw(stage, desc, "water_foam");
  CHECK(foam.at(kMiddle, kMiddle).r > plain.at(kMiddle, kMiddle).r);
}
TEST_CASE("an opaque object above water keeps its pixels when refraction changes") {
  RenderStage stage;
  SceneDesc desc = pond();
  desc.materials.push_back(redgem::test::glowing_material({1.0f, 0.0f, 0.0f, 1.0f}, 1.0f));
  desc.meshes.push_back(redgem::test::cube_mesh(kRockAsset));
  NodeDesc rock;
  rock.name = "Above water";
  rock.mesh = kRockAsset;
  constexpr Vec3 kAboveWater{0.0f, 0.6f, 0.9f};
  rock.local.position = kAboveWater;
  desc.nodes.push_back(rock);
  const auto first = draw(stage, desc, "water_opaque_foreground");
  constexpr float kOtherRefraction = 0.1f;
  desc.materials[0].water.refraction = kOtherRefraction;
  const auto bent = draw(stage, desc, "water_bent_foreground");
  CHECK(first.at(kMiddle, kMiddle).r > first.at(kMiddle, kMiddle).g);
  CHECK(first.at(kMiddle, kMiddle).r == bent.at(kMiddle, kMiddle).r);
  CHECK(first.at(kMiddle, kMiddle).g == bent.at(kMiddle, kMiddle).g);
}
TEST_CASE("water preserves absorption when the scene uses multiple samples and temporal smoothing") {
  RenderStage stage;
  SceneDesc desc = pond();
  constexpr double kFourSamples = 4.0;
  desc.nodes[kEnvironmentNode].props["msaa"] = kFourSamples;
  desc.materials[0].base_color.a = 0.0f;
  desc.nodes[kEnvironmentNode].props["antialias"] = std::string("temporal");
  desc.materials[0].water.wave_height = 0.0f;
  desc.materials[0].water.absorption = {1.0f, 0.0f, 0.0f};
  const auto picture = settled_water(stage, desc);
  CHECK(picture.at(kMiddle, kMiddle).r < picture.at(kMiddle, kMiddle).g);
}

TEST_CASE("deep water stays dark without incident light") {
  RenderStage stage;
  SceneDesc desc = pond();
  darken(desc);
  desc.materials[0].water.wave_height = 0.0f;
  desc.materials[1] = redgem::test::glowing_material(kBlackMaterial, 0.0f);
  constexpr float kDeepBottom = -3.0f;
  desc.nodes[1].local.position.y = kDeepBottom;
  const auto picture = draw(stage, desc, "water_unlit_deep");
  constexpr int kBlackTolerance = 1;  // an unlit, non-emissive surface must remain black after tone mapping
  const auto pixel = picture.at(kMiddle, kMiddle);
  CHECK(pixel.r <= kBlackTolerance);
  CHECK(pixel.g <= kBlackTolerance);
  CHECK(pixel.b <= kBlackTolerance);
}

TEST_CASE("shore foam receives the shadow of an overhead object") {
  RenderStage stage;
  SceneDesc desc = foam_shadow_pond();
  const auto shadow = draw(stage, desc, "water_foam_shadow");
  desc.nodes.back().props["cast_shadow"] = false;
  const auto lit = draw(stage, desc, "water_foam_sun");
  constexpr int kShadowDifference = 50;
  REQUIRE(lit.at(kMiddle, kMiddle).r > kShadowDifference);
  CHECK(shadow.at(kMiddle, kMiddle).r + kShadowDifference < lit.at(kMiddle, kMiddle).r);
}

TEST_CASE("clear water does not apply atmospheric fog twice to its bottom") {
  RenderStage stage;
  SceneDesc desc = foggy_pond();
  const auto water = draw(stage, desc, "water_fog_transmission");
  desc.nodes[0].mesh = kNoIndex;
  const auto uncovered = draw(stage, desc, "water_fog_uncovered_bottom");
  constexpr int kVisibleRed = 100;
  constexpr int kFogTolerance = 3;  // Fresnel changes the short air path slightly; it must not add a second fog layer
  REQUIRE(uncovered.at(kMiddle, kMiddle).r > kVisibleRed);
  CHECK(std::abs(water.at(kMiddle, kMiddle).r - uncovered.at(kMiddle, kMiddle).r) <= kFogTolerance);
}

TEST_CASE("traveling waves move the water silhouette even without any light") {
  RenderStage stage;
  const SceneDesc desc = silhouette_pond();
  const auto first = draw(stage, desc, "water_geometry_start");
  const auto later = draw(stage, desc, "water_geometry_later", kLater);
  constexpr std::size_t kMinimumMovingEdge = 10;
  REQUIRE(dark_pixel(first.at(kMiddle, kMiddle)));
  CHECK(changed_silhouette(first, later) > kMinimumMovingEdge);
}

TEST_CASE("screen reflection fog follows the reflected optical path instead of fogging its source twice") {
  RenderStage stage;
  SceneDesc desc = fog_reflection_pond();
  const auto clear = draw(stage, desc, "water_reflection_without_fog");
  const ReflectionProbe probe = reflection_probe(clear);
  constexpr int kVisibleReflection = 30;
  REQUIRE(probe.red > kVisibleReflection);
  const auto reference = attenuated_reflection(stage, desc, probe.path);
  desc.nodes[kEnvironmentNode].props["fog_density"] = kReflectionFog;
  const auto fogged = draw(stage, desc, "water_reflection_with_fog");
  constexpr int kFogPathTolerance = 4;  // ray-march thickness and pixel centers slightly perturb the geometric hit
  REQUIRE(reference.pixels[probe.pixel].r < clear.pixels[probe.pixel].r);
  CHECK(std::abs(fogged.pixels[probe.pixel].r - reference.pixels[probe.pixel].r) <= kFogPathTolerance);
}

TEST_CASE("still water refracts a patterned bottom according to its depth") {
  RenderStage stage;
  SceneDesc desc = pond();
  desc.materials[0].water.wave_height = 0.0f;
  desc.materials[0].water.refraction = 0.0f;
  desc.images = {patterned_bottom()};
  desc.materials[1].base_color_image = 0;
  desc.materials[1].emissive_image = 0;
  const auto straight = draw(stage, desc, "water_unrefracted_pattern");
  desc.materials[0].water.refraction = kWaterRefraction;
  const auto bent = draw(stage, desc, "water_refracted_pattern");
  CHECK(changed(straight, bent) > kMinimumChangingPixels);
}

TEST_CASE("Snell refraction intersects an inclined floor instead of projecting its unrefracted depth once") {
  RenderStage stage;
  SceneDesc desc = tilted_refraction_pond();
  REQUIRE(central_snell_hit() > kBandFar);
  REQUIRE(central_snell_hit() < kBandNear);
  const auto straight = draw(stage, desc, "water_sloped_unrefracted");
  desc.materials[0].water.refraction = kWaterRefraction;
  const auto bent = draw(stage, desc, "water_sloped_snell_hit");
  constexpr int kBandContrast = 50;
  REQUIRE(straight.at(kMiddle, kMiddle).r > straight.at(kMiddle, kMiddle).g + kBandContrast);
  CHECK(bent.at(kMiddle, kMiddle).g > bent.at(kMiddle, kMiddle).r + kBandContrast);
}

TEST_CASE("off-screen water refraction keeps the available bottom instead of opening black holes") {
  RenderStage stage;
  SceneDesc desc = clear_dark_pond();
  desc.materials[1] = redgem::test::glowing_material({1.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
  constexpr float kDeepBottom = -3.0f;
  constexpr float kBottomEdge = 0.98f;
  constexpr int kVisibleBottom = 50;
  desc.nodes[1].local.position.y = kDeepBottom;
  const auto straight = draw(stage, desc, "water_screen_edge_straight");
  desc.materials[0].water.refraction = kWaterRefraction;
  const auto bent = draw(stage, desc, "water_screen_edge_fallback");
  REQUIRE(straight.at(kMiddle, kBottomEdge).r > kVisibleBottom);
  CHECK(bent.at(kMiddle, kBottomEdge).r > kVisibleBottom);
}

TEST_CASE("water reflects an on-screen red object below its direct silhouette") {
  RenderStage stage;
  SceneDesc desc = pond();
  desc.materials[0].base_color = {0.0f, 0.0f, 0.0f, 1.0f};
  desc.materials[0].water.wave_height = 0.0f;
  desc.materials[0].water.refraction = 0.0f;
  desc.materials[1] = redgem::test::glowing_material({0.0f, 0.0f, 0.0f, 1.0f}, 0.0f);
  desc.materials.push_back(redgem::test::glowing_material({1.0f, 0.0f, 0.0f, 1.0f}, 1.0f));
  desc.meshes.push_back(redgem::test::cube_mesh(kRockAsset));
  NodeDesc post;
  post.name = "Reflected post";
  post.mesh = kRockAsset;
  constexpr Vec3 kPostPosition{0.0f, 1.0f, -1.0f};
  post.local.position = kPostPosition;
  desc.nodes.push_back(post);
  const auto reflected = draw(stage, desc, "water_screen_reflection");
  std::size_t red = 0;
  constexpr float kBelowMiddle = 0.57f;
  constexpr int kRedMargin = 30;
  for (int y = static_cast<int>(reflected.height * kBelowMiddle); y < reflected.height; ++y) {
    for (int x = 0; x < reflected.width; ++x) {
      const auto pixel = reflected.pixels[static_cast<std::size_t>(y * reflected.width + x)];
      if (pixel.r > pixel.g + kRedMargin) ++red;
    }
  }
  constexpr std::size_t kMinimumReflectionPixels = 10;
  CHECK(red > kMinimumReflectionPixels);
}

TEST_CASE("water impulses follow simulation age and repeated drawing does not advance them") {
  RenderStage stage;
  const SceneDesc desc = ripple_pond();
  const Scene scene = Scene::from_desc(desc);
  auto snapshot = ripple_snapshot(scene);
  stage.upload(desc);
  stage.set_clock(0.0);
  const auto still = stage.draw(scene, "water_impulse_absent");
  const auto first = stage.draw(scene, "water_impulse_first", nullptr, &snapshot);
  stage.set_clock(1.0);  // rendering time advances while the simulation snapshot remains paused
  const auto repeated = stage.draw(scene, "water_impulse_paused", nullptr, &snapshot);
  CHECK(changed(still, first) > kMinimumChangingPixels);
  CHECK(changed(first, repeated) == 0);
  snapshot.ripples[0].age = 1.0f;
  snapshot.time = 1.0;
  const auto later = stage.draw(scene, "water_impulse_later", nullptr, &snapshot);
  CHECK(changed(first, later) > kMinimumChangingPixels);
}

TEST_CASE("shared water mesh and material keep impulses on their owning object") {
  RenderStage stage;
  SceneDesc desc = ripple_pond();
  constexpr float kPondOffset = 2.0f;
  constexpr float kPondWidth = 4.0f;
  desc.nodes[0].local.position.x = -kPondOffset;
  desc.nodes[0].local.scale = {kPondWidth, 1.0f, kWaterSize};
  NodeDesc other = desc.nodes[0];
  other.name = "Other water";
  other.local.position.x = kPondOffset;
  desc.nodes.push_back(other);
  const Scene scene = Scene::from_desc(desc);
  const auto snapshot = ripple_snapshot(scene);
  stage.upload(desc);
  stage.set_clock(0.0);
  const auto before = stage.draw(scene, "water_shared_lakes_still");
  const auto after = stage.draw(scene, "water_shared_lakes_contact", nullptr, &snapshot);
  CHECK(changed(before, after) > kMinimumChangingPixels);
  CHECK(changed_right(before, after) == 0);
}

TEST_CASE("water ripple packets isolate primitive identity and retain the last drawn ages") {
  using namespace render_detail;
  const Scene scene = Scene::from_desc(ripple_pond());
  const auto current = ripple_snapshot(scene);
  auto previous = current;
  previous.ripples[0].age = 0.0f;
  const auto packet = previous_water_ripple_packet(previous, current, scene.handle(0), 0);
  REQUIRE(packet.count == 1);
  CHECK(packet.waves[0].z == 0.0f);
  CHECK(water_ripple_packet(current, scene.handle(0), 1).count == 0);
  CHECK(water_ripple_packet(current, scene.handle(1), 0).count == 0);
  const auto newborn = previous_water_ripple_packet({}, current, scene.handle(0), 0);
  REQUIRE(newborn.count == 1);
  CHECK(newborn.waves[0].z == kRippleTestAge);
  CHECK(water_ripple_bound(current, scene.handle(0)) == kRippleTestStrength);
}

TEST_CASE("shared water material keeps impulses on their original primitive") {
  RenderStage stage;
  SceneDesc desc = ripple_pond();
  constexpr float kHalfWidth = 0.5f;
  constexpr float kHalfOffset = 0.25f;
  auto left = desc.meshes[0].primitives.front();
  auto right = left;
  for (std::size_t index = 0; index < left.positions.size(); index += kCoordinates) {
    left.positions[index] = left.positions[index] * kHalfWidth - kHalfOffset;
    right.positions[index] = right.positions[index] * kHalfWidth + kHalfOffset;
  }
  desc.meshes[0].primitives = {left, right};
  const Scene scene = Scene::from_desc(desc);
  auto snapshot = ripple_snapshot(scene);
  constexpr float kContactX = -2.0f;
  snapshot.ripples[0].origin.x = kContactX;
  stage.upload(desc);
  stage.set_clock(0.0);
  const auto before = stage.draw(scene, "water_shared_primitives_still");
  const auto after = stage.draw(scene, "water_shared_primitives_contact", nullptr, &snapshot);
  CHECK(changed(before, after) > kMinimumChangingPixels);
  CHECK(changed_right(before, after) == 0);
}

TEST_CASE("previous water ripple packets retain expired impulses and bound their union") {
  using namespace render_detail;
  const Scene scene = Scene::from_desc(ripple_pond());
  auto previous = ripple_snapshot(scene);
  auto current = previous;
  previous.count = kMaxWaterRipples;
  current.count = kMaxWaterRipples;
  for (std::size_t index = 0; index < kMaxWaterRipples; ++index) {
    previous.ripples[index] = previous.ripples[0];
    current.ripples[index] = current.ripples[0];
    previous.ripples[index].serial = index + 1;
    current.ripples[index].serial = index + kMaxWaterRipples + 1;
  }
  const auto expired = previous_water_ripple_packet(previous, {}, scene.handle(0), 0);
  CHECK(expired.count == static_cast<int>(kMaxWaterRipples));
  const auto combined = previous_water_ripple_packet(previous, current, scene.handle(0), 0);
  CHECK(combined.count == static_cast<int>(kMaxDrawnWaterRipples));
  CHECK(water_ripple_packet(current, kNoHandle, 0).count == 0);
}

TEST_CASE("empty water snapshots and asset reload restore the unchanged water picture") {
  RenderStage stage;
  const SceneDesc desc = ripple_pond();
  const Scene scene = Scene::from_desc(desc);
  const auto snapshot = ripple_snapshot(scene);
  stage.upload(desc);
  stage.set_clock(0.0);
  const auto baseline = stage.draw(scene, "water_empty_baseline");
  stage.draw(scene, "water_empty_before_expiry", nullptr, &snapshot);
  const WaterRippleSnapshot empty;
  const auto expired = stage.draw(scene, "water_empty_after_expiry", nullptr, &empty);
  CHECK(changed(baseline, expired) == 0);
  stage.upload(desc);
  const auto reloaded = stage.draw(scene, "water_empty_after_reload");
  CHECK(changed(baseline, reloaded) == 0);
}

TEST_CASE("public water snapshots reject nonfinite values and out-of-range impulses before uniform upload") {
  using namespace render_detail;
  const Scene scene = Scene::from_desc(ripple_pond());
  auto snapshot = ripple_snapshot(scene);
  SUBCASE("nonfinite origin") { snapshot.ripples[0].origin.x = std::numeric_limits<float>::infinity(); }
  SUBCASE("nonfinite age") { snapshot.ripples[0].age = std::numeric_limits<float>::quiet_NaN(); }
  SUBCASE("expired age") { snapshot.ripples[0].age = kWaterRippleLifetime + 1.0f; }
  SUBCASE("negative strength") { snapshot.ripples[0].strength = -1.0f; }
  SUBCASE("unbounded strength") { snapshot.ripples[0].strength = kWaterRippleMaxStrength + 1.0f; }
  SUBCASE("unbounded radius") { snapshot.ripples[0].radius = kWaterRippleMaxRadius + 1.0f; }
  SUBCASE("missing owner") { snapshot.ripples[0].surface = kNoHandle; }
  SUBCASE("missing serial") { snapshot.ripples[0].serial = 0; }
  SUBCASE("nonfinite snapshot clock") { snapshot.time = std::numeric_limits<double>::infinity(); }
  CHECK(validated_water_ripples(&snapshot).count == 0);
}

TEST_CASE("temporal water impulses settle without trails after age changes pauses expiry and reset") {
  RenderStage stage;
  SceneDesc desc = ripple_pond();
  desc.nodes[kEnvironmentNode].props["antialias"] = std::string("temporal");
  SUBCASE("single sample") { desc.nodes[kEnvironmentNode].props["msaa"] = 0.0; }
  SUBCASE("four samples") { desc.nodes[kEnvironmentNode].props["msaa"] = 4.0; }
  desc.meshes[0].primitives = {ripple_grid()};
  const Scene scene = Scene::from_desc(desc);
  auto snapshot = ripple_snapshot(scene);
  stage.upload(desc);
  const auto baseline = settled_ripples(stage, scene, nullptr, "water_impulse_taa_baseline", 0.0);
  stage.draw(scene, "water_impulse_taa_contact", nullptr, &snapshot);
  snapshot.ripples[0].age = 1.0f;
  snapshot.time = 1.0;
  const auto aged = settled_ripples(stage, scene, &snapshot, "water_impulse_taa_aged", 1.0);
  constexpr double kPausedRenderClock = 100.0;
  const auto paused = settled_ripples(stage, scene, &snapshot, "water_impulse_taa_paused", kPausedRenderClock);
  CHECK(changed(baseline, aged) > kMinimumChangingPixels);
  CHECK(ripple_picture_error(aged, paused) < kRippleHistoryError);
  CHECK(snapshot.ripples[0].age == 1.0f);
  CHECK(snapshot.time == 1.0);
  stage.upload(desc);
  const auto fresh = settled_ripples(stage, scene, &snapshot, "water_impulse_taa_fresh_pose", kPausedRenderClock);
  CHECK(ripple_picture_error(paused, fresh) < kRippleHistoryError);
  const auto expired = settled_ripples(stage, scene, nullptr, "water_impulse_taa_expired", kPausedRenderClock);
  CHECK(ripple_picture_error(baseline, expired) < kRippleHistoryError);
  stage.upload(desc);
  const auto reset = settled_ripples(stage, scene, nullptr, "water_impulse_taa_reset", kPausedRenderClock);
  CHECK(ripple_picture_error(baseline, reset) < kRippleHistoryError);
  constexpr int kVisibleWater = 20;  // a broken or nonfinite shader must not black out the lit water
  CHECK(reset.at(kMiddle, kMiddle).g > kVisibleWater);
}
