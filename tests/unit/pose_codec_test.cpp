#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "net/network_session.hpp"
#include "net/pose_baselines.hpp"
#include "net/pose_codec.hpp"
#include "net/pose_quantization.hpp"

using namespace redgem;
using namespace redgem::net;

namespace {

constexpr float kMostTurnError = 0.3f;  // degrees a rotation comes back off by, at most
constexpr float kAngleStep = 7.0f;      // degrees between the rotations tried, around each axis
constexpr float kFullTurn = 360.0f;
constexpr float kHalfStep = kPoseStep / 2.0f;
constexpr float kTooFar = 40.0f;  // meters: more than a move holds
constexpr float kFarAway = 10000.0f;
constexpr int kSender = 2;
constexpr int kOtherSender = 3;
constexpr int kManyMoves = 2000;
constexpr float kCreep = 0.3f * kPoseStep;           // a move smaller than a step, again and again
constexpr std::size_t kWholeBytes = 2 + 6 + 12 + 4;  // flags, key size, "hero-2", where, rotation
constexpr std::size_t kMoveBytes = 2 + 6 + 6 + 4;
constexpr std::size_t kStillBytes = 2 + 6;
constexpr std::uint8_t kUnknownFlag = 8;
constexpr std::uint8_t kBothPositions = 3;
constexpr std::uint32_t kRandomSeed = 12345;
constexpr std::uint32_t kRandomMultiplier = 1664525;
constexpr std::uint32_t kRandomIncrement = 1013904223;
constexpr int kRandomTries = 20000;
constexpr std::size_t kMostRandomBytes = 40;
constexpr int kRandomShift = 24;
constexpr std::uint8_t kFlagCombinations = 8;  // of the three flags a pose has
const std::string kKey = "hero-2";

// The angle, in degrees, between two rotations.
double degrees_between(Quat a, Quat b) {
  const double dot = std::fabs(double{a.x} * b.x + double{a.y} * b.y + double{a.z} * b.z + double{a.w} * b.w);
  return 2.0 * std::acos(std::min(1.0, dot)) * kDegreesPerHalfTurn / kPi;
}

Quat round_trip(Quat rotation) { return unpacked_rotation(packed_rotation(rotation)); }

PoseUpdate whole_pose() {
  PoseUpdate update;
  update.key = kKey;
  update.position = Vec3{1.0f, -2.5f, 1e5f};
  update.rotation = packed_rotation(quat_from_euler_degrees({10.0f, 20.0f, 30.0f}));
  return update;
}

// What a sender's update becomes on the way: bytes, and back.
PoseUpdate sent(const PoseUpdate& update) { return decoded(encoded(update)).value(); }

// A small random number generator, the same on every machine.
struct Random {
  std::uint32_t state = kRandomSeed;
  std::uint8_t next() {
    state = state * kRandomMultiplier + kRandomIncrement;
    return static_cast<std::uint8_t>(state >> kRandomShift);
  }
};

}  // namespace

TEST_CASE("a rotation comes back within a third of a degree, all the way round") {
  double worst = 0.0;
  for (float x = 0.0f; x < kFullTurn; x += kAngleStep) {
    for (float y = 0.0f; y < kFullTurn; y += kAngleStep) {
      for (float z = 0.0f; z < kFullTurn; z += kAngleStep) {
        const Quat rotation = quat_from_euler_degrees({x, y, z});
        worst = std::max(worst, degrees_between(rotation, round_trip(rotation)));
      }
    }
  }
  CHECK(worst <= kMostTurnError);
  CHECK(degrees_between(Quat::identity(), round_trip(Quat::identity())) <= kMostTurnError);
}

TEST_CASE("a rotation and its opposite quaternion travel as the same 32 bits") {
  const Quat rotation = normalize(Quat{0.3f, -0.5f, 0.1f, -0.8f});
  CHECK(packed_rotation(rotation) == packed_rotation(Quat{-rotation.x, -rotation.y, -rotation.z, -rotation.w}));
}

TEST_CASE("a move travels in steps of about a millimeter, as far as 16 bits hold") {
  for (const Vec3 from : {Vec3{0.0f, 0.0f, 0.0f}, Vec3{kFarAway, -kFarAway, 1.0f}}) {
    const Vec3 to = from + Vec3{0.1234f, -3.21f, 31.0f};
    const auto steps = steps_between(from, to);
    REQUIRE(steps.has_value());
    CHECK(approx_equal(moved_by(from, *steps), to, kPoseStep));
  }
  CHECK_FALSE(steps_between({}, {kTooFar, 0.0f, 0.0f}).has_value());
  CHECK_FALSE(steps_between({}, {0.0f, std::nanf(""), 0.0f}).has_value());
  CHECK_FALSE(moves(*steps_between({}, {kHalfStep / 2.0f, 0.0f, 0.0f})));
}

