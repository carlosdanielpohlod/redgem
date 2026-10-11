#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <cstdarg>
#include <cstdio>
#include <map>
#include <mutex>
#include <set>
#include <utility>

#include "core/config.hpp"
#include "core/log.hpp"
#include "physics/jolt/jolt_shapes.hpp"
#include "physics/jolt/jolt_vehicle.hpp"
#include "physics/physics_world.hpp"

namespace redgem {
namespace {

using jolt::from_jolt;
using jolt::to_jolt;

constexpr std::size_t kTraceBufferSize = 1024;
// Vertical speed (relative to the ground) below which a grounded character sticks to the ground.
constexpr float kGroundedVerticalTolerance = 0.1f;
// Trigger shapes are built at their final size, so queries use them unscaled.
constexpr float kUnitScale = 1.0f;
constexpr int kNeutralGear = 0;
constexpr float kClutchDisengaged = 0.0f;

namespace layers {
constexpr JPH::ObjectLayer kNonMoving = 0;
constexpr JPH::ObjectLayer kMoving = 1;
constexpr JPH::uint kCount = 2;
}  // namespace layers

namespace broad_phase_layers {
constexpr JPH::BroadPhaseLayer kNonMoving(0);
constexpr JPH::BroadPhaseLayer kMoving(1);
}  // namespace broad_phase_layers

void trace(const char* format, ...) {
  char buffer[kTraceBufferSize];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log::debug(std::string("jolt: ") + buffer);
}

// Jolt keeps process-wide registries; reference count them across worlds.
class JoltRuntime {
 public:
  JoltRuntime() {
    std::lock_guard<std::mutex> lock(mutex());
    if (count()++ == 0) {
      JPH::RegisterDefaultAllocator();
      JPH::Trace = trace;
      JPH::Factory::sInstance = new JPH::Factory();
      JPH::RegisterTypes();
    }
  }
  ~JoltRuntime() {
    std::lock_guard<std::mutex> lock(mutex());
    if (--count() == 0) {
      JPH::UnregisterTypes();
      delete JPH::Factory::sInstance;
      JPH::Factory::sInstance = nullptr;
    }
  }
  JoltRuntime(const JoltRuntime&) = delete;
  JoltRuntime& operator=(const JoltRuntime&) = delete;

 private:
  static std::mutex& mutex() {
    static std::mutex instance;
    return instance;
  }
  static int& count() {
    static int instance = 0;
    return instance;
  }
};

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
 public:
  JPH::uint GetNumBroadPhaseLayers() const override { return layers::kCount; }
  JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
    return layer == layers::kNonMoving ? broad_phase_layers::kNonMoving : broad_phase_layers::kMoving;
  }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
    return layer == broad_phase_layers::kNonMoving ? "NonMoving" : "Moving";
  }
#endif
};

class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
 public:
  bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broad) const override {
    return layer == layers::kMoving || broad == broad_phase_layers::kMoving;
  }
};

class ObjectPairs final : public JPH::ObjectLayerPairFilter {
 public:
  bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
    return a == layers::kMoving || b == layers::kMoving;
  }
};

using ContactPair = std::pair<ObjectId, ObjectId>;

ContactPair ordered(ObjectId a, ObjectId b) { return a < b ? ContactPair{a, b} : ContactPair{b, a}; }

class JoltPhysicsWorld final : public PhysicsWorld, JPH::ContactListener, JPH::CharacterContactListener {
 public:
  explicit JoltPhysicsWorld(Vec3 gravity)
      : gravity_(gravity), temp_allocator_(config::kPhysicsTempAllocatorBytes), job_system_(config::kMaxPhysicsJobs) {
    system_.Init(config::kMaxBodies, config::kNumBodyMutexes, config::kMaxBodyPairs, config::kMaxContactConstraints,
                 broad_phase_layers_, object_vs_broad_phase_, object_pairs_);
    system_.SetGravity(to_jolt(gravity));
    system_.SetContactListener(this);
  }

