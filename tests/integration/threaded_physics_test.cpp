// Physics solved on more than one thread, and stepped on a thread of its own: the same world, step
// after step, as the one thread always gave; and a game that draws one frame while its physics
// steps the next. (The sanitizer builds of the continuous integration run these for data races.)

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/config.hpp"
#include "physics/overlapped_physics_world.hpp"
#include "physics/physics_config.hpp"
#include "physics/physics_world.hpp"
#include "physics_game.hpp"

using namespace redgem;

namespace {

constexpr float kDt = config::kFixedDeltaSeconds;
constexpr int kSteps = 240;
constexpr int kPileSide = 6;  // a pile of kPileSide³ boxes: islands enough for a pool to share
constexpr float kBoxHalf = 0.5f;
constexpr float kBoxSpacing = 1.1f;  // apart, a little: they fall onto each other
constexpr float kPileBase = 2.0f;
constexpr int kManyThreads = 4;
constexpr int kTooManyThreads = config::kMaxPhysicsThreads + 1;
constexpr ObjectId kGround = 0;
constexpr ObjectId kFirstBox = 1;
constexpr float kDropHeight = 5.0f;
constexpr int kFrames = 90;

BodyDesc box(ObjectId id, const PropertyMap& props, Vec3 position, Vec3 half) {
  BodyDesc desc;
  desc.object = id;
  desc.config = body_config_from_props(props);
  desc.world.position = position;
  desc.bounds.include(-half);
  desc.bounds.include(half);
  return desc;
}

Vec3 pile_position(int i) {
  const int x = i % kPileSide;
  const int y = (i / kPileSide) % kPileSide;
  const int z = i / (kPileSide * kPileSide);
  return {kBoxSpacing * static_cast<float>(x), kPileBase + kBoxSpacing * static_cast<float>(y),
          kBoxSpacing * static_cast<float>(z)};
}

constexpr int kPileBoxes = kPileSide * kPileSide * kPileSide;

// Ground and a pile of boxes, in the same order every time (the world is deterministic only so).
void fill(PhysicsWorld& world) {
  const Vec3 half{kBoxHalf, kBoxHalf, kBoxHalf};
  REQUIRE_FALSE(world.add_object(box(kGround, {{"collider", std::string("box")}}, {0, -0.5f, 0}, {40, 0.5f, 40})));
  for (int i = 0; i < kPileBoxes; ++i) {
    const auto id = static_cast<ObjectId>(kFirstBox + i);
    REQUIRE_FALSE(world.add_object(box(id, {{"body", std::string("dynamic")}}, pile_position(i), half)));
  }
}

// Every box's pose after the steps, and every contact told on the way, in order.
struct Run {
  std::vector<Transform> poses;
  std::vector<ContactEvent> events;
};

Run run(PhysicsWorld& world) {
  fill(world);
  Run result;
  for (int step = 0; step < kSteps; ++step) {
    world.step(kDt);
    for (const ContactEvent& event : world.drain_events()) result.events.push_back(event);
  }
  for (int i = 0; i < kPileBoxes; ++i) result.poses.push_back(*world.simulated_pose(kFirstBox + i));
  return result;
}

Run run_on(int threads) {
  const auto world = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f}, {}, threads);
  return run(*world);
}

bool same_pose(const Transform& a, const Transform& b) {
  return a.position.x == b.position.x && a.position.y == b.position.y && a.position.z == b.position.z &&
         a.rotation.x == b.rotation.x && a.rotation.y == b.rotation.y && a.rotation.z == b.rotation.z &&
         a.rotation.w == b.rotation.w;
}

bool same_event(const ContactEvent& a, const ContactEvent& b) {
  return a.self == b.self && a.other == b.other && a.phase == b.phase && a.trigger == b.trigger;
}

// Bit for bit: the same poses and the same contacts in the same order.
void check_same(const Run& a, const Run& b) {
  REQUIRE(a.poses.size() == b.poses.size());
  for (std::size_t i = 0; i < a.poses.size(); ++i) CHECK(same_pose(a.poses[i], b.poses[i]));
  REQUIRE(a.events.size() == b.events.size());
  for (std::size_t i = 0; i < a.events.size(); ++i) CHECK(same_event(a.events[i], b.events[i]));
}

}  // namespace

TEST_CASE("a world solved on one thread steps exactly as a world made without saying how many") {
  const auto unsaid = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  const Run before = run(*unsaid);
  check_same(before, run_on(config::kDefaultPhysicsThreads));
  check_same(before, run_on(config::kDefaultPhysicsThreads));  // and again: nothing changes from run to run
}

TEST_CASE("a world solved on many threads gives the world one thread gives, step after step") {
  const Run one = run_on(config::kDefaultPhysicsThreads);
  const Run many = run_on(kManyThreads);
  check_same(one, many);
  check_same(many, run_on(kManyThreads));
}

TEST_CASE(
    "a world asked for more threads than there may be gets as many as there may be, and fewer than one gets one") {
  CHECK(config::bounded_physics_threads(kTooManyThreads) == config::kMaxPhysicsThreads);
  CHECK(config::bounded_physics_threads(0) == config::kDefaultPhysicsThreads);
  CHECK_FALSE(config::solves_in_parallel(config::kDefaultPhysicsThreads));
  CHECK(config::solves_in_parallel(kManyThreads));
  check_same(run_on(0), run_on(config::kDefaultPhysicsThreads));
}

