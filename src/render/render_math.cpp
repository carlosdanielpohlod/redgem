#include "render/render_math.hpp"

#include <raymath.h>

#include <cmath>
#include <cstring>

#include "core/config.hpp"

namespace redgem::render_math {
namespace {

constexpr const char* kRendererLine = "Renderer:";
constexpr int kSmallestTarget = 1;      // pixels: a render target cannot be empty
constexpr std::size_t kMatrixSide = 4;  // rows, and columns, of a matrix

float longest_scale(Vec3 scale) { return std::max({std::fabs(scale.x), std::fabs(scale.y), std::fabs(scale.z)}); }

// Half of a box's diagonal: the radius of the sphere around it.
float half_diagonal(const Bounds& bounds) { return length(bounds.max - bounds.min) * kHalf; }

// Where an object is in the world, from the places already worked out for the objects before
// it. (When the parent is not there yet, e.g. after a reparent, ask the scene.)
Transform placed(const Scene& scene, const GameObject& object, const std::vector<Transform>& worlds,
                 std::size_t index) {
  if (object.parent == kNoObject) return object.local;
  const auto parent = static_cast<std::size_t>(object.parent);
  return parent < index ? combine(worlds[parent], object.local) : scene.world_transform(object.id);
}

}  // namespace

// --- Conversions -----------------------------------------------------------------------------

::Color to_raylib(redgem::Color c) {
  auto channel = [](float v) { return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * config::kMaxColorByte); };
  return {channel(c.r), channel(c.g), channel(c.b), channel(c.a)};
}

Vector3 to_raylib(Vec3 v) { return {v.x, v.y, v.z}; }

Vec3 to_vec3(Vector3 v) { return {v.x, v.y, v.z}; }

std::array<float, kColorChannels> color_floats(::Color color) {
  const float full = static_cast<float>(config::kMaxColorByte);
  return {static_cast<float>(color.r) / full, static_cast<float>(color.g) / full, static_cast<float>(color.b) / full,
          static_cast<float>(color.a) / full};
}

Matrix from_column_major(const std::array<float, kMatrixFloats>& m) {
  // A Matrix keeps its numbers row after row (m0, m4, m8, m12, then m1, ...): the array, turned.
  std::array<float, kMatrixFloats> rows{};
  for (std::size_t row = 0; row < kMatrixSide; ++row) {
    for (std::size_t column = 0; column < kMatrixSide; ++column) {
      rows[row * kMatrixSide + column] = m[column * kMatrixSide + row];
    }
  }
  static_assert(sizeof(Matrix) == sizeof(rows), "a Matrix is sixteen floats and nothing else");
  Matrix matrix;
  std::memcpy(&matrix, rows.data(), sizeof(matrix));
  return matrix;
}

Matrix model_matrix(const Transform& t) {
  const Matrix scale = MatrixScale(t.scale.x, t.scale.y, t.scale.z);
  const Matrix rotation = QuaternionToMatrix({t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w});
  const Matrix translation = MatrixTranslate(t.position.x, t.position.y, t.position.z);
  return MatrixMultiply(MatrixMultiply(scale, rotation), translation);
}

// --- Small decisions -------------------------------------------------------------------------

std::optional<std::string> graphics_card_name(const std::string& trace_line) {
  const std::size_t label = trace_line.find(kRendererLine);
  if (label == std::string::npos) return std::nullopt;
  return trace_line.substr(label + std::string(kRendererLine).size());
}

int downscaled(int size, int factor) { return std::max(size / factor, kSmallestTarget); }

// --- Materials -------------------------------------------------------------------------------

AlphaTest alpha_test(const MaterialDesc& material, float wind) {
  const bool textured = material.base_color_image != kNoIndex;
  const bool masked = material.alpha_mode == AlphaMode::Mask;
  const bool cutout = masked || (material.alpha_mode == AlphaMode::Blend && textured);
  AlphaTest test;
  test.cutoff = masked ? material.alpha_cutoff : cutout ? kBlendAsCutoutThreshold : kNoCutoff;
  test.sway = cutout && textured ? wind : kNoWind;
  test.two_sided = material.double_sided || cutout;
  return test;
}

// --- Lights ----------------------------------------------------------------------------------

PointLightUniforms pack_point_lights(const std::vector<PointLight>& lights) {
  PointLightUniforms packed;
  packed.count = static_cast<int>(std::min(lights.size(), static_cast<std::size_t>(shaders::kMaxPointLights)));
  for (int i = 0; i < packed.count; ++i) {
    const PointLight& light = lights[static_cast<std::size_t>(i)];
    packed.positions[i] = to_raylib(light.position);
    packed.colors[i] = to_raylib(light.color);
    packed.ranges[i] = light.range;
  }
  return packed;
}

// --- Where things are ------------------------------------------------------------------------

void world_transforms(const Scene& scene, std::vector<Transform>& worlds) {
  const std::size_t count = scene.size();
  worlds.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    const GameObject& object = scene.at(static_cast<ObjectId>(i));
    if (!object.destroyed) worlds[i] = placed(scene, object, worlds, i);
  }
}

// --- What is worth drawing -------------------------------------------------------------------

BoundingSphere world_sphere(const Bounds& bounds, const Transform& world) {
  const Vec3 center = world.position + rotate(world.rotation, hadamard(bounds.center(), world.scale));
  return {center, half_diagonal(bounds) * longest_scale(world.scale)};
}

