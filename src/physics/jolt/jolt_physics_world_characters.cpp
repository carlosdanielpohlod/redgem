// Part of the Jolt world: characters, which walk on what is under them instead of being pushed
// around by forces.

#include "physics/jolt/jolt_physics_world.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

#include "core/math.hpp"
#include "physics/physics_config.hpp"

namespace redgem::jolt {
namespace {

// Vertical speed (relative to the ground) below which a grounded character sticks to the ground.
constexpr float kGroundedVerticalTolerance = 0.1f;

// Only contacts on the lower hemisphere of the capsule can support the character.
JPH::Plane supporting_volume(const ShapeInfo& info) {
  const float bottom_sphere_center = info.center_offset.y - info.capsule_half_height;
  return JPH::Plane(JPH::Vec3::sAxisY(), -bottom_sphere_center);
}

JPH::Ref<JPH::CharacterVirtualSettings> character_settings(const ShapeInfo& info, JPH::ObjectLayer layer) {
  JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
  settings->mShape = info.shape;
  settings->mInnerBodyShape = info.shape;
  settings->mInnerBodyLayer = layer;
  settings->mMaxSlopeAngle = degrees_to_radians(config::kCharacterMaxSlopeDegrees);
  settings->mMass = config::kCharacterMass;
  settings->mMaxStrength = config::kCharacterMaxStrength;
  settings->mCharacterPadding = config::kCharacterPadding;
  settings->mPenetrationRecoverySpeed = config::kCharacterPenetrationRecoverySpeed;
  settings->mPredictiveContactDistance = config::kCharacterPredictiveContactDistance;
  settings->mSupportingVolume = supporting_volume(info);
  return settings;
}

bool on_ground(const JPH::CharacterVirtual& character) {
  return character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
}

// Standing on the ground and not leaving it: its vertical speed is the ground's, or nearly.
bool rests_on_ground(const JPH::CharacterVirtual& character) {
  const float rising = character.GetLinearVelocity().GetY() - character.GetGroundVelocity().GetY();
  return on_ground(character) && rising < kGroundedVerticalTolerance;
}

// What a character's moves meet: everything but its own inner body and the bodies it carries.
class CarrierFilter final : public JPH::BodyFilter {
 public:
  CarrierFilter(JPH::BodyID inner, const std::vector<JPH::BodyID>& carried) : inner_(inner), carried_(carried) {}
  bool ShouldCollide(const JPH::BodyID& body) const override {
    return body != inner_ && std::find(carried_.begin(), carried_.end(), body) == carried_.end();
  }

