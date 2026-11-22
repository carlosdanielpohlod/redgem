// Lag compensation's history (LagHistory): where a kept object and the bodies under it were each
// frame, put back where they were when shown at a time asked, in the scene and in the physics, and
// put back where they are after.

#include <doctest/doctest.h>

#include <memory>
#include <optional>

#include "app/lag_history.hpp"
#include "core/config.hpp"
#include "physics/body_config.hpp"
#include "physics/physics_world.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

using namespace redgem;

namespace {

constexpr double kFrame = 0.1;      // seconds between two recorded frames
constexpr float kStride = 1.0f;     // meters the target goes along x each frame
constexpr int kFrames = 20;         // recorded: the target ends at x = 19, at time 1.9
constexpr double kLongest = 0.5;    // seconds a rewind may go back
constexpr float kBoxHeight = 1.0f;  // meters the box hangs above the target
constexpr float kNear = 0.01f;      // meters: the same place
constexpr float kRayLength = 20.0f;
constexpr Vec3 kDown{0, -1, 0};
constexpr float kAbove = 10.0f;       // meters above the box a ray starts
constexpr double kShownBehind = 0.3;  // seconds the target is shown behind the record

enum Index : int { kTarget = 0, kBox = 1 };

Scene target_scene() {
  SceneDesc desc;
  NodeDesc target;
  target.name = "Target";
  target.children = {kBox};
  NodeDesc box;
  box.name = "Box";
  box.parent = kTarget;
  box.local.position = {0, kBoxHeight, 0};
  desc.nodes = {target, box};
  return Scene::from_desc(desc);
}

struct Range {
  Scene scene = target_scene();
  std::unique_ptr<PhysicsWorld> physics = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f}, {});
  LagHistory history;

  Range() {
    BodyDesc body;
    body.object = kBox;
    body.config = body_config_from_props({{"body", std::string("kinematic")}});
    body.world = scene.world_transform(kBox);
    body.bounds.include({-0.5f, -0.5f, -0.5f});
    body.bounds.include({0.5f, 0.5f, 0.5f});
    REQUIRE_FALSE(physics->add_object(body));
    history.set_longest(kLongest);
    history.track(scene, kTarget);
  }

  // The target `frame` strides along, its box's body with it, recorded at `frame` frames' time.
  void play(int frame, std::optional<double> shown = std::nullopt) {
    scene.set_world_position(kTarget, {kStride * static_cast<float>(frame), 0, 0});
    physics->teleport(kBox, scene.world_transform(kBox));
    if (shown) history.shown_at(kTarget, *shown);
    history.record(scene, *physics, kFrame * frame);
  }

  void play_all() {
    for (int frame = 0; frame < kFrames; ++frame) play(frame);
  }

  double now() const { return kFrame * (kFrames - 1); }

  // What a ray straight down onto x meets: the box, or nothing.
  bool box_at(float x) const {
    const auto hit = physics->raycast({x, kBoxHeight + kAbove, 0}, kDown, kRayLength);
    return hit && hit->object == kBox;
  }
};

}  // namespace

TEST_CASE("a rewind puts a kept object and the bodies under it where they were, and a restore puts them back") {
  Range range;
  range.play_all();
  REQUIRE(range.box_at(19.0f));
  // (1.75 is between the frames of 1.7 and 1.8: the target was halfway between 17 and 18.)
  REQUIRE(range.history.rewind(range.scene, *range.physics, 1.75));
  CHECK(range.history.rewound());
  CHECK(range.scene.world_transform(kTarget).position.x == doctest::Approx(17.5f).epsilon(kNear));
  CHECK(range.scene.world_transform(kBox).position.y == doctest::Approx(kBoxHeight).epsilon(kNear));
  CHECK(range.box_at(17.5f));
  CHECK_FALSE(range.box_at(19.0f));
  CHECK_FALSE(range.history.rewind(range.scene, *range.physics, 1.5));  // (once at a time)

  range.history.restore(range.scene, *range.physics);
  CHECK_FALSE(range.history.rewound());
  CHECK(range.scene.world_transform(kTarget).position.x == doctest::Approx(19.0f).epsilon(kNear));
  CHECK(range.scene.world_transform(kBox).position.x == doctest::Approx(19.0f).epsilon(kNear));
  CHECK(range.box_at(19.0f));
  CHECK_FALSE(range.box_at(17.5f));
}

TEST_CASE("a rewind goes no further back than the longest one, and never after now") {
  Range range;
  range.play_all();
  CHECK(range.history.clamped(range.now() + 1.0, range.now()) == doctest::Approx(range.now()));
  CHECK(range.history.clamped(0.0, range.now()) == doctest::Approx(range.now() - kLongest));
  CHECK(range.history.clamped(1.7, range.now()) == doctest::Approx(1.7));
}

TEST_CASE("a rewind goes by when an object was shown, not by when it was recorded") {
  Range range;
  // Each frame shows the target as it was kShownBehind before (as a guest's object is shown on the host).
  for (int frame = 0; frame < kFrames; ++frame) range.play(frame, kFrame * frame - kShownBehind);
  // Shown at 1.4 is the frame recorded at 1.7: the target at 17.
  REQUIRE(range.history.rewind(range.scene, *range.physics, 1.4));
  CHECK(range.scene.world_transform(kTarget).position.x == doctest::Approx(17.0f).epsilon(kNear));
  CHECK(range.box_at(17.0f));
  range.history.restore(range.scene, *range.physics);
}

TEST_CASE("a rewind to before what is kept goes to the oldest frame, and a cleared history moves nothing") {
  Range range;
  range.play_all();
  REQUIRE(range.history.rewind(range.scene, *range.physics, -5.0));
  // (Frames older than the longest rewind and a margin are forgotten: the oldest left is at 9.)
  CHECK(range.scene.world_transform(kTarget).position.x == doctest::Approx(9.0f).epsilon(kNear));
  range.history.restore(range.scene, *range.physics);

  range.history.clear();
  REQUIRE(range.history.rewind(range.scene, *range.physics, 1.5));
  CHECK(range.scene.world_transform(kTarget).position.x == doctest::Approx(19.0f).epsilon(kNear));
  range.history.restore(range.scene, *range.physics);
}
