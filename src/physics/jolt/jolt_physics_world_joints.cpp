// Part of the Jolt world: joints, which hold two bodies together or one to the world, until either
// body goes, a script removes them, or they are pulled harder than they can take.

#include "physics/jolt/jolt_physics_world.hpp"

#include <Jolt/Physics/Constraints/HingeConstraint.h>

#include <iterator>

#include "physics/jolt/jolt_joints.hpp"

namespace redgem::jolt {
namespace {

// The world, as the other end of a joint: no body.
bool is_world(ObjectId object) { return object == kNoObject; }

// Whether a joint can break: it has a break force, and it was solved in the last step (Jolt leaves
// the impulses of a disabled joint, or of one between sleeping bodies, as they were).
bool can_break(const JointRecord& joint) { return joint.break_force > 0.0f && joint.constraint->IsActive(); }

// Whether a joint was pulled (or twisted) harder than it can take.
bool overloaded(const JointRecord& joint, float dt) {
  return can_break(joint) && joint_pull(*joint.constraint, joint.kind, dt) > joint.break_force;
}

// The second end of a joint is an object without a body (the world needs none).
bool lacks_body(ObjectId object, const Record* record) { return !is_world(object) && !has_body(record); }

// Neither end is moved by forces: nothing for a joint to do.
bool neither_dynamic(const Record* a, const Record* b) { return !is_dynamic_body(a) && !is_dynamic_body(b); }

bool is_hinge(const JointRecord* joint) { return joint != nullptr && joint->kind == JointKind::Hinge; }

}  // namespace

std::optional<std::string> World::joint_problem(const JointDesc& desc) const {
  const Record* a = find(desc.a);
  const Record* b = find(desc.b);
  if (!has_body(a)) return std::string("a joint holds rigid bodies: the first object has none");
  if (desc.a == desc.b) return std::string("a joint holds two objects: both ends are the same one");
  if (lacks_body(desc.b, b)) return std::string("a joint holds rigid bodies: the second object has none");
  if (neither_dynamic(a, b)) return std::string("a joint needs a dynamic body at one end at least");
  return std::nullopt;
}

std::variant<JointId, std::string> World::add_joint(const JointDesc& desc) {
  if (auto problem = joint_problem(desc)) return *problem;
  JPH::BodyInterface& bodies = system_.GetBodyInterface();
  const JPH::BodyID other = is_world(desc.b) ? JPH::BodyID() : body_of(desc.b);
  JointRecord joint{desc.kind, desc.a, desc.b, bodies.CreateConstraint(joint_settings(desc), other, body_of(desc.a)),
                    desc.break_force};
  start_motor(*joint.constraint, desc);
  system_.AddConstraint(joint.constraint);
  bodies.ActivateConstraint(joint.constraint);
  const JointId id = next_joint_++;
  joints_.emplace(id, std::move(joint));
  return id;
}

void World::remove_joint(JointId joint) {
  const auto it = joints_.find(joint);
  if (it == joints_.end()) return;
  system_.GetBodyInterface().ActivateConstraint(it->second.constraint);  // what it held starts moving again
  system_.RemoveConstraint(it->second.constraint);
  joints_.erase(it);
}

void World::remove_joints_of(ObjectId object) {
  for (auto it = joints_.begin(); it != joints_.end();) {
    const JointId id = it->first;
    it = std::next(it);
    if (find_joint(id)->holds(object)) remove_joint(id);
  }
}

void World::break_overloaded_joints(float dt) {
  for (auto it = joints_.begin(); it != joints_.end();) {
    const JointId id = it->first;
    const JointRecord& joint = it->second;
    it = std::next(it);
    if (!overloaded(joint, dt)) continue;
    remove_joint(id);
    broken_joints_.insert(id);
  }
}

void World::set_joint_enabled(JointId joint, bool enabled) {
  const JointRecord* record = find_joint(joint);
  if (record == nullptr) return;
  record->constraint->SetEnabled(enabled);
  system_.GetBodyInterface().ActivateConstraint(record->constraint);
}

bool World::joint_enabled(JointId joint) const {
  const JointRecord* record = find_joint(joint);
  return record != nullptr && record->constraint->GetEnabled();
}

void World::set_joint_break_force(JointId joint, float force) {
  const auto it = joints_.find(joint);
  if (it != joints_.end()) it->second.break_force = force;
}

float World::joint_break_force(JointId joint) const {
  const JointRecord* record = find_joint(joint);
  return record != nullptr ? record->break_force : 0.0f;
}

void World::set_hinge_motor(JointId joint, float speed, float max_torque) {
  const JointRecord* record = find_joint(joint);
  if (!is_hinge(record)) return;
  set_motor(*record->constraint, speed, max_torque);
  system_.GetBodyInterface().ActivateConstraint(record->constraint);
}

float World::hinge_angle(JointId joint) const {
  const JointRecord* record = find_joint(joint);
  if (!is_hinge(record)) return 0.0f;
  return static_cast<const JPH::HingeConstraint&>(*record->constraint).GetCurrentAngle();
}

}  // namespace redgem::jolt
