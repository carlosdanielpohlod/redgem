#include "physics/jolt/jolt_physics_world.hpp"

#include <utility>

#include "physics/physics_config.hpp"

namespace redgem {
namespace jolt {
namespace {

// A kinematic body that was given somewhere to be by the end of the next step.
bool has_move_pending(const Record& record) { return record.has_target && has_body(&record); }

}  // namespace

World::World(Vec3 gravity)
    : gravity_(gravity), temp_allocator_(config::kPhysicsTempAllocatorBytes), job_system_(config::kMaxPhysicsJobs) {
  system_.Init(config::kMaxBodies, config::kNumBodyMutexes, config::kMaxBodyPairs, config::kMaxContactConstraints,
               broad_phase_layers_, object_vs_broad_phase_, object_pairs_);
  system_.SetGravity(to_jolt(gravity));
  system_.SetContactListener(this);
}

World::~World() {
  std::vector<ObjectId> ids;
  ids.reserve(records_.size());
  for (const auto& [id, record] : records_) ids.push_back(id);
  for (const ObjectId id : ids) remove_object(id);
}

std::optional<std::string> World::add_object(const BodyDesc& desc) {
  if (has_object(desc.object)) return "object already has a physics body";
  auto shape = build_shape(desc);
  if (auto* error = std::get_if<std::string>(&shape)) return *error;
  const ShapeInfo& info = std::get<ShapeInfo>(shape);
  Record record;
  record.type = desc.config.type;
  record.trigger = desc.config.trigger;
  record.shape = info.shape;
  record.pose = desc.world;
  if (!record.trigger) create_simulated(desc, info, record);
  records_.emplace(desc.object, std::move(record));
  return std::nullopt;
}

// What the simulation moves, or keeps things from moving through: a character or a body.
void World::create_simulated(const BodyDesc& desc, const ShapeInfo& info, Record& record) {
  if (desc.config.type == BodyType::Character) {
    create_character(desc, info, record);
  } else {
    create_body(desc, info, record);
  }
}

void World::remove_object(ObjectId object) {
  const auto it = records_.find(object);
  if (it == records_.end()) return;
  release_vehicle(it->second);
  release_body(it->second);
  release_character(it->second);
  records_.erase(it);
  forget(object);
}

BodyType World::body_type(ObjectId object) const {
  const Record* record = find(object);
  return record != nullptr ? record->type : BodyType::None;
}

bool World::is_trigger(ObjectId object) const {
  const Record* record = find(object);
  return record != nullptr && record->trigger;
}

void World::set_target_pose(ObjectId object, const Transform& world) {
  Record* record = find(object);
  if (record == nullptr) return;
  record->pose = world;
  record->has_target = record->type == BodyType::Kinematic;
}

void World::teleport(ObjectId object, const Transform& world) {
  Record* record = find(object);
  if (record == nullptr) return;
  record->pose = world;
  if (is_character(record)) {
    record->character->SetPosition(JPH::RVec3(to_jolt(world.position)));
    record->character->SetLinearVelocity(JPH::Vec3::sZero());
  } else if (has_body(record)) {
    teleport_body(*record, world);
  }
}

void World::move_kinematic_bodies(float dt) {
  JPH::BodyInterface& bodies = system_.GetBodyInterface();
  for (auto& [id, record] : records_) {
    if (!has_move_pending(record)) continue;
    bodies.MoveKinematic(record.body, JPH::RVec3(to_jolt(record.pose.position)), to_jolt(record.pose.rotation), dt);
    record.has_target = false;
  }
}

void World::step(float dt) {
  move_kinematic_bodies(dt);
  system_.Update(dt, config::kPhysicsCollisionSteps, &temp_allocator_, &job_system_);
  update_characters(dt);
  update_triggers();
  collect_contact_events();
}

std::optional<Transform> World::simulated_pose(ObjectId object) const {
  const Record* record = find(object);
  if (record == nullptr) return std::nullopt;
  Transform pose = record->pose;
  if (is_character(record)) {
    pose.position = from_jolt(JPH::Vec3(record->character->GetPosition()));
    return pose;
  }
  if (!is_dynamic_body(record)) return std::nullopt;
  const JPH::BodyInterface& bodies = system_.GetBodyInterface();
  pose.position = from_jolt(JPH::Vec3(bodies.GetPosition(record->body)));
  pose.rotation = from_jolt(bodies.GetRotation(record->body));
  return pose;
}

}  // namespace jolt

std::unique_ptr<PhysicsWorld> create_jolt_physics_world(Vec3 gravity) { return std::make_unique<jolt::World>(gravity); }

}  // namespace redgem
