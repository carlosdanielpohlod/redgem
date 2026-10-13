#include "physics/jolt/jolt_vehicle.hpp"

#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include "physics/physics_config.hpp"
#include "physics/jolt/jolt_shapes.hpp"

namespace redgem::jolt {
namespace {

constexpr float kHalf = 0.5f;
constexpr int kNeutralGear = 0;
constexpr float kClutchDisengaged = 0.0f;

bool is_front(const WheelDesc& wheel) { return wheel.center.z < 0.0f; }
bool is_left(const WheelDesc& wheel) { return wheel.center.x < 0.0f; }

AxleLayout::Axle pair_sides(const std::vector<WheelDesc>& wheels, const std::vector<int>& indices) {
  AxleLayout::Axle axle;
  for (const int index : indices) {
    int& side = is_left(wheels[index]) ? axle.left : axle.right;
    if (side == -1) side = index;
  }
  return axle;
}

// Suspension hangs from above the modeled wheel center so the car rests near its Blender pose.
constexpr float kSuspensionRestLength =
    (config::kVehicleSuspensionMinLength + config::kVehicleSuspensionMaxLength) * kHalf;

// Front wheels steer, rear wheels have the hand brake.
JPH::Ref<JPH::WheelSettings> wheel_settings(const WheelDesc& wheel) {
  const JPH::Vec3 up = to_jolt(Vec3::up());
  const bool front = is_front(wheel);
  JPH::Ref<JPH::WheelSettingsWV> settings = new JPH::WheelSettingsWV();
  settings->mPosition = to_jolt(wheel.center) + up * kSuspensionRestLength;
  settings->mWheelForward = to_jolt(Vec3::forward());
  settings->mWheelUp = up;
  settings->mRadius = wheel.radius;
  settings->mWidth = wheel.width;
  settings->mSuspensionMinLength = config::kVehicleSuspensionMinLength;
  settings->mSuspensionMaxLength = config::kVehicleSuspensionMaxLength;
  settings->mMaxSteerAngle = front ? JPH::DegreesToRadians(config::kVehicleMaxSteerDegrees) : 0.0f;
  settings->mMaxHandBrakeTorque = front ? 0.0f : config::kVehicleHandBrakeTorque;
  return JPH::Ref<JPH::WheelSettings>(settings.GetPtr());
}

// An engine whose torque is shared equally between the axles.
JPH::Ref<JPH::WheeledVehicleControllerSettings> controller_settings(const std::vector<WheelDesc>& wheels) {
  JPH::Ref<JPH::WheeledVehicleControllerSettings> controller = new JPH::WheeledVehicleControllerSettings();
  controller->mEngine.mMaxTorque = config::kVehicleEngineMaxTorque;
  const AxleLayout layout = layout_axles(wheels);
  const float torque_share = layout.axles.empty() ? 0.0f : 1.0f / static_cast<float>(layout.axles.size());
  for (const AxleLayout::Axle& axle : layout.axles) {
    JPH::VehicleDifferentialSettings differential;
    differential.mLeftWheel = axle.left;
    differential.mRightWheel = axle.right;
    differential.mEngineTorqueRatio = torque_share;
    controller->mDifferentials.push_back(differential);
  }
  return controller;
}

}  // namespace

AxleLayout layout_axles(const std::vector<WheelDesc>& wheels) {
  AxleLayout layout;
  for (int i = 0; i < static_cast<int>(wheels.size()); ++i) {
    (is_front(wheels[i]) ? layout.front_wheels : layout.rear_wheels).push_back(i);
  }
  if (!layout.front_wheels.empty()) layout.axles.push_back(pair_sides(wheels, layout.front_wheels));
  if (!layout.rear_wheels.empty()) layout.axles.push_back(pair_sides(wheels, layout.rear_wheels));
  return layout;
}

JPH::Ref<JPH::VehicleConstraint> build_vehicle(JPH::Body& chassis, const std::vector<WheelDesc>& wheels,
                                               JPH::ObjectLayer layer) {
  JPH::VehicleConstraintSettings settings;
  settings.mUp = to_jolt(Vec3::up());
  settings.mForward = to_jolt(Vec3::forward());
  settings.mMaxPitchRollAngle = JPH::DegreesToRadians(config::kVehicleMaxPitchRollDegrees);
  for (const WheelDesc& wheel : wheels) settings.mWheels.push_back(wheel_settings(wheel));
  settings.mController = controller_settings(wheels);

  JPH::Ref<JPH::VehicleConstraint> vehicle = new JPH::VehicleConstraint(chassis, settings);
  vehicle->SetVehicleCollisionTester(
      new JPH::VehicleCollisionTesterCastCylinder(layer, config::kVehicleWheelConvexRadiusFraction));
  return vehicle;
}

void stop_vehicle(JPH::VehicleConstraint& vehicle) {
  for (JPH::uint i = 0; i < static_cast<JPH::uint>(vehicle.GetWheels().size()); ++i) {
    vehicle.GetWheel(i)->SetAngularVelocity(0.0f);
  }
  auto* controller = static_cast<JPH::WheeledVehicleController*>(vehicle.GetController());
  controller->SetDriverInput(0.0f, 0.0f, 0.0f, 0.0f);
  controller->GetEngine().SetCurrentRPM(controller->GetEngine().mMinRPM);
  controller->GetTransmission().Set(kNeutralGear, kClutchDisengaged);
}

}  // namespace redgem::jolt