  ~JoltPhysicsWorld() override {
    std::vector<ObjectId> ids;
    ids.reserve(records_.size());
    for (const auto& [id, record] : records_) ids.push_back(id);
    for (const ObjectId id : ids) remove_object(id);
  }

  std::optional<std::string> add_object(const BodyDesc& desc) override {
    if (records_.count(desc.object) != 0) return "object already has a physics body";
    auto shape = jolt::build_shape(desc);
    if (auto* error = std::get_if<std::string>(&shape)) return *error;
    const jolt::ShapeInfo& info = std::get<jolt::ShapeInfo>(shape);

    Record record;
    record.type = desc.config.type;
    record.trigger = desc.config.trigger;
    record.shape = info.shape;
    record.pose = desc.world;
    if (!record.trigger) create_simulated(desc, info, record);
    records_.emplace(desc.object, std::move(record));
    return std::nullopt;
  }

  void remove_object(ObjectId object) override {
    const auto it = records_.find(object);
    if (it == records_.end()) return;
    release_vehicle(it->second);
    release_body(it->second);
    release_character(it->second);
    records_.erase(it);
    forget(object);
  }

  bool has_object(ObjectId object) const override { return records_.count(object) != 0; }

  BodyType body_type(ObjectId object) const override {
    const Record* record = find(object);
    return record ? record->type : BodyType::None;
  }

  bool is_trigger(ObjectId object) const override {
    const Record* record = find(object);
    return record != nullptr && record->trigger;
  }

  void set_target_pose(ObjectId object, const Transform& world) override {
    Record* record = find(object);
    if (record == nullptr) return;
    record->pose = world;
    record->has_target = record->type == BodyType::Kinematic;
  }

  void teleport(ObjectId object, const Transform& world) override {
    Record* record = find(object);
    if (record == nullptr) return;
    record->pose = world;
    if (record->character != nullptr) {
      record->character->SetPosition(JPH::RVec3(to_jolt(world.position)));
      record->character->SetLinearVelocity(JPH::Vec3::sZero());
    } else if (!record->body.IsInvalid()) {
      teleport_body(*record, world);
    }
  }

  void set_body_rotation(ObjectId object, Quat rotation) override {
    Record* record = find(object);
    if (record == nullptr || record->body.IsInvalid()) return;
    record->pose.rotation = rotation;
    system_.GetBodyInterface().SetRotation(record->body, to_jolt(rotation), JPH::EActivation::Activate);
  }

  void step(float dt) override {
    JPH::BodyInterface& bodies = system_.GetBodyInterface();
    for (auto& [id, record] : records_) {
      if (record.has_target && !record.body.IsInvalid()) {
        bodies.MoveKinematic(record.body, JPH::RVec3(to_jolt(record.pose.position)), to_jolt(record.pose.rotation), dt);
        record.has_target = false;
      }
    }
    system_.Update(dt, config::kPhysicsCollisionSteps, &temp_allocator_, &job_system_);
    for (auto& [id, record] : records_) {
      if (record.character != nullptr) update_character(record, dt);
    }
    update_triggers();
    collect_contact_events();
  }

  std::optional<Transform> simulated_pose(ObjectId object) const override {
    const Record* record = find(object);
    if (record == nullptr) return std::nullopt;
    Transform pose = record->pose;
    if (record->character != nullptr) {
      pose.position = from_jolt(JPH::Vec3(record->character->GetPosition()));
      return pose;
    }
    if ((record->type == BodyType::Dynamic || record->type == BodyType::Vehicle) && !record->body.IsInvalid()) {
      const JPH::BodyInterface& bodies = system_.GetBodyInterface();
      pose.position = from_jolt(JPH::Vec3(bodies.GetPosition(record->body)));
      pose.rotation = from_jolt(bodies.GetRotation(record->body));
      return pose;
    }
    return std::nullopt;
  }

