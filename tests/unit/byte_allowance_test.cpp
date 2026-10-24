// What a machine may send to a program that serves many, and the stalls of the receiver forgiven.

#include <doctest/doctest.h>

#include "net/byte_allowance.hpp"

using redgem::net::ByteAllowance;

namespace {

constexpr float kPerSecond = 100.0f;
constexpr float kBurst = 200.0f;
constexpr float kFrameSeconds = 0.1f;
constexpr std::size_t kPerFrame = 10;  // what the sender says a frame: exactly its allowance
constexpr float kStallSeconds = 5.0f;
constexpr float kMostForgivenSeconds = 10.0f;
constexpr int kSlowDrainFrames = 100;  // ten seconds for the stall's backlog to arrive

}  // namespace

TEST_CASE("an allowance holds a burst at once and earns the rest back over time, up to the burst") {
  ByteAllowance allowance(kPerSecond, kBurst);
  CHECK(allowance.charge(static_cast<std::size_t>(kBurst)));
  CHECK_FALSE(allowance.charge(1));
  ByteAllowance rested(kPerSecond, kBurst);
  rested.refill(kMostForgivenSeconds);  // (resting long earns no more than the burst)
  CHECK(rested.charge(static_cast<std::size_t>(kBurst)));
  CHECK_FALSE(rested.charge(1));
}

TEST_CASE("what a stall forgave pays for the backlog however late it arrives, and the sender's pace goes on") {
  ByteAllowance allowance(kPerSecond, kBurst);
  allowance.forgive(kStallSeconds, kMostForgivenSeconds);
  // The backlog is slow to come: frame after frame only what the sender says at its pace arrives...
  for (int frame = 0; frame < kSlowDrainFrames; ++frame) {
    allowance.refill(kFrameSeconds);
    REQUIRE(allowance.charge(kPerFrame));
  }
  // ... and then all the stall's at once, with a full burst on top.
  allowance.refill(kFrameSeconds);
  CHECK(allowance.charge(static_cast<std::size_t>(kStallSeconds * kPerSecond + kBurst)));
  CHECK_FALSE(allowance.charge(1));  // and nothing past it: forgiven once, spent once
}

TEST_CASE("stalls forgiven one after another add up, never past the most forgiven") {
  ByteAllowance allowance(kPerSecond, kBurst);
  allowance.forgive(kStallSeconds, kMostForgivenSeconds);
  allowance.forgive(kStallSeconds, kMostForgivenSeconds);
  allowance.forgive(kStallSeconds, kMostForgivenSeconds);
  CHECK(allowance.charge(static_cast<std::size_t>(kMostForgivenSeconds * kPerSecond + kBurst)));
  CHECK_FALSE(allowance.charge(1));
}
