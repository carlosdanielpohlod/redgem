#include "physics/buoyancy.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

constexpr float kColumnShare = 1.0f / static_cast<float>(kBuoyancyColumns);
constexpr float kColumnWidth = 1.0f / static_cast<float>(kBuoyancyColumnsPerSide);
constexpr std::size_t kAxes = 3;
constexpr std::size_t kAxisX = 0;
constexpr std::size_t kAxisY = 1;
constexpr float kBoxStart = 0.0f;  // of the way along a box's axis: its minimum
constexpr float kBoxEnd = 1.0f;    // its maximum

bool finite(float value) { return std::isfinite(value); }
bool positive(float value) { return finite(value) && value > 0.0f; }
bool finite_point(Vec3 point) { return finite(point.x) && finite(point.y) && finite(point.z); }
bool has_volume(const Bounds& box) {
  const Vec3 size = box.max - box.min;
  return box.valid && positive(size.x) && positive(size.y) && positive(size.z);
}

float component(Vec3 v, std::size_t axis) {
  if (axis == kAxisX) return v.x;
  return axis == kAxisY ? v.y : v.z;
}

Vec3 with_component(Vec3 v, std::size_t axis, float value) {
  (axis == kAxisX ? v.x : (axis == kAxisY ? v.y : v.z)) = value;
  return v;
}

// The axis of the box that points most nearly up in the world: columns run along it.
std::size_t upright_axis(Quat rotation) {
  const Vec3 up_in_box = rotate(conjugate(rotation), Vec3::up());
  std::size_t best = kAxisY;
  for (std::size_t axis = 0; axis < kAxes; ++axis) {
    if (std::fabs(component(up_in_box, axis)) > std::fabs(component(up_in_box, best))) best = axis;
  }
  return best;
}

Vec3 world_point(const Transform& world, Vec3 local) { return world.position + rotate(world.rotation, local); }

// A point of the box `share` of the way from its minimum to its maximum along each axis.
Vec3 box_point(const Bounds& box, Vec3 share) { return box.min + hadamard(box.max - box.min, share); }

BuoyancyColumn column_along(const BuoyancyBody& body, std::size_t up, Vec3 share) {
  const Vec3 a = world_point(body.world, box_point(body.box, with_component(share, up, kBoxStart)));
  const Vec3 b = world_point(body.world, box_point(body.box, with_component(share, up, kBoxEnd)));
  return a.y <= b.y ? BuoyancyColumn{a, b} : BuoyancyColumn{b, a};
}

float cell_share(std::size_t cell) { return (static_cast<float>(cell) + kHalf) * kColumnWidth; }

// How fast the body moves through the water at a point of it.
Vec3 point_velocity(const BuoyancyBody& body, Vec3 point) {
  const Vec3 center = world_point(body.world, body.box.center());
  return body.velocity + cross(body.angular_velocity, point - center);
}

std::optional<BuoyancyPush> column_push(const BuoyancyBody& body, const BuoyancyColumn& column,
                                        std::optional<float> surface) {
  if (!surface) return std::nullopt;
  const float fraction = submerged_fraction(column.low.y, column.high.y, *surface);
  if (fraction <= 0.0f) return std::nullopt;
  const Vec3 point = mix(column.low, column.high, fraction * kHalf);  // the middle of its wet part
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

bool buoyant(const BuoyancyBody& body) {
  const bool placed = finite_point(body.world.position) && finite(body.gravity);
  return has_volume(body.box) && placed && positive(body.mass) && positive(body.density);
}

BuoyancyColumns buoyancy_columns(const BuoyancyBody& body) {
  const std::size_t up = upright_axis(body.world.rotation);
  BuoyancyColumns columns{};
  for (std::size_t i = 0; i < kBuoyancyColumns; ++i) {
    Vec3 share = with_component(Vec3::zero(), (up + 2) % kAxes, cell_share(i / kBuoyancyColumnsPerSide));
    share = with_component(share, (up + 1) % kAxes, cell_share(i % kBuoyancyColumnsPerSide));
    columns[i] = column_along(body, up, share);
  }
  return columns;
}

std::vector<BuoyancyPush> buoyancy_pushes(const BuoyancyBody& body, const BuoyancyColumns& columns,
                                          const ColumnWater& water) {
  std::vector<BuoyancyPush> pushes;
  if (!buoyant(body)) return pushes;
  for (std::size_t i = 0; i < kBuoyancyColumns; ++i) {
    if (const auto push = column_push(body, columns[i], water[i])) pushes.push_back(*push);
  }
  return pushes;
}

float floating_fraction(float density) { return std::clamp(density / kWaterDensity, 0.0f, 1.0f); }

}  // namespace redgem
