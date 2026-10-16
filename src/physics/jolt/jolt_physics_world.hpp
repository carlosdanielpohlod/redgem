#pragma once

// The physics world, simulated by Jolt. One class, implemented part by part; only those parts
// include this:
//   jolt_physics_world.cpp             the world: what is in it, a step, where things are
//   jolt_physics_world_bodies.cpp      rigid bodies
//   jolt_physics_world_characters.cpp  characters
//   jolt_physics_world_vehicles.cpp    vehicles
//   jolt_physics_world_contacts.cpp    who touches whom: contacts and triggers
//   jolt_physics_world_queries.cpp     what a ray hits

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>

#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "physics/contact_events.hpp"
#include "physics/jolt/jolt_layers.hpp"
#include "physics/jolt/jolt_runtime.hpp"
#include "physics/jolt/jolt_shapes.hpp"
#include "physics/physics_world.hpp"

namespace redgem::jolt {

// What the world keeps for one object: a rigid body, a character, a vehicle on a body, or (for a
// trigger) only a shape and where it is.
struct Record {
  BodyType type = BodyType::None;
  bool trigger = false;
  JPH::BodyID body;
  JPH::Ref<JPH::CharacterVirtual> character;
  JPH::Ref<JPH::VehicleConstraint> vehicle;
  std::vector<ObjectHandle> wheel_objects;  // same order as the vehicle's wheels
  JPH::RefConst<JPH::Shape> shape;
  Transform pose;
  bool has_target = false;
  Vec3 desired_move;
  std::optional<float> pending_jump;
  std::set<ObjectId> overlaps;  // triggers only
};

// What an object is in the world (`record` is null for one that is not in it).
inline bool has_body(const Record* record) { return record != nullptr && !record->body.IsInvalid(); }
inline bool is_character(const Record* record) { return record != nullptr && record->character != nullptr; }
inline bool is_vehicle(const Record* record) { return record != nullptr && record->vehicle != nullptr; }
// A body forces move: the simulation decides where it is, and the scene follows.
inline bool is_dynamic_body(const Record* record) { return has_body(record) && is_dynamic(record->type); }

class World final : public PhysicsWorld, JPH::ContactListener, JPH::CharacterContactListener {
 public:
  explicit World(Vec3 gravity);
  ~World() override;

  std::optional<std::string> add_object(const BodyDesc& desc) override;
  void remove_object(ObjectId object) override;
  bool has_object(ObjectId object) const override { return records_.count(object) != 0; }
  BodyType body_type(ObjectId object) const override;
  bool is_trigger(ObjectId object) const override;
  void set_target_pose(ObjectId object, const Transform& world) override;
  void teleport(ObjectId object, const Transform& world) override;
  void set_body_rotation(ObjectId object, Quat rotation) override;
  void step(float dt) override;
  std::optional<Transform> simulated_pose(ObjectId object) const override;
  std::vector<ContactEvent> drain_events() override;
  Vec3 velocity(ObjectId object) const override;
  void set_velocity(ObjectId object, Vec3 velocity) override;
  void add_impulse(ObjectId object, Vec3 impulse) override;
  void set_character_move(ObjectId object, Vec3 horizontal_velocity) override;
  void character_jump(ObjectId object, float speed) override;
  bool character_grounded(ObjectId object) const override;
  void set_vehicle_input(ObjectId object, const VehicleInput& input) override;
  float vehicle_forward_speed(ObjectId object) const override;
  std::vector<WheelPose> wheel_poses(ObjectId object) const override;
  std::optional<RaycastHit> raycast(Vec3 origin, Vec3 direction, float max_distance, ObjectId ignore) const override;

 private:
  Record* find(ObjectId object) {
    const auto it = records_.find(object);
    return it == records_.end() ? nullptr : &it->second;
  }
  const Record* find(ObjectId object) const {
    const auto it = records_.find(object);
    return it == records_.end() ? nullptr : &it->second;
  }
  ObjectId owner_of(const JPH::BodyID& body) const {
    const auto it = body_owners_.find(body.GetIndexAndSequenceNumber());
    return it == body_owners_.end() ? kNoObject : it->second;
  }
  JPH::BodyID body_of(ObjectId object) const;
  // The normal of the surface a ray hit, or zero when the body is gone by now.
  Vec3 surface_normal(const JPH::RayCastResult& result, JPH::RVec3Arg point) const;
  void create_simulated(const BodyDesc& desc, const ShapeInfo& info, Record& record);
  void move_kinematic_bodies(float dt);

  // Rigid bodies.
  void create_body(const BodyDesc& desc, const ShapeInfo& info, Record& record);
  void release_body(Record& record);
  void teleport_body(Record& record, const Transform& world);

  // Characters.
  void create_character(const BodyDesc& desc, const ShapeInfo& info, Record& record);
  void release_character(Record& record);
  void update_characters(float dt);
  void update_character(Record& record, float dt);
  // Moves a character by its velocity, sliding along what it meets, climbing steps and keeping to the floor.
  void move_character(JPH::CharacterVirtual& character, float dt);

  // Vehicles.
  void attach_vehicle(const BodyDesc& desc, Record& record);
  void release_vehicle(Record& record);

  // Contacts and triggers.
  std::set<ObjectId> objects_inside(ObjectId id, const Record& trigger) const;
  void update_triggers();
  void collect_contact_events();
  void record_contact(ObjectId a, ObjectId b);
  void record_character_contact(const JPH::CharacterVirtual* character, const JPH::BodyID& body);
  // Removes every trace of an object from contact bookkeeping (no exit events for a destroyed object).
  void forget(ObjectId object);
  void OnContactAdded(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold&,
                      JPH::ContactSettings&) override;
  void OnContactPersisted(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold&,
                          JPH::ContactSettings&) override;
  void OnContactAdded(const JPH::CharacterVirtual* character, const JPH::BodyID& body, const JPH::SubShapeID&,
                      JPH::RVec3Arg, JPH::Vec3Arg, JPH::CharacterContactSettings&) override;
  void OnContactPersisted(const JPH::CharacterVirtual* character, const JPH::BodyID& body, const JPH::SubShapeID&,
                          JPH::RVec3Arg, JPH::Vec3Arg, JPH::CharacterContactSettings&) override;

  Runtime runtime_;
  Vec3 gravity_;
  BroadPhaseLayers broad_phase_layers_;
  ObjectVsBroadPhase object_vs_broad_phase_;
  ObjectPairs object_pairs_;
  JPH::TempAllocatorImpl temp_allocator_;
  JPH::JobSystemSingleThreaded job_system_;
  JPH::PhysicsSystem system_;
  std::map<ObjectId, Record> records_;  // ordered: deterministic iteration
  std::map<JPH::uint32, ObjectId> body_owners_;
  std::map<const JPH::CharacterVirtual*, ObjectId> character_owners_;
  std::mutex contacts_mutex_;
  std::set<ContactPair> current_contacts_;
  std::set<ContactPair> previous_contacts_;
  std::vector<ContactEvent> events_;
};

}  // namespace redgem::jolt