BoundingSphere skinned_sphere(const Bounds& bounds, const Matrix& first_bone) {
  const Vector3 moved = Vector3Transform(to_raylib(bounds.center()), first_bone);
  // A scaled rig is bigger: the length of the bone's first axis says by how much.
  const float stretch = kSkinnedBoundsMargin * Vector3Length({first_bone.m0, first_bone.m1, first_bone.m2});
  return {to_vec3(moved), half_diagonal(bounds) * stretch};
}

float smallest_angle_drawn(float asked, bool depth_pass) {
  if (asked > 0.0f) return asked;
  return depth_pass ? kMinShadowAngle : kMinVisibleAngle;
}

bool has_something_to_draw(const GameObject& object, bool show_prefabs, std::size_t mesh_count) {
  if (object.destroyed || object.mesh == kNoIndex || !object.visible) return false;
  if (object.prefab && !show_prefabs) return false;
  return static_cast<std::size_t>(object.mesh) < mesh_count;
}

bool worth_drawing(const View& view, const BoundingSphere& sphere, float from, float to) {
  if (!sphere_in_frustum(view.frustum, sphere.center, sphere.radius)) return false;
  if (too_small_to_see(sphere.center, sphere.radius, view.viewer, view.min_angle)) return false;
  return !outside_distance_range(sphere.center, view.viewer, from, to);
}

// --- Sun shadows -----------------------------------------------------------------------------

float sun_shadow_strength(Vec3 to_sun, float asked) { return to_sun.y > kMinSunHeight ? asked : kNoShadows; }

float snapped_down(float value, float step) { return std::floor(value / step) * step; }

Vector3 shadow_map_vertical(Vector3 sun_axis) {
  return std::fabs(sun_axis.y) > kNearlyVertical ? kForwardAxis : kUpAxis;
}

SunView fit_sun_view(Vector3 camera_position, Vector3 camera_target, Vec3 to_sun) {
  const Vector3 forward = Vector3Normalize(Vector3Subtract(camera_target, camera_position));
  const Vector3 ahead = Vector3Add(camera_position, Vector3Scale(forward, kShadowExtent * kShadowFocusAhead));
  const Vector3 sun_axis = to_raylib(to_sun);
  const Vector3 map_vertical = shadow_map_vertical(sun_axis);
  const Vector3 map_right = Vector3Normalize(Vector3CrossProduct(map_vertical, sun_axis));
  const Vector3 map_up = Vector3CrossProduct(sun_axis, map_right);
  const float texel_size = kShadowExtent / static_cast<float>(kShadowMapSize);
  // Snapped across the map; along the sun's own axis the place does not matter to the texels.
  const Vector3 across = Vector3Scale(map_right, snapped_down(Vector3DotProduct(ahead, map_right), texel_size));
  const Vector3 upward = Vector3Scale(map_up, snapped_down(Vector3DotProduct(ahead, map_up), texel_size));
  const Vector3 focus =
      Vector3Add(Vector3Add(across, upward), Vector3Scale(sun_axis, Vector3DotProduct(ahead, sun_axis)));
  return {Vector3Add(focus, Vector3Scale(sun_axis, kShadowSunDistance)), focus, map_vertical};
}

// --- Reflections -----------------------------------------------------------------------------

FaceAxes face_axes(const CubeFace& face) {
  const Vector3 right = Vector3Normalize(Vector3CrossProduct(face.forward, face.up));
  return {right, Vector3CrossProduct(right, face.forward)};
}

Vector3 reflection_eye(Vector3 camera_position) {
  return {camera_position.x, std::max(camera_position.y, kReflectionEyeHeight), camera_position.z};
}

int face_after(int face) { return (face + 1) % kCubeFaces; }

float reflection_strength(int faces_captured) {
  return faces_captured >= kCubeFaces ? kReflectionsOn : kReflectionsOff;
}

// --- Particles -------------------------------------------------------------------------------

Facing facing(Vec3 eye, Vec3 target, Vec3 up) {
  const Vec3 forward = normalize(target - eye);
  const Vec3 right = normalize(cross(forward, up));
  return {right, cross(right, forward)};
}

std::array<Vec3, kQuadCorners> particle_corners(Vec3 position, float size, float angle, const Facing& view) {
  const float half = size * kHalf;
  const float turned_cos = std::cos(angle);
  const float turned_sin = std::sin(angle);
  const Vec3 across = (view.right * turned_cos + view.up * turned_sin) * half;
  const Vec3 along = (view.up * turned_cos - view.right * turned_sin) * half;
  return {position - across - along, position + across - along, position + across + along, position - across + along};
}

// --- What games draw over the picture --------------------------------------------------------

Size image_size(int asked_width, int asked_height, int picture_width, int picture_height) {
  Size size{static_cast<float>(asked_width), static_cast<float>(asked_height)};
  const float shape = static_cast<float>(picture_width) / static_cast<float>(picture_height);
  if (size.width <= 0.0f && size.height <= 0.0f) {
    size = {static_cast<float>(picture_width), static_cast<float>(picture_height)};
  }
  if (size.width <= 0.0f) size.width = size.height * shape;
  if (size.height <= 0.0f) size.height = size.width / shape;
  return size;
}

float corner_roundness(float radius, float width, float height) {
  const float shorter = std::min(width, height);
  if (radius <= 0.0f || shorter <= 0.0f) return kSquareCorners;
  return std::min(radius * kDiameterPerRadius / shorter, kFullyRound);
}

}  // namespace redgem::render_math
