#include <doctest/doctest.h>

#include <cmath>

#include "scene/animation/animation.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

namespace {

constexpr float kHalfTurn = 3.14159265f;

AnimationChannel slide() {
  AnimationChannel channel;
  channel.node = 0;
  channel.path = AnimationPath::Translation;
  channel.times = {0.0f, 1.0f, 2.0f};
  channel.values = {0, 0, 0, 10, 0, 0, 10, 4, 0};
  return channel;
}

}  // namespace

TEST_CASE("keys are interpolated, and held before the first and after the last") {
  const AnimationChannel channel = slide();
  CHECK(sample_channel(channel, 0.5f)[0] == doctest::Approx(5.0f));
  CHECK(sample_channel(channel, 1.5f)[1] == doctest::Approx(2.0f));
  CHECK(sample_channel(channel, -1.0f)[0] == doctest::Approx(0.0f));
  CHECK(sample_channel(channel, 9.0f)[1] == doctest::Approx(4.0f));
  CHECK(sample_channel(AnimationChannel{}, 1.0f)[0] == doctest::Approx(0.0f));  // no keys
}

TEST_CASE("rotations blend along the shortest arc and stay normalized") {
  AnimationChannel channel;
  channel.path = AnimationPath::Rotation;
  channel.times = {0.0f, 1.0f};
  const Quat quarter = quat_from_axis_angle(Vec3::up(), kHalfTurn / 2.0f);
  // The second key is written as -q: the same rotation, the long way round if blended naively.
  channel.values = {0, 0, 0, 1, -quarter.x, -quarter.y, -quarter.z, -quarter.w};
  const auto half = sample_channel(channel, 0.5f);
  const float length = std::sqrt(half[0] * half[0] + half[1] * half[1] + half[2] * half[2] + half[3] * half[3]);
  CHECK(length == doctest::Approx(1.0f));
  const Quat eighth = quat_from_axis_angle(Vec3::up(), kHalfTurn / 4.0f);
  CHECK(std::fabs(half[1]) == doctest::Approx(std::fabs(eighth.y)).epsilon(0.01));
  CHECK(std::fabs(half[3]) == doctest::Approx(std::fabs(eighth.w)).epsilon(0.01));
}

TEST_CASE("a clip poses its nodes and loops") {
  SceneDesc desc;
  NodeDesc mover;
  mover.name = "Mover";
  desc.nodes = {mover, mover};
  Scene scene = Scene::from_desc(desc);
  AnimationDesc clip;
  clip.duration = 2.0f;
  clip.channels = {slide()};
  AnimationChannel gone = slide();
  gone.node = 7;  // not in the scene: ignored
  clip.channels.push_back(gone);

  apply_animation(clip, 0.5f, scene);
  CHECK(scene.at(0).local.position.x == doctest::Approx(5.0f));
  apply_animation(clip, 2.5f, scene);  // one loop later
  CHECK(scene.at(0).local.position.x == doctest::Approx(5.0f));
  CHECK(approx_equal(scene.at(1).local.position, Vec3::zero(), kTolerance));  // untouched
}

TEST_CASE("copies of animated nodes move like the original, each at its own offset") {
  constexpr float kOffset = 0.5f;
  SceneDesc desc;
  NodeDesc root;
  root.name = "Character";
  root.children = {1};
  NodeDesc bone;
  bone.name = "Bone";
  bone.parent = 0;
  desc.nodes = {root, bone};
  Scene scene = Scene::from_desc(desc);
  AnimationDesc clip;
  clip.duration = 2.0f;
  AnimationChannel bone_slide = slide();
  bone_slide.node = 1;
  AnimationChannel root_slide = slide();  // also animates the character's root
  clip.channels = {bone_slide, root_slide};

  const std::vector<ObjectId> copy = scene.clone(0);
  REQUIRE(copy.size() == 2);
  scene.at(copy[0]).local.position = {100.0f, 0.0f, 0.0f};  // where the copy was put
  const std::vector<ObjectId> copy_of_copy = scene.clone(copy[0]);
  scene.at(copy_of_copy[0]).animation_offset = kOffset;
  CHECK(scene.copies_of(1) == std::vector<ObjectId>{copy[1], copy_of_copy[1]});

  apply_animation(clip, 0.5f, scene);
  CHECK(scene.at(1).local.position.x == doctest::Approx(5.0f));
  CHECK(scene.at(copy[1]).local.position.x == doctest::Approx(5.0f));
  CHECK(scene.at(copy_of_copy[1]).local.position.x == doctest::Approx(10.0f));  // half a second ahead
  CHECK(scene.at(0).local.position.x == doctest::Approx(5.0f));
  CHECK(scene.at(copy[0]).local.position.x == doctest::Approx(100.0f));  // a copy's root stays put

  scene.finalize_destroy(copy[1]);
  CHECK(scene.copies_of(1) == std::vector<ObjectId>{copy_of_copy[1]});
}
