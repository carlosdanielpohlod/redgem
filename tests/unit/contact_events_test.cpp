#include <doctest/doctest.h>

#include <set>
#include <vector>

#include "physics/contact_events.hpp"

using namespace redgem;

namespace {

constexpr ObjectId kCrate = 3;
constexpr ObjectId kHero = 7;
constexpr ObjectId kFloor = 9;
constexpr ObjectId kZone = 12;  // a trigger volume
constexpr std::size_t kToldToBoth = 2;

bool same(const ContactEvent& event, ObjectId self, ObjectId other, ContactPhase phase, bool trigger) {
  return event.self == self && event.other == other && event.phase == phase && event.trigger == trigger;
}

}  // namespace

TEST_CASE("a pair of objects in contact is kept lower id first, whichever way it is met") {
  CHECK(ordered(kHero, kCrate) == ContactPair{kCrate, kHero});
  CHECK(ordered(kCrate, kHero) == ContactPair{kCrate, kHero});
}

TEST_CASE("only two different objects make a contact") {
  CHECK(between_two_objects(kCrate, kHero));
  CHECK_FALSE(between_two_objects(kCrate, kCrate));
  CHECK_FALSE(between_two_objects(kNoObject, kHero));
  CHECK_FALSE(between_two_objects(kCrate, kNoObject));
}

TEST_CASE("a contact that begins is told to both objects, and one that goes on is not told again") {
  std::vector<ContactEvent> events;
  const std::set<ContactPair> touching = {ordered(kCrate, kHero)};
  append_contact_changes({}, touching, events);
  REQUIRE(events.size() == kToldToBoth);
  CHECK(same(events[0], kCrate, kHero, ContactPhase::Enter, false));
  CHECK(same(events[1], kHero, kCrate, ContactPhase::Enter, false));

  append_contact_changes(touching, touching, events);
  CHECK(events.size() == kToldToBoth);
}

TEST_CASE("contacts that begin are told before the ones that end") {
  std::vector<ContactEvent> events;
  append_contact_changes({ordered(kCrate, kHero)}, {ordered(kHero, kFloor)}, events);
  REQUIRE(events.size() == kToldToBoth + kToldToBoth);
  CHECK(same(events[0], kHero, kFloor, ContactPhase::Enter, false));
  CHECK(same(events[1], kFloor, kHero, ContactPhase::Enter, false));
  CHECK(same(events[2], kCrate, kHero, ContactPhase::Exit, false));
  CHECK(same(events[3], kHero, kCrate, ContactPhase::Exit, false));
}

TEST_CASE("a trigger is told who came in and who left, and they are not") {
  std::vector<ContactEvent> events;
  append_trigger_changes(kZone, {kCrate}, {kHero}, events);
  REQUIRE(events.size() == kToldToBoth);
  CHECK(same(events[0], kZone, kHero, ContactPhase::Enter, true));
  CHECK(same(events[1], kZone, kCrate, ContactPhase::Exit, true));

  append_trigger_changes(kZone, {kHero}, {kHero}, events);
  CHECK(events.size() == kToldToBoth);  // staying inside is not news
}

TEST_CASE("forgetting an object drops every pair it is part of and no other") {
  std::set<ContactPair> pairs = {ordered(kCrate, kHero), ordered(kHero, kFloor), ordered(kCrate, kFloor)};
  erase_pairs_with(kHero, pairs);
  CHECK(pairs == std::set<ContactPair>{ordered(kCrate, kFloor)});
  erase_pairs_with(kZone, pairs);
  CHECK(pairs.size() == 1);
}
