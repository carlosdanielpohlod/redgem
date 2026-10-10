#include "physics/jolt/jolt_vehicle.hpp"

#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include "core/config.hpp"
#include "physics/jolt/jolt_shapes.hpp"

namespace redgem::jolt {
namespace {

constexpr float kHalf = 0.5f;

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
  const JPH::Vec3 up = to_jolt(Vec3::up());
  const JPH::Vec3 forward = to_jolt(Vec3::forward());
  // Suspension hangs from above the modeled wheel center so the car rests near its Blender pose.
  const float rest_length = (config::kVehicleSuspensionMinLength + config::kVehicleSuspensionMaxLength) * kHalf;

  JPH::VehicleConstraintSettings settings;
  settings.mUp = up;
  settings.mForward = forward;
  settings.mMaxPitchRollAngle = JPH::DegreesToRadians(config::kVehicleMaxPitchRollDegrees);

  for (const WheelDesc& wheel : wheels) {
    JPH::Ref<JPH::WheelSettingsWV> settings_wheel = new JPH::WheelSettingsWV();
    settings_wheel->mPosition = to_jolt(wheel.center) + up * rest_length;
    settings_wheel->mWheelForward = forward;
    settings_wheel->mWheelUp = up;
    settings_wheel->mRadius = wheel.radius;
    settings_wheel->mWidth = wheel.width;
    settings_wheel->mSuspensionMinLength = config::kVehicleSuspensionMinLength;
    settings_wheel->mSuspensionMaxLength = config::kVehicleSuspensionMaxLength;
    const bool front = is_front(wheel);
    settings_wheel->mMaxSteerAngle = front ? JPH::DegreesToRadians(config::kVehicleMaxSteerDegrees) : 0.0f;
    settings_wheel->mMaxHandBrakeTorque = front ? 0.0f : config::kVehicleHandBrakeTorque;
    settings.mWheels.push_back(JPH::Ref<JPH::WheelSettings>(settings_wheel.GetPtr()));
  }

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
  settings.mController = controller;

  JPH::Ref<JPH::VehicleConstraint> vehicle = new JPH::VehicleConstraint(chassis, settings);
  vehicle->SetVehicleCollisionTester(
      new JPH::VehicleCollisionTesterCastCylinder(layer, config::kVehicleWheelConvexRadiusFraction));
  return vehicle;
}

}  // namespace redgem::jolt
