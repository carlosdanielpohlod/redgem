#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "physics/body_config.hpp"
#include "physics/collision_layers.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem {

struct WheelDesc {
  ObjectHandle object;  // the child object that renders the wheel (kept for the vehicle's life)
  Vec3 center;          // wheel center in the chassis' (unscaled) local space
  float radius = 0.0f;
  float width = 0.0f;
};

struct BodyDesc {
  ObjectId object = kNoObject;
  BodyConfig config;
  Transform world;                 // includes scale, baked into the collider
  Bounds bounds;                   // local mesh bounds; drive the collider size
  const MeshDesc* mesh = nullptr;  // required for mesh colliders
  std::vector<WheelDesc> wheels;   // vehicles only
};

// Driver controls, each in -1..1 (brakes 0..1). Positive steering turns right.
struct VehicleInput {
  float throttle = 0.0f;
  float steering = 0.0f;
  float brake = 0.0f;
  float handbrake = 0.0f;
};

struct WheelPose {
  ObjectHandle object;
  Transform world;  // scale is not tracked
};

enum class ContactPhase { Enter, Exit };

struct ContactEvent {
  ObjectId self = kNoObject;  // for triggers: the trigger volume
  ObjectId other = kNoObject;
  ContactPhase phase = ContactPhase::Enter;
  bool trigger = false;
};

// What a ray, a cast or an overlap found: the object, a point of its surface there, the surface's
// normal (pointing out of it) and how far along the query that is.
struct RaycastHit {
  ObjectId object = kNoObject;
  Vec3 point;
  Vec3 normal;
  float distance = 0.0f;
};

// What a query looks at: bodies on the layers of `layers`, but none of the objects in `ignore`.
struct QueryFilter {
  LayerMask layers = kAllLayers;
  std::vector<ObjectId> ignore;
};

// The shape swept by a cast, or tested by an overlap.
enum class QueryShapeKind { Sphere, Box, Capsule };
struct QueryShape {
  QueryShapeKind kind = QueryShapeKind::Sphere;
  float radius = 0.0f;  // spheres and capsules
  float height = 0.0f;  // capsules, from end to end along their own y
  Vec3 half_extents;    // boxes
  Quat rotation = Quat::identity();
};

// Physics backend as seen by the engine. Static, kinematic and trigger poses follow the scene;
// dynamic bodies, vehicles and characters drive the scene.
class PhysicsWorld {
 public:
  virtual ~PhysicsWorld() = default;

  // Returns an error message when the body cannot be created.
  virtual std::optional<std::string> add_object(const BodyDesc& desc) = 0;
  virtual void remove_object(ObjectId object) = 0;
  virtual bool has_object(ObjectId object) const = 0;
  virtual BodyType body_type(ObjectId object) const = 0;
  virtual bool is_trigger(ObjectId object) const = 0;

  // Kinematic bodies move there during the next step; trigger volumes are placed there.
  virtual void set_target_pose(ObjectId object, const Transform& world) = 0;
  // Instantly moves a body (e.g. a script assigned its position). Moving bodies also stop.
  virtual void teleport(ObjectId object, const Transform& world) = 0;
  // Turns a body in place, keeping its velocity (characters stay upright and ignore it).
  virtual void set_body_rotation(ObjectId object, Quat rotation) = 0;
  virtual void step(float dt) = 0;
  // Current simulated pose of dynamic bodies and characters (scale is not tracked).
  virtual std::optional<Transform> simulated_pose(ObjectId object) const = 0;
  // Contact and trigger enter/exit events produced by the last steps, in deterministic order.
  virtual std::vector<ContactEvent> drain_events() = 0;

  // Rigid bodies.
  virtual Vec3 velocity(ObjectId object) const = 0;
  virtual void set_velocity(ObjectId object, Vec3 velocity) = 0;
  virtual void add_impulse(ObjectId object, Vec3 impulse) = 0;

