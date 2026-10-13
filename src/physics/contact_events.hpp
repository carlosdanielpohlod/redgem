#pragma once

// Which objects began and stopped touching between two steps: the bookkeeping behind contact and
// trigger events. No physics library needed, so it can be tested.

#include <set>
#include <utility>
#include <vector>

#include "physics/physics_world.hpp"

namespace redgem {

// Two objects in contact, the lower id first.
using ContactPair = std::pair<ObjectId, ObjectId>;
ContactPair ordered(ObjectId a, ObjectId b);

// Whether two bodies touching is a contact between two objects: a body nobody owns, or two bodies
// of the same object, are not.
bool between_two_objects(ObjectId a, ObjectId b);

// Appends what changed from one step to the next: an Enter for every pair that is new, then an
// Exit for every pair that is gone, each told to both of its objects.
void append_contact_changes(const std::set<ContactPair>& before, const std::set<ContactPair>& now,
                            std::vector<ContactEvent>& events);

// The same for a trigger volume and the objects inside it. Only the trigger is told.
void append_trigger_changes(ObjectId trigger, const std::set<ObjectId>& before, const std::set<ObjectId>& now,
                            std::vector<ContactEvent>& events);

// Forgets every pair `object` is part of.
void erase_pairs_with(ObjectId object, std::set<ContactPair>& pairs);

}  // namespace redgem