  std::vector<ContactEvent> drain_events() override { return std::exchange(events_, {}); }

  Vec3 velocity(ObjectId object) const override {
    const Record* record = find(object);
    if (record == nullptr) return Vec3::zero();
    if (record->character != nullptr) return from_jolt(record->character->GetLinearVelocity());
    if (record->body.IsInvalid()) return Vec3::zero();
    return from_jolt(system_.GetBodyInterface().GetLinearVelocity(record->body));
  }

  void set_velocity(ObjectId object, Vec3 velocity) override {
    const Record* record = find(object);
    if (record == nullptr || record->body.IsInvalid()) return;
    system_.GetBodyInterface().SetLinearVelocity(record->body, to_jolt(velocity));
  }

  void add_impulse(ObjectId object, Vec3 impulse) override {
    const Record* record = find(object);
    if (record == nullptr || record->body.IsInvalid()) return;
    if (record->type != BodyType::Dynamic && record->type != BodyType::Vehicle) return;
    system_.GetBodyInterface().AddImpulse(record->body, to_jolt(impulse));
  }

  void set_character_move(ObjectId object, Vec3 horizontal_velocity) override {
    Record* record = find(object);
    if (record == nullptr || record->character == nullptr) return;
    record->desired_move = {horizontal_velocity.x, 0.0f, horizontal_velocity.z};
  }

  void character_jump(ObjectId object, float speed) override {
    Record* record = find(object);
    if (record == nullptr || record->character == nullptr) return;
    record->pending_jump = speed;
  }

  bool character_grounded(ObjectId object) const override {
    const Record* record = find(object);
    return record != nullptr && record->character != nullptr &&
           record->character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
  }

  void set_vehicle_input(ObjectId object, const VehicleInput& input) override {
    Record* record = find(object);
    if (record == nullptr || record->vehicle == nullptr) return;
    auto* controller = static_cast<JPH::WheeledVehicleController*>(record->vehicle->GetController());
    controller->SetDriverInput(input.throttle, input.steering, input.brake, input.handbrake);
  }

  float vehicle_forward_speed(ObjectId object) const override {
    const Record* record = find(object);
    if (record == nullptr || record->vehicle == nullptr) return 0.0f;
    const JPH::BodyInterface& bodies = system_.GetBodyInterface();
    const JPH::Vec3 forward = bodies.GetRotation(record->body) * record->vehicle->GetLocalForward();
    return bodies.GetLinearVelocity(record->body).Dot(forward);
  }

  std::vector<WheelPose> wheel_poses(ObjectId object) const override {
    std::vector<WheelPose> poses;
    const Record* record = find(object);
    if (record == nullptr || record->vehicle == nullptr) return poses;
    poses.reserve(record->wheel_objects.size());
    for (std::size_t i = 0; i < record->wheel_objects.size(); ++i) poses.push_back(wheel_pose(*record, i));
    return poses;
  }

  std::optional<RaycastHit> raycast(Vec3 origin, Vec3 direction, float max_distance, ObjectId ignore) const override {
    const Vec3 dir = normalize(direction);
    if (length(dir) <= kEpsilon || max_distance <= 0.0f) return std::nullopt;
    const JPH::RRayCast ray(JPH::RVec3(to_jolt(origin)), to_jolt(dir * max_distance));
    JPH::RayCastResult result;
    const JPH::IgnoreSingleBodyFilter body_filter(body_of(ignore));
    if (!system_.GetNarrowPhaseQuery().CastRay(ray, result, {}, {}, body_filter)) return std::nullopt;
    RaycastHit hit;
    hit.object = owner_of(result.mBodyID);
    hit.distance = result.mFraction * max_distance;
    const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
    hit.point = from_jolt(JPH::Vec3(point));
    hit.normal = surface_normal(result, point);
    return hit;
  }

