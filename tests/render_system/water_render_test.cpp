// Water must move, transmit the actual bottom, absorb by depth and respect opaque occlusion.
#include <doctest/doctest.h>
#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

#include "render/shader_source.hpp"
#include "render/shaders/lit_water.hpp"
#include "render/shaders/water_filter.hpp"
#include "render/shaders/water_waves.hpp"
#include "render/water_ripple_packet.hpp"
#include "render_system/render_stage.hpp"
#include "scene/shapes/primitives.hpp"

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

std::size_t brighter_pixels(const Picture& picture, const Picture& baseline) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < picture.pixels.size(); ++i)
    if (picture.pixels[i].r > baseline.pixels[i].r) ++count;
  return count;
}

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
MeshDesc shore_bottom(float slope, float shore = 0.0f) {
  MeshDesc mesh = plane_mesh(1);
  mesh.bounds = Bounds{};
  auto& positions = mesh.primitives.front().positions;
  for (std::size_t i = 0; i < positions.size(); i += kCoordinates) {
    positions[i + 1] = -slope * (positions[i] * kWaterSize - shore);
    mesh.bounds.include(vec3_from(&positions[i]));
  }
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
struct FoamShadowContrast {
  int lit = 0;
  int difference = 0;
};

FoamShadowContrast foam_shadow_contrast(const Picture& shadow, const Picture& lit) {
  constexpr int kProbeCount = 9;
  constexpr float kProbeStart = 0.46f;  // central water region beneath the roof, clear of its direct silhouette
  constexpr float kProbeStride = 0.01f;
  FoamShadowContrast contrast;
  for (int row = 0; row < kProbeCount; ++row) {
    for (int column = 0; column < kProbeCount; ++column) {
      const float x = kProbeStart + static_cast<float>(column) * kProbeStride;
      const float y = kProbeStart + static_cast<float>(row) * kProbeStride;
      contrast.lit = std::max(contrast.lit, static_cast<int>(lit.at(x, y).r));
      contrast.difference = std::max(contrast.difference, static_cast<int>(lit.at(x, y).r) - shadow.at(x, y).r);
    }
  }
  return contrast;
}

SceneDesc foam_shadow_pond() {
  SceneDesc desc = pond();
  desc.materials[0].water.wave_height = 0.0f;
  constexpr float kRoofShore = -0.3f;
  constexpr float kRoofSlope = 0.5f;
  constexpr float kFoamReach = 10.0f;
  desc.meshes[1] = shore_bottom(kRoofSlope, kRoofShore);
  desc.nodes[1].local.position.y = 0.0f;
  desc.materials[0].water.foam_width = kFoamReach;
  desc.materials[1] = test::glowing_material(kBlackMaterial, 0.0f);  // isolate the foam's incident light
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

// These controls remove transmission, scattering, probes and direct light: red in water is SSR alone.
constexpr float kOpticalWaterExtent = 40.0f;
constexpr float kOpticalAbsorption = 10000.0f;
constexpr float kOpticalEmitterGlow = 10.0f;
constexpr float kOpticalGrazingHeight = 0.35f;
constexpr float kOpticalCameraDistance = 6.0f;
constexpr float kOpticalCameraHeight = 4.0f;
constexpr float kOpticalFov = 50.0f;
constexpr float kOpticalEmitterAlong = -1.0f;
constexpr float kOpticalSubmergedHeight = -0.35f;
constexpr float kOpticalExposedHeight = 0.6f;
constexpr Vec3 kOpticalEmitterSize{0.5f, 0.25f, 0.5f};
constexpr float kOpticalWaveHeight = 0.008f;  // matches the low waves of the impact study
constexpr float kOpticalWavelength = 2.3f;
constexpr float kOpticalSmoothness = 0.065f;
constexpr float kOpticalRoughness = 0.55f;
constexpr float kOpticalMaskMargin = 1.0f;  // exclude the water mesh's rasterized boundary
constexpr int kOpticalRedThreshold = 20;
constexpr std::size_t kOpticalMinimumPixels = 1000;
constexpr std::size_t kOpticalMinimumReflection = 10;
constexpr redgem::Color kOpticalEmitterColor{1.0f, 0.0f, 0.0f, 1.0f};

struct OpticalWaterCase {
  float eye_height = kOpticalCameraHeight;
  float wave_height = 0.0f;
  float roughness = kOpticalSmoothness;
  const char* picture = "water_ssr_submerged_flat";
  Vec3 emitter_size = kOpticalEmitterSize;
};

struct OpticalWaterPixels {
  std::size_t water = 0;
  std::size_t red = 0;
};

Camera3D optical_camera(const OpticalWaterCase& config) {
  return {{0.0f, config.eye_height, kOpticalCameraDistance}, {}, {0.0f, 1.0f, 0.0f}, kOpticalFov, CAMERA_PERSPECTIVE};
}

BoundingBox optical_emitter_box(float height, const OpticalWaterCase& config) {
  const Vec3 center{0.0f, height, kOpticalEmitterAlong};
  const Vec3 half = config.emitter_size * kMiddle;
  return {{center.x - half.x, center.y - half.y, center.z - half.z},
          {center.x + half.x, center.y + half.y, center.z + half.z}};
}

void add_optical_emitter(SceneDesc& desc, float height, const OpticalWaterCase& config) {
  desc.materials.push_back(redgem::test::glowing_material(kOpticalEmitterColor, kOpticalEmitterGlow));
  desc.meshes.push_back(redgem::test::cube_mesh(kRockAsset));
  NodeDesc emitter;
  emitter.name = "Isolated reflection emitter";
  emitter.mesh = kRockAsset;
  emitter.local.position = {0.0f, height, kOpticalEmitterAlong};
  emitter.local.scale = config.emitter_size;
  desc.nodes.push_back(emitter);
}

SceneDesc optical_water_scene(const OpticalWaterCase& config, float emitter_height) {
  SceneDesc desc = clear_dark_pond();
  desc.nodes[kEnvironmentNode].props["msaa"] = 0.0;  // isolate shader sampling from multisample coverage
  desc.nodes[kEnvironmentNode].props["antialias"] = std::string("none");
  desc.materials[0].water.absorption = Vec3::one() * kOpticalAbsorption;
  desc.materials[0].water.wave_height = config.wave_height;
  desc.materials[0].water.wavelength = kOpticalWavelength;
  desc.materials[0].roughness = config.roughness;
  desc.materials[1] = redgem::test::glowing_material(kBlackMaterial, 0.0f);
  desc.nodes[0].local.scale = {kOpticalWaterExtent / kPrimitivePlaneSide, 1.0f,
                               kOpticalWaterExtent / kPrimitivePlaneSide};
  desc.nodes[2] = redgem::test::camera_at({0.0f, config.eye_height, kOpticalCameraDistance});
  desc.nodes[2].camera->fov_y_radians = degrees_to_radians(kOpticalFov);
  add_optical_emitter(desc, emitter_height, config);
  return desc;
}

bool inside_optical_water(Vector3 point) {
  const float limit = kOpticalWaterExtent * kMiddle - kOpticalMaskMargin;
  return std::fabs(point.x) < limit && std::fabs(point.z) < limit;
}

bool unobstructed_optical_water(const Ray& ray, const BoundingBox& emitter) {
  if (ray.direction.y >= 0.0f) return false;
  const float distance = -ray.position.y / ray.direction.y;
  const Vector3 point{ray.position.x + ray.direction.x * distance, 0.0f, ray.position.z + ray.direction.z * distance};
  if (!inside_optical_water(point)) return false;
  const auto hit = GetRayCollisionBox(ray, emitter);
  return !hit.hit || hit.distance >= distance;
}

bool optical_red(::Color pixel) {
  return pixel.r > pixel.g + kOpticalRedThreshold && pixel.r > pixel.b + kOpticalRedThreshold;
}

bool optical_water_pixel(Vector2 center, const Camera3D& camera, const BoundingBox& emitter, const Picture& picture) {
  const auto ray = GetScreenToWorldRayEx(center, camera, picture.width, picture.height);
  return unobstructed_optical_water(ray, emitter);
}

OpticalWaterPixels optical_pixels(const Picture& picture, const OpticalWaterCase& config, float emitter_height) {
  const Camera3D camera = optical_camera(config);
  const BoundingBox emitter = optical_emitter_box(emitter_height, config);
  OpticalWaterPixels found;
  for (int y = 0; y < picture.height; ++y) {
    for (int x = 0; x < picture.width; ++x) {
      const Vector2 center{static_cast<float>(x) + kMiddle, static_cast<float>(y) + kMiddle};
      if (!optical_water_pixel(center, camera, emitter, picture)) continue;
      ++found.water;
      const auto pixel = picture.pixels[static_cast<std::size_t>(y * picture.width + x)];
      if (optical_red(pixel)) ++found.red;
    }
  }
  return found;
}

constexpr float kOpticalWaterIor = 1.333f;
constexpr float kOpticalTransmissionDepth = -1.6f;
constexpr float kOpticalFloorDepth = -3.0f;
// The non-temporal final pass always uses antialias.hpp, including antialias="none".
// Its maximum support is ceil(SPAN_MAX * FAR_TAP + bilinear half texel), independently of MSAA.
constexpr float kOpticalFxaaSpan = 8.0f;
constexpr float kOpticalFxaaFarTap = 0.5f;
const int kOpticalFootprintRadius = static_cast<int>(std::ceil(kOpticalFxaaSpan * kOpticalFxaaFarTap + kMiddle));

Ray optical_snell_ray(const Ray& camera_ray) {
  const float distance = -camera_ray.position.y / camera_ray.direction.y;
  const Vector3 start{camera_ray.position.x + camera_ray.direction.x * distance, 0.0f,
                      camera_ray.position.z + camera_ray.direction.z * distance};
  const float x = camera_ray.direction.x / kOpticalWaterIor;
  const float z = camera_ray.direction.z / kOpticalWaterIor;
  return {start, {x, -std::sqrt(1.0f - x * x - z * z), z}};
}

bool optical_snell_hit(Vector2 center, const Picture& picture, const OpticalWaterCase& config,
                       const BoundingBox& emitter) {
  const Ray ray = GetScreenToWorldRayEx(center, optical_camera(config), picture.width, picture.height);
  if (ray.direction.y >= 0.0f) return false;
  const auto direct = GetRayCollisionBox(ray, emitter);
  const float surface_distance = -ray.position.y / ray.direction.y;
  if (direct.hit && direct.distance < surface_distance) return true;
  return GetRayCollisionBox(optical_snell_ray(ray), emitter).hit;
}

bool missed_optical_snell(Vector2 center, const Picture& picture, const OpticalWaterCase& config,
                          const BoundingBox& emitter) {
  if (!optical_water_pixel(center, optical_camera(config), emitter, picture)) return false;
  for (int y = -kOpticalFootprintRadius; y <= kOpticalFootprintRadius; ++y) {
    for (int x = -kOpticalFootprintRadius; x <= kOpticalFootprintRadius; ++x) {
      const Vector2 neighbor{center.x + x, center.y + y};
      if (optical_snell_hit(neighbor, picture, config, emitter)) return false;
    }
  }
  return true;
}

std::size_t unexpected_transmitted_red(const Picture& picture, const OpticalWaterCase& config, float height) {
  const auto emitter = optical_emitter_box(height, config);
  std::size_t unexpected = 0;
  for (int y = 0; y < picture.height; ++y) {
    for (int x = 0; x < picture.width; ++x) {
      const auto pixel = picture.pixels[static_cast<std::size_t>(y * picture.width + x)];
      if (!optical_red(pixel)) continue;
      const Vector2 center{static_cast<float>(x) + kMiddle, static_cast<float>(y) + kMiddle};
      if (missed_optical_snell(center, picture, config, emitter)) ++unexpected;
    }
  }
  return unexpected;
}

SceneDesc transmission_only_water(const OpticalWaterCase& config, float height) {
  SceneDesc desc = optical_water_scene(config, height);
  desc.materials[0].roughness =
      1.0f;  // SSR confidence is zero; dark lighting also removes rough environment reflection
  desc.materials[0].water.absorption = Vec3::zero();
  desc.nodes[1].local.position.y = kOpticalFloorDepth;
  return desc;
}

constexpr float kOpticalPartialHeight = -0.1f;
constexpr Vec3 kOpticalShoreRockSize{1.5f, 1.0f, 1.0f};
constexpr Vec3 kOpticalPondAbsorption{0.42f, 0.07f, 0.035f};
constexpr int kOpticalRadianceRounding = 1;  // final 8-bit grade rounding, not a spatial fringe

SceneDesc attenuated_optical_water(const OpticalWaterCase& config) {
  SceneDesc desc = transmission_only_water(config, kOpticalPartialHeight);
  desc.materials[0].water.refraction = kWaterRefraction;
  desc.materials[0].water.absorption = kOpticalPondAbsorption;
  desc.materials[1] = redgem::test::glowing_material({1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
  desc.materials[kRockAsset] = redgem::test::glowing_material(kBlackMaterial, 0.0f);
  return desc;
}

void incline_optical_bottom(SceneDesc& desc) {
  constexpr float kOpticalBottomSlope = 0.1f;  // a gentle continuous bottom, rising ten centimeters per meter
  constexpr float kOpticalBottomReach = 4.0f;  // keep the direct depth snapshot's far edge beyond the water fixture
  desc.nodes[1].local.scale.x *= kOpticalBottomReach;
  desc.nodes[1].local.scale.z *= kOpticalBottomReach;
  auto& mesh = desc.meshes[1];
  mesh.bounds = Bounds{};
  auto& positions = mesh.primitives.front().positions;
  for (std::size_t index = 0; index < positions.size(); index += kCoordinates) {
    positions[index + 1] = positions[index + 2] * desc.nodes[1].local.scale.z * kOpticalBottomSlope;
    mesh.bounds.include(vec3_from(positions.data() + index));
  }
}

int optical_radiance_error(::Color reference, ::Color actual) {
  return std::max({std::abs(static_cast<int>(reference.r) - actual.r),
                   std::abs(static_cast<int>(reference.g) - actual.g),
                   std::abs(static_cast<int>(reference.b) - actual.b)});
}

std::size_t unexpected_transmission_change(const Picture& reference, const Picture& actual,
                                           const OpticalWaterCase& config) {
  const auto emitter = optical_emitter_box(kOpticalPartialHeight, config);
  std::size_t unexpected = 0;
  for (int y = 0; y < actual.height; ++y) {
    for (int x = 0; x < actual.width; ++x) {
      const auto pixel = static_cast<std::size_t>(y * actual.width + x);
      if (optical_radiance_error(reference.pixels[pixel], actual.pixels[pixel]) <= kOpticalRadianceRounding) continue;
      const Vector2 center{static_cast<float>(x) + kMiddle, static_cast<float>(y) + kMiddle};
      if (missed_optical_snell(center, actual, config, emitter)) ++unexpected;
    }
  }
  return unexpected;
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
  constexpr float kNearShore = -0.3f;
  constexpr float kNearSlope = 0.5f;
  desc.meshes[1] = shore_bottom(kNearSlope, kNearShore);
  desc.nodes[1].local.position.y = 0.0f;
  desc.materials[1] = redgem::test::glowing_material({0.0f, 0.0f, 0.0f, 1.0f}, 0.0f);
  const auto plain = draw(stage, desc, "water_without_foam");
  constexpr float kFoamReach = 2.0f;
  desc.materials[0].water.foam_width = kFoamReach;
  const auto foam = draw(stage, desc, "water_foam");
  // Open water pockets can include the center pixel; the band must still brighten visible patches.
  CHECK(brighter_pixels(foam, plain) > kMinimumChangingPixels);
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
  const auto contrast = foam_shadow_contrast(shadow, lit);
  REQUIRE(contrast.lit > kShadowDifference);
  CHECK(contrast.difference > kShadowDifference);
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

namespace {

constexpr Vec3 kFoamBandViewer{0.0f, 8.0f, 0.1f};  // nearly overhead: equal depth stays comparable along the shore
constexpr Vec3 kFoamBandSun{0.8f, -0.6f, 0.0f};    // avoids a direct glint masking the foam's diffuse signal
constexpr float kFoamBandFov = 45.0f;
constexpr float kFoamBandWidth = 1.0f;  // one horizontal meter leaves room for a broad half-meter band
constexpr float kFoamBandSlope = 0.5f;  // bottom drops half a meter for every meter from shore
constexpr float kFoamProbeStride = 0.05f;
constexpr int kFoamNormalProbes = 40;  // two meters from exact contact to open water
constexpr int kFoamAlongProbes = 21;
constexpr float kFoamAlongStride = 0.125f;
constexpr float kFoamAlongMiddle = 1.25f;
constexpr int kFoamVisible = 8;  // byte brightness above the identical scene without foam
constexpr int kFoamIrregularVariation = 15;
constexpr std::size_t kFoamBroadSamples = 10;  // about half a meter of visible coverage
constexpr std::size_t kFoamSoftSamples = 6;    // several samples must be partially covered
constexpr float kFoamSoftLow = 0.15f;
constexpr float kFoamSoftHigh = 0.75f;
constexpr float kFoamContactMaximum = 0.8f;  // contact must fade instead of being the brightest line

MeshDesc foam_band_bottom() { return shore_bottom(kFoamBandSlope); }

SceneDesc foam_band_scene() {
  SceneDesc desc = clear_dark_pond();
  desc.materials[1] = test::glowing_material(kBlackMaterial, 0.0f);
  desc.meshes[1] = foam_band_bottom();
  desc.nodes[1].local.position.y = 0.0f;
  desc.nodes[2] = test::camera_at(kFoamBandViewer);
  desc.nodes[2].camera->fov_y_radians = degrees_to_radians(kFoamBandFov);
  desc.nodes.back().light->intensity = 1.0f;
  desc.nodes.back().local.rotation = quat_look_rotation(kFoamBandSun, Vec3::up());
  return desc;
}

::Color foam_band_probe(const Picture& picture, Vec3 point) {
  Camera3D camera{};
  camera.position = {kFoamBandViewer.x, kFoamBandViewer.y, kFoamBandViewer.z};
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.up = {0.0f, 1.0f, 0.0f};
  camera.fovy = kFoamBandFov;
  camera.projection = CAMERA_PERSPECTIVE;
  const auto pixel = GetWorldToScreenEx({point.x, point.y, point.z}, camera, picture.width, picture.height);
  return picture.at(pixel.x / static_cast<float>(picture.width - 1), pixel.y / static_cast<float>(picture.height - 1));
}

int foam_band_signal(const Picture& foam, const Picture& plain, float x, float z) {
  return static_cast<int>(foam_band_probe(foam, {x, 0.0f, z}).r) - foam_band_probe(plain, {x, 0.0f, z}).r;
}

std::vector<int> foam_band_profile(const Picture& foam, const Picture& plain, float z) {
  std::vector<int> profile;
  for (int i = 0; i < kFoamNormalProbes; ++i)
    profile.push_back(foam_band_signal(foam, plain, static_cast<float>(i) * kFoamProbeStride, z));
  return profile;
}

std::vector<int> mean_foam_band_profile(const Picture& foam, const Picture& plain) {
  std::vector<int> mean(kFoamNormalProbes, 0);
  for (int i = 0; i < kFoamAlongProbes; ++i) {
    const float z = static_cast<float>(i) * kFoamAlongStride - kFoamAlongMiddle;
    const auto profile = foam_band_profile(foam, plain, z);
    std::transform(mean.begin(), mean.end(), profile.begin(), mean.begin(), std::plus<int>());
  }
  // Measure the band's envelope across pores instead of depending on one noise slice.
  for (auto& value : mean) value /= kFoamAlongProbes;
  return mean;
}

bool partially_foamed(int value, int peak) {
  return static_cast<float>(value) > static_cast<float>(peak) * kFoamSoftLow &&
         static_cast<float>(value) < static_cast<float>(peak) * kFoamSoftHigh;
}

std::size_t soft_foam_samples(const std::vector<int>& profile, int peak) {
  return static_cast<std::size_t>(
      std::count_if(profile.begin(), profile.end(), [peak](int value) { return partially_foamed(value, peak); }));
}

}  // namespace

TEST_CASE("shore foam occupies a broad soft band and fades at the exact contact") {
  RenderStage stage;
  SceneDesc desc = foam_band_scene();
  const Picture plain = draw(stage, desc, "water_foam_band_off");
  desc.materials[0].water.foam_width = kFoamBandWidth;
  const Picture foam = draw(stage, desc, "water_foam_band_on");
  const auto profile = mean_foam_band_profile(foam, plain);
  const int peak = *std::max_element(profile.begin(), profile.end());
  REQUIRE(peak > kFoamVisible);
  const auto visible = std::count_if(profile.begin(), profile.end(), [](int value) { return value > kFoamVisible; });
  CHECK(static_cast<std::size_t>(visible) >= kFoamBroadSamples);
  CHECK(soft_foam_samples(profile, peak) >= kFoamSoftSamples);
  CHECK(static_cast<float>(profile.front()) < static_cast<float>(peak) * kFoamContactMaximum);
  CHECK(profile.back() <= kFoamVisible);
}

namespace {

constexpr int kWaveProbeSide = 96;
constexpr ::Color kWaveProbeWhite{255, 255, 255, 255};
constexpr ::Color kWaveProbeBlack{0, 0, 0, 255};
constexpr float kWaveProbeHeight = 0.4f;
constexpr float kWaveProbeLength = 3.0f;
constexpr const char* kWaveProbeMain = R"GLSL(
out vec4 finalColor;
const float PROBE_SIDE = 96.0;
const float PROBE_EXTENT = 24.0;
const float PROBE_STEP = 0.002;
const float PROBE_GRADIENT_TOLERANCE = 0.015;
const float PROBE_BOUND_TOLERANCE = 0.0001;
const float PROBE_VISIBLE_HEIGHT = 0.01;
const float PROBE_CLOCK = 0.7;
void main() {
  vec2 p = (gl_FragCoord.xy / PROBE_SIDE - 0.5) * PROBE_EXTENT;
  vec3 shape = waterShape(p, PROBE_CLOCK);
  float dx = waterShape(p + vec2(PROBE_STEP, 0.0), PROBE_CLOCK).x - waterShape(p - vec2(PROBE_STEP, 0.0), PROBE_CLOCK).x;
  float dz = waterShape(p + vec2(0.0, PROBE_STEP), PROBE_CLOCK).x - waterShape(p - vec2(0.0, PROBE_STEP), PROBE_CLOCK).x;
  vec2 numerical = vec2(dx, dz) / (2.0 * PROBE_STEP);
  bool gradient = length(numerical - shape.yz) <= PROBE_GRADIENT_TOLERANCE * (1.0 + length(shape.yz));
  bool bounded = abs(shape.x) <= waterWaves.x + PROBE_BOUND_TOLERANCE;
  finalColor = vec4(gradient ? 1.0 : 0.0, bounded ? 1.0 : 0.0, abs(shape.x) > PROBE_VISIBLE_HEIGHT ? 1.0 : 0.0, 1.0);
}
)GLSL";

void draw_wave_probe(const Shader& shader, const RenderTexture2D& target) {
  BeginDrawing();
  BeginTextureMode(target);
  ClearBackground(kWaveProbeBlack);
  BeginShaderMode(shader);
  DrawRectangle(0, 0, kWaveProbeSide, kWaveProbeSide, kWaveProbeWhite);
  EndShaderMode();
  EndTextureMode();
  EndDrawing();
}

Picture wave_probe_picture(const RenderTexture2D& target) {
  Image image = LoadImageFromTexture(target.texture);
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  const auto* pixels = static_cast<const ::Color*>(image.data);
  Picture picture{kWaveProbeSide, kWaveProbeSide, {pixels, pixels + kWaveProbeSide * kWaveProbeSide}};
  UnloadImage(image);
  return picture;
}

Picture shader_wave_probe(const std::string& source) {
  const Shader shader = LoadShaderFromMemory(nullptr, shaders::for_this_gpu(source.c_str()).c_str());
  REQUIRE(GetShaderLocation(shader, "waterWaves") >= 0);
  const float parameters[] = {kWaveProbeHeight, kWaveProbeLength, 1.0f};
  SetShaderValue(shader, GetShaderLocation(shader, "waterWaves"), parameters, SHADER_UNIFORM_VEC3);
  const RenderTexture2D target = LoadRenderTexture(kWaveProbeSide, kWaveProbeSide);
  draw_wave_probe(shader, target);
  const Picture picture = wave_probe_picture(target);
  UnloadRenderTexture(target);
  UnloadShader(shader);
  return picture;
}

Picture water_wave_probe(const char* main = kWaveProbeMain) {
  return shader_wave_probe(std::string(shaders::kDesktopVersionLine) + "\n" + shaders::kWaterWaves +
                           shaders::kWaterFilter + main);
}

}  // namespace

TEST_CASE("water spectrum stays bounded and its rendered normals follow the enveloped displacement") {
  RenderStage stage;
  const Picture probe = water_wave_probe();
  constexpr int kPassedChannel = 255;
  constexpr std::size_t kMinimumVisibleSamples = 100;
  CHECK(std::all_of(probe.pixels.begin(), probe.pixels.end(),
                    [](const ::Color& pixel) { return pixel.r == kPassedChannel && pixel.g == kPassedChannel; }));
  CHECK(static_cast<std::size_t>(std::count_if(probe.pixels.begin(), probe.pixels.end(), [](const ::Color& pixel) {
          return pixel.b == kPassedChannel;
        })) > kMinimumVisibleSamples);
}

TEST_CASE("water waves are deterministic when stopped and a zero height remains clock independent") {
  RenderStage stage;
  SceneDesc desc = pond();
  desc.materials[0].water.speed = 0.0f;
  desc.nodes[kEnvironmentNode].props["grain"] = 0.0;
  const Picture stopped = draw(stage, desc, "water_spectrum_stopped");
  const Picture later = draw(stage, desc, "water_spectrum_stopped_later", kLater);
  CHECK(changed(stopped, later) == 0);
  desc.materials[0].water.wave_height = 0.0f;
  desc.materials[0].water.speed = 1.0f;
  const Picture flat = draw(stage, desc, "water_spectrum_flat");
  const Picture flat_later = draw(stage, desc, "water_spectrum_flat_later", kLater);
  CHECK(changed(flat, flat_later) == 0);
}

namespace {

constexpr const char* kWaveFilterProbeMain = R"GLSL(
out vec4 finalColor;
const float FILTER_PROBE_MIDDLE = 48.0;
const float FILTER_PROBE_NEAR_FOOTPRINT = 0.001;
const float FILTER_PROBE_FAR_FOOTPRINT = 4.0;
const float FILTER_PROBE_TOLERANCE = 0.00001;
const float FILTER_PROBE_CLOCK = 0.7;
const float FILTER_PROBE_LATER = 10.0;
void main() {
  bool near = gl_FragCoord.x < FILTER_PROBE_MIDDLE;
  float footprint = near ? FILTER_PROBE_NEAR_FOOTPRINT : FILTER_PROBE_FAR_FOOTPRINT;
  vec2 p = gl_FragCoord.xy * footprint;
  WaterWaveNormal filtered = waterFilteredShape(p, FILTER_PROBE_CLOCK);
  WaterWaveNormal later = waterFilteredShape(p, FILTER_PROBE_LATER);
  vec2 expected = near ? waterShape(p, FILTER_PROBE_CLOCK).yz : vec2(0.0);
  bool slope = length(filtered.slope - expected) < FILTER_PROBE_TOLERANCE;
  bool variance = near ? filtered.variance == 0.0 : filtered.variance > 0.0;
  bool stable = filtered.variance == later.variance;
  finalColor = vec4(slope ? 1.0 : 0.0, variance ? 1.0 : 0.0, stable ? 1.0 : 0.0, 1.0);
}
)GLSL";

}  // namespace

TEST_CASE("water normal filtering preserves nearby detail and transfers distant unresolved slopes to stable variance") {
  RenderStage stage;
  const Picture probe = water_wave_probe(kWaveFilterProbeMain);
  CHECK(std::all_of(probe.pixels.begin(), probe.pixels.end(), [](const ::Color& pixel) {
    return pixel.r == kWaveProbeWhite.r && pixel.g == kWaveProbeWhite.g && pixel.b == kWaveProbeWhite.b;
  }));
}

namespace {

constexpr const char* kMicroEnergyProbeMain = R"GLSL(
out vec4 finalColor;
const float MICRO_PROBE_MIDDLE = 48.0;
const float MICRO_PROBE_NEAR = 0.00001;
const float MICRO_PROBE_FAR = 4.0;
const float MICRO_PROBE_TOLERANCE = 0.00001;
const int MICRO_PROBE_PHASE_SAMPLES = 4;
float referenceMicroEnergy(vec2 p, vec2 direction, float wavelength, float weight) {
  float k = WATER_TAU / wavelength;
  vec3 phase = waterPhase(p, direction, k);
  vec2 gradient = phase.yz / k * weight;
  float energy = 0.0;
  for (int phaseIndex = 0; phaseIndex < MICRO_PROBE_PHASE_SAMPLES; ++phaseIndex) {
    // Carrier mean-square energy is phase independent; integrate a local cycle without large-world cancellation.
    vec2 slope = gradient * cos(WATER_TAU * float(phaseIndex) / float(MICRO_PROBE_PHASE_SAMPLES));
    energy += dot(slope, slope);
  }
  return energy / float(MICRO_PROBE_PHASE_SAMPLES);
}
vec2 referenceMicroSlope(vec2 p, vec2 direction, float wavelength, float weight, float offset) {
  float k = WATER_TAU / wavelength;
  vec3 phase = waterPhase(p, direction, k);
  return phase.yz / k * (cos(phase.x + offset) * weight);
}
vec2 referenceDetailSlope(vec2 p) {
  vec2 slope = vec2(0.0);
  for (int index = 0; index < WATER_DETAIL_COUNT; ++index) {
    vec4 component = WATER_DETAIL_COMPONENTS[index];
    slope += referenceMicroSlope(p, component.xy, component.z, component.w, WATER_DETAIL_PHASES[index]);
  }
  return slope;
}
float referenceDetailEnergy(vec2 p) {
  float energy = 0.0;
  for (int index = 0; index < WATER_DETAIL_COUNT; ++index) {
    vec4 component = WATER_DETAIL_COMPONENTS[index];
    energy += referenceMicroEnergy(p, component.xy, component.z, component.w);
  }
  return energy;
}
void main() {
  bool near = gl_FragCoord.x < MICRO_PROBE_MIDDLE;
  vec2 p = gl_FragCoord.xy * (near ? MICRO_PROBE_NEAR : MICRO_PROBE_FAR);
  fragPosition = vec3(p.x, 0.0, p.y);
  WaterNormal normal = waterNormal();
  WaterWaveNormal macro = waterFilteredShape(p, 0.0);
  float wind = clamp(waterWaves.x / waterWaves.y * WATER_WIND_SCALE, 0.0, WATER_MAX_RIPPLE_SLOPE);
  vec2 slope = waterShape(p, 0.0).yz + referenceDetailSlope(p) * wind;
  vec3 expected = near ? normalize(vec3(-slope.x, 1.0, -slope.y)) : vec3(0.0, 1.0, 0.0);
  float energy = near ? 0.0 : referenceDetailEnergy(p) * wind * wind;
  bool direction = 1.0 - abs(dot(normal.direction, expected)) < MICRO_PROBE_TOLERANCE;
  bool conserved = abs(normal.variance - macro.variance - energy) < MICRO_PROBE_TOLERANCE;
  finalColor = vec4(direction ? 1.0 : 0.0, conserved ? 1.0 : 0.0, 1.0, 1.0);
}
)GLSL";

std::string micro_energy_probe_source() {
  const std::string water = shaders::kLitWater;
  const auto end = water.find("// Bounded secant");
  REQUIRE(end != std::string::npos);
  const char* inputs =
      "\n#define WATER\nconst float MIN_ROUGHNESS = 0.045;\n"
      "const float EPSILON = 0.0001;\nvec3 "
      "fragPosition;\nfloat roughness = 0.07;\n";
  return std::string(shaders::kDesktopVersionLine) + "\n" + shaders::kWaterWaves + shaders::kWaterFilter + inputs +
         water.substr(0, end) + "\n#endif\n" + kMicroEnergyProbeMain;
}

}  // namespace

TEST_CASE("water micro normals preserve resolved detail and conserve the energy of filtered distant waves") {
  RenderStage stage;
  const Picture probe = shader_wave_probe(micro_energy_probe_source());
  const auto bad_direction = std::count_if(probe.pixels.begin(), probe.pixels.end(),
                                           [](const ::Color& pixel) { return pixel.r != kWaveProbeWhite.r; });
  const auto bad_energy = std::count_if(probe.pixels.begin(), probe.pixels.end(),
                                        [](const ::Color& pixel) { return pixel.g != kWaveProbeWhite.g; });
  INFO("Invalid normal pixels: ", bad_direction, "; invalid conserved-energy pixels: ", bad_energy);
  CHECK(std::all_of(probe.pixels.begin(), probe.pixels.end(),
                    [](const ::Color& pixel) { return pixel.r == kWaveProbeWhite.r && pixel.g == kWaveProbeWhite.g; }));
}

namespace {

constexpr int kFoamShelfProbes = 33;
constexpr float kFoamShelfMiddle = 0.6f;
constexpr float kFoamShelfFirstX = 0.1f;
constexpr float kFoamShelfAcrossStride = 0.0125f;
constexpr Vec3 kFoamShelfTarget{0.3f, 0.0f, 0.0f};
constexpr float kFoamShelfStride = 0.0375f;          // 1.2 meters sampled by 33 distinct, physically resolved probes
constexpr Vec3 kFoamShelfViewer{0.3f, 1.6f, 0.02f};  // about six pixels per 3.125 cm cell at 256 pixels
constexpr float kFoamShelfDepth = -0.15f;            // negative control: shallow water above a parallel bottom
constexpr int kFoamPoreTolerance = 2;                // byte difference from the identical scene without foam
constexpr std::size_t kFoamMinimumPores = 100;       // about a tenth of the sampled shallow shelf must expose water
constexpr std::size_t kFoamMinimumPatches = 100;
constexpr float kFoamMinimumTransmission = 0.4f;  // display-space share, conservatively below 28% linear throughput

SceneDesc foam_shelf_scene() {
  SceneDesc desc = foam_band_scene();
  desc.nodes[2] = test::camera_at(kFoamShelfViewer);
  desc.nodes[2].local.rotation = quat_look_rotation(kFoamShelfTarget - kFoamShelfViewer, Vec3::up());
  desc.nodes[2].camera->fov_y_radians = degrees_to_radians(kFoamBandFov);
  return desc;
}

::Color foam_shelf_probe(const Picture& picture, Vec3 point) {
  Camera3D camera{};
  camera.position = {kFoamShelfViewer.x, kFoamShelfViewer.y, kFoamShelfViewer.z};
  camera.target = {kFoamShelfTarget.x, kFoamShelfTarget.y, kFoamShelfTarget.z};
  camera.up = {0.0f, 1.0f, 0.0f};
  camera.fovy = kFoamBandFov;
  camera.projection = CAMERA_PERSPECTIVE;
  const auto pixel = GetWorldToScreenEx({point.x, point.y, point.z}, camera, picture.width, picture.height);
  return picture.at(pixel.x / static_cast<float>(picture.width - 1), pixel.y / static_cast<float>(picture.height - 1));
}

int foam_shelf_signal(const Picture& foam, const Picture& plain, float x, float z) {
  return static_cast<int>(foam_shelf_probe(foam, {x, 0.0f, z}).r) - foam_shelf_probe(plain, {x, 0.0f, z}).r;
}

std::vector<int> foam_shelf_signals(const Picture& foam, const Picture& plain) {
  std::vector<int> signals;
  for (int row = 0; row < kFoamShelfProbes; ++row) {
    const float z = static_cast<float>(row) * kFoamShelfStride - kFoamShelfMiddle;
    for (int column = 0; column < kFoamShelfProbes; ++column) {
      const float x = kFoamShelfFirstX + static_cast<float>(column) * kFoamShelfAcrossStride;
      signals.push_back(foam_shelf_signal(foam, plain, x, z));
    }
  }
  return signals;
}

std::vector<int> foam_shelf_along_shore(const Picture& foam, const Picture& plain) {
  std::vector<int> along;
  for (int i = 0; i < kFoamShelfProbes; ++i) {
    const float z = static_cast<float>(i) * kFoamShelfStride - kFoamShelfMiddle;
    along.push_back(foam_shelf_signal(foam, plain, kFoamShelfTarget.x, z));
  }
  return along;
}

bool foam_shelf_retains_bottom(const Picture& foam, const Picture& plain) {
  for (int row = 0; row < kFoamShelfProbes; ++row) {
    const float z = static_cast<float>(row) * kFoamShelfStride - kFoamShelfMiddle;
    for (int column = 0; column < kFoamShelfProbes; ++column) {
      const float x = kFoamShelfFirstX + static_cast<float>(column) * kFoamShelfAcrossStride;
      const int baseline = foam_shelf_probe(plain, {x, 0.0f, z}).r;
      const int retained = foam_shelf_probe(foam, {x, 0.0f, z}).r;
      if (static_cast<float>(retained) < static_cast<float>(baseline) * kFoamMinimumTransmission) return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("shore foam has world-space irregularity and remains deterministic with waves stopped") {
  RenderStage stage;
  SceneDesc desc = foam_shelf_scene();
  const Picture plain = draw(stage, desc, "water_foam_pattern_off");
  desc.materials[0].water.foam_width = kFoamBandWidth;
  const Picture foam = draw(stage, desc, "water_foam_pattern_on");
  const auto along = foam_shelf_along_shore(foam, plain);
  const auto range = std::minmax_element(along.begin(), along.end());
  CHECK(*range.second - *range.first >= kFoamIrregularVariation);
  const Picture later = draw(stage, desc, "water_foam_pattern_later", kLater);
  CHECK(changed(foam, later) == 0);
  desc.materials[0].water.foam_width = 0.0f;
  const Picture disabled = draw(stage, desc, "water_foam_pattern_disabled");
  CHECK(changed(plain, disabled) == 0);
}

TEST_CASE("shallow shore foam opens water pockets instead of coating an entire flat shelf") {
  RenderStage stage;
  SceneDesc desc = foam_shelf_scene();
  const Picture plain = draw(stage, desc, "water_foam_shelf_off");
  desc.materials[0].water.foam_width = kFoamBandWidth;
  const Picture foam = draw(stage, desc, "water_foam_shelf_porous");
  const auto signals = foam_shelf_signals(foam, plain);
  const auto pores =
      std::count_if(signals.begin(), signals.end(), [](int value) { return std::abs(value) <= kFoamPoreTolerance; });
  const auto patches = std::count_if(signals.begin(), signals.end(), [](int value) { return value > kFoamVisible; });
  CHECK(static_cast<std::size_t>(pores) >= kFoamMinimumPores);
  CHECK(static_cast<std::size_t>(patches) >= kFoamMinimumPatches);
}

TEST_CASE("dense shore foam keeps the submerged surface partly visible") {
  RenderStage stage;
  SceneDesc desc = foam_shelf_scene();
  desc.nodes.back().light->intensity = 0.0f;  // isolate bottom transmission from the foam's incident light
  desc.materials[1] = test::glowing_material(redgem::Color{1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
  const Picture plain = draw(stage, desc, "water_foam_shelf_bottom");
  desc.materials[0].water.foam_width = kFoamBandWidth;
  const Picture foam = draw(stage, desc, "water_foam_shelf_transmission");
  REQUIRE(plain.at(kMiddle, kMiddle).r > kFoamVisible);
  CHECK(foam_shelf_retains_bottom(foam, plain));
}

namespace {

constexpr int kFoamCoverageFineSide = 1024;  // two-meter patch: sixteen samples per fine cell
constexpr int kFoamCoverageCoarseSide = 32;
constexpr int kFoamCoverageMiddleSide = 64;
constexpr float kFoamCoverageExtent = 2.0f;
constexpr float kFoamCoverageByte = 255.0f;
constexpr int kFoamFrequencyRows = 8;
constexpr std::size_t kFoamMinimumCrossings = 100;  // old 31 cm cells cannot provide this many crossings over eight 2 m
                                                    // rows
constexpr double kFoamMeanTolerance = 0.025;  // finite 4096-cell quadrature error plus 8-bit readback quantization
constexpr double kFoamVisibleMean = 0.1;
constexpr const char* kFoamSourceStart = "const vec2 WATER_FOAM_HASH_DIRECTION";
constexpr const char* kFoamSourceEnd = "vec3 waterCompose(";
constexpr const char* kFoamCoverageHead = R"GLSL(#version 330
uniform vec2 waterSurface;
uniform float foamProbeSide;
uniform float foamProbeExtent;
out vec4 finalColor;
vec3 foamProbePosition() {
  vec2 p = (gl_FragCoord.xy / foamProbeSide - 0.5) * foamProbeExtent;
  return vec3(p.x, 0.0, p.y);
}
#define fragPosition foamProbePosition()
)GLSL";
constexpr const char* kFoamCoverageMain = R"GLSL(
void main() {
  const float PROBE_SHORE_DISTANCE = 0.15;
  vec2 p = foamProbePosition().xz;
  float footprint = max(length(dFdx(p)), length(dFdy(p)));
  float coverage = waterFoamCoverage(PROBE_SHORE_DISTANCE, footprint);
  finalColor = vec4(vec3(coverage), 1.0);
}
)GLSL";

std::string foam_coverage_source() {
  const std::string water = shaders::kLitWater;
  const auto begin = water.find(kFoamSourceStart);
  const auto end = water.find(kFoamSourceEnd, begin);
  REQUIRE(begin != std::string::npos);
  REQUIRE(end != std::string::npos);
  return std::string(kFoamCoverageHead) + water.substr(begin, end - begin) + kFoamCoverageMain;
}

void draw_foam_coverage(const Shader& shader, const RenderTexture2D& target) {
  BeginDrawing();
  BeginTextureMode(target);
  ClearBackground(kWaveProbeBlack);
  BeginShaderMode(shader);
  DrawRectangle(0, 0, target.texture.width, target.texture.height, kWaveProbeWhite);
  EndShaderMode();
  EndTextureMode();
  EndDrawing();
}

Picture foam_coverage_pixels(const RenderTexture2D& target) {
  Image image = LoadImageFromTexture(target.texture);
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  const auto* pixels = static_cast<const ::Color*>(image.data);
  const auto count = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
  Picture picture{image.width, image.height, {pixels, pixels + count}};
  UnloadImage(image);
  return picture;
}

Picture foam_coverage_probe(int side) {
  const std::string source = foam_coverage_source();
  const Shader shader = LoadShaderFromMemory(nullptr, shaders::for_this_gpu(source.c_str()).c_str());
  REQUIRE(GetShaderLocation(shader, "foamProbeSide") >= 0);
  const float resolution = static_cast<float>(side);
  const float surface[] = {kFoamBandWidth, 0.0f};
  SetShaderValue(shader, GetShaderLocation(shader, "foamProbeSide"), &resolution, SHADER_UNIFORM_FLOAT);
  SetShaderValue(shader, GetShaderLocation(shader, "foamProbeExtent"), &kFoamCoverageExtent, SHADER_UNIFORM_FLOAT);
  SetShaderValue(shader, GetShaderLocation(shader, "waterSurface"), surface, SHADER_UNIFORM_VEC2);
  const RenderTexture2D target = LoadRenderTexture(side, side);
  draw_foam_coverage(shader, target);
  const Picture picture = foam_coverage_pixels(target);
  UnloadRenderTexture(target);
  UnloadShader(shader);
  return picture;
}

double mean_foam_coverage(const Picture& picture) {
  double total = 0.0;
  for (const auto& pixel : picture.pixels) total += pixel.r;
  return total / (static_cast<double>(picture.pixels.size()) * kFoamCoverageByte);
}

std::size_t foam_row_crossings(const Picture& picture, int row) {
  const auto first = static_cast<std::size_t>(row * picture.width);
  double threshold = 0.0;
  for (int x = 0; x < picture.width; ++x) threshold += picture.pixels[first + static_cast<std::size_t>(x)].r;
  threshold /= picture.width;
  std::size_t count = 0;
  for (int x = 1; x < picture.width; ++x) {
    const auto pixel = first + static_cast<std::size_t>(x);
    const bool before = picture.pixels[pixel - 1].r > threshold;
    const bool after = picture.pixels[pixel].r > threshold;
    if (before != after) ++count;
  }
  return count;
}

std::size_t fine_foam_crossings(const Picture& picture) {
  std::size_t count = 0;
  const int stride = picture.height / kFoamFrequencyRows;
  for (int row = 0; row < kFoamFrequencyRows; ++row) count += foam_row_crossings(picture, row * stride + stride / 2);
  return count;
}

}  // namespace

TEST_CASE(
    "nearby shore foam resolves centimeter pores instead of broad white "
    "clouds") {
  RenderStage stage;
  const Picture picture = foam_coverage_probe(kFoamCoverageFineSide);
  REQUIRE(mean_foam_coverage(picture) > kFoamVisibleMean);
  const auto crossings = fine_foam_crossings(picture);
  INFO("Resolved crossings over eight two-meter rows: ", crossings);
  CHECK(crossings >= kFoamMinimumCrossings);
}

TEST_CASE("unresolved shore foam preserves the area mean of a resolved quadrature") {
  RenderStage stage;
  const double reference = mean_foam_coverage(foam_coverage_probe(kFoamCoverageFineSide));
  REQUIRE(reference > kFoamVisibleMean);
  for (const int side : {kFoamCoverageCoarseSide, kFoamCoverageMiddleSide}) {
    const double distant = mean_foam_coverage(foam_coverage_probe(side));
    INFO("Probe resolution: ", side, "; resolved mean: ", reference, "; filtered mean: ", distant);
    CHECK(std::abs(distant - reference) <= kFoamMeanTolerance);
  }
}

namespace {

constexpr Vec3 kShoreTiltedViewer{3.0f, 6.0f, 0.1f};
constexpr float kShoreShallowSlope = 0.15f;
constexpr float kShoreSteepSlope = 1.2f;
constexpr int kShoreInteriorProbe = 10;      // half a meter from shore, inside the one-meter band
constexpr int kShoreExteriorProbe = 25;      // 1.25 meters is outside the maximum 1.15-meter reach
constexpr float kShoreEdgeTolerance = 0.1f;  // two world-space probes, allowing projected depth/pixel quantization

::Color shore_probe_from(const Picture& picture, Vec3 point, Vec3 viewer) {
  Camera3D camera{};
  camera.position = {viewer.x, viewer.y, viewer.z};
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.up = {0.0f, 1.0f, 0.0f};
  camera.fovy = kFoamBandFov;
  camera.projection = CAMERA_PERSPECTIVE;
  const auto pixel = GetWorldToScreenEx({point.x, point.y, point.z}, camera, picture.width, picture.height);
  return picture.at(pixel.x / static_cast<float>(picture.width - 1), pixel.y / static_cast<float>(picture.height - 1));
}

std::vector<int> shore_distance_profile(const Picture& foam, const Picture& plain, Vec3 viewer,
                                        float water_slope = 0.0f) {
  std::vector<int> profile(kFoamNormalProbes, 0);
  for (int x = 0; x < kFoamNormalProbes; ++x) {
    for (int z = 0; z < kFoamAlongProbes; ++z) {
      const Vec3 point{static_cast<float>(x) * kFoamProbeStride, static_cast<float>(x) * kFoamProbeStride * water_slope,
                       static_cast<float>(z) * kFoamAlongStride - kFoamAlongMiddle};
      profile[static_cast<std::size_t>(x)] +=
          static_cast<int>(shore_probe_from(foam, point, viewer).r) - shore_probe_from(plain, point, viewer).r;
    }
    profile[static_cast<std::size_t>(x)] /= kFoamAlongProbes;
  }
  return profile;
}

std::vector<int> rendered_shore_profile(RenderStage& stage, float slope, Vec3 viewer) {
  SceneDesc desc = foam_band_scene();
  desc.meshes[1] = shore_bottom(slope);
  desc.nodes[2] = test::camera_at(viewer);
  desc.nodes[2].camera->fov_y_radians = degrees_to_radians(kFoamBandFov);
  const Picture plain = draw(stage, desc, "water_horizontal_shore_off");
  desc.materials[0].water.foam_width = kFoamBandWidth;
  const Picture foam = draw(stage, desc, "water_horizontal_shore_on");
  return shore_distance_profile(foam, plain, viewer);
}

void check_horizontal_shore_support(const std::vector<int>& profile) {
  REQUIRE(profile[static_cast<std::size_t>(kShoreInteriorProbe)] > kFoamVisible);
  for (int i = kShoreExteriorProbe; i < kFoamNormalProbes; ++i)
    CHECK(profile[static_cast<std::size_t>(i)] <= kFoamVisible);
}

constexpr float kShoreWaterSlope = 0.5f;  // y = slope * x: CPU shore distance is exactly abs(x)
constexpr Vec3 kShoreWallNormal{1.0f, 0.0f, 0.0f};

MeshDesc inclined_shore_water() {
  MeshDesc mesh = plane_mesh(0);
  mesh.bounds = Bounds{};
  auto& positions = mesh.primitives.front().positions;
  for (std::size_t i = 0; i < positions.size(); i += kCoordinates) {
    positions[i + 1] = positions[i] * kWaterSize * kShoreWaterSlope;
    mesh.bounds.include(vec3_from(&positions[i]));
  }
  return mesh;
}

MeshDesc vertical_shore_wall() {
  MeshDesc mesh = plane_mesh(1);
  mesh.bounds = Bounds{};
  auto& primitive = mesh.primitives.front();
  for (std::size_t i = 0; i < primitive.positions.size(); i += kCoordinates) {
    primitive.positions[i + 1] = -primitive.positions[i] * kWaterSize;
    primitive.positions[i] = 0.0f;
    primitive.normals[i] = kShoreWallNormal.x;
    primitive.normals[i + 1] = kShoreWallNormal.y;
    primitive.normals[i + 2] = kShoreWallNormal.z;
    mesh.bounds.include(vec3_from(&primitive.positions[i]));
  }
  return mesh;
}

std::vector<int> geometry_shore_profile(RenderStage& stage, SceneDesc desc, float water_slope) {
  desc.nodes[2] = test::camera_at(kShoreTiltedViewer);
  desc.nodes[2].camera->fov_y_radians = degrees_to_radians(kFoamBandFov);
  const Picture plain = draw(stage, desc, "water_geometry_shore_off");
  desc.materials[0].water.foam_width = kFoamBandWidth;
  const Picture foam = draw(stage, desc, "water_geometry_shore_on");
  return shore_distance_profile(foam, plain, kShoreTiltedViewer, water_slope);
}

float visible_shore_edge(const std::vector<int>& profile) {
  float edge = 0.0f;
  for (std::size_t i = 0; i < profile.size(); ++i)
    if (profile[i] > kFoamVisible) edge = static_cast<float>(i) * kFoamProbeStride;
  return edge;
}

}  // namespace

TEST_CASE("a flat shallow plateau without a local shore receives no shore foam") {
  RenderStage stage;
  for (const Vec3 viewer : {kFoamBandViewer, kShoreTiltedViewer}) {
    SceneDesc desc = foam_band_scene();
    desc.meshes[1] = plane_mesh(1);
    desc.nodes[1].local.position.y = kFoamShelfDepth;
    desc.nodes[2] = test::camera_at(viewer);
    desc.nodes[2].camera->fov_y_radians = degrees_to_radians(kFoamBandFov);
    const Picture plain = draw(stage, desc, "water_shallow_plateau_off");
    desc.materials[0].water.foam_width = kFoamBandWidth;
    const Picture foam = draw(stage, desc, "water_shallow_plateau_on");
    CHECK(changed(plain, foam) == 0);
  }
}

TEST_CASE("shore foam width is horizontal and stays stable with bottom slope and camera tilt") {
  RenderStage stage;
  const auto reference = rendered_shore_profile(stage, kFoamBandSlope, kFoamBandViewer);
  check_horizontal_shore_support(reference);
  const float reference_edge = visible_shore_edge(reference);
  for (const float slope : {kShoreShallowSlope, kShoreSteepSlope}) {
    for (const Vec3 viewer : {kFoamBandViewer, kShoreTiltedViewer}) {
      const auto profile = rendered_shore_profile(stage, slope, viewer);
      INFO("Bottom slope: ", slope, "; camera height: ", viewer.y);
      check_horizontal_shore_support(profile);
      CHECK(std::abs(visible_shore_edge(profile) - reference_edge) <= kShoreEdgeTolerance);
    }
  }
}

TEST_CASE("geometrically inclined water measures horizontal shore distance above a flat bottom") {
  RenderStage stage;
  SceneDesc desc = foam_band_scene();
  desc.meshes[0] = inclined_shore_water();
  desc.meshes[1] = plane_mesh(1);
  REQUIRE(desc.materials[0].water.wave_height == 0.0f);
  const auto profile = geometry_shore_profile(stage, desc, kShoreWaterSlope);
  check_horizontal_shore_support(profile);
}

TEST_CASE("a visible vertical wall establishes a horizontal shore band without a vertical bottom normal") {
  RenderStage stage;
  SceneDesc desc = foam_band_scene();
  desc.meshes[1] = vertical_shore_wall();
  REQUIRE(desc.materials[0].water.wave_height == 0.0f);
  const auto profile = geometry_shore_profile(stage, desc, 0.0f);
  check_horizontal_shore_support(profile);
}

TEST_CASE("water SSR isolation excludes fully submerged emissive geometry") {
  RenderStage stage;
  OpticalWaterCase config;
  SUBCASE("flat surface from above") {}
  SUBCASE("flat surface at grazing view") {
    config.eye_height = kOpticalGrazingHeight;
    config.picture = "water_ssr_submerged_grazing_flat";
  }
  SUBCASE("small waves at grazing view with smooth water") {
    config.eye_height = kOpticalGrazingHeight;
    config.wave_height = kOpticalWaveHeight;
    config.picture = "water_ssr_submerged_grazing_smooth";
  }
  SUBCASE("small waves at grazing view with rough water") {
    config.eye_height = kOpticalGrazingHeight;
    config.wave_height = kOpticalWaveHeight;
    config.roughness = kOpticalRoughness;
    config.picture = "water_ssr_submerged_grazing_rough";
  }
  const auto desc = optical_water_scene(config, kOpticalSubmergedHeight);
  const auto picture = draw(stage, desc, config.picture);
  const auto measured = optical_pixels(picture, config, kOpticalSubmergedHeight);
  INFO("SSR-only red pixels: ", measured.red);
  REQUIRE(measured.water > kOpticalMinimumPixels);
  CHECK(measured.red == 0);
}

TEST_CASE("water SSR isolation preserves the reflection of exposed emissive geometry") {
  RenderStage stage;
  OpticalWaterCase config;
  config.picture = "water_ssr_exposed_flat";
  SUBCASE("flat surface from above") {}
  SUBCASE("flat surface at grazing view") {
    config.eye_height = kOpticalGrazingHeight;
    config.picture = "water_ssr_exposed_grazing_flat";
  }
  const auto desc = optical_water_scene(config, kOpticalExposedHeight);
  const auto picture = draw(stage, desc, config.picture);
  const auto measured = optical_pixels(picture, config, kOpticalExposedHeight);
  INFO("SSR-only reflected red pixels: ", measured.red);
  REQUIRE(measured.water > kOpticalMinimumPixels);
  CHECK(measured.red > kOpticalMinimumReflection);
}

TEST_CASE("water transmission isolation follows a single Snell silhouette instead of retaining an undistorted copy") {
  RenderStage stage;
  const OpticalWaterCase config;
  auto desc = transmission_only_water(config, kOpticalTransmissionDepth);
  const auto straight = draw(stage, desc, "water_transmission_isolated_straight");
  const auto visible = optical_pixels(straight, config, kOpticalTransmissionDepth);
  REQUIRE(visible.red > kOpticalMinimumReflection);
  desc.materials[0].water.refraction = kWaterRefraction;
  const auto refracted = draw(stage, desc, "water_transmission_isolated_snell");
  const auto transmitted = optical_pixels(refracted, config, kOpticalTransmissionDepth);
  REQUIRE(transmitted.red > kOpticalMinimumReflection);
  const auto unexpected = unexpected_transmitted_red(refracted, config, kOpticalTransmissionDepth);
  INFO(
      "Transmission-only red pixels outside the postprocess-supported Snell "
      "silhouette: ",
      unexpected);
  CHECK(unexpected == 0);
}

TEST_CASE("water transmission isolation respects the submerged silhouette of a partially exposed object") {
  RenderStage stage;
  OpticalWaterCase config;
  config.emitter_size = kOpticalShoreRockSize;
  auto desc = transmission_only_water(config, kOpticalPartialHeight);
  desc.materials[0].water.refraction = kWaterRefraction;
  const auto picture = draw(stage, desc, "water_transmission_partial_snell");
  const auto visible = optical_pixels(picture, config, kOpticalPartialHeight);
  REQUIRE(visible.water > kOpticalMinimumPixels);
  REQUIRE(visible.red > kOpticalMinimumReflection);
  CHECK(unexpected_transmitted_red(picture, config, kOpticalPartialHeight) == 0);
}

TEST_CASE("water transmission preserves bottom attenuation outside a partially exposed object's Snell footprint") {
  RenderStage stage;
  OpticalWaterCase config;
  config.emitter_size = kOpticalShoreRockSize;
  auto desc = attenuated_optical_water(config);
  const auto obstructed = draw(stage, desc, "water_transmission_attenuated_partial");
  desc.nodes.pop_back();  // the reference has the identical flat bottom and optical path, without the black object
  const auto reference = draw(stage, desc, "water_transmission_attenuated_reference");
  REQUIRE(reference.at(kMiddle, kMiddle).g > kOpticalRedThreshold);
  const auto unexpected = unexpected_transmission_change(reference, obstructed, config);
  INFO(
      "Changed attenuation pixels outside the direct/Snell silhouette and "
      "postprocess support: ",
      unexpected);
  CHECK(unexpected == 0);
}

TEST_CASE("water transmission preserves an inclined bottom's attenuation outside an occluder's Snell footprint") {
  RenderStage stage;
  OpticalWaterCase config;
  config.emitter_size = kOpticalShoreRockSize;
  auto desc = attenuated_optical_water(config);
  incline_optical_bottom(desc);
  const auto obstructed = draw(stage, desc, "water_transmission_attenuated_inclined_partial");
  desc.nodes.pop_back();
  const auto reference = draw(stage, desc, "water_transmission_attenuated_inclined_reference");
  REQUIRE(reference.at(kMiddle, kMiddle).g > kOpticalRedThreshold);
  CHECK(unexpected_transmission_change(reference, obstructed, config) == 0);
}

namespace {

constexpr int kRefractionProbeTextureSide = 8;
constexpr int kRefractionProbeTargetSide = 1;
constexpr float kRefractionProbeNear = 0.1f;
constexpr float kRefractionProbeFar = 100.0f;
constexpr float kRefractionProbeRayX = 0.3f;
constexpr float kRefractionProbeRootU = 0.58f;
constexpr float kRefractionProbeDepthSlope = 0.3f;  // affine hardware depth: a continuous projected plane
constexpr float kRefractionProbeDepthCeiling = 0.999999f;
constexpr float kRefractionProbeTwice = 2.0f;
constexpr ::Color kRefractionProbeWhite{255, 255, 255, 255};
constexpr ::Color kRefractionProbeBlack{0, 0, 0, 255};
constexpr const char* kRefractionProbeHead = R"GLSL(#version 330
// The rectangle batch owns matProjection; the analytic ray needs its independent perspective matrix.
#define matProjection probeProjection
uniform sampler2D waterDepth;
uniform vec2 waterPlanes;
uniform mat4 matProjection;
const float HALF = 0.5;
const float EPSILON = 0.00001;
)GLSL";
constexpr const char* kRefractionProbeMain = R"GLSL(
out vec4 finalColor;
const float PROBE_RAY_X = 0.3;
const float PROBE_ROOT_U = 0.58;
const float PROBE_ROOT_U_TOLERANCE = 0.0005;
const float PROBE_PREMATURE_STEP = 8.0;  // of WATER_REFRACTION_SEARCH_STEPS: where nearest depth crosses early
bool refractionProbePrematureCrossing(vec3 surface, vec3 ray) {
  WaterRefractionScreenRay screenRay = waterRefractionScreenRay(surface, ray);
  float endpoint = waterRefractionScreenDistance(screenRay, PROBE_PREMATURE_STEP / float(WATER_REFRACTION_SEARCH_STEPS));
  vec2 uv;
  float nearestGap;
  float continuousGap;
  bool sampled = waterRefractionPointSample(surface, ray, endpoint, uv, nearestGap) &&
    waterRefractionSample(surface, ray, endpoint, uv, continuousGap);
  return sampled && nearestGap > 0.0 && continuousGap < -WATER_REFRACTION_THICKNESS;
}
float refractionProbeExpectedDistance(vec3 ray) {
  float coordinate = 2.0 * (PROBE_ROOT_U - HALF);
  return coordinate / (ray.x + ray.z * coordinate);
}
void main() {
  vec3 surface = vec3(0.0, 0.0, -1.0);
  vec3 ray = vec3(PROBE_RAY_X, 0.0, -sqrt(1.0 - PROBE_RAY_X * PROBE_RAY_X));
  float column = waterLinearDepth(texture(waterDepth, vec2(HALF)).r) - 1.0;
  vec3 hit = waterSearchRefraction(surface, ray, column, vec3(HALF, HALF, 1.0));
  bool root = abs(hit.x - PROBE_ROOT_U) < PROBE_ROOT_U_TOLERANCE;
  bool path = abs(hit.z - refractionProbeExpectedDistance(ray)) < WATER_REFRACTION_THICKNESS;
  bool counterexample = refractionProbePrematureCrossing(surface, ray);
  finalColor = vec4(counterexample ? 1.0 : 0.0, root ? 1.0 : 0.0, path ? 1.0 : 0.0, 1.0);
}
)GLSL";

std::string refraction_source_section(const char* begin, const char* end) {
  const std::string source = shaders::kLitWater;
  const auto first = source.find(begin);
  REQUIRE(first != std::string::npos);
  const auto last = source.find(end, first);
  REQUIRE(last != std::string::npos);
  return source.substr(first, last - first);
}

std::string refraction_probe_source() {
  return std::string(kRefractionProbeHead) +
         refraction_source_section("const float WATER_DEPTH_LIMIT", "float waterLinearDepth(") +
         refraction_source_section("float waterLinearDepth(", "struct WaterNormal") +
         refraction_source_section("const int WATER_REFRACTION_STEPS", "vec4 waterReflectedFog(") +
         refraction_source_section("bool waterInsideScreen(", "vec3 waterUv(") + kRefractionProbeMain;
}

float refraction_probe_root_depth() {
  const float coordinate = kRefractionProbeTwice * (kRefractionProbeRootU - kMiddle);
  const float down = std::sqrt(1.0f - kRefractionProbeRayX * kRefractionProbeRayX);
  const float distance = coordinate / (kRefractionProbeRayX - down * coordinate);
  return 1.0f + down * distance;
}

float refraction_probe_hardware_depth(float depth) {
  const float extent = kRefractionProbeFar - kRefractionProbeNear;
  return kRefractionProbeFar / extent - kRefractionProbeFar * kRefractionProbeNear / (extent * depth);
}

// A post in front of the inclined floor, in the texture's columns across this range of u.
constexpr float kRefractionPostFrom = 0.53f;
constexpr float kRefractionPostTo = 0.56f;
constexpr float kRefractionPostDepth = 1.05f;   // view meters: just below the water surface at 1
constexpr int kRefractionPostTextureSide = 64;  // texels fine enough for a post between ray and floor root

bool refraction_post_at(float u) { return u >= kRefractionPostFrom && u < kRefractionPostTo; }

float refraction_probe_texel(float u, bool post) {
  if (post && refraction_post_at(u)) return refraction_probe_hardware_depth(kRefractionPostDepth);
  const float root = refraction_probe_hardware_depth(refraction_probe_root_depth());
  return std::min(root + kRefractionProbeDepthSlope * (u - kRefractionProbeRootU), kRefractionProbeDepthCeiling);
}

Texture2D refraction_probe_depth_texture(int side = kRefractionProbeTextureSide, bool post = false) {
  std::vector<float> pixels(static_cast<std::size_t>(side * side));
  for (std::size_t index = 0; index < pixels.size(); ++index) {
    const float u = (static_cast<float>(index % static_cast<std::size_t>(side)) + kMiddle) / static_cast<float>(side);
    pixels[index] = refraction_probe_texel(u, post);
  }
  const Image image{pixels.data(), side, side, 1, PIXELFORMAT_UNCOMPRESSED_R32};
  Texture2D texture = LoadTextureFromImage(image);
  SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
  SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
  return texture;
}

Matrix refraction_probe_projection() {
  const float extent = kRefractionProbeFar - kRefractionProbeNear;
  Matrix projection{};
  projection.m0 = 1.0f;
  projection.m5 = 1.0f;
  projection.m10 = -(kRefractionProbeFar + kRefractionProbeNear) / extent;
  projection.m11 = -1.0f;
  projection.m14 = -kRefractionProbeTwice * kRefractionProbeFar * kRefractionProbeNear / extent;
  return projection;
}

void configure_refraction_probe(const Shader& shader) {
  const float planes[] = {kRefractionProbeNear, kRefractionProbeFar};
  SetShaderValue(shader, GetShaderLocation(shader, "waterPlanes"), planes, SHADER_UNIFORM_VEC2);
  SetShaderValueMatrix(shader, GetShaderLocation(shader, "probeProjection"), refraction_probe_projection());
}

void draw_refraction_probe(const Shader& shader, const RenderTexture2D& target, const Texture2D& depth) {
  BeginDrawing();
  BeginTextureMode(target);
  ClearBackground(kRefractionProbeBlack);
  BeginShaderMode(shader);
  SetShaderValueTexture(shader, GetShaderLocation(shader, "waterDepth"), depth);
  DrawRectangle(0, 0, kRefractionProbeTargetSide, kRefractionProbeTargetSide, kRefractionProbeWhite);
  EndShaderMode();
  EndTextureMode();
  EndDrawing();
}

::Color refraction_probe_pixel(const RenderTexture2D& target) {
  Image image = LoadImageFromTexture(target.texture);
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  const auto pixel = *static_cast<const ::Color*>(image.data);
  UnloadImage(image);
  return pixel;
}

struct RefractionProbe {
  Shader shader = LoadShaderFromMemory(nullptr, shaders::for_this_gpu(refraction_probe_source().c_str()).c_str());
  Texture2D depth = refraction_probe_depth_texture();
  RenderTexture2D target = LoadRenderTexture(kRefractionProbeTargetSide, kRefractionProbeTargetSide);

  RefractionProbe() { configure_refraction_probe(shader); }
  RefractionProbe(const RefractionProbe&) = delete;
  RefractionProbe& operator=(const RefractionProbe&) = delete;
  ~RefractionProbe() {
    UnloadRenderTexture(target);
    UnloadTexture(depth);
    UnloadShader(shader);
  }
};

}  // namespace

TEST_CASE("refraction recovery brackets the same inclined depth field that it refines") {
  RenderStage stage;
  RefractionProbe probe;
  REQUIRE(GetShaderLocation(probe.shader, "waterDepth") >= 0);
  REQUIRE(GetShaderLocation(probe.shader, "probeProjection") >= 0);
  draw_refraction_probe(probe.shader, probe.target, probe.depth);
  const auto pixel = refraction_probe_pixel(probe.target);
  CHECK(pixel.r == kRefractionProbeWhite.r);  // fixture reproduces the premature nearest crossing
  CHECK(pixel.g == kRefractionProbeWhite.g);  // the actual continuous root, not the undistorted fallback
  CHECK(pixel.b == kRefractionProbeWhite.b);  // optical path keeps the original two-centimeter tolerance
}

namespace {

struct OpticalTransmissionCoverage {
  std::size_t expected = 0;
  std::size_t missing = 0;
};

bool interior_optical_snell(Vector2 center, const Picture& picture, const OpticalWaterCase& config,
                            const BoundingBox& emitter) {
  if (!optical_snell_hit(center, picture, config, emitter)) return false;
  for (int y = -kOpticalFootprintRadius; y <= kOpticalFootprintRadius; ++y) {
    for (int x = -kOpticalFootprintRadius; x <= kOpticalFootprintRadius; ++x) {
      if (!optical_snell_hit({center.x + x, center.y + y}, picture, config, emitter)) return false;
    }
  }
  return true;
}

OpticalTransmissionCoverage optical_transmission_coverage(const Picture& picture, const OpticalWaterCase& config) {
  const auto emitter = optical_emitter_box(kOpticalTransmissionDepth, config);
  OpticalTransmissionCoverage coverage;
  for (int y = 0; y < picture.height; ++y) {
    for (int x = 0; x < picture.width; ++x) {
      const Vector2 center{static_cast<float>(x) + kMiddle, static_cast<float>(y) + kMiddle};
      if (!interior_optical_snell(center, picture, config, emitter)) continue;
      ++coverage.expected;
      const auto index = static_cast<std::size_t>(y * picture.width + x);
      if (!optical_red(picture.pixels[index])) ++coverage.missing;
    }
  }
  return coverage;
}

}  // namespace

TEST_CASE("water transmission covers the supported interior of a submerged object's Snell silhouette") {
  RenderStage stage;
  OpticalWaterCase config;
  config.emitter_size = kOpticalShoreRockSize;
  auto desc = transmission_only_water(config, kOpticalTransmissionDepth);
  desc.materials[0].water.refraction = kWaterRefraction;
  const auto picture = draw(stage, desc, "water_transmission_snell_interior_coverage");
  const auto coverage = optical_transmission_coverage(picture, config);
  INFO("Expected interior transmission pixels: ", coverage.expected, "; missing: ", coverage.missing);
  REQUIRE(coverage.expected > kOpticalMinimumReflection);
  CHECK(coverage.missing == 0);
}

namespace {

constexpr const char* kDetailGradientProbeMain = R"GLSL(
out vec4 finalColor;
const vec4 DETAIL_PROBE_CLOCKS = vec4(0.7, 0.0, 1.1, 1.7);
const vec2 DETAIL_PROBE_ORIGIN_A = vec2(0.12, -0.27);
const vec2 DETAIL_PROBE_ORIGIN_B = vec2(-0.31, 0.18);
const vec2 DETAIL_PROBE_ORIGIN_C = vec2(0.45, 0.22);
const vec2 DETAIL_PROBE_ORIGIN_D = vec2(-0.22, -0.44);
const float DETAIL_PROBE_MIDDLE = 48.0;
const float DETAIL_REFERENCE_GRAVITY = 9.81;
const float DETAIL_REFERENCE_BEND = 0.8;
const float DETAIL_REFERENCE_BEND_FREQUENCY = 0.31;
const float DETAIL_REFERENCE_CLOCK_SCALE = 0.4;
const float DETAIL_PROBE_FOOTPRINT = 0.00001;
const float DETAIL_PROBE_DIFFERENCE = 0.0002;
// Central differences: float32 height subtraction plus O((k*h)^2) truncation at k<=210/m.
const float DETAIL_PROBE_GRADIENT_ERROR = 0.0005;
const float DETAIL_SLOPE_ENERGY_TARGET = 0.30;
const float DETAIL_ENERGY_ERROR = 0.00001;
const float DETAIL_MAX_ENERGY_SHARE = 0.125;
const float DETAIL_MIN_EFFECTIVE_COMPONENTS = 8.0;
const float DETAIL_MIN_BAND_SHARE = 0.12;
const float DETAIL_MIN_WAVELENGTH_RATIO = 12.0;
const float DETAIL_MIN_TRANSVERSE_SHARE = 0.15;
const vec3 DETAIL_BAND_BOUNDARIES = vec3(0.24, 0.12, 0.06);
float independentDetailHeight(vec2 p, float clock) {
  float height = 0.0;
  for (int index = 0; index < WATER_DETAIL_COUNT; ++index) {
    vec4 c = WATER_DETAIL_COMPONENTS[index];
    float k = WATER_TAU / c.z;
    float bend = (-c.y * p.x + c.x * p.y) * k * DETAIL_REFERENCE_BEND_FREQUENCY;
    float phase = (c.x * p.x + c.y * p.y) * k + DETAIL_REFERENCE_BEND * sin(bend) + WATER_DETAIL_PHASES[index];
    phase -= sqrt(DETAIL_REFERENCE_GRAVITY * k) * clock * waterWaves.z * DETAIL_REFERENCE_CLOCK_SCALE;
    height += c.w / k * sin(phase);
  }
  return height;
}
vec2 independentDetailGradient(vec2 p, float clock) {
  vec2 dx = vec2(DETAIL_PROBE_DIFFERENCE, 0.0);
  vec2 dy = dx.yx;
  float x = independentDetailHeight(p + dx, clock) - independentDetailHeight(p - dx, clock);
  float y = independentDetailHeight(p + dy, clock) - independentDetailHeight(p - dy, clock);
  return vec2(x, y) / (2.0 * DETAIL_PROBE_DIFFERENCE);
}
int detailEnergyBand(float wavelength) {
  if (wavelength >= DETAIL_BAND_BOUNDARIES.x) return 0;
  if (wavelength >= DETAIL_BAND_BOUNDARIES.y) return 1;
  return wavelength >= DETAIL_BAND_BOUNDARIES.z ? 2 : 3;
}
struct DetailSpectrum {
  vec4 bands;
  vec3 energy;  // sum of squares, sum of fourth powers, largest squared coefficient
  vec2 wavelength;  // shortest, longest
};
DetailSpectrum detailSpectrum() {
  DetailSpectrum spectrum = DetailSpectrum(vec4(0.0), vec3(0.0), vec2(1.0, 0.0));
  for (int index = 0; index < WATER_DETAIL_COUNT; ++index) {
    vec4 c = WATER_DETAIL_COMPONENTS[index];
    float energy = c.w * c.w;
    spectrum.energy.x += energy;
    spectrum.energy.y += energy * energy;
    spectrum.energy.z = max(spectrum.energy.z, energy);
    spectrum.bands[detailEnergyBand(c.z)] += energy;
    spectrum.wavelength = vec2(min(spectrum.wavelength.x, c.z), max(spectrum.wavelength.y, c.z));
  }
  return spectrum;
}
bool detailEnergyIsDistributed(vec3 energy) {
  bool dominanceBounded = energy.z / energy.x <= DETAIL_MAX_ENERGY_SHARE;
  bool participationBroad = energy.x * energy.x / energy.y >= DETAIL_MIN_EFFECTIVE_COMPONENTS;
  return dominanceBounded && participationBroad;
}
bool detailSpectrumIsBroad() {
  DetailSpectrum spectrum = detailSpectrum();
  bool normalized = abs(spectrum.energy.x - DETAIL_SLOPE_ENERGY_TARGET) < DETAIL_ENERGY_ERROR;
  bool distributed = detailEnergyIsDistributed(spectrum.energy);
  bool bandsCovered = all(greaterThanEqual(spectrum.bands / spectrum.energy.x, vec4(DETAIL_MIN_BAND_SHARE)));
  bool wavelengthsBroad = spectrum.wavelength.y / spectrum.wavelength.x >= DETAIL_MIN_WAVELENGTH_RATIO;
  return all(bvec4(normalized, distributed, bandsCovered, wavelengthsBroad));
}
bool detailDirectionsAreBroad() {
  vec3 moment = vec3(0.0);
  for (int index = 0; index < WATER_DETAIL_COUNT; ++index) {
    vec4 c = WATER_DETAIL_COMPONENTS[index];
    moment += c.w * c.w * vec3(c.x * c.x, c.x * c.y, c.y * c.y);
  }
  float trace = moment.x + moment.z;
  float discriminant = sqrt((moment.x - moment.z) * (moment.x - moment.z) + 4.0 * moment.y * moment.y);
  return (trace - discriminant) * 0.5 / trace >= DETAIL_MIN_TRANSVERSE_SHARE;
}
bool detailGradientIntegratesAt(vec2 p, float clock) {
  WaterWaveNormal detail = waterFilteredDetailShape(p, clock);
  bool integrable = length(detail.slope - independentDetailGradient(p, clock)) < DETAIL_PROBE_GRADIENT_ERROR;
  bool resolved = detail.variance == 0.0;
  return integrable && resolved;
}
bool detailGradientsIntegrate(vec2 offset) {
  bool first = detailGradientIntegratesAt(DETAIL_PROBE_ORIGIN_A + offset, DETAIL_PROBE_CLOCKS.x);
  bool second = detailGradientIntegratesAt(DETAIL_PROBE_ORIGIN_B + offset, DETAIL_PROBE_CLOCKS.y);
  bool third = detailGradientIntegratesAt(DETAIL_PROBE_ORIGIN_C + offset, DETAIL_PROBE_CLOCKS.z);
  bool fourth = detailGradientIntegratesAt(DETAIL_PROBE_ORIGIN_D + offset, DETAIL_PROBE_CLOCKS.w);
  return all(bvec4(first, second, third, fourth));
}
void main() {
  vec2 offset = (gl_FragCoord.xy - vec2(DETAIL_PROBE_MIDDLE)) * DETAIL_PROBE_FOOTPRINT;
  finalColor = vec4(detailGradientsIntegrate(offset) ? 1.0 : 0.0, detailSpectrumIsBroad() ? 1.0 : 0.0,
    detailDirectionsAreBroad() ? 1.0 : 0.0, 1.0);
}
)GLSL";

bool detail_probe_passed(const ::Color& pixel) {
  const bool gradient = pixel.r == kWaveProbeWhite.r;
  const bool spectrum = pixel.g == kWaveProbeWhite.g;
  const bool direction = pixel.b == kWaveProbeWhite.b;
  return gradient && spectrum && direction;
}

}  // namespace

TEST_CASE("water micro slopes integrate a height field and distribute energy across wavelengths and directions") {
  RenderStage stage;
  const Picture probe = water_wave_probe(kDetailGradientProbeMain);
  const auto bad_gradient = std::count_if(probe.pixels.begin(), probe.pixels.end(),
                                          [](const ::Color& pixel) { return pixel.r != kWaveProbeWhite.r; });
  INFO("Invalid finite-difference gradient pixels: ", bad_gradient);
  CHECK(std::all_of(probe.pixels.begin(), probe.pixels.end(), detail_probe_passed));
}

namespace {

constexpr const char* kWaveSlopeProbeMain = R"GLSL(
out vec4 finalColor;
const float PROBE_SPACING = 0.37;  // meters between samples
const float PROBE_QUAD = 0.5;      // a 2x2 block of pixels shares a sample: no phase change across it
const float PROBE_CLOCK = 0.7;
const float PROBE_TOLERANCE = 0.00001;
void main() {
  vec2 p = floor(gl_FragCoord.xy * PROBE_QUAD) * PROBE_SPACING;
  WaterWaveNormal filtered = waterFilteredShape(p, PROBE_CLOCK);
  bool same = length(filtered.slope - waterShape(p, PROBE_CLOCK).yz) <= PROBE_TOLERANCE;
  finalColor = vec4(same ? 1.0 : 0.0, filtered.variance == 0.0 ? 1.0 : 0.0, 1.0, 1.0);
}
)GLSL";

bool wave_slope_probe_passed(const ::Color& pixel) {
  return pixel.r == kWaveProbeWhite.r && pixel.g == kWaveProbeWhite.g;
}

}  // namespace

TEST_CASE("resolved water normals are the gradient of the displacement itself") {
  RenderStage stage;
  const Picture probe = water_wave_probe(kWaveSlopeProbeMain);
  CHECK(std::all_of(probe.pixels.begin(), probe.pixels.end(), wave_slope_probe_passed));
}

namespace {

constexpr int kFoamReachProbeSide = 1;
constexpr float kFoamReachWidth = 0.6f;  // meters: the authored nominal shore width
constexpr float kFoamReachPitch = 0.5f;  // radians the camera looks down: the column is not the view ray
constexpr const char* kFoamReachHead = R"GLSL(#version 330
// The rectangle batch owns matView; the probe needs a camera that looks down.
#define matView probeView
uniform vec2 waterSurface;
uniform mat4 matView;
)GLSL";
constexpr const char* kFoamReachMain = R"GLSL(
out vec4 finalColor;
const float PROBE_MARGIN = 0.01;     // meters either side of the deepest column foam can show over
const float PROBE_BEACH = 0.15;      // a gentle beach: its bottom drops this much a meter out
const float PROBE_NEAR_SHORE = 0.5;  // meters from the waterline: well inside the foam band
const float PROBE_FAR_SIGHT = 30.0;  // meters out along the beach a grazing view sees the bottom
const vec3 PROBE_SURFACE = vec3(0.0, 0.0, -5.0);
vec3 toView(vec3 world) { return mat3(matView) * world; }
void main() {
  float bound = waterSurface.x * WATER_FOAM_MAX_REACH * WATER_FOAM_STEEPEST_SHORE;
  vec3 up = toView(vec3(0.0, 1.0, 0.0));
  bool shallow = !waterFoamOutOfReach(PROBE_SURFACE, PROBE_SURFACE - up * (bound - PROBE_MARGIN), up);
  bool deep = waterFoamOutOfReach(PROBE_SURFACE, PROBE_SURFACE - up * (bound + PROBE_MARGIN), up);
  vec3 normal = normalize(toView(vec3(PROBE_BEACH, 1.0, 0.0)));
  vec3 seaward = normalize(toView(vec3(1.0, -PROBE_BEACH, 0.0)));
  vec3 below = PROBE_SURFACE - up * (PROBE_BEACH * PROBE_NEAR_SHORE);
  bool grazing = !waterFoamOutOfReach(PROBE_SURFACE, below + seaward * PROBE_FAR_SIGHT, normal);
  finalColor = vec4(shallow ? 1.0 : 0.0, deep ? 1.0 : 0.0, grazing ? 1.0 : 0.0, 1.0);
}
)GLSL";

std::string foam_reach_source() {
  return std::string(kFoamReachHead) +
         refraction_source_section("const float WATER_FOAM_MIN_REACH", "const float WATER_FOAM_PORE_START") +
         refraction_source_section("const float WATER_FOAM_MIN_SURFACE_UP", "bool waterFoamSurfaceSlope(") +
         refraction_source_section("const float WATER_FOAM_STEEPEST_SHORE", "bool waterFoamBottomKnown(") +
         kFoamReachMain;
}

::Color foam_reach_pixel() {
  const Shader shader = LoadShaderFromMemory(nullptr, shaders::for_this_gpu(foam_reach_source().c_str()).c_str());
  REQUIRE(GetShaderLocation(shader, "probeView") >= 0);
  const float surface[] = {kFoamReachWidth, 1.0f};
  SetShaderValue(shader, GetShaderLocation(shader, "waterSurface"), surface, SHADER_UNIFORM_VEC2);
  SetShaderValueMatrix(shader, GetShaderLocation(shader, "probeView"), MatrixRotateX(kFoamReachPitch));
  const RenderTexture2D target = LoadRenderTexture(kFoamReachProbeSide, kFoamReachProbeSide);
  draw_foam_coverage(shader, target);
  const ::Color pixel = refraction_probe_pixel(target);
  UnloadRenderTexture(target);
  UnloadShader(shader);
  return pixel;
}

}  // namespace

TEST_CASE("shore foam skips its estimate only where the water column is deeper than foam can show") {
  RenderStage stage;
  const ::Color pixel = foam_reach_pixel();
  CHECK(pixel.r == kRefractionProbeWhite.r);  // just shallower than the bound: estimated
  CHECK(pixel.g == kRefractionProbeWhite.g);  // just deeper: skipped
  CHECK(pixel.b == kRefractionProbeWhite.b);  // near a gentle beach's waterline, seen far out: still estimated
}

TEST_CASE("refraction recovery looks past a post in front of the bottom instead of giving up at its edge") {
  RenderStage stage;
  RefractionProbe probe;
  UnloadTexture(probe.depth);
  probe.depth = refraction_probe_depth_texture(kRefractionPostTextureSide, true);
  draw_refraction_probe(probe.shader, probe.target, probe.depth);
  const auto pixel = refraction_probe_pixel(probe.target);
  CHECK(pixel.g == kRefractionProbeWhite.g);  // the floor's root behind the post, not the undistorted fallback
  CHECK(pixel.b == kRefractionProbeWhite.b);
}
