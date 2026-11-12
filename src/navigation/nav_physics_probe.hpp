#pragma once

// The level as navigation sees it: the static bodies of a physics world (floors, walls, terrain,
// mesh colliders). Moving bodies, characters and triggers are not part of it.

#include <vector>

#include "navigation/nav_bake.hpp"
#include "navigation/nav_settings.hpp"
#include "physics/physics_world.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

class PhysicsNavProbe final : public NavProbe {
 public:
  // Rays look down from `top` to `bottom`; a character is the settings' agent.
  PhysicsNavProbe(const PhysicsWorld& physics, const NavSettings& settings, float top, float bottom);
  std::vector<NavSurface> surfaces(float x, float z) const override;
  bool blocked(Vec3 feet) const override;

 private:
  const PhysicsWorld& physics_;
  QueryShape body_;         // the part of a character above the highest step it climbs
  float body_lift_ = 0.0f;  // from its feet to that part's middle
  float top_ = 0.0f;
  float depth_ = 0.0f;
};

// The box around every static body of the scene, in world space (not valid when there is none).
Bounds static_level_bounds(const Scene& scene, const SceneDesc& desc, const PhysicsWorld& physics);

}  // namespace redgem
