#include "render/render_math.hpp"

#include <raymath.h>

#include <cmath>
#include <cstring>

#include "scene/scene_config.hpp"

namespace redgem::render_math {
namespace {

constexpr const char* kRendererLine = "Renderer:";
constexpr int kSmallestTarget = 1;      // pixels: a render target cannot be empty
constexpr std::size_t kMatrixSide = 4;  // rows, and columns, of a matrix

// A size the game left out (zero or less): the picture's own takes its place.
bool missing(float size) { return size <= 0.0f; }

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
  const bool as_large_as_it_is = missing(size.width) && missing(size.height);
  if (as_large_as_it_is) size = {static_cast<float>(picture_width), static_cast<float>(picture_height)};
  if (missing(size.width)) size.width = size.height * shape;
  if (missing(size.height)) size.height = size.width / shape;
  return size;
}

float corner_roundness(float radius, float width, float height) {
  const float shorter = std::min(width, height);
  const bool nothing_to_round = radius <= 0.0f || shorter <= 0.0f;
  if (nothing_to_round) return kSquareCorners;
  return std::min(radius * kDiameterPerRadius / shorter, kFullyRound);
}

}  // namespace redgem::render_math
