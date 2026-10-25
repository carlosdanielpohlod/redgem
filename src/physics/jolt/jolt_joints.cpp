#include "physics/jolt/jolt_joints.hpp"

#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>

#include <cmath>

#include "physics/jolt/jolt_shapes.hpp"

namespace redgem::jolt {
namespace {

// Below this, an axis is too close to the world's x axis to be crossed with it.
constexpr float kParallelCosine = 0.9f;

JPH::RVec3 point_of(Vec3 point) { return JPH::RVec3(to_jolt(point)); }

JPH::Ref<JPH::TwoBodyConstraintSettings> fixed_settings(const JointDesc& desc) {
  auto* settings = new JPH::FixedConstraintSettings();
  settings->mSpace = JPH::EConstraintSpace::WorldSpace;
  settings->mPoint1 = settings->mPoint2 = point_of(desc.anchor);
  return settings;
}

JPH::Ref<JPH::TwoBodyConstraintSettings> hinge_settings(const JointDesc& desc) {
  auto* settings = new JPH::HingeConstraintSettings();
  settings->mSpace = JPH::EConstraintSpace::WorldSpace;
  settings->mPoint1 = settings->mPoint2 = point_of(desc.anchor);
  settings->mHingeAxis1 = settings->mHingeAxis2 = to_jolt(normalize(desc.axis));
  settings->mNormalAxis1 = settings->mNormalAxis2 = to_jolt(perpendicular_to(desc.axis));
  if (desc.limits) {
    settings->mLimitsMin = desc.limits->first;
    settings->mLimitsMax = desc.limits->second;
  }
  settings->mMotorSettings.SetTorqueLimit(desc.motor_torque);
  return settings;
}

JPH::Ref<JPH::TwoBodyConstraintSettings> slider_settings(const JointDesc& desc) {
  auto* settings = new JPH::SliderConstraintSettings();
  settings->mSpace = JPH::EConstraintSpace::WorldSpace;
  settings->mPoint1 = settings->mPoint2 = point_of(desc.anchor);
  settings->mSliderAxis1 = settings->mSliderAxis2 = to_jolt(normalize(desc.axis));
  settings->mNormalAxis1 = settings->mNormalAxis2 = to_jolt(perpendicular_to(desc.axis));
  if (desc.limits) {
    settings->mLimitsMin = desc.limits->first;
    settings->mLimitsMax = desc.limits->second;
  }
  return settings;
}

JPH::Ref<JPH::TwoBodyConstraintSettings> point_settings(const JointDesc& desc) {
  auto* settings = new JPH::PointConstraintSettings();
  settings->mSpace = JPH::EConstraintSpace::WorldSpace;
  settings->mPoint1 = settings->mPoint2 = point_of(desc.anchor);
  return settings;
}

JPH::Ref<JPH::TwoBodyConstraintSettings> distance_settings(const JointDesc& desc) {
  auto* settings = new JPH::DistanceConstraintSettings();
  settings->mSpace = JPH::EConstraintSpace::WorldSpace;
  settings->mPoint1 = point_of(desc.anchor_b);
  settings->mPoint2 = point_of(desc.anchor);
  settings->mMinDistance = desc.min_distance;
  settings->mMaxDistance = desc.max_distance;
  settings->mLimitsSpringSettings =
      JPH::SpringSettings(JPH::ESpringMode::FrequencyAndDamping, desc.spring_frequency, desc.spring_damping);
  return settings;
}

// A slider's position impulse: across its axis, two ways.
constexpr JPH::uint kSliderLambdaRows = 2;
using SliderLambda = JPH::Vector<kSliderLambdaRows>;

float length_of(const SliderLambda& lambda) { return std::sqrt(lambda[0] * lambda[0] + lambda[1] * lambda[1]); }

// The impulse (newton seconds) that held the joint together during the last step.
float held_impulse(const JPH::TwoBodyConstraint& constraint, JointKind kind) {
  switch (kind) {
    case JointKind::Fixed:
      return static_cast<const JPH::FixedConstraint&>(constraint).GetTotalLambdaPosition().Length();
    case JointKind::Hinge:
      return static_cast<const JPH::HingeConstraint&>(constraint).GetTotalLambdaPosition().Length();
    case JointKind::Slider:
      return length_of(static_cast<const JPH::SliderConstraint&>(constraint).GetTotalLambdaPosition());
    case JointKind::Point:
      return static_cast<const JPH::PointConstraint&>(constraint).GetTotalLambdaPosition().Length();
    case JointKind::Distance:
      return std::fabs(static_cast<const JPH::DistanceConstraint&>(constraint).GetTotalLambdaPosition());
  }
  return 0.0f;
}

}  // namespace

JPH::Ref<JPH::TwoBodyConstraintSettings> joint_settings(const JointDesc& desc) {
  switch (desc.kind) {
    case JointKind::Hinge:
      return hinge_settings(desc);
    case JointKind::Slider:
      return slider_settings(desc);
    case JointKind::Point:
      return point_settings(desc);
    case JointKind::Distance:
      return distance_settings(desc);
    case JointKind::Fixed:
      break;
  }
  return fixed_settings(desc);
}

void start_motor(JPH::TwoBodyConstraint& constraint, const JointDesc& desc) {
  if (desc.kind == JointKind::Hinge) set_motor(constraint, desc.motor_speed, desc.motor_torque);
}

void set_motor(JPH::TwoBodyConstraint& hinge, float speed, float max_torque) {
  auto& joint = static_cast<JPH::HingeConstraint&>(hinge);
  joint.GetMotorSettings().SetTorqueLimit(max_torque);
  joint.SetTargetAngularVelocity(speed);
  joint.SetMotorState(max_torque > 0.0f ? JPH::EMotorState::Velocity : JPH::EMotorState::Off);
}

float joint_pull(const JPH::TwoBodyConstraint& constraint, JointKind kind, float dt) {
  return dt > 0.0f ? held_impulse(constraint, kind) / dt : 0.0f;
}

Vec3 perpendicular_to(Vec3 axis) {
  const Vec3 unit = normalize(axis);
  const Vec3 across = std::fabs(unit.x) < kParallelCosine ? Vec3::right() : Vec3::up();
  return normalize(cross(unit, across));
}

}  // namespace redgem::jolt
