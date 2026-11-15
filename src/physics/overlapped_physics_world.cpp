#include "physics/overlapped_physics_world.hpp"

#include <utility>

namespace redgem {

OverlappedPhysicsWorld::OverlappedPhysicsWorld(std::unique_ptr<PhysicsWorld> inner) : inner_(std::move(inner)) {}

void OverlappedPhysicsWorld::begin_step(float dt) {
  PhysicsWorld* world = inner_.get();
  worker_.start([world, dt] { world->step(dt); });
}

PhysicsWorld& OverlappedPhysicsWorld::settled() {
  finish_step();
  return *inner_;
}

const PhysicsWorld& OverlappedPhysicsWorld::settled() const {
  finish_step();
  return *inner_;
}

void OverlappedPhysicsWorld::step(float dt) { settled().step(dt); }

// The rest waits for the step under way, then asks the world.

std::optional<std::string> OverlappedPhysicsWorld::add_object(const BodyDesc& desc) {
  return settled().add_object(desc);
}
void OverlappedPhysicsWorld::remove_object(ObjectId object) { settled().remove_object(object); }
bool OverlappedPhysicsWorld::has_object(ObjectId object) const { return settled().has_object(object); }
BodyType OverlappedPhysicsWorld::body_type(ObjectId object) const { return settled().body_type(object); }
bool OverlappedPhysicsWorld::is_trigger(ObjectId object) const { return settled().is_trigger(object); }
PhysicsDebugGeometry OverlappedPhysicsWorld::debug_geometry(std::size_t triangle_limit) const {
  return settled().debug_geometry(triangle_limit);
}
void OverlappedPhysicsWorld::set_target_pose(ObjectId object, const Transform& world) {
  settled().set_target_pose(object, world);
}
void OverlappedPhysicsWorld::teleport(ObjectId object, const Transform& world) { settled().teleport(object, world); }
void OverlappedPhysicsWorld::set_body_rotation(ObjectId object, Quat rotation) {
  settled().set_body_rotation(object, rotation);
}
std::optional<Transform> OverlappedPhysicsWorld::simulated_pose(ObjectId object) const {
  return settled().simulated_pose(object);
}
std::vector<ContactEvent> OverlappedPhysicsWorld::drain_events() { return settled().drain_events(); }
Vec3 OverlappedPhysicsWorld::velocity(ObjectId object) const { return settled().velocity(object); }
void OverlappedPhysicsWorld::set_velocity(ObjectId object, Vec3 velocity) { settled().set_velocity(object, velocity); }
void OverlappedPhysicsWorld::add_impulse(ObjectId object, Vec3 impulse) { settled().add_impulse(object, impulse); }
void OverlappedPhysicsWorld::set_character_move(ObjectId object, Vec3 horizontal_velocity) {
  settled().set_character_move(object, horizontal_velocity);
}
void OverlappedPhysicsWorld::character_jump(ObjectId object, float speed) { settled().character_jump(object, speed); }
bool OverlappedPhysicsWorld::character_grounded(ObjectId object) const { return settled().character_grounded(object); }
void OverlappedPhysicsWorld::set_vehicle_input(ObjectId object, const VehicleInput& input) {
  settled().set_vehicle_input(object, input);
}
float OverlappedPhysicsWorld::vehicle_forward_speed(ObjectId object) const {
  return settled().vehicle_forward_speed(object);
}
std::vector<WheelPose> OverlappedPhysicsWorld::wheel_poses(ObjectId object) const {
  return settled().wheel_poses(object);
}
void OverlappedPhysicsWorld::add_force(ObjectId object, Vec3 force) { settled().add_force(object, force); }
void OverlappedPhysicsWorld::add_force_at(ObjectId object, Vec3 force, Vec3 point) {
  settled().add_force_at(object, force, point);
}
void OverlappedPhysicsWorld::add_torque(ObjectId object, Vec3 torque) { settled().add_torque(object, torque); }
Vec3 OverlappedPhysicsWorld::angular_velocity(ObjectId object) const { return settled().angular_velocity(object); }
void OverlappedPhysicsWorld::set_angular_velocity(ObjectId object, Vec3 velocity) {
  settled().set_angular_velocity(object, velocity);
}
float OverlappedPhysicsWorld::mass(ObjectId object) const { return settled().mass(object); }
void OverlappedPhysicsWorld::set_mass(ObjectId object, float mass) { settled().set_mass(object, mass); }
bool OverlappedPhysicsWorld::set_body_type(ObjectId object, BodyType type) {
  return settled().set_body_type(object, type);
}
float OverlappedPhysicsWorld::gravity_scale(ObjectId object) const { return settled().gravity_scale(object); }
void OverlappedPhysicsWorld::set_gravity_scale(ObjectId object, float scale) {
  settled().set_gravity_scale(object, scale);
}
void OverlappedPhysicsWorld::set_sleeping(ObjectId object, bool sleeping) { settled().set_sleeping(object, sleeping); }
bool OverlappedPhysicsWorld::sleeping(ObjectId object) const { return settled().sleeping(object); }
const CollisionLayers& OverlappedPhysicsWorld::layers() const { return settled().layers(); }
std::size_t OverlappedPhysicsWorld::layer(ObjectId object) const { return settled().layer(object); }
void OverlappedPhysicsWorld::set_layer(ObjectId object, std::size_t layer) { settled().set_layer(object, layer); }
std::optional<RaycastHit> OverlappedPhysicsWorld::raycast(Vec3 origin, Vec3 direction, float max_distance,
                                                          const QueryFilter& filter) const {
  return settled().raycast(origin, direction, max_distance, filter);
}
std::vector<RaycastHit> OverlappedPhysicsWorld::raycast_all(Vec3 origin, Vec3 direction, float max_distance,
                                                            const QueryFilter& filter) const {
  return settled().raycast_all(origin, direction, max_distance, filter);
}
std::optional<RaycastHit> OverlappedPhysicsWorld::shape_cast(const QueryShape& shape, Vec3 from, Vec3 to,
                                                             const QueryFilter& filter) const {
  return settled().shape_cast(shape, from, to, filter);
}
std::vector<RaycastHit> OverlappedPhysicsWorld::overlap(const QueryShape& shape, Vec3 center,
                                                        const QueryFilter& filter) const {
  return settled().overlap(shape, center, filter);
}
std::vector<RaycastHit> OverlappedPhysicsWorld::static_surfaces(Vec3 origin, Vec3 direction, float max_distance) const {
  return settled().static_surfaces(origin, direction, max_distance);
}
bool OverlappedPhysicsWorld::overlaps_static(const QueryShape& shape, Vec3 center) const {
  return settled().overlaps_static(shape, center);
}
Bounds OverlappedPhysicsWorld::static_bounds() const { return settled().static_bounds(); }
void OverlappedPhysicsWorld::optimize_broad_phase() { settled().optimize_broad_phase(); }
std::variant<JointId, std::string> OverlappedPhysicsWorld::add_joint(const JointDesc& desc) {
  return settled().add_joint(desc);
}
void OverlappedPhysicsWorld::remove_joint(JointId joint) { settled().remove_joint(joint); }
bool OverlappedPhysicsWorld::has_joint(JointId joint) const { return settled().has_joint(joint); }
bool OverlappedPhysicsWorld::joint_broken(JointId joint) const { return settled().joint_broken(joint); }
void OverlappedPhysicsWorld::set_joint_enabled(JointId joint, bool enabled) {
  settled().set_joint_enabled(joint, enabled);
}
bool OverlappedPhysicsWorld::joint_enabled(JointId joint) const { return settled().joint_enabled(joint); }
void OverlappedPhysicsWorld::set_joint_break_force(JointId joint, float force) {
  settled().set_joint_break_force(joint, force);
}
float OverlappedPhysicsWorld::joint_break_force(JointId joint) const { return settled().joint_break_force(joint); }
void OverlappedPhysicsWorld::set_hinge_motor(JointId joint, float speed, float max_torque) {
  settled().set_hinge_motor(joint, speed, max_torque);
}
float OverlappedPhysicsWorld::hinge_angle(JointId joint) const { return settled().hinge_angle(joint); }

}  // namespace redgem
