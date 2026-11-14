#pragma once

#include <vector>

#include "physics/physics_world.hpp"
#include "render/underwater.hpp"
#include "scene/water_interactions.hpp"

namespace redgem {

enum class TransformWrite;
bool water_teleport_write(const PhysicsWorld& physics, ObjectId id, TransformWrite write);

// App joins physics to the graphics-independent water detector; neither lower layer depends on the other.
void update_scene_water(WaterInteractions& water, const Scene& scene, const SceneDesc& desc,
                        const PhysicsWorld& physics, float delta);

// Water holds up the dynamic bodies and vehicles in it (physics/buoyancy.hpp) during the next
// physics step, each as dense as its `density` property says. Sleeping bodies are left to sleep.
void apply_scene_buoyancy(const WaterInteractions& water, const Scene& scene, const SceneDesc& desc,
                          PhysicsWorld& physics);

// Where an eye is against the water: what the renderer's underwater look follows.
UnderwaterView underwater_view(const std::vector<WaterSurface>& surfaces, Vec3 eye);

}  // namespace redgem
