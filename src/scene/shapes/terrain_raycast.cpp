// Where a ray meets a terrain.

#include "scene/shapes/terrain.hpp"

namespace redgem {
namespace {

constexpr float kRayStepShare = 0.5f;  // of a cell, between samples along a ray
constexpr int kRayRefinements = 12;    // halvings that pin the hit down

// A ray over a terrain, in the terrain's own space.
struct TerrainRay {
  const PrimitiveDesc& mesh;
  const TerrainGrid& grid;
  Vec3 origin;
  Vec3 direction;

  Vec3 at(float distance) const { return origin + direction * distance; }
  float above_ground(float distance) const {
    const Vec3 point = at(distance);
    return point.y - terrain_height(mesh, grid, point.x, point.z);
  }
  bool over_terrain(float distance) const {
    const Vec3 point = at(distance);
    return grid.covers(point.x, point.z);
  }
  bool in_clear_air(float distance) const { return over_terrain(distance) && above_ground(distance) > 0.0f; }
  bool under_ground(float distance) const { return over_terrain(distance) && above_ground(distance) <= 0.0f; }
};

// The ray crossed the surface between two samples, above it at `low` and under it at `high`:
// closes in on the crossing.
Vec3 crossing(const TerrainRay& ray, float low, float high) {
  for (int i = 0; i < kRayRefinements; ++i) {
    const float middle = (low + high) * kHalf;
    (ray.above_ground(middle) > 0.0f ? low : high) = middle;
  }
  Vec3 hit = ray.at(high);
  hit.y = terrain_height(ray.mesh, ray.grid, hit.x, hit.z);
  return hit;
}

// The ray is sampled every `step`, from one step on: the first sample that is under the ground,
// if any is within `max_distance`.
std::optional<int> first_sample_under_ground(const TerrainRay& ray, float step, float max_distance) {
  for (int sample = 1; step * static_cast<float>(sample) <= max_distance; ++sample) {
    if (ray.under_ground(step * static_cast<float>(sample))) return sample;
  }
  return std::nullopt;
}

}  // namespace

std::optional<Vec3> terrain_raycast(const PrimitiveDesc& mesh, const TerrainGrid& grid, Vec3 origin, Vec3 direction,
                                    float max_distance) {
  if (!grid.fits(mesh)) return std::nullopt;
  const TerrainRay ray{mesh, grid, origin, direction};
  const float step = grid.spacing() * kRayStepShare;
  const std::optional<int> under = first_sample_under_ground(ray, step, max_distance);
  if (!under) return std::nullopt;
  // The sample before must be in clear air, or the ray started under the ground or came in from
  // the side below it.
  const float above = step * static_cast<float>(*under - 1);
  if (!ray.in_clear_air(above)) return std::nullopt;
  return crossing(ray, above, step * static_cast<float>(*under));
}

}  // namespace redgem
