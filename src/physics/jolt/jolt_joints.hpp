#pragma once

// Joints as Jolt has them: the constraint settings a joint is made from, and how hard a joint is
// being pulled apart.

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>

#include "physics/physics_world.hpp"

namespace redgem::jolt {

// What Jolt makes a joint from, every point and axis in world space. Jolt's first body is the
// joint's other end (`b`, or the world) and its second the object (`a`), so that a hinge's angle
// and a slider's position say how far the object turned or slid from where it started, along the
// axis as given.
JPH::Ref<JPH::TwoBodyConstraintSettings> joint_settings(const JointDesc& desc);

// After it was made: a hinge's motor, if it was asked one.
void start_motor(JPH::TwoBodyConstraint& constraint, const JointDesc& desc);
void set_motor(JPH::TwoBodyConstraint& hinge, float speed, float max_torque);

// The force (newtons) that held the joint together during the last step of `dt` seconds.
float joint_pull(const JPH::TwoBodyConstraint& constraint, JointKind kind, float dt);

// Any axis at right angles to `axis` (which need not be of unit length).
Vec3 perpendicular_to(Vec3 axis);

}  // namespace redgem::jolt
