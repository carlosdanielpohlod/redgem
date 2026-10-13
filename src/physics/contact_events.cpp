#include "physics/contact_events.hpp"

#include <iterator>

namespace redgem {
namespace {

void tell_both(const ContactPair& pair, ContactPhase phase, std::vector<ContactEvent>& events) {
  events.push_back({pair.first, pair.second, phase, false});
  events.push_back({pair.second, pair.first, phase, false});
}

bool involves(const ContactPair& pair, ObjectId object) { return pair.first == object || pair.second == object; }

}  // namespace

ContactPair ordered(ObjectId a, ObjectId b) { return a < b ? ContactPair{a, b} : ContactPair{b, a}; }

bool between_two_objects(ObjectId a, ObjectId b) { return a != kNoObject && b != kNoObject && a != b; }

void append_contact_changes(const std::set<ContactPair>& before, const std::set<ContactPair>& now,
                            std::vector<ContactEvent>& events) {
  for (const ContactPair& pair : now) {
    if (before.count(pair) == 0) tell_both(pair, ContactPhase::Enter, events);
  }
  for (const ContactPair& pair : before) {
    if (now.count(pair) == 0) tell_both(pair, ContactPhase::Exit, events);
  }
}

void append_trigger_changes(ObjectId trigger, const std::set<ObjectId>& before, const std::set<ObjectId>& now,
                            std::vector<ContactEvent>& events) {
  for (const ObjectId other : now) {
    if (before.count(other) == 0) events.push_back({trigger, other, ContactPhase::Enter, true});
  }
  for (const ObjectId other : before) {
    if (now.count(other) == 0) events.push_back({trigger, other, ContactPhase::Exit, true});
  }
}

void erase_pairs_with(ObjectId object, std::set<ContactPair>& pairs) {
  for (auto it = pairs.begin(); it != pairs.end();) {
    it = involves(*it, object) ? pairs.erase(it) : std::next(it);
  }
}

}  // namespace redgem
