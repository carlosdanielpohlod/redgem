#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "physics/physics_world.hpp"
#include "physics/step_worker.hpp"

namespace redgem {

// A physics world whose steps may run on a thread of their own while the thread that owns it goes
// on (drawing the frame before, say): begin_step() starts one, finish_step() waits for it. Every
// other call waits for the step under way first, so whoever uses it sees the world either before
// a step or after it, never in the middle of one: the step only overlaps the work that leaves the
// world alone. Only the thread that owns it calls it.
class OverlappedPhysicsWorld final : public PhysicsWorld {
 public:
  explicit OverlappedPhysicsWorld(std::unique_ptr<PhysicsWorld> inner);
  ~OverlappedPhysicsWorld() override = default;  // (the worker waits for its step first)
  OverlappedPhysicsWorld(const OverlappedPhysicsWorld&) = delete;
  OverlappedPhysicsWorld& operator=(const OverlappedPhysicsWorld&) = delete;

  // Starts a step on the worker thread and returns at once.
  void begin_step(float dt);
  // Returns once the step begun has ended (at once when there is none).
  void finish_step() const { worker_.wait(); }
  // Whether a step begun is still under way.
  bool stepping() const { return worker_.busy(); }

  std::optional<std::string> add_object(const BodyDesc& desc) override;
  void remove_object(ObjectId object) override;
  bool has_object(ObjectId object) const override;
  BodyType body_type(ObjectId object) const override;
  bool is_trigger(ObjectId object) const override;
  PhysicsDebugGeometry debug_geometry(std::size_t triangle_limit) const override;
  void set_target_pose(ObjectId object, const Transform& world) override;
  void teleport(ObjectId object, const Transform& world) override;
  void set_body_rotation(ObjectId object, Quat rotation) override;
  // A step on the calling thread, done when it returns.
  void step(float dt) override;
  std::optional<Transform> simulated_pose(ObjectId object) const override;
  std::vector<ContactEvent> drain_events() override;
  Vec3 velocity(ObjectId object) const override;
  void set_velocity(ObjectId object, Vec3 velocity) override;
  void add_impulse(ObjectId object, Vec3 impulse) override;
  void set_character_move(ObjectId object, Vec3 horizontal_velocity) override;
  void character_jump(ObjectId object, float speed) override;
  bool character_grounded(ObjectId object) const override;
  void set_vehicle_input(ObjectId object, const VehicleInput& input) override;
  float vehicle_forward_speed(ObjectId object) const override;
  std::vector<WheelPose> wheel_poses(ObjectId object) const override;
  void add_force(ObjectId object, Vec3 force) override;
  void add_force_at(ObjectId object, Vec3 force, Vec3 point) override;
  void add_torque(ObjectId object, Vec3 torque) override;
  Vec3 angular_velocity(ObjectId object) const override;
  void set_angular_velocity(ObjectId object, Vec3 velocity) override;
  float mass(ObjectId object) const override;
  void set_mass(ObjectId object, float mass) override;
  bool set_body_type(ObjectId object, BodyType type) override;
  float gravity_scale(ObjectId object) const override;
  void set_gravity_scale(ObjectId object, float scale) override;
  void set_sleeping(ObjectId object, bool sleeping) override;
  bool sleeping(ObjectId object) const override;
  const CollisionLayers& layers() const override;
  std::size_t layer(ObjectId object) const override;
  void set_layer(ObjectId object, std::size_t layer) override;
  using PhysicsWorld::raycast;
  std::optional<RaycastHit> raycast(Vec3 origin, Vec3 direction, float max_distance,
                                    const QueryFilter& filter) const override;
  std::vector<RaycastHit> raycast_all(Vec3 origin, Vec3 direction, float max_distance,
                                      const QueryFilter& filter) const override;
  std::optional<RaycastHit> shape_cast(const QueryShape& shape, Vec3 from, Vec3 to,
                                       const QueryFilter& filter) const override;
  std::vector<RaycastHit> overlap(const QueryShape& shape, Vec3 center, const QueryFilter& filter) const override;
  std::vector<RaycastHit> static_surfaces(Vec3 origin, Vec3 direction, float max_distance) const override;
  bool overlaps_static(const QueryShape& shape, Vec3 center) const override;
  Bounds static_bounds() const override;
  void optimize_broad_phase() override;
  std::variant<JointId, std::string> add_joint(const JointDesc& desc) override;
  void remove_joint(JointId joint) override;
  bool has_joint(JointId joint) const override;
  bool joint_broken(JointId joint) const override;
  void set_joint_enabled(JointId joint, bool enabled) override;
  bool joint_enabled(JointId joint) const override;
  void set_joint_break_force(JointId joint, float force) override;
  float joint_break_force(JointId joint) const override;
  void set_hinge_motor(JointId joint, float speed, float max_torque) override;
  float hinge_angle(JointId joint) const override;

 private:
  // The world, once no step is under way.
  PhysicsWorld& settled();
  const PhysicsWorld& settled() const;

  std::unique_ptr<PhysicsWorld> inner_;
  StepWorker worker_;  // after the world: it ends, and its step with it, before the world goes
};

}  // namespace redgem
