#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "physics/body_config.hpp"
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

struct RaycastHit {
  ObjectId object = kNoObject;
  Vec3 point;
  Vec3 normal;
  float distance = 0.0f;
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

  virtual std::optional<RaycastHit> raycast(Vec3 origin, Vec3 direction, float max_distance, ObjectId ignore) const = 0;
};

std::unique_ptr<PhysicsWorld> create_jolt_physics_world(Vec3 gravity);

}  // namespace redgem