 private:
  JPH::BodyID inner_;
  const std::vector<JPH::BodyID>& carried_;
};

// What a character's moves (and its changes of shape) meet: the bodies on the layers its own
// collides with, but not its inner body or what it carries.
struct CharacterFilters {
  CharacterFilters(const ObjectVsBroadPhase& broad_phase, const ObjectPairs& pairs, JPH::ObjectLayer layer,
                   const Record& record)
      : broad(broad_phase, layer), layers(pairs, layer), bodies(record.character->GetInnerBodyID(), record.carried) {}
  JPH::DefaultBroadPhaseLayerFilter broad;
  JPH::DefaultObjectLayerFilter layers;
  CarrierFilter bodies;
  JPH::ShapeFilter shapes;
};

bool is_positive(float measure) { return std::isfinite(measure) && measure > 0.0f; }

// A capsule a character can have: both measures positive, and no shorter than it is wide.
bool is_valid_capsule(CapsuleSize size) {
  return is_positive(size.height) && is_positive(size.radius) && size.height >= size.radius / kHalf;
}

bool same_measure(float a, float b) { return std::fabs(a - b) <= config::kCharacterSizeTolerance; }

bool same_size(CapsuleSize a, CapsuleSize b) {
  return same_measure(a.height, b.height) && same_measure(a.radius, b.radius);
}

// Whether a resize to that size can change anything: the object is a character, the size is one a
// capsule can have, and not the one it has.
bool can_resize(const Record* record, CapsuleSize size) {
  return is_character(record) && is_valid_capsule(size) && !same_size(record->character_size, size);
}

bool longer(float to, float from) { return to > from + config::kCharacterSizeTolerance; }

// Whether a character's new capsule (on the same feet) reaches out of its old one anywhere: only
// then can it meet what the old one did not.
bool grows(CapsuleSize from, CapsuleSize to) {
  return longer(to.height, from.height) || longer(to.radius, from.radius);
}

// How deep a new shape may sink into what is around it: what Jolt lets bodies rest in anyway.
float allowed_penetration(const JPH::PhysicsSystem& system) {
  return system.GetPhysicsSettings().mPenetrationSlop * config::kCharacterResizePenetrationSlopFactor;
}

// Whether a character stands on a body it carries: the box it picked up from under its feet is
// still its ground until its next move, which passes through the box.
bool stands_on_what_it_carries(const Record& record) {
  const JPH::BodyID ground = record.character->GetGroundBodyID();
  return std::find(record.carried.begin(), record.carried.end(), ground) != record.carried.end();
}

// The speed a character carries into a step, before gravity and its own move: the ground's
// (plus a jump) while it stands on it, else its own fall. What it carries is no ground: taking the
// speed of a box rising to its hand, it would rise with the box and lift it on, and fly.
JPH::Vec3 carried_velocity(const Record& record) {
  const JPH::CharacterVirtual& character = *record.character;
  if (!rests_on_ground(character)) return JPH::Vec3(0.0f, character.GetLinearVelocity().GetY(), 0.0f);
  const JPH::Vec3 ground_velocity =
      stands_on_what_it_carries(record) ? JPH::Vec3::sZero() : character.GetGroundVelocity();
  if (!record.pending_jump) return ground_velocity;
  return ground_velocity + JPH::Vec3(0.0f, *record.pending_jump, 0.0f);
}

}  // namespace

void World::create_character(const BodyDesc& desc, const ShapeInfo& info, Record& record) {
  // Characters stay upright: physics only drives their position.
  record.character =
      new Character(character_settings(info, object_layer_of(record)), JPH::RVec3(to_jolt(desc.world.position)),
                    JPH::Quat::sIdentity(), static_cast<JPH::uint64>(desc.object), &system_);
  record.character_bottom = collider_bottom(info);
  record.character_size = capsule_size_of(info);
  record.character->SetListener(this);
  // (A world that holds all the bodies it can makes the character without the body that others
  // collide with.)
  const JPH::BodyID inner = record.character->GetInnerBodyID();
  if (inner.IsInvalid()) tell_bodies_full();
  if (!inner.IsInvalid()) body_owners_[inner.GetIndexAndSequenceNumber()] = desc.object;
  character_owners_[record.character.GetPtr()] = desc.object;
}

void World::release_character(Record& record) {
  if (!is_character(&record)) return;
  body_owners_.erase(record.character->GetInnerBodyID().GetIndexAndSequenceNumber());
  record.character = nullptr;  // destroys the inner body too
}

void World::update_characters(float dt) {
  for (auto& [id, record] : records_) {
    if (is_character(&record)) update_character(record, dt);
  }
}

void World::update_character(Record& record, float dt) {
  JPH::CharacterVirtual& character = *record.character;
  character.UpdateGroundVelocity();
  JPH::Vec3 velocity = carried_velocity(record);
  record.pending_jump.reset();
  velocity += to_jolt(gravity_) * dt;
  velocity += to_jolt(record.desired_move);
  if (dt > 0.0f) velocity += to_jolt(record.pending_displacement) / dt;
  record.pending_displacement = Vec3{};
  character.SetLinearVelocity(velocity);
  move_character(record, dt);
}

void World::move_character(const Record& record, float dt) {
  JPH::CharacterVirtual& character = *record.character;
  JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
  update_settings.mStickToFloorStepDown = JPH::Vec3(0.0f, -config::kCharacterStickToFloorDistance, 0.0f);
  update_settings.mWalkStairsStepUp = JPH::Vec3(0.0f, config::kCharacterStairStepHeight, 0.0f);
  const CharacterFilters filters(object_vs_broad_phase_, object_pairs_, object_layer_of(record), record);
  character.ExtendedUpdate(dt, to_jolt(gravity_), update_settings, filters.broad, filters.layers, filters.bodies,
                           filters.shapes, temp_allocator_);
}

bool World::switch_character_shape(Record& record, const ShapeInfo& info, bool check) {
  const CharacterFilters filters(object_vs_broad_phase_, object_pairs_, object_layer_of(record), record);
  const float allowed = check ? allowed_penetration(system_) : FLT_MAX;
  Character& character = *record.character;
  if (!character.SetShape(info.shape, allowed, filters.broad, filters.layers, filters.bodies, filters.shapes,
                          temp_allocator_)) {
    return false;
  }
  if (!character.GetInnerBodyID().IsInvalid()) character.SetInnerBodyShape(info.shape);
  character.set_supporting_volume(supporting_volume(info));
  // (What it stands on is found again now: grounded? answers for the new shape before the next step.)
  character.RefreshContacts(filters.broad, filters.layers, filters.bodies, filters.shapes, temp_allocator_);
  record.shape = info.shape;
  return true;
}

bool World::resize_character(ObjectId object, float height, float radius) {
  Record* record = find(object);
  const CapsuleSize size{height, radius};
  if (!can_resize(record, size)) return false;
  const auto made = capsule_standing_on(record->character_bottom, size);
  if (std::holds_alternative<std::string>(made)) return false;
  const ShapeInfo& info = std::get<ShapeInfo>(made);
  const CapsuleSize built = capsule_size_of(info);
  if (!switch_character_shape(*record, info, grows(record->character_size, built))) return false;
  record->character_size = built;
  return true;
}

void World::set_character_move(ObjectId object, Vec3 horizontal_velocity) {
  Record* record = find(object);
  if (!is_character(record)) return;
  record->desired_move = {horizontal_velocity.x, 0.0f, horizontal_velocity.z};
}

void World::add_character_displacement(ObjectId object, Vec3 offset) {
  Record* record = find(object);
  if (!is_character(record)) return;
  record->pending_displacement = record->pending_displacement + Vec3{offset.x, 0.0f, offset.z};
}

void World::set_carried_bodies(ObjectId character, const std::vector<ObjectId>& carried) {
  Record* record = find(character);
  if (is_character(record)) record->carried = bodies_of(carried);
}

void World::character_jump(ObjectId object, float speed) {
  Record* record = find(object);
  if (!is_character(record)) return;
  record->pending_jump = speed;
}

bool World::character_grounded(ObjectId object) const {
  const Record* record = find(object);
  return is_character(record) && on_ground(*record->character);
}

}  // namespace redgem::jolt
