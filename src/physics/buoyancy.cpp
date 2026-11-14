#include "physics/buoyancy.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

constexpr float kColumnShare = 1.0f / static_cast<float>(kBuoyancyColumns);
constexpr float kColumnWidth = 1.0f / static_cast<float>(kBuoyancyColumnsPerSide);

bool finite(float value) { return std::isfinite(value); }
bool positive(float value) { return finite(value) && value > 0.0f; }

Vec3 column_center(const Bounds& bounds, std::size_t across, std::size_t along) {
  const Vec3 size = bounds.max - bounds.min;
  const float x = (static_cast<float>(across) + kHalf) * kColumnWidth;
  const float z = (static_cast<float>(along) + kHalf) * kColumnWidth;
  return {bounds.min.x + size.x * x, bounds.min.y, bounds.min.z + size.z * z};
}

// How fast the body moves through the water at a point of it.
Vec3 point_velocity(const BuoyancyBody& body, Vec3 point) {
  return body.velocity + cross(body.angular_velocity, point - body.bounds.center());
}

std::optional<BuoyancyPush> column_push(const BuoyancyBody& body, Vec3 column, std::optional<float> surface) {
  if (!surface) return std::nullopt;
  const float fraction = submerged_fraction(body.bounds.min.y, body.bounds.max.y, *surface);
  if (fraction <= 0.0f) return std::nullopt;
  const float wet_top = std::min(*surface, body.bounds.max.y);
  const Vec3 point{column.x, (column.y + wet_top) * kHalf, column.z};
  const float mass = body.mass * kColumnShare;
  const Vec3 lift = buoyant_force(mass, body.density, body.gravity, fraction);
  return BuoyancyPush{lift + water_drag(point_velocity(body, point), mass, fraction), point};
}

}  // namespace

float submerged_fraction(float bottom, float top, float surface) {
  const float height = top - bottom;
  if (!positive(height) || !finite(surface)) return 0.0f;
  return std::clamp((surface - bottom) / height, 0.0f, 1.0f);
}

Vec3 buoyant_force(float mass, float density, float gravity, float fraction) {
  if (!positive(density)) return Vec3::zero();
  return Vec3::up() * (mass * gravity * (kWaterDensity / density) * fraction);
}

Vec3 water_drag(Vec3 velocity, float mass, float fraction) {
  const Vec3 rate{kWaterHorizontalDrag, kWaterVerticalDrag, kWaterHorizontalDrag};
  return -hadamard(velocity, rate) * (mass * fraction);
}

std::array<Vec3, kBuoyancyColumns> buoyancy_columns(const Bounds& bounds) {
  std::array<Vec3, kBuoyancyColumns> columns{};
  for (std::size_t across = 0; across < kBuoyancyColumnsPerSide; ++across) {
    for (std::size_t along = 0; along < kBuoyancyColumnsPerSide; ++along)
      columns[across * kBuoyancyColumnsPerSide + along] = column_center(bounds, across, along);
  }
  return columns;
}

bool buoyant(const BuoyancyBody& body) {
  const bool sized = body.bounds.valid && body.bounds.max.y > body.bounds.min.y;
  return sized && positive(body.mass) && positive(body.density) && finite(body.gravity);
}

std::vector<BuoyancyPush> buoyancy_pushes(const BuoyancyBody& body, const ColumnWater& water) {
  std::vector<BuoyancyPush> pushes;
  if (!buoyant(body)) return pushes;
  const auto columns = buoyancy_columns(body.bounds);
  for (std::size_t i = 0; i < kBuoyancyColumns; ++i) {
    if (const auto push = column_push(body, columns[i], water[i])) pushes.push_back(*push);
  }
  return pushes;
}

float floating_fraction(float density) { return std::clamp(density / kWaterDensity, 0.0f, 1.0f); }

}  // namespace redgem
