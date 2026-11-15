#pragma once

// What muffles a sound: the level's static bodies (walls, floors, rocks) between the listener and
// it. Moving bodies and characters do not (the player's own capsule would muffle everything).

#include "core/math.hpp"
#include "physics/physics_world.hpp"

namespace redgem {

bool static_body_between(const PhysicsWorld& physics, Vec3 from, Vec3 to);

}  // namespace redgem
