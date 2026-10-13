// Part of the Jolt world: rigid bodies, from the ground that never moves to what forces push around.

#include "physics/jolt/jolt_physics_world.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>

#include "physics/jolt/jolt_vehicle.hpp"
#include "physics/physics_config.hpp"

namespace redgem::jolt {
namespace {

JPH::EMotionType motion_of(BodyType type) {
  if (is_dynamic(type)) return JPH::EMotionType::Dynamic;
  return type == BodyType::Kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static;
}

// A low center of mass keeps the car on its wheels.
JPH::RefConst<JPH::Shape> chassis_shape(const ShapeInfo& info) {
  const JPH::Vec3 drop(0.0f, -info.half_extents.y * config::kVehicleCenterOfMassDrop, 0.0f);
  return JPH::OffsetCenterOfMassShapeSettings(drop, info.shape).Create().Get();
}

JPH::BodyCreationSettings body_settings(const BodyDesc& desc, const JPH::RefConst<JPH::Shape>& shape,
                                        JPH::EMotionType motion) {
  const JPH::ObjectLayer layer = motion == JPH::EMotionType::Static ? layers::kNonMoving : layers::kMoving;
  JPH::BodyCreationSettings settings(shape, JPH::RVec3(to_jolt(desc.world.position)), to_jolt(desc.world.rotation),
                                     motion, layer);
  settings.mUserData = static_cast<JPH::uint64>(desc.object);
  // Contacts are tracked every step, so bodies must never fall asleep.
  settings.mAllowSleeping = false;
  if (motion == JPH::EMotionType::Dynamic) {
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = desc.config.mass;
  }
  return settings;
}

}  // namespace

void World::create_body(const BodyDesc& desc, const ShapeInfo& info, Record& record) {
  const bool vehicle = desc.config.type == BodyType::Vehicle;
  const JPH::EMotionType motion = motion_of(desc.config.type);
  const JPH::RefConst<JPH::Shape> shape = vehicle ? chassis_shape(info) : info.shape;
  const JPH::EActivation activation =
      motion == JPH::EMotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
  record.body = system_.GetBodyInterface().CreateAndAddBody(body_settings(desc, shape, motion), activation);
  body_owners_[record.body.GetIndexAndSequenceNumber()] = desc.object;
  if (vehicle) attach_vehicle(desc, record);
}

void World::release_body(Record& record) {
  if (!has_body(&record)) return;
  body_owners_.erase(record.body.GetIndexAndSequenceNumber());
  system_.GetBodyInterface().RemoveBody(record.body);
  system_.GetBodyInterface().DestroyBody(record.body);
}

void World::teleport_body(Record& record, const Transform& world) {
  JPH::BodyInterface& bodies = system_.GetBodyInterface();
  bodies.SetPositionAndRotation(record.body, JPH::RVec3(to_jolt(world.position)), to_jolt(world.rotation),
                                JPH::EActivation::Activate);
  if (is_dynamic(record.type)) bodies.SetLinearAndAngularVelocity(record.body, JPH::Vec3::sZero(), JPH::Vec3::sZero());
  if (is_vehicle(&record)) stop_vehicle(*record.vehicle);
}

void World::set_body_rotation(ObjectId object, Quat rotation) {
  Record* record = find(object);
  if (!has_body(record)) return;
  record->pose.rotation = rotation;
  system_.GetBodyInterface().SetRotation(record->body, to_jolt(rotation), JPH::EActivation::Activate);
}

Vec3 World::velocity(ObjectId object) const {
  const Record* record = find(object);
  if (is_character(record)) return from_jolt(record->character->GetLinearVelocity());
  if (!has_body(record)) return Vec3::zero();
  return from_jolt(system_.GetBodyInterface().GetLinearVelocity(record->body));
}

void World::set_velocity(ObjectId object, Vec3 velocity) {
  const Record* record = find(object);
  if (!has_body(record)) return;
  system_.GetBodyInterface().SetLinearVelocity(record->body, to_jolt(velocity));
}

void World::add_impulse(ObjectId object, Vec3 impulse) {
  const Record* record = find(object);
  if (!is_dynamic_body(record)) return;
  system_.GetBodyInterface().AddImpulse(record->body, to_jolt(impulse));
}

}  // namespace redgem::jolt
