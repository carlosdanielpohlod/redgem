// The objects the host's game placed: the poses their machine sent before it heard are not used.

#include <doctest/doctest.h>

#include <string>

#include "net/placed_objects.hpp"

using redgem::Vec3;
using redgem::net::kPlacedReach;
using redgem::net::kPlacedWaitSeconds;
using redgem::net::PlacedObjects;

namespace {

constexpr int kPeer = 2;
constexpr int kOtherPeer = 3;
constexpr const char* kHero = "hero-2";
constexpr float kSpawn = 100.0f;
constexpr float kWhereItWas = 0.0f;
constexpr float kStep = kPlacedReach / 2.0f;  // a move its machine made right after it heard

Vec3 along_x(float meters) { return {meters, 0.0f, 0.0f}; }

}  // namespace

TEST_CASE("the poses from before the machine heard are not used, until one from the placed spot") {
  PlacedObjects placed;
  CHECK_FALSE(placed.from_before(kPeer, kHero, along_x(kWhereItWas)));  // nothing placed: every pose counts
  placed.place(kPeer, kHero, along_x(kSpawn));
  CHECK(placed.from_before(kPeer, kHero, along_x(kWhereItWas)));
  CHECK(placed.from_before(kPeer, kHero, along_x(kWhereItWas)));
  CHECK_FALSE(placed.from_before(kOtherPeer, kHero, along_x(kWhereItWas)));  // another machine's is not placed
  CHECK_FALSE(placed.from_before(kPeer, kHero, along_x(kSpawn + kStep)));    // it heard
  CHECK(placed.empty());
  CHECK_FALSE(placed.from_before(kPeer, kHero, along_x(kWhereItWas)));  // from then on its poses count
}

TEST_CASE("a placed object's poses count again after a while, or once its machine left") {
  PlacedObjects placed;
  placed.place(kPeer, kHero, along_x(kSpawn));
  placed.advance(kPlacedWaitSeconds / 2.0f);
  CHECK(placed.from_before(kPeer, kHero, along_x(kWhereItWas)));
  placed.advance(kPlacedWaitSeconds);
  CHECK_FALSE(placed.from_before(kPeer, kHero, along_x(kWhereItWas)));
  placed.place(kPeer, kHero, along_x(kSpawn));
  placed.forget(kPeer);
  CHECK(placed.empty());
}
