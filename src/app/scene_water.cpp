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

// A collider's box in its own space (scale included), and where that space is in the world.
struct ColliderBox {
  Bounds local;
  Transform world;  // unit scale: the box already includes it
};

ColliderBox collider_box(const Scene& scene, const SceneDesc& desc, ObjectId id, const BodyConfig& config) {
  Transform world = scene.world_transform(id);
  const Bounds mesh = mesh_bounds(scene, desc, id);
  const Vec3 half = shape_half(scaled_half(mesh, world.scale), config.collider);
  const Vec3 center = hadamard(mesh.center(), world.scale);
  world.scale = Vec3::one();
  return {Bounds{center - half, center + half, true}, world};
}

Bounds collider_bounds(const Scene& scene, const SceneDesc& desc, ObjectId id, const BodyConfig& config) {
  const ColliderBox box = collider_box(scene, desc, id, config);
  return water_world_bounds(box.local, box.world);
}

// Whether an object could reach into the water at all: within reach of the box around every
// surface (the object's mesh, scaled, from its origin; no collider is larger).
bool near_water(const Scene& scene, const SceneDesc& desc, ObjectId id, const Bounds& water) {
  const Transform world = scene.world_transform(id);
  const Bounds mesh = mesh_bounds(scene, desc, id);
  const float reach = length(hadamard(mesh.center(), world.scale)) + length(scaled_half(mesh, world.scale));
  const bool across = world.position.x + reach >= water.min.x && world.position.x - reach <= water.max.x;
  const bool along = world.position.z + reach >= water.min.z && world.position.z - reach <= water.max.z;
  return across && along && world.position.y - reach <= water.max.y;
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
  if (!live_object(scene, id) || physics.is_trigger(id)) return false;
  return is_dynamic(physics.body_type(id)) && !physics.sleeping(id);
}

BuoyancyBody buoyancy_body(const Scene& scene, const SceneDesc& desc, const PhysicsWorld& physics, ObjectId id) {
  const BodyConfig config = body_config_from_props(scene.at(id).props);
  const ColliderBox box = collider_box(scene, desc, id, config);
  BuoyancyBody body;
  body.box = box.local;
  body.world = box.world;
  body.velocity = physics.velocity(id);
  body.angular_velocity = physics.angular_velocity(id);
  body.mass = physics.mass(id);
  body.density = config.density;
  body.gravity = -config::kGravityY * physics.gravity_scale(id);
  return body;
}

// The water over each column: the surface nearest above its lower end (a pool on a roof does not
// count for a lake under it).
ColumnWater column_water(const std::vector<WaterSurface>& surfaces, const BuoyancyColumns& columns) {
  ColumnWater water{};
  for (std::size_t i = 0; i < kBuoyancyColumns; ++i) {
    const auto column = water_column_over(surfaces, columns[i].low);
    if (column) water[i] = column->height;
  }
  return water;
}

void float_body(const std::vector<WaterSurface>& surfaces, const Scene& scene, const SceneDesc& desc,
                PhysicsWorld& physics, ObjectId id) {
  const BuoyancyBody body = buoyancy_body(scene, desc, physics, id);
  const BuoyancyColumns columns = buoyancy_columns(body);
  for (const BuoyancyPush& push : buoyancy_pushes(body, columns, column_water(surfaces, columns)))
    physics.add_force_at(id, push.force, push.point);
}

bool has_water(const WaterInteractions& water) { return water.enabled() && !water.surfaces().empty(); }

// The box around every water surface.
Bounds all_water(const std::vector<WaterSurface>& surfaces) {
  Bounds all;
  for (const WaterSurface& surface : surfaces) {
    all.include(surface.bounds.min);
    all.include(surface.bounds.max);
  }
  return all;
}

}  // namespace

void apply_scene_buoyancy(const WaterInteractions& water, const Scene& scene, const SceneDesc& desc,
                          PhysicsWorld& physics) {
  if (!has_water(water)) return;
  const Bounds everywhere = all_water(water.surfaces());
  for (const ObjectId id : scene.alive_ids()) {
    if (!floats(scene, physics, id)) continue;
    if (near_water(scene, desc, id, everywhere)) float_body(water.surfaces(), scene, desc, physics, id);
  }
}

UnderwaterView underwater_view(const std::vector<WaterSurface>& surfaces, Vec3 eye) {
  if (!finite_water_point(eye)) return {};
  const auto column = water_column_over(surfaces, eye);
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