TEST_CASE("a pose comes back from its bytes as it was, in as few bytes as what changed takes") {
  const PoseUpdate whole = whole_pose();
  const std::vector<std::uint8_t> bytes = encoded(whole);
  CHECK(bytes.size() == kWholeBytes);
  const PoseUpdate back = sent(whole);
  CHECK(back.key == kKey);
  CHECK(approx_equal(*back.position, *whole.position, 0.0f));
  CHECK(back.rotation == whole.rotation);
  CHECK_FALSE(back.moved.has_value());
  CHECK(back.whole());

  PoseUpdate move;
  move.key = kKey;
  move.moved = PoseSteps{-32767, 0, 32767};
  move.rotation = whole.rotation;
  CHECK(encoded(move).size() == kMoveBytes);
  CHECK(sent(move).moved == move.moved);

  PoseUpdate still;
  still.key = kKey;
  CHECK(encoded(still).size() == kStillBytes);
  CHECK_FALSE(sent(still).whole());
}

TEST_CASE("bytes that are not laid out as a pose are no pose") {
  std::vector<std::uint8_t> bytes = encoded(whole_pose());
  for (std::size_t size = 0; size < bytes.size(); ++size) {
    CHECK_FALSE(decoded({bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size)}).has_value());
  }
  std::vector<std::uint8_t> longer = bytes;
  longer.push_back(0);
  CHECK_FALSE(decoded(longer).has_value());
  for (const std::uint8_t flags : {kUnknownFlag, kBothPositions}) {
    std::vector<std::uint8_t> odd = bytes;
    odd[0] = flags;
    CHECK_FALSE(decoded(odd).has_value());
  }
  std::vector<std::uint8_t> long_key{0, static_cast<std::uint8_t>(kMaxPoseKeyBytes + 1)};
  long_key.resize(long_key.size() + kMaxPoseKeyBytes + 1, 'k');
  CHECK_FALSE(decoded(long_key).has_value());
  CHECK_FALSE(decoded({0, 1, 0xFF}).has_value());  // a key that is not UTF-8
}

TEST_CASE("whatever bytes arrive, what is read as a pose is exactly what those bytes say") {
  Random random;
  int poses = 0;
  for (int i = 0; i < kRandomTries; ++i) {
    std::vector<std::uint8_t> bytes(random.next() % kMostRandomBytes);
    for (std::uint8_t& byte : bytes) byte = random.next();
    // (Flags it knows and short keys, so that some fit.)
    if (bytes.size() > 1) {
      bytes[0] = static_cast<std::uint8_t>(bytes[0] % kFlagCombinations);
      bytes[1] = static_cast<std::uint8_t>(bytes[1] % kStillBytes);
    }
    const auto pose = decoded(bytes);
    if (!pose) continue;
    ++poses;
    // (A position that is not a number may not keep its bits on the way through a float.)
    const bool comparable = !pose->position || std::isfinite(pose->position->x + pose->position->y + pose->position->z);
    if (comparable) CHECK(encoded(*pose) == bytes);
  }
  CHECK(poses > 0);
}

TEST_CASE("a sender says a whole pose first, then only what changed, and a whole one again now and then") {
  PoseSender sender;
  const Quat facing = quat_from_euler_degrees({0.0f, 45.0f, 0.0f});
  CHECK(sender.update(kKey, {1.0f, 0.0f, 0.0f}, facing).whole());
  const PoseUpdate still = sender.update(kKey, {1.0f, 0.0f, 0.0f}, facing);
  CHECK_FALSE(still.position.has_value());
  CHECK_FALSE(still.moved.has_value());
  CHECK_FALSE(still.rotation.has_value());
  const PoseUpdate moved = sender.update(kKey, {1.5f, 0.0f, 0.0f}, facing);
  CHECK(moved.moved.has_value());
  CHECK_FALSE(moved.rotation.has_value());
  const PoseUpdate turned = sender.update(kKey, {1.5f, 0.0f, 0.0f}, Quat::identity());
  CHECK_FALSE(turned.moved.has_value());
  CHECK(turned.rotation.has_value());
  const Vec3 far{1.5f + kTooFar, 0.0f, 0.0f};
  CHECK(sender.update(kKey, far, Quat::identity()).whole());  // too far for a move
  for (int i = 1; i < kSendsPerWholePose; ++i) CHECK_FALSE(sender.update(kKey, far, Quat::identity()).whole());
  CHECK(sender.update(kKey, far, Quat::identity()).whole());
  sender.start_over();
  CHECK(sender.update(kKey, far, Quat::identity()).whole());
  CHECK(sender.update("another", far, Quat::identity()).whole());
}

