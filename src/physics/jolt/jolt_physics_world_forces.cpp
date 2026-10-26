// Part of the Jolt world: what scripts do to rigid bodies besides moving them. Forces and torques,
// spin, mass, gravity, sleep, and switching between dynamic and kinematic.

#include "physics/jolt/jolt_physics_world.hpp"

#include <Jolt/Physics/Body/BodyLock.h>

namespace redgem::jolt {
namespace {

// The body types a script may switch between.
bool switchable(BodyType type) { return type == BodyType::Dynamic || type == BodyType::Kinematic; }

bool can_switch(const Record* record, BodyType to) {
  return has_body(record) && switchable(record->type) && switchable(to);
}

JPH::EMotionType motion_for(BodyType type) {
  return type == BodyType::Dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Kinematic;
}

// Gravity at full strength: what a body that cannot move is pulled by, for all it matters.
constexpr float kFullGravity = 1.0f;

}  // namespace

void World::add_force(ObjectId object, Vec3 force) {
  const Record* record = find(object);
  if (is_dynamic_body(record)) system_.GetBodyInterface().AddForce(record->body, to_jolt(force));
}

void World::add_force_at(ObjectId object, Vec3 force, Vec3 point) {
  const Record* record = find(object);
  if (!is_dynamic_body(record)) return;
  system_.GetBodyInterface().AddForce(record->body, to_jolt(force), JPH::RVec3(to_jolt(point)));
}

void World::add_torque(ObjectId object, Vec3 torque) {
  const Record* record = find(object);
  if (is_dynamic_body(record)) system_.GetBodyInterface().AddTorque(record->body, to_jolt(torque));
}

Vec3 World::angular_velocity(ObjectId object) const {
  const Record* record = find(object);
  if (!is_movable_body(record)) return Vec3::zero();
  return from_jolt(system_.GetBodyInterface().GetAngularVelocity(record->body));
}

void World::set_angular_velocity(ObjectId object, Vec3 velocity) {
  const Record* record = find(object);
  if (is_movable_body(record)) system_.GetBodyInterface().SetAngularVelocity(record->body, to_jolt(velocity));
}

float World::mass(ObjectId object) const {
  const Record* record = find(object);
  if (!is_movable_body(record)) return 0.0f;
  const JPH::BodyLockRead lock(system_.GetBodyLockInterface(), record->body);
  if (!lock.Succeeded()) return 0.0f;
  return 1.0f / lock.GetBody().GetMotionPropertiesUnchecked()->GetInverseMassUnchecked();
}

void World::set_mass(ObjectId object, float mass) {
  const Record* record = find(object);
  if (!is_movable_body(record)) return;
  const JPH::BodyLockWrite lock(system_.GetBodyLockInterface(), record->body);
  if (lock.Succeeded()) lock.GetBody().GetMotionPropertiesUnchecked()->ScaleToMass(mass);
}

bool World::set_body_type(ObjectId object, BodyType type) {
  Record* record = find(object);
  if (!can_switch(record, type)) return false;
  if (record->type == type) return true;  // already that: a body keeps moving as it did
  JPH::BodyInterface& bodies = system_.GetBodyInterface();
  bodies.SetMotionType(record->body, motion_for(type), JPH::EActivation::Activate);
  // A kinematic body's speed is whatever its last move took (a jump across the level in one step):
  // made dynamic, it starts at rest instead of flying off with it.
  bodies.SetLinearAndAngularVelocity(record->body, JPH::Vec3::sZero(), JPH::Vec3::sZero());
  record->type = type;
  record->has_target = false;  // a kinematic move asked for before the switch is not made
  return true;
}

float World::gravity_scale(ObjectId object) const {
  const Record* record = find(object);
  if (!is_movable_body(record)) return kFullGravity;
  return system_.GetBodyInterface().GetGravityFactor(record->body);
}

void World::set_gravity_scale(ObjectId object, float scale) {
  const Record* record = find(object);
  if (is_movable_body(record)) system_.GetBodyInterface().SetGravityFactor(record->body, scale);
}

void World::set_sleeping(ObjectId object, bool sleeping) {
  const Record* record = find(object);
  if (!is_movable_body(record)) return;
  JPH::BodyInterface& bodies = system_.GetBodyInterface();
  if (sleeping) {
    bodies.DeactivateBody(record->body);
  } else {
    bodies.ActivateBody(record->body);
  }
}

bool World::sleeping(ObjectId object) const {
  const Record* record = find(object);
  return is_movable_body(record) && !system_.GetBodyInterface().IsActive(record->body);
}

bool World::at_rest(ObjectId object) const {
  const Record* record = find(object);
  return has_body(record) && !is_character(record) && !system_.GetBodyInterface().IsActive(record->body);
}

}  // namespace redgem::jolt
