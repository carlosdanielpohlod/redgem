#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include "scene/animation/head_aim.hpp"
#include "scene/scene_desc.hpp"

using namespace redgem;

namespace {

constexpr float kTolerance = 1e-3f;
constexpr float kRightAngle = 90.0f;  // degrees
constexpr float kSmallTurn = 30.0f;   // degrees: the most a test's head turns
constexpr float kHeadHeight = 1.6f;   // meters above the character's origin
constexpr float kFar = 10.0f;         // meters to what the head looks at
constexpr float kFrame = 1.0f / 60.0f;
constexpr int kEaseFrames = 30;  // more frames than HeadAims::kEaseSeconds takes
constexpr ObjectId kBody = 0;
constexpr ObjectId kHead = 1;

// Degrees between a direction and straight ahead (-Z).
float degrees_off_forward(Vec3 direction) {
  return radians_to_degrees(std::acos(std::clamp(dot(normalize(direction), Vec3::forward()), -1.0f, 1.0f)));
}

// A character facing -Z, with a head bone above its origin.
struct Character {
  Scene scene;
  HeadAims aims;

  Character() {
    NodeDesc body;
    body.name = "Body";
    body.children = {static_cast<int>(kHead)};
    NodeDesc head;
    head.name = "Head";
    head.parent = static_cast<int>(kBody);
    head.local.position = {0.0f, kHeadHeight, 0.0f};
    SceneDesc desc;
    desc.nodes = {body, head};
    scene = Scene::from_desc(desc);
  }

  Vec3 head_forward() const { return rotate(scene.world_transform(kHead).rotation, Vec3::forward()); }
  void frames(int count) {
    for (int i = 0; i < count; ++i) aims.apply(scene, kFrame);
  }
  void look_right(float most_degrees) { aims.aim(scene, kBody, kHead, Vec3{kFar, kHeadHeight, 0.0f}, most_degrees); }
};

}  // namespace

TEST_CASE("an aimed rotation turns a forward toward a direction, no further than its limit") {
  const float all_the_way = degrees_to_radians(kRightAngle);
  const Quat full = aimed_rotation(Quat::identity(), Vec3::forward(), Vec3::right(), all_the_way, 1.0f);
  CHECK(approx_equal(rotate(full, Vec3::forward()), Vec3::right(), kTolerance));
  const float limit = degrees_to_radians(kSmallTurn);
  const Quat limited = aimed_rotation(Quat::identity(), Vec3::forward(), Vec3::right(), limit, 1.0f);
  CHECK(degrees_off_forward(rotate(limited, Vec3::forward())) == doctest::Approx(kSmallTurn).epsilon(kTolerance));
  const Quat none = aimed_rotation(Quat::identity(), Vec3::forward(), Vec3::right(), all_the_way, 0.0f);
  CHECK(approx_equal(none, Quat::identity(), kTolerance));
}

TEST_CASE("a direction straight ahead or straight behind leaves the rotation as it is") {
  const float limit = degrees_to_radians(kRightAngle);
  const Quat ahead = aimed_rotation(Quat::identity(), Vec3::forward(), Vec3::forward(), limit, 1.0f);
  CHECK(approx_equal(ahead, Quat::identity(), kTolerance));
  const Quat behind = aimed_rotation(Quat::identity(), Vec3::forward(), Vec3::back(), limit, 1.0f);
  CHECK(approx_equal(behind, Quat::identity(), kTolerance));
}

TEST_CASE("an aimed head eases toward the point, then holds there without turning further") {
  Character character;
  character.look_right(kRightAngle);
  character.frames(1);
  CHECK(character.head_forward().x > 0.0f);
  CHECK(character.head_forward().x < 1.0f - kTolerance);  // easing in, not there yet
  character.frames(kEaseFrames);
  CHECK(approx_equal(character.head_forward(), Vec3::right(), kTolerance));
  character.frames(kEaseFrames);  // the same turn each frame, not added to itself
  CHECK(approx_equal(character.head_forward(), Vec3::right(), kTolerance));
}

TEST_CASE("a head turns no further than its limit") {
  Character character;
  character.look_right(kSmallTurn);
  character.frames(kEaseFrames);
  CHECK(degrees_off_forward(character.head_forward()) == doctest::Approx(kSmallTurn).epsilon(kTolerance));
}

TEST_CASE("a released head eases back to its pose and is let go") {
  Character character;
  character.look_right(kRightAngle);
  character.frames(kEaseFrames);
  character.aims.release(character.scene, kHead);
  CHECK(character.aims.aiming(character.scene, kHead));  // on its way back
  character.frames(kEaseFrames);
  CHECK(approx_equal(character.head_forward(), Vec3::forward(), kTolerance));
  CHECK_FALSE(character.aims.aiming(character.scene, kHead));
}

TEST_CASE("a pose a clip or a script sets is what the head turns from") {
  Character character;
  character.look_right(kSmallTurn);
  character.frames(kEaseFrames);
  // A clip turns the head to the right by itself; the aim's turn comes on top of that.
  character.scene.at(kHead).local.rotation = quat_from_axis_angle(Vec3::up(), degrees_to_radians(-kSmallTurn));
  character.frames(1);
  CHECK(degrees_off_forward(character.head_forward()) == doctest::Approx(2.0f * kSmallTurn).epsilon(kTolerance));
}
