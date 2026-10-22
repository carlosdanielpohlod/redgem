#pragma once

#include "physics/physics_world.hpp"
#include "scene/water_interactions.hpp"

namespace redgem {

enum class TransformWrite;
bool water_teleport_write(const PhysicsWorld& physics, ObjectId id, TransformWrite write);

// App joins physics to the graphics-independent water detector; neither lower layer depends on the other.
void update_scene_water(WaterInteractions& water, const Scene& scene, const SceneDesc& desc,
                        const PhysicsWorld& physics, float delta);

}  // namespace redgem
