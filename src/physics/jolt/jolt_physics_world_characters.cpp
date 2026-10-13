// Part of the Jolt world: characters, which walk on what is under them instead of being pushed
// around by forces.

#include "physics/jolt/jolt_physics_world.hpp"

#include "physics/physics_config.hpp"

namespace redgem::jolt {
namespace {

// Vertical speed (relative to the ground) below which a grounded character sticks to the ground.
constexpr float kGroundedVerticalTolerance = 0.1f;

JPH::Ref<JPH::CharacterVirtualSettings> character_settings(const ShapeInfo& info) {
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

bool on_ground(const JPH::CharacterVirtual& character) {
  return character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
}

// Standing on the ground and not leaving it: its vertical speed is the ground's, or nearly.
bool rests_on_ground(const JPH::CharacterVirtual& character) {
  const float rising = character.GetLinearVelocity().GetY() - character.GetGroundVelocity().GetY();
  return on_ground(character) && rising < kGroundedVerticalTolerance;
}

// The speed a character carries into a step, before gravity and its own move: the ground's
// (plus a jump) while it stands on it, else its own fall.
JPH::Vec3 carried_velocity(const Record& record) {
  const JPH::CharacterVirtual& character = *record.character;
  if (!rests_on_ground(character)) return JPH::Vec3(0.0f, character.GetLinearVelocity().GetY(), 0.0f);
  const JPH::Vec3 ground_velocity = character.GetGroundVelocity();
  if (!record.pending_jump) return ground_velocity;
  return ground_velocity + JPH::Vec3(0.0f, *record.pending_jump, 0.0f);
}

}  // namespace

void World::create_character(const BodyDesc& desc, const ShapeInfo& info, Record& record) {
  // Characters stay upright: physics only drives their position.
  record.character = new JPH::CharacterVirtual(character_settings(info), JPH::RVec3(to_jolt(desc.world.position)),
                                               JPH::Quat::sIdentity(), static_cast<JPH::uint64>(desc.object), &system_);
  record.character->SetListener(this);
  body_owners_[record.character->GetInnerBodyID().GetIndexAndSequenceNumber()] = desc.object;
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
  character.SetLinearVelocity(velocity);
  move_character(character, dt);
}

void World::move_character(JPH::CharacterVirtual& character, float dt) {
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

void World::set_character_move(ObjectId object, Vec3 horizontal_velocity) {
  Record* record = find(object);
  if (!is_character(record)) return;
  record->desired_move = {horizontal_velocity.x, 0.0f, horizontal_velocity.z};
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
