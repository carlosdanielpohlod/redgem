#include "app/scene_water.hpp"

#include <algorithm>
#include <cmath>

#include "physics/buoyancy.hpp"
#include "physics/physics_config.hpp"
#include "scene/water_volume.hpp"
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

Bounds collider_bounds(const Scene& scene, const SceneDesc& desc, ObjectId id, const BodyConfig& config) {
  Transform world = scene.world_transform(id);
  const Bounds mesh = mesh_bounds(scene, desc, id);
  const Vec3 half = shape_half(scaled_half(mesh, world.scale), config.collider);
  const Vec3 center = hadamard(mesh.center(), world.scale);
  const Bounds local{center - half, center + half, true};
  world.scale = Vec3::one();  // collider dimensions and offset above already include scale
  return water_world_bounds(local, world);
}

// An object of the running scene: not a template, and not on its way out.
bool live_object(const Scene& scene, ObjectId id) { return !scene.is_prefab(id) && !scene.at(id).pending_destroy; }

std::vector<WaterBodySample> water_bodies(const Scene& scene, const SceneDesc& desc, const PhysicsWorld& physics) {
  std::vector<WaterBodySample> bodies;
  for (const ObjectId id : scene.alive_ids()) {
    if (bodies.size() >= kMaxWaterBodies) break;
    if (!live_object(scene, id) || !moves_water(physics, id)) continue;
    const Bounds bounds = collider_bounds(scene, desc, id, body_config_from_props(scene.at(id).props));
    bodies.push_back({scene.handle(id), bounds, physics.velocity(id)});
  }
  return bodies;
}

// A body water holds up: a dynamic one or a vehicle, awake, and a live object of the scene.
bool floats(const Scene& scene, const PhysicsWorld& physics, ObjectId id) {
  if (!live_object(scene, id) || !moves_water(physics, id)) return false;
  return is_dynamic(physics.body_type(id)) && !physics.sleeping(id);
}

BuoyancyBody buoyancy_body(const Scene& scene, const SceneDesc& desc, const PhysicsWorld& physics, ObjectId id) {
  const BodyConfig config = body_config_from_props(scene.at(id).props);
  BuoyancyBody body;
  body.bounds = collider_bounds(scene, desc, id, config);
  body.velocity = physics.velocity(id);
  body.angular_velocity = physics.angular_velocity(id);
  body.mass = physics.mass(id);
  body.density = config.density;
  body.gravity = -config::kGravityY * physics.gravity_scale(id);
  return body;
}

ColumnWater column_water(const std::vector<WaterSurface>& surfaces, const Bounds& bounds) {
  ColumnWater water{};
  const auto columns = buoyancy_columns(bounds);
  for (std::size_t i = 0; i < kBuoyancyColumns; ++i) {
    const auto column = water_column_at(surfaces, columns[i].x, columns[i].z);
    if (column) water[i] = column->height;
  }
  return water;
}

void float_body(const std::vector<WaterSurface>& surfaces, const Scene& scene, const SceneDesc& desc,
                PhysicsWorld& physics, ObjectId id) {
  const BuoyancyBody body = buoyancy_body(scene, desc, physics, id);
  if (!buoyant(body)) return;
  for (const BuoyancyPush& push : buoyancy_pushes(body, column_water(surfaces, body.bounds)))
    physics.add_force_at(id, push.force, push.point);
}

}  // namespace

void apply_scene_buoyancy(const WaterInteractions& water, const Scene& scene, const SceneDesc& desc,
                          PhysicsWorld& physics) {
  if (!water.enabled() || water.surfaces().empty()) return;
  for (const ObjectId id : scene.alive_ids()) {
    if (floats(scene, physics, id)) float_body(water.surfaces(), scene, desc, physics, id);
  }
}

UnderwaterView underwater_view(const std::vector<WaterSurface>& surfaces, Vec3 eye) {
  if (!finite_water_point(eye)) return {};
  const auto column = water_column_at(surfaces, eye.x, eye.z);
  if (!column) return {};
  const WaterDesc& water = column->surface->water;
  return {true, column->height - eye.y, underwater_band(water.wave_height), water.absorption};
}

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