TEST_CASE("a step begun on its own thread ends as the same step taken in place") {
  const auto in_place = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f}, {}, kManyThreads);
  OverlappedPhysicsWorld overlapped(create_jolt_physics_world({0.0f, config::kGravityY, 0.0f}, {}, kManyThreads));
  fill(*in_place);
  fill(overlapped);
  for (int step = 0; step < kSteps; ++step) {
    in_place->step(kDt);
    overlapped.begin_step(kDt);
    overlapped.finish_step();
    CHECK_FALSE(overlapped.stepping());
  }
  for (int i = 0; i < kPileBoxes; ++i) {
    CHECK(same_pose(*in_place->simulated_pose(kFirstBox + i), *overlapped.simulated_pose(kFirstBox + i)));
  }
}

TEST_CASE("what is asked of a world while it steps on its own thread is answered once the step is over") {
  OverlappedPhysicsWorld overlapped(create_jolt_physics_world({0.0f, config::kGravityY, 0.0f}));
  const auto in_place = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  const Vec3 half{kBoxHalf, kBoxHalf, kBoxHalf};
  const PropertyMap dynamic{{"body", std::string("dynamic")}};
  REQUIRE_FALSE(overlapped.add_object(box(kFirstBox, dynamic, {0, kDropHeight, 0}, half)));
  REQUIRE_FALSE(in_place->add_object(box(kFirstBox, dynamic, {0, kDropHeight, 0}, half)));
  in_place->step(kDt);
  overlapped.begin_step(kDt);
  // (No finish_step: the question waits for the step, and sees the world after it, never during.)
  CHECK(same_pose(*overlapped.simulated_pose(kFirstBox), *in_place->simulated_pose(kFirstBox)));
  CHECK_FALSE(overlapped.stepping());
}

TEST_CASE("a world that goes while it steps on its own thread first lets the step end") {
  auto overlapped =
      std::make_unique<OverlappedPhysicsWorld>(create_jolt_physics_world({0.0f, config::kGravityY, 0.0f}, {}, 2));
  fill(*overlapped);
  overlapped->begin_step(kDt);
  overlapped.reset();  // (the sanitizers see a step still at work on a world taken apart)
  CHECK_FALSE(overlapped);
}

// --- A game whose physics steps while its frame is drawn -------------------------------------

namespace {

namespace game = redgem::test::physics_game;

std::unique_ptr<Engine> engine_on(const game::TempGame& folder, const SceneDesc& scene, int threads) {
  EngineOptions options;
  options.game_dir = folder.dir;
  options.fixed_step = true;
  options.scene = &scene;
  options.save_file = folder.dir / "save.json";
  options.physics_threads = threads;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

Transform crate_of(Engine& engine) { return engine.scene().world_transform(engine.scene().find("Crate")); }

SceneDesc falling_crate() {
  return game::scene_with({game::node("Crate", {0, kDropHeight, 0}, {1, 1, 1}, game::moving("dynamic"))});
}

}  // namespace

TEST_CASE("a game drawn while its physics steps shows each frame the step before, then catches up at the next") {
  const game::TempGame folder(R"({"title": "Overlap"})");
  const SceneDesc scene = falling_crate();
  auto in_place = engine_on(folder, scene, config::kDefaultPhysicsThreads);
  auto overlapped = engine_on(folder, scene, kManyThreads);
  for (int i = 0; i < kFrames; ++i) {
    // The frame drawn: the crate where the in-place game had it a frame (a step) before.
    overlapped->frame(kDt);
    CHECK(same_pose(crate_of(*overlapped), crate_of(*in_place)));
    in_place->frame(kDt);
  }
  // At the next frame's start the step under way is finished: the two games are at one again.
  overlapped->frame(kDt);
  CHECK(same_pose(crate_of(*overlapped), crate_of(*in_place)));
}

TEST_CASE("a game drawn while its physics steps tells its scripts of contacts and time at the next frame's start") {
  const game::TempGame folder(R"({"title": "Overlap"})");
  const SceneDesc scene = falling_crate();
  auto overlapped = engine_on(folder, scene, kManyThreads);
  game::ruby(*overlapped, R"(
    $touched = false
    class Toucher < Behaviour
      def on_collision_enter(_other)
        $touched = true
      end
    end
    Scene.find('Crate').add_component(Toucher)
  )");
  game::frames(*overlapped, game::kTwoSeconds);
  CHECK(game::ruby(*overlapped, "$touched") == "true");
  // Every frame's step but the last has been finished; that one is, at the start of the next.
  const float elapsed = game::number(*overlapped, "Time.elapsed");
  const float finished = kDt * static_cast<float>(game::kTwoSeconds - 1);
  CHECK(elapsed > finished - kDt / 2);
  CHECK(elapsed < finished + kDt / 2);
}

TEST_CASE("a game drawn while its physics steps can destroy and spawn objects with a step under way") {
  const game::TempGame folder(R"({"title": "Overlap"})");
  const SceneDesc scene = falling_crate();
  auto overlapped = engine_on(folder, scene, kManyThreads);
  game::frames(*overlapped, game::kOneSecond);
  // (Between frames the last one's step is still under way: what is asked of the world waits for it.)
  game::ruby(*overlapped, "$copy = Scene.spawn('Crate'); Scene.find('Crate').destroy");
  game::frames(*overlapped, game::kOneSecond);
  CHECK(game::ruby(*overlapped, "Scene.find('Crate').nil?") == "false");  // (the copy, by its name)
  CHECK(game::number(*overlapped, "$copy.transform.position.y") < kDropHeight);
}
