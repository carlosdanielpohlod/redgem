#include "app/scene_bodies.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/config.hpp"
#include "core/log.hpp"
#include "physics/body_config.hpp"
#include "physics/physics_config.hpp"
#include "scene/props.hpp"

namespace redgem::bodies {
namespace {

constexpr float kWheelRotationTolerance = 1e-3f;

bool simulated(BodyType type) {
  return type == BodyType::Dynamic || type == BodyType::Character || type == BodyType::Static ||
         type == BodyType::Vehicle;
}

// An object's mesh bounds, or the default collider's when it has no mesh.
Bounds bounds_of(const Source& source, const GameObject& object) {
  return object.mesh != kNoIndex ? source.desc.meshes[object.mesh].bounds : default_collider_bounds();
}

WheelDesc describe_wheel(const Source& source, ObjectId child, const Transform& chassis) {
  const Transform world = source.scene.world_transform(child);
  // Wheels are modeled with their axle along X: width on X, radius in the Y/Z plane.
  const Vec3 half = hadamard(bounds_of(source, source.scene.at(child)).half_extents(), world.scale);
  WheelDesc wheel;
  wheel.object = source.scene.handle(child);
  wheel.center = rotate(conjugate(chassis.rotation), world.position - chassis.position);
  wheel.radius = std::max(half.y, half.z);
  wheel.width = half.x * kTwo;
  return wheel;
}

void warn_of_turned_wheel(const GameObject& wheel) {
  if (approx_equal(wheel.local.rotation, Quat::identity(), kWheelRotationTolerance)) return;
  log::warning("wheel '" + wheel.name +
               "' has an unapplied rotation; apply it in Blender (Ctrl+A) so its "
               "axle lies along X");
}

std::vector<WheelDesc> collect_wheels(const Source& source, ObjectId vehicle) {
  const Transform chassis = source.scene.world_transform(vehicle);
  std::vector<WheelDesc> wheels;
  for (const ObjectId child : source.scene.at(vehicle).children) {
    const GameObject& object = source.scene.at(child);
    if (!get_bool(object.props, props::kWheel).value_or(false)) continue;
    warn_of_turned_wheel(object);
    wheels.push_back(describe_wheel(source, child, chassis));
  }
  return wheels;
}

BodyDesc describe_body(const Source& source, ObjectId id, BodyConfig config) {
  const GameObject& object = source.scene.at(id);
  BodyDesc desc;
  desc.object = id;
  desc.config = std::move(config);
  desc.world = source.scene.world_transform(id);
  if (object.mesh != kNoIndex) desc.mesh = &source.desc.meshes[object.mesh];
  desc.bounds = bounds_of(source, object);
  if (desc.config.type != BodyType::Vehicle) return desc;
  desc.wheels = collect_wheels(source, id);
  if (desc.wheels.empty()) log::warning("vehicle '" + object.name + "' has no children with wheel = true");
  return desc;
}

// Moves and turns an object to where physics has it, keeping its scale.
void move_to(Scene& scene, ObjectId id, const Transform& pose) {
  Transform world = scene.world_transform(id);
  world.position = pose.position;
  world.rotation = pose.rotation;
  scene.set_world_transform(id, world);
}

void follow_body(Scene& scene, PhysicsWorld& physics, ObjectId id, const Transform& pose) {
  const BodyType type = physics.body_type(id);
  if (type == BodyType::Character) {
    scene.set_world_position(id, pose.position);
  } else {
    move_to(scene, id, pose);
  }
  if (type != BodyType::Vehicle) return;
  // After the chassis moved, so wheels are expressed relative to its new pose.
  for (const WheelPose& wheel : physics.wheel_poses(id)) {
    const ObjectId object = scene.resolve(wheel.object);  // a wheel destroyed on its own is gone
    if (object != kNoObject) move_to(scene, object, wheel.world);
  }
}

// Whether a body goes where its object is each step, instead of the other way round.
bool follows_its_object(PhysicsWorld& physics, ObjectId id) {
  return physics.body_type(id) == BodyType::Kinematic || physics.is_trigger(id);
}

// A layer the game does not have: the body goes on the default one, with a warning.
void settle_layer(const PhysicsWorld& physics, const GameObject& object, BodyConfig& body) {
  if (physics.layers().find(body.layer)) return;
  log::warning("object '" + object.name + "': no physics layer '" + body.layer +
               "' in game.json (layers: " + physics.layers().names_listed() + "), using " + kDefaultLayerName);
  body.layer = kDefaultLayerName;
}

}  // namespace

void create_all(const Source& source, PhysicsWorld& physics) {
  for (const ObjectId id : source.scene.alive_ids()) create(source, physics, id);
}

void create(const Source& source, PhysicsWorld& physics, ObjectId id) {
  if (source.scene.is_prefab(id)) return;  // templates are inert
  const GameObject& object = source.scene.at(id);
  BodyConfig body = body_config_from_props(object.props);
  for (const std::string& warning : body.warnings) log::warning("object '" + object.name + "': " + warning);
  if (!body.wants_physics()) return;
  settle_layer(physics, object, body);
  const BodyDesc desc = describe_body(source, id, std::move(body));
  if (auto error = physics.add_object(desc)) log::error("object '" + object.name + "': " + *error);
}

void push_poses(const Scene& scene, PhysicsWorld& physics) {
  for (const ObjectId id : scene.alive_ids()) {
    if (!physics.has_object(id)) continue;
    if (follows_its_object(physics, id)) physics.set_target_pose(id, scene.world_transform(id));
  }
}

void pull_poses(Scene& scene, PhysicsWorld& physics) {
  for (const ObjectId id : scene.alive_ids()) {
    const std::optional<Transform> pose = physics.simulated_pose(id);
    if (pose) follow_body(scene, physics, id, *pose);
  }
}

void follow_write(const Scene& scene, PhysicsWorld& physics, ObjectId id, TransformWrite write) {
  // Kinematic bodies and triggers already follow the scene every step.
  if (!simulated(physics.body_type(id))) return;
  if (write == TransformWrite::Position) {
    physics.teleport(id, scene.world_transform(id));
  } else {
    // Turning must not stop a moving body (e.g. a player facing where it walks while jumping).
    physics.set_body_rotation(id, scene.world_transform(id).rotation);
  }
}

}  // namespace redgem::bodies