 private:
  struct Record {
    BodyType type = BodyType::None;
    bool trigger = false;
    JPH::BodyID body;
    JPH::Ref<JPH::CharacterVirtual> character;
    JPH::Ref<JPH::VehicleConstraint> vehicle;
    std::vector<ObjectId> wheel_objects;  // same order as the vehicle's wheels
    JPH::RefConst<JPH::Shape> shape;
    Transform pose;
    bool has_target = false;
    Vec3 desired_move;
    std::optional<float> pending_jump;
    std::set<ObjectId> overlaps;  // triggers only
  };

  Record* find(ObjectId object) {
    const auto it = records_.find(object);
    return it == records_.end() ? nullptr : &it->second;
  }
  const Record* find(ObjectId object) const {
    const auto it = records_.find(object);
    return it == records_.end() ? nullptr : &it->second;
  }

  JPH::BodyID body_of(ObjectId object) const {
    const Record* record = find(object);
    if (record == nullptr) return JPH::BodyID();
    if (record->character != nullptr) return record->character->GetInnerBodyID();
    return record->body;
  }

  ObjectId owner_of(const JPH::BodyID& body) const {
    const auto it = body_owners_.find(body.GetIndexAndSequenceNumber());
    return it == body_owners_.end() ? kNoObject : it->second;
  }

