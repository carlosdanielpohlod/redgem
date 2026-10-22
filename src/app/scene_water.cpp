#include "app/scene_water.hpp"

#include <algorithm>
#include <cmath>

#include "physics/physics_config.hpp"
#include "script/script_context.hpp"

namespace redgem {
namespace {

bool moves_water(const PhysicsWorld& physics, ObjectId id) {
  if (!physics.has_object(id) || physics.is_trigger(id)) return false;
  const BodyType type = physics.body_type(id);
  return type != BodyType::None && type != BodyType::Static;
}

Bounds mesh_bounds(const Scene& scene, const SceneDesc& desc, ObjectId id) {
  const int mesh = scene.at(id).mesh;
  if (!is_index_of(mesh, desc.meshes.size())) return default_collider_bounds();
  return desc.meshes[static_cast<std::size_t>(mesh)].bounds;
}

Vec3 scaled_half(const Bounds& bounds, Vec3 scale) {
  const Vec3 half = hadamard(bounds.half_extents(), scale);
  return {std::max(std::fabs(half.x), config::kMinColliderHalfExtent),
          std::max(std::fabs(half.y), config::kMinColliderHalfExtent),
          std::max(std::fabs(half.z), config::kMinColliderHalfExtent)};
}

Vec3 shape_half(Vec3 half, ColliderShape shape) {
  if (shape == ColliderShape::Sphere) return Vec3::one() * std::max({half.x, half.y, half.z});
  if (shape != ColliderShape::Capsule) return half;
  const float radius = std::max(half.x, half.z);
  return {radius, std::max(half.y - radius, config::kCharacterMinCylinderHalfHeight) + radius, radius};
}

Bounds collider_bounds(const Scene& scene, const SceneDesc& desc, ObjectId id) {
  Transform world = scene.world_transform(id);
  const Bounds mesh = mesh_bounds(scene, desc, id);
  const auto body_config = body_config_from_props(scene.at(id).props);
  const Vec3 half = shape_half(scaled_half(mesh, world.scale), body_config.collider);
  const Vec3 center = hadamard(mesh.center(), world.scale);
  const Bounds local{center - half, center + half, true};
  world.scale = Vec3::one();  // collider dimensions and offset above already include scale
  return water_world_bounds(local, world);
}

std::vector<WaterBodySample> water_bodies(const Scene& scene, const SceneDesc& desc, const PhysicsWorld& physics) {
  std::vector<WaterBodySample> bodies;
  for (const ObjectId id : scene.alive_ids()) {
    if (bodies.size() >= kMaxWaterBodies) break;
    if (scene.is_prefab(id) || scene.at(id).pending_destroy) continue;
    if (!moves_water(physics, id)) continue;
    bodies.push_back({scene.handle(id), collider_bounds(scene, desc, id), physics.velocity(id)});
  }
  return bodies;
}

}  // namespace

bool water_teleport_write(const PhysicsWorld& physics, ObjectId id, TransformWrite write) {
  if (write != TransformWrite::Position) return false;
  if (!moves_water(physics, id)) return false;
  return physics.body_type(id) != BodyType::Kinematic;
}

void update_scene_water(WaterInteractions& water, const Scene& scene, const SceneDesc& desc,
                        const PhysicsWorld& physics, float delta) {
  if (!water.enabled()) return;
  water.step(scene, desc, water_bodies(scene, desc, physics), delta);
}

}  // namespace redgem
