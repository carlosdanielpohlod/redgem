#pragma once

// The level as navigation sees it: the static bodies of a physics world (floors, walls, terrain,
// mesh colliders). Moving bodies, characters and triggers are not part of it.

#include <vector>

#include "navigation/nav_bake.hpp"
#include "navigation/nav_settings.hpp"
#include "physics/physics_world.hpp"

namespace redgem {

class PhysicsNavProbe final : public NavProbe {
 public:
  // Rays look down from `top` to `bottom`. A character is the settings' agent, but never thinner
  // than half the diagonal of a cell of `cell_size`: then nothing between the middles of two cells
  // side by side (diagonals too) goes unseen, as it is within that of one of them.
  PhysicsNavProbe(const PhysicsWorld& physics, const NavSettings& settings, float cell_size, float top, float bottom);
  std::vector<NavSurface> surfaces(float x, float z) const override;
  bool blocked(Vec3 feet) const override;

 private:
  const PhysicsWorld& physics_;
  QueryShape body_;         // the part of a character above the highest step it climbs
  float body_lift_ = 0.0f;  // from its feet to that part's middle
  float top_ = 0.0f;
  float depth_ = 0.0f;
};

}  // namespace redgem
