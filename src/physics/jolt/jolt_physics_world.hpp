#pragma once

// The physics world, simulated by Jolt. One class, implemented part by part; only those parts
// include this:
//   jolt_physics_world.cpp             the world: what is in it, a step, where things are
//   jolt_physics_world_bodies.cpp      rigid bodies
//   jolt_physics_world_characters.cpp  characters
//   jolt_physics_world_vehicles.cpp    vehicles
//   jolt_physics_world_contacts.cpp    who touches whom: contacts and triggers
//   jolt_physics_world_forces.cpp      what pushes, turns, weighs and wakes rigid bodies
//   jolt_physics_world_queries.cpp     what a ray, a moving shape or a shape in place finds

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
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
  std::size_t layer = kDefaultLayer;
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
// A body that can move at all (dynamic, kinematic or a vehicle's): it has a mass and can sleep.
inline bool is_movable_body(const Record* record) { return has_body(record) && record->type != BodyType::Static; }

class World final : public PhysicsWorld, JPH::ContactListener, JPH::CharacterContactListener {
 public:
  World(Vec3 gravity, const CollisionLayers& layers);
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
  void add_force(ObjectId object, Vec3 force) override;
  void add_force_at(ObjectId object, Vec3 force, Vec3 point) override;
  void add_torque(ObjectId object, Vec3 torque) override;
  Vec3 angular_velocity(ObjectId object) const override;
  void set_angular_velocity(ObjectId object, Vec3 velocity) override;
  float mass(ObjectId object) const override;
  void set_mass(ObjectId object, float mass) override;
  bool set_body_type(ObjectId object, BodyType type) override;
  float gravity_scale(ObjectId object) const override;
  void set_gravity_scale(ObjectId object, float scale) override;
  void set_sleeping(ObjectId object, bool sleeping) override;
  bool sleeping(ObjectId object) const override;
  const CollisionLayers& layers() const override { return layers_; }
  std::size_t layer(ObjectId object) const override;
  void set_layer(ObjectId object, std::size_t layer) override;
  using PhysicsWorld::raycast;
  std::optional<RaycastHit> raycast(Vec3 origin, Vec3 direction, float max_distance,
                                    const QueryFilter& filter) const override;
  std::vector<RaycastHit> raycast_all(Vec3 origin, Vec3 direction, float max_distance,
                                      const QueryFilter& filter) const override;
  std::optional<RaycastHit> shape_cast(const QueryShape& shape, Vec3 from, Vec3 to,
                                       const QueryFilter& filter) const override;
  std::vector<RaycastHit> overlap(const QueryShape& shape, Vec3 center, const QueryFilter& filter) const override;

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
  // The bodies of the objects a query ignores.
  std::vector<JPH::BodyID> bodies_of(const std::vector<ObjectId>& objects) const;
  // The normal of the surface a ray hit, or zero when the body is gone by now.
  Vec3 surface_normal(const JPH::RayCastResult& result, JPH::RVec3Arg point) const;
  // What a ray hit, told as scripts see it.
  RaycastHit ray_hit(const JPH::RRayCast& ray, const JPH::RayCastResult& result, float max_distance) const;
  // The layer of an object's body in Jolt's terms.
  JPH::ObjectLayer object_layer_of(const Record& record) const;
  // A rigid body neither moving nor about to: static, or asleep. Characters never are.
  bool at_rest(ObjectId object) const;
  bool both_at_rest(const ContactPair& pair) const { return at_rest(pair.first) && at_rest(pair.second); }
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
  void move_character(const Record& record, float dt);

  // Vehicles.
  void attach_vehicle(const BodyDesc& desc, Record& record);
  void release_vehicle(Record& record);

  // Contacts and triggers.
  std::set<ObjectId> objects_inside(ObjectId id, const Record& trigger) const;
  void update_triggers();
  void collect_contact_events();
  // Touching bodies that are both at rest are not reported by Jolt, yet they still touch.
  void keep_resting_contacts(std::set<ContactPair>& current) const;
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
  CollisionLayers layers_;
  BroadPhaseLayers broad_phase_layers_;
  ObjectVsBroadPhase object_vs_broad_phase_;
  ObjectPairs object_pairs_{layers_};
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