TEST_CASE("what a receiver makes of the changes is where the sender has the object, however many there are") {
  PoseSender sender;
  PoseReceiver receiver;
  Vec3 position{kFarAway, 1.0f, -kFarAway};
  for (int i = 0; i < kManyMoves; ++i) {
    position = position + Vec3{kCreep, -kCreep, 0.01f};
    const auto pose = receiver.take(kSender, sent(sender.update(kKey, position, Quat::identity())));
    REQUIRE(pose.has_value());
    CHECK(approx_equal(pose->position, position, kPoseStep));  // never further off than a step
  }
}

TEST_CASE("a receiver makes nothing of changes until a whole pose came from that machine") {
  PoseSender sender;
  PoseReceiver receiver;
  const PoseUpdate first = sent(sender.update(kKey, {1.0f, 2.0f, 3.0f}, Quat::identity()));
  const PoseUpdate change = sent(sender.update(kKey, {1.5f, 2.0f, 3.0f}, Quat::identity()));
  CHECK_FALSE(receiver.take(kSender, change).has_value());  // it arrived late: the whole pose went by
  REQUIRE(receiver.take(kSender, first).has_value());
  CHECK(receiver.take(kSender, change)->position.x == doctest::Approx(1.5f));
  // The same key from another machine is another object.
  CHECK_FALSE(receiver.take(kOtherSender, change).has_value());
  // A machine that left is forgotten, and the others are not.
  receiver.take(kOtherSender, first);
  receiver.forget(kSender);
  CHECK_FALSE(receiver.take(kSender, change).has_value());
  CHECK(receiver.take(kOtherSender, change).has_value());
}

namespace {

std::string key_number(std::size_t number) { return "crate#" + std::to_string(number); }

}  // namespace

TEST_CASE("a sender that keeps all the poses it may forgets none for an object it knows") {
  PoseSender sender;
  for (std::size_t i = 0; i < kMaxRememberedPoses; ++i) sender.update(key_number(i), {}, Quat::identity());
  // The first object's whole pose comes round again: the others stay known.
  for (int i = 0; i < kSendsPerWholePose; ++i) sender.update(key_number(0), {}, Quat::identity());
  CHECK_FALSE(sender.update(key_number(1), {}, Quat::identity()).whole());
  // A new one is one too many: all start over.
  CHECK(sender.update("one more", {}, Quat::identity()).whole());
  CHECK(sender.update(key_number(1), {}, Quat::identity()).whole());
}

TEST_CASE("a receiver keeps so many poses of each machine, and one machine's never wipe another's") {
  PoseReceiver receiver;
  PoseSender sender;
  const auto whole = [&sender](const std::string& key) {
    sender.start_over();
    return sent(sender.update(key, {}, Quat::identity()));
  };
  PoseUpdate still;
  still.key = key_number(0);
  REQUIRE(receiver.take(kOtherSender, whole(key_number(0))).has_value());
  for (std::size_t i = 0; i < kMaxRememberedPoses; ++i)
    REQUIRE(receiver.take(kSender, whole(key_number(i))).has_value());
  // A whole pose of an object it knows already forgets nothing.
  REQUIRE(receiver.take(kSender, whole(key_number(0))).has_value());
  CHECK(receiver.take(kSender, still).has_value());
  // One more of that machine's: its own start over, the other machine's are kept.
  REQUIRE(receiver.take(kSender, whole("one more")).has_value());
  CHECK_FALSE(receiver.take(kSender, still).has_value());
  CHECK(receiver.take(kOtherSender, still).has_value());
}

TEST_CASE("a pose that is not numbers is not sent") {
  // (Network.__pose cannot make one, but a session takes it from anywhere.)
  NetworkSession session;
  std::string error;
  REQUIRE(session.host(0, error));
  CHECK(session.send_pose("ok", {}, Quat::identity()));
  CHECK_FALSE(session.send_pose("nan", {std::nanf(""), 0.0f, 0.0f}, Quat::identity()));
  CHECK_FALSE(session.send_pose("inf", {}, Quat{0.0f, std::numeric_limits<float>::infinity(), 0.0f, 1.0f}));
  CHECK_FALSE(all_finite(Quat{std::nanf(""), 0.0f, 0.0f, 1.0f}));
  CHECK(all_finite(Vec3{1e30f, -1e30f, 0.0f}));
}