  // Characters. The horizontal velocity persists until changed; gravity is applied by the engine.
  virtual void set_character_move(ObjectId object, Vec3 horizontal_velocity) = 0;
  // Jumps on the next step if the character is grounded then.
  virtual void character_jump(ObjectId object, float speed) = 0;
  virtual bool character_grounded(ObjectId object) const = 0;

  // Vehicles. Wheels are driven by the simulation and reported for rendering.
  virtual void set_vehicle_input(ObjectId object, const VehicleInput& input) = 0;
  // Signed speed along the vehicle's forward axis, in m/s.
  virtual float vehicle_forward_speed(ObjectId object) const = 0;
  virtual std::vector<WheelPose> wheel_poses(ObjectId object) const = 0;

  // Forces on rigid bodies; anything else is left alone. A force (and a torque) acts during the next
  // step only: keep adding it every fixed_update to keep pushing.
  virtual void add_force(ObjectId object, Vec3 force) = 0;
  // A force pushing at a point (world space): off the center of mass, it also turns the body.
  virtual void add_force_at(ObjectId object, Vec3 force, Vec3 point) = 0;
  virtual void add_torque(ObjectId object, Vec3 torque) = 0;
  // Radians per second around each axis.
  virtual Vec3 angular_velocity(ObjectId object) const = 0;
  virtual void set_angular_velocity(ObjectId object, Vec3 velocity) = 0;
  virtual float mass(ObjectId object) const = 0;
  // Its inertia scales with it, so the body keeps how it turns.
  virtual void set_mass(ObjectId object, float mass) = 0;
  // Turns a dynamic body kinematic, or a kinematic one dynamic; false (and nothing changes) for
  // any other body or type.
  virtual bool set_body_type(ObjectId object, BodyType type) = 0;
  // How much of the world's gravity pulls the body: 0 floats, 1 is normal.
  virtual float gravity_scale(ObjectId object) const = 0;
  virtual void set_gravity_scale(ObjectId object, float scale) = 0;
  // A sleeping body is not simulated until something touches it, or it is woken.
  virtual void set_sleeping(ObjectId object, bool sleeping) = 0;
  virtual bool sleeping(ObjectId object) const = 0;

  // Collision layers: the game's, and the one each object is on (kDefaultLayer for an object
  // without a body). A layer that is not one of the game's is ignored.
  virtual const CollisionLayers& layers() const = 0;
  virtual std::size_t layer(ObjectId object) const = 0;
  virtual void set_layer(ObjectId object, std::size_t layer) = 0;

  // Queries. Triggers are never found; hits come nearest first (the same object once, at its
  // nearest), at most config::kMaxQueryHits of them.
  virtual std::optional<RaycastHit> raycast(Vec3 origin, Vec3 direction, float max_distance,
                                            const QueryFilter& filter) const = 0;
  std::optional<RaycastHit> raycast(Vec3 origin, Vec3 direction, float max_distance, ObjectId ignore) const {
    return raycast(origin, direction, max_distance, QueryFilter{kAllLayers, {ignore}});
  }
  virtual std::vector<RaycastHit> raycast_all(Vec3 origin, Vec3 direction, float max_distance,
                                              const QueryFilter& filter) const = 0;
  // The first thing a shape moving from `from` to `to` touches (distance along the way; a hit at
  // 0 overlaps it at the start).
  virtual std::optional<RaycastHit> shape_cast(const QueryShape& shape, Vec3 from, Vec3 to,
                                               const QueryFilter& filter) const = 0;
  // What a shape at `center` overlaps (distance from the center to each point).
  virtual std::vector<RaycastHit> overlap(const QueryShape& shape, Vec3 center, const QueryFilter& filter) const = 0;
};

std::unique_ptr<PhysicsWorld> create_jolt_physics_world(Vec3 gravity, const CollisionLayers& layers = {});

}  // namespace redgem