  // The normal of the surface a ray hit, or zero when the body is gone by now.
  Vec3 surface_normal(const JPH::RayCastResult& result, JPH::RVec3Arg point) const {
    JPH::BodyLockRead lock(system_.GetBodyLockInterface(), result.mBodyID);
    if (!lock.Succeeded()) return {};
    return from_jolt(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
  }

  WheelPose wheel_pose(const Record& record, std::size_t wheel) const {
    // Wheel meshes are modeled with their axle along X and Y up.
    const JPH::RMat44 matrix = record.vehicle->GetWheelWorldTransform(static_cast<JPH::uint>(wheel),
                                                                      to_jolt(Vec3::right()), to_jolt(Vec3::up()));
    WheelPose pose;
    pose.object = record.wheel_objects[wheel];
    pose.world.position = from_jolt(JPH::Vec3(matrix.GetTranslation()));
    pose.world.rotation = from_jolt(matrix.GetQuaternion());
    return pose;
  }

  void teleport_body(Record& record, const Transform& world) {
    JPH::BodyInterface& bodies = system_.GetBodyInterface();
    bodies.SetPositionAndRotation(record.body, JPH::RVec3(to_jolt(world.position)), to_jolt(world.rotation),
                                  JPH::EActivation::Activate);
    if (record.type == BodyType::Dynamic || record.type == BodyType::Vehicle) {
      bodies.SetLinearAndAngularVelocity(record.body, JPH::Vec3::sZero(), JPH::Vec3::sZero());
    }
    if (record.vehicle != nullptr) stop_vehicle(*record.vehicle);
  }

  void release_vehicle(Record& record) {
    if (record.vehicle == nullptr) return;
    system_.RemoveStepListener(record.vehicle.GetPtr());
    system_.RemoveConstraint(record.vehicle.GetPtr());
    record.vehicle = nullptr;
  }

  void release_body(Record& record) {
    if (record.body.IsInvalid()) return;
    body_owners_.erase(record.body.GetIndexAndSequenceNumber());
    system_.GetBodyInterface().RemoveBody(record.body);
    system_.GetBodyInterface().DestroyBody(record.body);
  }

  void release_character(Record& record) {
    if (record.character == nullptr) return;
    body_owners_.erase(record.character->GetInnerBodyID().GetIndexAndSequenceNumber());
    record.character = nullptr;  // destroys the inner body too
  }

  // What the simulation moves, or keeps things from moving through: a character or a body.
  void create_simulated(const BodyDesc& desc, const jolt::ShapeInfo& info, Record& record) {
    if (desc.config.type == BodyType::Character) {
      create_character(desc, info, record);
    } else {
      create_body(desc, info, record);
    }
  }

  static JPH::EMotionType motion_of(BodyType type) {
    if (type == BodyType::Dynamic || type == BodyType::Vehicle) return JPH::EMotionType::Dynamic;
    return type == BodyType::Kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static;
  }

  // A low center of mass keeps the car on its wheels.
  static JPH::RefConst<JPH::Shape> chassis_shape(const jolt::ShapeInfo& info) {
    const JPH::Vec3 drop(0.0f, -info.half_extents.y * config::kVehicleCenterOfMassDrop, 0.0f);
    return JPH::OffsetCenterOfMassShapeSettings(drop, info.shape).Create().Get();
  }

  static JPH::BodyCreationSettings body_settings(const BodyDesc& desc, const JPH::RefConst<JPH::Shape>& shape,
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

  void create_body(const BodyDesc& desc, const jolt::ShapeInfo& info, Record& record) {
    const bool vehicle = desc.config.type == BodyType::Vehicle;
    const JPH::EMotionType motion = motion_of(desc.config.type);
    const JPH::RefConst<JPH::Shape> shape = vehicle ? chassis_shape(info) : info.shape;
    const JPH::EActivation activation =
        motion == JPH::EMotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
    record.body = system_.GetBodyInterface().CreateAndAddBody(body_settings(desc, shape, motion), activation);
    body_owners_[record.body.GetIndexAndSequenceNumber()] = desc.object;
    if (vehicle) attach_vehicle(desc, record);
  }

  // Wheels and engine keep their own spin; a teleported car must not drive off by itself.
  static void stop_vehicle(JPH::VehicleConstraint& vehicle) {
    for (JPH::uint i = 0; i < static_cast<JPH::uint>(vehicle.GetWheels().size()); ++i) {
      vehicle.GetWheel(i)->SetAngularVelocity(0.0f);
    }
    auto* controller = static_cast<JPH::WheeledVehicleController*>(vehicle.GetController());
    controller->SetDriverInput(0.0f, 0.0f, 0.0f, 0.0f);
    controller->GetEngine().SetCurrentRPM(controller->GetEngine().mMinRPM);
    controller->GetTransmission().Set(kNeutralGear, kClutchDisengaged);
  }

  void attach_vehicle(const BodyDesc& desc, Record& record) {
    JPH::BodyLockWrite lock(system_.GetBodyLockInterface(), record.body);
    if (!lock.Succeeded()) return;
    record.vehicle = jolt::build_vehicle(lock.GetBody(), desc.wheels, layers::kMoving);
    for (const WheelDesc& wheel : desc.wheels) record.wheel_objects.push_back(wheel.object);
    system_.AddConstraint(record.vehicle.GetPtr());
    system_.AddStepListener(record.vehicle.GetPtr());
  }

  static JPH::Ref<JPH::CharacterVirtualSettings> character_settings(const jolt::ShapeInfo& info) {
    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
    settings->mShape = info.shape;
    settings->mInnerBodyShape = info.shape;
    settings->mInnerBodyLayer = layers::kMoving;
    settings->mMaxSlopeAngle = degrees_to_radians(config::kCharacterMaxSlopeDegrees);
    settings->mMass = config::kCharacterMass;
    settings->mMaxStrength = config::kCharacterMaxStrength;
    settings->mCharacterPadding = config::kCharacterPadding;
    settings->mPenetrationRecoverySpeed = config::kCharacterPenetrationRecoverySpeed;
    settings->mPredictiveContactDistance = config::kCharacterPredictiveContactDistance;
    // Only contacts on the lower hemisphere of the capsule can support the character.
    const float bottom_sphere_center = info.center_offset.y - info.capsule_half_height;
    settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -bottom_sphere_center);
    return settings;
  }

  void create_character(const BodyDesc& desc, const jolt::ShapeInfo& info, Record& record) {
    // Characters stay upright: physics only drives their position.
    record.character =
        new JPH::CharacterVirtual(character_settings(info), JPH::RVec3(to_jolt(desc.world.position)),
                                  JPH::Quat::sIdentity(), static_cast<JPH::uint64>(desc.object), &system_);
    record.character->SetListener(this);
    body_owners_[record.character->GetInnerBodyID().GetIndexAndSequenceNumber()] = desc.object;
    character_owners_[record.character.GetPtr()] = desc.object;
  }

  // The speed a character carries into a step, before gravity and its own move: the ground's
  // (plus a jump) while it stands on it, else its own fall.
  static JPH::Vec3 carried_velocity(const Record& record) {
    const JPH::CharacterVirtual& character = *record.character;
    const JPH::Vec3 ground_velocity = character.GetGroundVelocity();
    const float vertical = character.GetLinearVelocity().GetY();
    const bool on_ground = character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
    const bool settling = vertical - ground_velocity.GetY() < kGroundedVerticalTolerance;
    if (!on_ground || !settling) return JPH::Vec3(0.0f, vertical, 0.0f);
    if (!record.pending_jump) return ground_velocity;
    return ground_velocity + JPH::Vec3(0.0f, *record.pending_jump, 0.0f);
  }

  void update_character(Record& record, float dt) {
    JPH::CharacterVirtual& character = *record.character;
    character.UpdateGroundVelocity();
    JPH::Vec3 velocity = carried_velocity(record);
    record.pending_jump.reset();
    velocity += to_jolt(gravity_) * dt;
    velocity += to_jolt(record.desired_move);
    character.SetLinearVelocity(velocity);
    move_character(character, dt);
  }

  // Moves a character by its velocity, sliding along what it meets, climbing steps and keeping to the floor.
  void move_character(JPH::CharacterVirtual& character, float dt) {
    JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
    update_settings.mStickToFloorStepDown = JPH::Vec3(0.0f, -config::kCharacterStickToFloorDistance, 0.0f);
    update_settings.mWalkStairsStepUp = JPH::Vec3(0.0f, config::kCharacterStairStepHeight, 0.0f);
    const JPH::DefaultBroadPhaseLayerFilter broad_filter(object_vs_broad_phase_, layers::kMoving);
    const JPH::DefaultObjectLayerFilter layer_filter(object_pairs_, layers::kMoving);
    const JPH::IgnoreSingleBodyFilter body_filter(character.GetInnerBodyID());
    const JPH::ShapeFilter shape_filter;
    character.ExtendedUpdate(dt, to_jolt(gravity_), update_settings, broad_filter, layer_filter, body_filter,
                             shape_filter, temp_allocator_);
  }

  static JPH::RMat44 matrix_of(const Transform& pose) {
    return JPH::RMat44::sRotationTranslation(to_jolt(pose.rotation), JPH::RVec3(to_jolt(pose.position)));
  }

  // The objects inside a trigger right now.
  std::set<ObjectId> objects_inside(ObjectId id, const Record& trigger) const {
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    const JPH::RMat44 transform = matrix_of(trigger.pose);
    // Only moving things (characters, dynamic and kinematic bodies) fire triggers.
    const JPH::SpecifiedBroadPhaseLayerFilter broad_filter(broad_phase_layers::kMoving);
    const JPH::SpecifiedObjectLayerFilter layer_filter(layers::kMoving);
    system_.GetNarrowPhaseQuery().CollideShape(trigger.shape, JPH::Vec3::sReplicate(kUnitScale), transform,
                                               JPH::CollideShapeSettings(), JPH::RVec3::sZero(), collector,
                                               broad_filter, layer_filter);
    std::set<ObjectId> inside;
    for (const JPH::CollideShapeResult& hit : collector.mHits) {
      const ObjectId other = owner_of(hit.mBodyID2);
      if (other != kNoObject && other != id) inside.insert(other);
    }
    return inside;
  }

  void update_trigger(ObjectId id, Record& record) {
    std::set<ObjectId> now = objects_inside(id, record);
    for (const ObjectId other : now) {
      if (record.overlaps.count(other) == 0) events_.push_back({id, other, ContactPhase::Enter, true});
    }
    for (const ObjectId other : record.overlaps) {
      if (now.count(other) == 0) events_.push_back({id, other, ContactPhase::Exit, true});
    }
    record.overlaps = std::move(now);
  }

  void update_triggers() {
    for (auto& [id, record] : records_) {
      if (record.trigger) update_trigger(id, record);
    }
  }

  void collect_contact_events() {
    std::set<ContactPair> current;
    {
      std::lock_guard<std::mutex> lock(contacts_mutex_);
      current.swap(current_contacts_);
    }
    for (const ContactPair& pair : current) {
      if (previous_contacts_.count(pair) == 0) push_contact_pair(pair, ContactPhase::Enter);
    }
    for (const ContactPair& pair : previous_contacts_) {
      if (current.count(pair) == 0) push_contact_pair(pair, ContactPhase::Exit);
    }
    previous_contacts_ = std::move(current);
  }

  void push_contact_pair(const ContactPair& pair, ContactPhase phase) {
    events_.push_back({pair.first, pair.second, phase, false});
    events_.push_back({pair.second, pair.first, phase, false});
  }

  void record_contact(ObjectId a, ObjectId b) {
    if (a == kNoObject || b == kNoObject || a == b) return;
    std::lock_guard<std::mutex> lock(contacts_mutex_);
    current_contacts_.insert(ordered(a, b));
  }

  // Removes every trace of an object from contact bookkeeping (no exit events for a destroyed object).
  void forget(ObjectId object) {
    erase_pairs_with(object, previous_contacts_);
    {
      std::lock_guard<std::mutex> lock(contacts_mutex_);
      erase_pairs_with(object, current_contacts_);
    }
    for (auto& [id, record] : records_) record.overlaps.erase(object);
    for (auto it = character_owners_.begin(); it != character_owners_.end();) {
      it = it->second == object ? character_owners_.erase(it) : std::next(it);
    }
  }

  static void erase_pairs_with(ObjectId object, std::set<ContactPair>& pairs) {
    for (auto it = pairs.begin(); it != pairs.end();) {
      it = (it->first == object || it->second == object) ? pairs.erase(it) : std::next(it);
    }
  }

  // JPH::ContactListener
  void OnContactAdded(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold&,
                      JPH::ContactSettings&) override {
    record_contact(static_cast<ObjectId>(a.GetUserData()), static_cast<ObjectId>(b.GetUserData()));
  }
  void OnContactPersisted(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold&,
                          JPH::ContactSettings&) override {
    record_contact(static_cast<ObjectId>(a.GetUserData()), static_cast<ObjectId>(b.GetUserData()));
  }

  // JPH::CharacterContactListener
  void OnContactAdded(const JPH::CharacterVirtual* character, const JPH::BodyID& body, const JPH::SubShapeID&,
                      JPH::RVec3Arg, JPH::Vec3Arg, JPH::CharacterContactSettings&) override {
    record_character_contact(character, body);
  }
  void OnContactPersisted(const JPH::CharacterVirtual* character, const JPH::BodyID& body, const JPH::SubShapeID&,
                          JPH::RVec3Arg, JPH::Vec3Arg, JPH::CharacterContactSettings&) override {
    record_character_contact(character, body);
  }
  void record_character_contact(const JPH::CharacterVirtual* character, const JPH::BodyID& body) {
    const auto it = character_owners_.find(character);
    if (it != character_owners_.end()) record_contact(it->second, owner_of(body));
  }

  JoltRuntime runtime_;
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

}  // namespace

std::unique_ptr<PhysicsWorld> create_jolt_physics_world(Vec3 gravity) {
  return std::make_unique<JoltPhysicsWorld>(gravity);
}

}  // namespace redgem
