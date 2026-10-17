#include "render/sky_directions.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace redgem::render_math {
namespace {

constexpr float kFullTurn = kTwo * kPi;

// Where the major axis and the two across a face lie, the graphics card's way: for (s, t) mapped
// to (a, b) in -1..1, the direction is axis + a * along_s + b * along_t.
struct FaceAxes {
  Vec3 axis;
  Vec3 along_s;
  Vec3 along_t;
};
constexpr std::array<FaceAxes, kCubeMapFaces> kFaces = {{
    {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}},
    {{-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
    {{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    {{0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
}};

// A pixel's column, wrapped around the horizon, and its row, held at the poles.
std::uint32_t column(int x, std::uint32_t width) {
  const int wide = static_cast<int>(width);
  return static_cast<std::uint32_t>(((x % wide) + wide) % wide);
}
std::uint32_t row(int y, std::uint32_t height) {
  return static_cast<std::uint32_t>(std::clamp(y, 0, static_cast<int>(height) - 1));
}

}  // namespace

Vec3 picture_direction(PictureSpot spot) {
  const float longitude = (spot.u - kHalf) * kFullTurn;
  const float latitude = (kHalf - spot.v) * kPi;
  const float across = std::cos(latitude);
  return {std::cos(longitude) * across, std::sin(latitude), std::sin(longitude) * across};
}

PictureSpot picture_spot(Vec3 direction) {
  const Vec3 d = normalize(direction);
  return {kHalf + std::atan2(d.z, d.x) / kFullTurn, kHalf - std::asin(std::clamp(d.y, -1.0f, 1.0f)) / kPi};
}

Vec3 turned_around_up(Vec3 direction, float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {direction.x * c - direction.z * s, direction.y, direction.x * s + direction.z * c};
}

Vec3 cube_direction(int face, float s, float t) {
  const FaceAxes& axes = kFaces[static_cast<std::size_t>(face)];
  return normalize(axes.axis + axes.along_s * (s * kTwo - 1.0f) + axes.along_t * (t * kTwo - 1.0f));
}

Vec3 picture_light(const FloatImage& picture, Vec3 direction) {
  const PictureSpot spot = picture_spot(direction);
  const float x = spot.u * static_cast<float>(picture.width) - kHalf;
  const float y = spot.v * static_cast<float>(picture.height) - kHalf;
  const int left = static_cast<int>(std::floor(x));
  const int top = static_cast<int>(std::floor(y));
  const auto at = [&picture](int px, int py) { return picture.at(column(px, picture.width), row(py, picture.height)); };
  const Vec3 upper = mix(at(left, top), at(left + 1, top), x - static_cast<float>(left));
  const Vec3 lower = mix(at(left, top + 1), at(left + 1, top + 1), x - static_cast<float>(left));
  return mix(upper, lower, y - static_cast<float>(top));
}

}  // namespace redgem::render_math
