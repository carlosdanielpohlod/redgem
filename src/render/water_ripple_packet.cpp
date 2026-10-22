#include "render/water_ripple_packet.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace redgem::render_detail {
namespace {

std::size_t ripple_count(const WaterRippleSnapshot& snapshot) {
  return std::min(snapshot.count, snapshot.ripples.size());
}

bool finite_origin(Vec3 origin) {
  return std::isfinite(origin.x) && std::isfinite(origin.y) && std::isfinite(origin.z);
}

bool finite_range(float value, float minimum, float maximum) {
  return std::isfinite(value) && value >= minimum && value <= maximum;
}

bool valid_ripple(const WaterRipple& ripple) {
  if (ripple.surface == kNoHandle || ripple.serial == 0) return false;
  if (!finite_origin(ripple.origin)) return false;
  if (!finite_range(ripple.age, 0.0f, kWaterRippleLifetime)) return false;
  return finite_range(ripple.strength, 0.0f, kWaterRippleMaxStrength) &&
         finite_range(ripple.radius, kWaterRippleMinRadius, kWaterRippleMaxRadius);
}

bool belongs_to(const WaterRipple& ripple, ObjectHandle surface, std::size_t primitive) {
  return ripple.surface == surface && ripple.primitive == primitive;
}

bool already_drawn(const WaterRippleSnapshot& previous, const WaterRipple& current) {
  const auto end = previous.ripples.begin() + static_cast<std::ptrdiff_t>(ripple_count(previous));
  return std::any_of(previous.ripples.begin(), end, [&](const WaterRipple& ripple) {
    return ripple.serial == current.serial && belongs_to(ripple, current.surface, current.primitive);
  });
}

void append(WaterRipplePacket& packet, const WaterRipple& ripple) {
  if (static_cast<std::size_t>(packet.count) == packet.waves.size()) return;
  const auto index = static_cast<std::size_t>(packet.count++);
  packet.waves[index] = {ripple.origin.x, ripple.origin.z, ripple.age, ripple.strength};
  packet.radii[index] = ripple.radius;
}

}  // namespace

WaterRippleSnapshot validated_water_ripples(const WaterRippleSnapshot* snapshot) {
  WaterRippleSnapshot result;
  if (snapshot == nullptr) return result;
  if (!std::isfinite(snapshot->time) || snapshot->time < 0.0) return result;
  result.time = snapshot->time;
  for (std::size_t index = 0; index < ripple_count(*snapshot); ++index) {
    const auto& ripple = snapshot->ripples[index];
    if (valid_ripple(ripple)) result.ripples[result.count++] = ripple;
  }
  return result;
}

WaterRipplePacket water_ripple_packet(const WaterRippleSnapshot& snapshot, ObjectHandle surface,
                                      std::size_t primitive) {
  WaterRipplePacket packet;
  if (surface == kNoHandle) return packet;
  for (std::size_t index = 0; index < ripple_count(snapshot); ++index) {
    const auto& ripple = snapshot.ripples[index];
    if (belongs_to(ripple, surface, primitive)) append(packet, ripple);
  }
  return packet;
}

WaterRipplePacket previous_water_ripple_packet(const WaterRippleSnapshot& previous, const WaterRippleSnapshot& current,
                                               ObjectHandle surface, std::size_t primitive) {
  WaterRipplePacket packet = water_ripple_packet(previous, surface, primitive);
  for (std::size_t index = 0; index < ripple_count(current); ++index) {
    const auto& ripple = current.ripples[index];
    if (!belongs_to(ripple, surface, primitive)) continue;
    if (!already_drawn(previous, ripple)) append(packet, ripple);
  }
  return packet;
}

float water_ripple_bound(const WaterRippleSnapshot& snapshot, ObjectHandle surface) {
  std::map<std::size_t, float> heights;
  float maximum = 0.0f;
  for (std::size_t index = 0; index < ripple_count(snapshot); ++index) {
    const auto& ripple = snapshot.ripples[index];
    if (ripple.surface != surface) continue;
    float& height = heights[ripple.primitive];
    height += ripple.strength;
    maximum = std::max(maximum, height);
  }
  return maximum;
}

}  // namespace redgem::render_detail
