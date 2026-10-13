// Part of the Jolt world: who touches whom. Jolt says which bodies are in contact during a step;
// what began and what ended is worked out after it, and told as events.

#include "physics/jolt/jolt_physics_world.hpp"

#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

#include <iterator>
#include <utility>

namespace redgem::jolt {
namespace {

// Trigger shapes are built at their final size, so queries use them unscaled.
constexpr float kUnitScale = 1.0f;

JPH::RMat44 matrix_of(const Transform& pose) {
  return JPH::RMat44::sRotationTranslation(to_jolt(pose.rotation), JPH::RVec3(to_jolt(pose.position)));
}

ObjectId object_of(const JPH::Body& body) { return static_cast<ObjectId>(body.GetUserData()); }

// Whether a trigger has `other` inside it: some object, and not the trigger's own.
bool holds(ObjectId trigger, ObjectId other) { return other != kNoObject && other != trigger; }

}  // namespace

// The objects inside a trigger right now.
std::set<ObjectId> World::objects_inside(ObjectId id, const Record& trigger) const {
  JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
  const JPH::RMat44 transform = matrix_of(trigger.pose);
  // Only moving things (characters, dynamic and kinematic bodies) fire triggers.
  const JPH::SpecifiedBroadPhaseLayerFilter broad_filter(broad_phase_layers::kMoving);
  const JPH::SpecifiedObjectLayerFilter layer_filter(layers::kMoving);
  system_.GetNarrowPhaseQuery().CollideShape(trigger.shape, JPH::Vec3::sReplicate(kUnitScale), transform,
                                             JPH::CollideShapeSettings(), JPH::RVec3::sZero(), collector, broad_filter,
                                             layer_filter);
  std::set<ObjectId> inside;
  for (const JPH::CollideShapeResult& hit : collector.mHits) {
    const ObjectId other = owner_of(hit.mBodyID2);
    if (holds(id, other)) inside.insert(other);
  }
  return inside;
}

void World::update_triggers() {
  for (auto& [id, record] : records_) {
    if (!record.trigger) continue;
    std::set<ObjectId> now = objects_inside(id, record);
    append_trigger_changes(id, record.overlaps, now, events_);
    record.overlaps = std::move(now);
  }
}

void World::collect_contact_events() {
  std::set<ContactPair> current;
  {
    std::lock_guard<std::mutex> lock(contacts_mutex_);
    current.swap(current_contacts_);
  }
  append_contact_changes(previous_contacts_, current, events_);
  previous_contacts_ = std::move(current);
}

std::vector<ContactEvent> World::drain_events() { return std::exchange(events_, {}); }

void World::record_contact(ObjectId a, ObjectId b) {
  if (!between_two_objects(a, b)) return;
  std::lock_guard<std::mutex> lock(contacts_mutex_);
  current_contacts_.insert(ordered(a, b));
}

void World::record_character_contact(const JPH::CharacterVirtual* character, const JPH::BodyID& body) {
  const auto it = character_owners_.find(character);
  if (it != character_owners_.end()) record_contact(it->second, owner_of(body));
}

void World::forget(ObjectId object) {
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

// What Jolt tells about rigid bodies (JPH::ContactListener).
void World::OnContactAdded(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold&, JPH::ContactSettings&) {
  record_contact(object_of(a), object_of(b));
}

void World::OnContactPersisted(const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold&,
                               JPH::ContactSettings&) {
  record_contact(object_of(a), object_of(b));
}

// What Jolt tells about characters (JPH::CharacterContactListener).
void World::OnContactAdded(const JPH::CharacterVirtual* character, const JPH::BodyID& body, const JPH::SubShapeID&,
                           JPH::RVec3Arg, JPH::Vec3Arg, JPH::CharacterContactSettings&) {
  record_character_contact(character, body);
}

void World::OnContactPersisted(const JPH::CharacterVirtual* character, const JPH::BodyID& body, const JPH::SubShapeID&,
                               JPH::RVec3Arg, JPH::Vec3Arg, JPH::CharacterContactSettings&) {
  record_character_contact(character, body);
}

}  // namespace redgem::jolt
