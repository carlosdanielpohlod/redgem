// What a pose carries of its object's own animation (net/animation_codec.hpp), and when its
// sender has it carry that (PoseSender).

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "net/pose_baselines.hpp"
#include "net/pose_codec.hpp"

using namespace redgem;
using namespace redgem::net;

namespace {

const std::string kKey = "samurai-2";
constexpr float kFade = 0.2f;
constexpr float kWeight = 0.5f;
constexpr float kAttackTime = 0.15f;
constexpr float kFaster = 1.5f;
constexpr std::uint32_t kFirstRevision = 3;
constexpr std::uint32_t kNextRevision = 4;
constexpr Vec3 kHere{1.0f, 0.0f, 2.0f};
constexpr std::size_t kHeadBytes = 2 * kFloatBytes;
constexpr std::size_t kNoClipBytes = 1;
constexpr int kSendsBeforeWhole = kSendsPerWholePose;

AnimationPart attacking() {
  AnimationPart part;
  part.fade = kFade;
  part.layer_weight = kWeight;
  part.own = ClipPart{"Attack", kAttackTime, kFaster, false, true};
  part.layer = ClipPart{"Breathe", 0.0f, 1.0f, true, false};
  return part;
}

PoseUpdate with_animation(const AnimationPart& part) {
  PoseUpdate update;
  update.key = kKey;
  update.animation = part;
  return update;
}

bool same_clip(const std::optional<ClipPart>& a, const std::optional<ClipPart>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  return a->clip == b->clip && a->time == b->time && a->speed == b->speed && a->loop == b->loop &&
         a->playing == b->playing;
}

}  // namespace

TEST_CASE("an animation comes back from a pose's bytes as it was") {
  const std::optional<PoseUpdate> back = decoded(encoded(with_animation(attacking())));
  REQUIRE(back.has_value());
  REQUIRE(back->animation.has_value());
  const AnimationPart& part = *back->animation;
  CHECK(part.fade == kFade);
  CHECK(part.layer_weight == kWeight);
  CHECK(same_clip(part.own, attacking().own));
  CHECK(same_clip(part.layer, attacking().layer));
}

TEST_CASE("a character following the scene's clips takes a few bytes") {
  const PoseUpdate update = with_animation(AnimationPart{});
  PoseUpdate bare;
  bare.key = kKey;
  CHECK(encoded(update).size() == encoded(bare).size() + kHeadBytes + 2 * kNoClipBytes);
  const std::optional<PoseUpdate> back = decoded(encoded(update));
  REQUIRE(back.has_value());
  CHECK_FALSE(back->animation->own.has_value());
  CHECK_FALSE(back->animation->layer.has_value());
}

TEST_CASE("bytes that are not laid out as an animation are no pose") {
  const std::vector<std::uint8_t> bytes = encoded(with_animation(attacking()));
  const std::size_t bare = encoded(PoseUpdate{kKey, {}, {}, {}, {}}).size();
  for (std::size_t size = bare; size < bytes.size(); ++size) {
    CHECK_FALSE(decoded({bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size)}).has_value());
  }
  std::vector<std::uint8_t> longer = bytes;
  longer.push_back(0);
  CHECK_FALSE(decoded(longer).has_value());
  std::vector<std::uint8_t> bad_flags = bytes;
  bad_flags.back() = 0xFF;  // the mixed clip's flags
  CHECK_FALSE(decoded(bad_flags).has_value());
  std::vector<std::uint8_t> long_name = encoded(with_animation(AnimationPart{}));
  long_name[bare + kHeadBytes] = static_cast<std::uint8_t>(kMaxClipNameBytes + 1);
  CHECK_FALSE(decoded(long_name).has_value());
}

TEST_CASE("a clip whose name is longer than an animation carries cannot be sent") {
  AnimationPart part = attacking();
  CHECK(carriable(part));
  part.layer->clip = std::string(kMaxClipNameBytes + 1, 'x');
  CHECK_FALSE(carriable(part));
}

TEST_CASE("a clip without a name cannot be sent: it would read as no clip, and the pose would be no pose") {
  AnimationPart part = attacking();
  part.own->clip.clear();
  CHECK_FALSE(carriable(part));
  // (What its bytes would be: a size of 0, then numbers nothing reads.)
  CHECK_FALSE(decoded(encoded(with_animation(part))).has_value());
}

TEST_CASE("the animation goes with a whole pose and when it changed, not with every pose") {
  PoseSender sender;
  const Animated first{kFirstRevision, attacking()};
  CHECK(sender.update(kKey, kHere, Quat::identity(), first).animation.has_value());  // the first pose is whole
  CHECK_FALSE(sender.update(kKey, kHere, Quat::identity(), first).animation.has_value());
  CHECK(sender.update(kKey, kHere, Quat::identity(), Animated{kNextRevision, AnimationPart{}}).animation.has_value());
  CHECK_FALSE(sender.update(kKey, kHere, Quat::identity(), Animated{kNextRevision, {}}).animation.has_value());
  sender.start_over();  // someone joined: they hear it with the next, whole, pose
  CHECK(sender.update(kKey, kHere, Quat::identity(), first).animation.has_value());
  bool again = false;
  for (int send = 0; send < kSendsBeforeWhole; ++send) {
    again = sender.update(kKey, kHere, Quat::identity(), first).animation.has_value();
  }
  CHECK(again);  // and every so often with the whole pose, in case it drifted
  CHECK_FALSE(sender.update(kKey, kHere, Quat::identity()).animation.has_value());  // an object that sends none
}

TEST_CASE("a sender says beforehand whether the next pose would tell an animation, so it is built only then") {
  PoseSender sender;
  CHECK(sender.tells_animation(kKey, kFirstRevision));  // the first pose is whole
  sender.update(kKey, kHere, Quat::identity(), Animated{kFirstRevision, attacking()});
  CHECK_FALSE(sender.tells_animation(kKey, kFirstRevision));  // heard already
  CHECK(sender.tells_animation(kKey, kNextRevision));         // changed since
  sender.start_over();
  CHECK(sender.tells_animation(kKey, kFirstRevision));  // someone joined
}

TEST_CASE("a pose with an animation still fits what a pose may be") {
  AnimationPart longest;
  longest.own = ClipPart{std::string(kMaxClipNameBytes, 'a'), 0.0f, 1.0f, true, true};
  longest.layer = longest.own;
  PoseUpdate update = with_animation(longest);
  update.key = std::string(kMaxPoseKeyBytes, 'k');
  update.position = kHere;
  update.rotation = 0;
  CHECK(encoded(update).size() <= kMaxPoseBytes);
}
