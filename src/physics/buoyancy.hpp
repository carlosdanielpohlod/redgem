#pragma once

// How water holds a body up: the math alone, with no world or water lookup. A body is sampled as a
// grid of vertical columns over its collider's world box; each column is pushed up by the water it
// displaces (Archimedes) at the middle of its wet part, and slowed by the water it moves through.
// Pushing each column where it is wet also turns a tipped body back upright.

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "core/math.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

inline constexpr float kWaterDensity = 1000.0f;       // kilograms per cubic meter (fresh water)
inline constexpr float kDefaultBodyDensity = 500.0f;  // a body that does not say floats half under
inline constexpr float kWaterVerticalDrag = 3.0f;     // per second, fully under: bobbing settles in seconds
inline constexpr float kWaterHorizontalDrag = 1.0f;   // per second, fully under: a pushed float drifts on
inline constexpr std::size_t kBuoyancyColumnsPerSide = 2;
inline constexpr std::size_t kBuoyancyColumns = kBuoyancyColumnsPerSide * kBuoyancyColumnsPerSide;

struct BuoyancyBody {
  Bounds bounds;  // the collider's box in world space
  Vec3 velocity;
  Vec3 angular_velocity;  // radians per second, around the box's center
  float mass = 0.0f;
  float density = kDefaultBodyDensity;  // kilograms per cubic meter, as a whole
  float gravity = 0.0f;                 // meters per second squared pulling it down (positive)
};

// The water over each column (columns_of's order); nothing where a column is out of any water.
using ColumnWater = std::array<std::optional<float>, kBuoyancyColumns>;

// A force (newtons) and where it pushes (world space).
struct BuoyancyPush {
  Vec3 force;
  Vec3 point;
};

// How much of a span from `bottom` to `top` is below `surface`, 0 to 1.
float submerged_fraction(float bottom, float top, float surface);
// Water's push up on `mass` kilograms of a body that dense, with that fraction of it under.
Vec3 buoyant_force(float mass, float density, float gravity, float fraction);
// Water slowing `mass` kilograms moving at `velocity`, with that fraction of it under.
Vec3 water_drag(Vec3 velocity, float mass, float fraction);
// The middle of each column's footprint, at the bottom of the box.
std::array<Vec3, kBuoyancyColumns> buoyancy_columns(const Bounds& bounds);
// Whether a body can be held up at all: a finite box, mass and density.
bool buoyant(const BuoyancyBody& body);
// Every push on the body this step: none when no column is wet.
std::vector<BuoyancyPush> buoyancy_pushes(const BuoyancyBody& body, const ColumnWater& water);
// The depth a body of that density rests at: the fraction of its height under the surface.
float floating_fraction(float density);

}  // namespace redgem
