#pragma once

// How water holds a body up: the math alone, with no world or water lookup. A body is sampled as a
// grid of columns through its collider's own box, running along the box's axis that points most
// nearly up. Each column is pushed up by the water it displaces (Archimedes) at the middle of its
// wet part, and slowed by the water it moves through. A tipped body's lower columns are deeper, so
// they push harder, off its middle: it turns back.

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "core/math.hpp"
#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem {

inline constexpr float kWaterDensity = 1000.0f;       // kilograms per cubic meter (fresh water)
inline constexpr float kDefaultBodyDensity = 500.0f;  // a body that does not say floats half under
inline constexpr float kWaterVerticalDrag = 3.0f;     // per second, fully under: bobbing settles in seconds
inline constexpr float kWaterHorizontalDrag = 1.0f;   // per second, fully under: a pushed float drifts on
inline constexpr std::size_t kBuoyancyColumnsPerSide = 2;
inline constexpr std::size_t kBuoyancyColumns = kBuoyancyColumnsPerSide * kBuoyancyColumnsPerSide;

struct BuoyancyBody {
  Bounds box;       // the collider's box in its own space, scale included
  Transform world;  // where that space is: position and rotation (its scale is ignored)
  Vec3 velocity;
  Vec3 angular_velocity;  // radians per second, around the box's center
  float mass = 0.0f;
  float density = kDefaultBodyDensity;  // kilograms per cubic meter, as a whole
  float gravity = 0.0f;                 // meters per second squared pulling it down (positive)
};

// A column through the body, in world space: its lower end and its upper end.
struct BuoyancyColumn {
  Vec3 low;
  Vec3 high;
};
using BuoyancyColumns = std::array<BuoyancyColumn, kBuoyancyColumns>;

// The water surface over each column (in buoyancy_columns' order); nothing where a column is out of
// any water.
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
// Whether a body can be held up at all: a box with height, a finite pose, mass and density.
bool buoyant(const BuoyancyBody& body);
// The columns a body is sampled as.
BuoyancyColumns buoyancy_columns(const BuoyancyBody& body);
// Every push on a body whose columns are `columns`, this step: none when no column is wet.
std::vector<BuoyancyPush> buoyancy_pushes(const BuoyancyBody& body, const BuoyancyColumns& columns,
                                          const ColumnWater& water);
// The depth a body of that density rests at: the fraction of its height under the surface.
float floating_fraction(float density);

}  // namespace redgem
