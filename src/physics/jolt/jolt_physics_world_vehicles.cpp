// Part of the Jolt world: vehicles, a chassis body with wheels the simulation drives.

#include "physics/jolt/jolt_physics_world.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include "physics/jolt/jolt_vehicle.hpp"

namespace redgem::jolt {
namespace {

WheelPose wheel_pose(const Record& record, std::size_t wheel) {
  // Wheel meshes are modeled with their axle along X and Y up.
  const JPH::RMat44 matrix = record.vehicle->GetWheelWorldTransform(static_cast<JPH::uint>(wheel),
                                                                    to_jolt(Vec3::right()), to_jolt(Vec3::up()));
  WheelPose pose;
  pose.object = record.wheel_objects[wheel];
  pose.world.position = from_jolt(JPH::Vec3(matrix.GetTranslation()));
  pose.world.rotation = from_jolt(matrix.GetQuaternion());
  return pose;
}

}  // namespace

void World::attach_vehicle(const BodyDesc& desc, Record& record) {
  JPH::BodyLockWrite lock(system_.GetBodyLockInterface(), record.body);
  if (!lock.Succeeded()) return;
  record.vehicle = build_vehicle(lock.GetBody(), desc.wheels, layers::kMoving);
  for (const WheelDesc& wheel : desc.wheels) record.wheel_objects.push_back(wheel.object);
  system_.AddConstraint(record.vehicle.GetPtr());
  system_.AddStepListener(record.vehicle.GetPtr());
}

void World::release_vehicle(Record& record) {
  if (!is_vehicle(&record)) return;
  system_.RemoveStepListener(record.vehicle.GetPtr());
  system_.RemoveConstraint(record.vehicle.GetPtr());
  record.vehicle = nullptr;
}

void World::set_vehicle_input(ObjectId object, const VehicleInput& input) {
  const Record* record = find(object);
  if (!is_vehicle(record)) return;
  auto* controller = static_cast<JPH::WheeledVehicleController*>(record->vehicle->GetController());
  controller->SetDriverInput(input.throttle, input.steering, input.brake, input.handbrake);
}

float World::vehicle_forward_speed(ObjectId object) const {
  const Record* record = find(object);
  if (!is_vehicle(record)) return 0.0f;
  const JPH::BodyInterface& bodies = system_.GetBodyInterface();
  const JPH::Vec3 forward = bodies.GetRotation(record->body) * record->vehicle->GetLocalForward();
  return bodies.GetLinearVelocity(record->body).Dot(forward);
}

std::vector<WheelPose> World::wheel_poses(ObjectId object) const {
  std::vector<WheelPose> poses;
  const Record* record = find(object);
  if (!is_vehicle(record)) return poses;
  poses.reserve(record->wheel_objects.size());
  for (std::size_t i = 0; i < record->wheel_objects.size(); ++i) poses.push_back(wheel_pose(*record, i));
  return poses;
}

}  // namespace redgem::jolt
