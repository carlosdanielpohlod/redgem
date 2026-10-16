#include <doctest/doctest.h>

#include "scene/animation_player.hpp"

using namespace redgem;

namespace {

constexpr float kDuration = 2.0f;
constexpr float kDistance = 10.0f;  // the clip slides its object this far along x
constexpr float kQuarter = 0.5f;    // seconds: a quarter of the clip

// One object and a clip, "Slide", that moves it from x = 0 to x = 10 in two seconds.
struct Stage {
  SceneDesc desc;
  Scene scene;
  AnimationPlayer player;

  Stage() {
    NodeDesc mover;
    mover.name = "Mover";
    desc.nodes = {mover};
    AnimationChannel channel;
    channel.node = 0;
    channel.times = {0.0f, kDuration};
    channel.values = {0, 0, 0, kDistance, 0, 0};
    AnimationDesc clip;
    clip.name = "Slide";
    clip.duration = kDuration;
    clip.channels = {channel};
    desc.animations = {clip};
    scene = Scene::from_desc(desc);
    player.reset(desc.animations, scene);
  }

  float x() { return scene.at(0).local.position.x; }
  void advance(float seconds) { player.advance(seconds, scene); }
};

}  // namespace

TEST_CASE("clips play in a loop from the start, without being asked") {
  Stage stage;
  CHECK(stage.player.names() == std::vector<std::string>{"Slide"});
  CHECK(stage.player.playing("Slide") == true);
  stage.advance(kQuarter);
  CHECK(stage.x() == doctest::Approx(kDistance / 4.0f));
  stage.advance(kDuration);  // once around
  CHECK(stage.x() == doctest::Approx(kDistance / 4.0f));
  CHECK(stage.player.time("Slide") == doctest::Approx(kQuarter));
  CHECK(stage.player.duration("Slide") == doctest::Approx(kDuration));
}

TEST_CASE("a paused clip holds its pose, and carries on from there") {
  Stage stage;
  stage.advance(kQuarter);
  CHECK(stage.player.pause("Slide"));
  stage.advance(kQuarter);
  CHECK(stage.x() == doctest::Approx(kDistance / 4.0f));
  CHECK(stage.player.playing("Slide") == false);
  stage.player.play("Slide", true, 1.0f, false);
  stage.advance(kQuarter);
  CHECK(stage.x() == doctest::Approx(kDistance / 2.0f));
}

TEST_CASE("a stopped clip lets go of its objects, and starts over when played again") {
  Stage stage;
  stage.advance(kQuarter);
  CHECK(stage.player.stop("Slide"));
  stage.scene.at(0).local.position.x = -3.0f;  // a script moves it
  stage.advance(kQuarter);
  CHECK(stage.x() == doctest::Approx(-3.0f));
  stage.player.play("Slide", true, 1.0f, false);
  stage.advance(kQuarter);
  CHECK(stage.x() == doctest::Approx(kDistance / 4.0f));
}

TEST_CASE("a clip played once stops at its end and holds the last pose") {
  Stage stage;
  stage.player.play("Slide", false, 1.0f, true);
  stage.advance(kDuration + kQuarter);
  CHECK(stage.x() == doctest::Approx(kDistance));
  CHECK(stage.player.playing("Slide") == false);
  stage.advance(kQuarter);
  CHECK(stage.x() == doctest::Approx(kDistance));
  // Played backwards from there, it returns to the start (a door closing).
  stage.player.play("Slide", false, -1.0f, false);
  stage.advance(kQuarter);
  CHECK(stage.x() == doctest::Approx(kDistance * 0.75f));
  stage.advance(kDuration);
  CHECK(stage.x() == doctest::Approx(0.0f));
  CHECK(stage.player.playing("Slide") == false);
  // And played forwards once more it starts over by itself.
  stage.player.play("Slide", false, 1.0f, false);
  stage.advance(kQuarter);
  CHECK(stage.x() == doctest::Approx(kDistance / 4.0f));
}

TEST_CASE("speed and seek") {
  Stage stage;
  stage.player.play("Slide", true, 2.0f, true);
  stage.advance(kQuarter);
  CHECK(stage.x() == doctest::Approx(kDistance / 2.0f));
  stage.player.pause("Slide");
  CHECK(stage.player.seek("Slide", kQuarter));
  stage.advance(kQuarter);
  CHECK(stage.x() == doctest::Approx(kDistance / 4.0f));
  stage.player.seek("Slide", kDuration * 5.0f);  // kept within the clip
  CHECK(stage.player.time("Slide") == doctest::Approx(kDuration));
}

TEST_CASE("unknown clips are refused") {
  Stage stage;
  CHECK_FALSE(stage.player.has("Dance"));
  CHECK_FALSE(stage.player.play("Dance", true, 1.0f, false));
  CHECK_FALSE(stage.player.pause("Dance"));
  CHECK_FALSE(stage.player.stop("Dance"));
  CHECK_FALSE(stage.player.seek("Dance", 0.0f));
  CHECK_FALSE(stage.player.playing("Dance").has_value());
  CHECK_FALSE(stage.player.time("Dance").has_value());
  CHECK_FALSE(stage.player.duration("Dance").has_value());
  AnimationPlayer empty;  // never given a scene
  CHECK(empty.names().empty());
  empty.advance(kQuarter, stage.scene);
}

namespace {

constexpr float kRunX = 100.0f;      // where the "Run" clip holds the arm
constexpr float kWaveHeight = 4.0f;  // how tall the "Wave" clip makes it
constexpr float kFade = 1.0f;        // seconds
constexpr float kHalfWeight = 0.5f;
constexpr int kManyFrames = 30;

// A person with an arm, and two copies of them. "Walk" slides the arm from x = 0 to 10 in two
// seconds, "Run" holds it at x = 100, "Wave" only stretches it. The scene plays Walk.
struct Crowd {
  SceneDesc desc;
  Scene scene;
  AnimationPlayer player;
  ObjectId first = kNoObject;  // roots of the copies
  ObjectId second = kNoObject;
  ObjectId first_arm = kNoObject;
  ObjectId second_arm = kNoObject;
  static constexpr ObjectId kPerson = 0;
  static constexpr ObjectId kArm = 1;

  Crowd() {
    NodeDesc person;
    person.name = "Person";
    person.children = {kArm};
    NodeDesc arm;
    arm.name = "Arm";
    arm.parent = kPerson;
    desc.nodes = {person, arm};
    desc.animations = {clip("Walk", AnimationPath::Translation, {0, 0, 0, kDistance, 0, 0}),
                       clip("Run", AnimationPath::Translation, {kRunX, 0, 0, kRunX, 0, 0}),
                       clip("Wave", AnimationPath::Scale, {1, kWaveHeight, 1, 1, kWaveHeight, 1})};
    scene = Scene::from_desc(desc);
    const std::vector<ObjectId> a = scene.clone(kPerson);
    const std::vector<ObjectId> b = scene.clone(kPerson);
    first = a.front();
    first_arm = a.back();
    second = b.front();
    second_arm = b.back();
    player.reset(desc.animations, scene);
    player.stop("Run");
    player.stop("Wave");
  }

  static AnimationDesc clip(const std::string& name, AnimationPath path, std::vector<float> values) {
    AnimationChannel channel;
    channel.node = kArm;
    channel.path = path;
    channel.times = {0.0f, kDuration};
    channel.values = std::move(values);
    AnimationDesc out;
    out.name = name;
    out.duration = kDuration;
    out.channels = {channel};
    return out;
  }

  float x(ObjectId arm) { return scene.at(arm).local.position.x; }
  void advance(float seconds) { player.advance(seconds, scene); }
};

}  // namespace

TEST_CASE("one copy of a character plays a clip of its own while the others follow the scene") {
  Crowd crowd;
  CHECK(crowd.player.play_on(crowd.scene, crowd.first, "Run", ClipStart{true, 1.0f, 0.0f, false}));
  crowd.advance(kQuarter);
  CHECK(crowd.x(crowd.first_arm) == doctest::Approx(kRunX));
  CHECK(crowd.x(crowd.second_arm) == doctest::Approx(kDistance / 4.0f));
  CHECK(crowd.x(Crowd::kArm) == doctest::Approx(kDistance / 4.0f));
  CHECK(crowd.player.clip_on(crowd.scene, crowd.first) == std::string("Run"));
  CHECK(crowd.player.clip_on(crowd.scene, crowd.first_arm) == std::string("Run"));  // any part of the character
  CHECK(crowd.player.playing_on(crowd.scene, crowd.first));
  CHECK(crowd.player.time_on(crowd.scene, crowd.first) == doctest::Approx(kQuarter));
  CHECK_FALSE(crowd.player.clip_on(crowd.scene, crowd.second).has_value());
  CHECK_FALSE(crowd.player.play_on(crowd.scene, crowd.second, "Dance", ClipStart{true, 1.0f, 0.0f, false}));
}

TEST_CASE("a character fades from what it was doing to its new clip") {
  Crowd crowd;
  crowd.advance(kQuarter);
  crowd.player.play_on(crowd.scene, crowd.first, "Run", ClipStart{true, 1.0f, kFade, false});
  crowd.advance(kFade * kHalfWeight);
  // Halfway between where the walk has it by now and where the run holds it.
  const float walking = crowd.x(crowd.second_arm);
  CHECK(crowd.x(crowd.first_arm) == doctest::Approx((walking + kRunX) * kHalfWeight));
  crowd.advance(kFade);
  CHECK(crowd.x(crowd.first_arm) == doctest::Approx(kRunX));

  // And back to the scene's clips the same way.
  crowd.player.follow_scene(crowd.scene, crowd.first, kFade);
  CHECK_FALSE(crowd.player.clip_on(crowd.scene, crowd.first).has_value());
  crowd.advance(kFade * kHalfWeight);
  CHECK(crowd.x(crowd.first_arm) == doctest::Approx((crowd.x(crowd.second_arm) + kRunX) * kHalfWeight));
  crowd.advance(kFade);
  CHECK(crowd.x(crowd.first_arm) == doctest::Approx(crowd.x(crowd.second_arm)));
}

TEST_CASE("a second clip is mixed over what a character does, by its weight") {
  Crowd crowd;
  CHECK(crowd.player.mix_on(crowd.scene, crowd.first, "Run", kHalfWeight, true, 1.0f));
  crowd.advance(kQuarter);
  CHECK(crowd.x(crowd.first_arm) == doctest::Approx((crowd.x(crowd.second_arm) + kRunX) * kHalfWeight));
  CHECK_FALSE(crowd.player.clip_on(crowd.scene, crowd.first).has_value());  // still following the scene underneath

  // A clip that is about something else (the arm's size) leaves the walk alone.
  crowd.player.mix_on(crowd.scene, crowd.first, "Wave", 1.0f, true, 1.0f);
  crowd.advance(kQuarter);
  CHECK(crowd.x(crowd.first_arm) == doctest::Approx(crowd.x(crowd.second_arm)));
  CHECK(crowd.scene.at(crowd.first_arm).local.scale.y == doctest::Approx(kWaveHeight));
  CHECK(crowd.scene.at(crowd.second_arm).local.scale.y == doctest::Approx(1.0f));

  // Half of it stays half, frame after frame (it does not creep up to the whole).
  crowd.player.mix_on(crowd.scene, crowd.first, "Wave", kHalfWeight, true, 1.0f);
  for (int frame = 0; frame < kManyFrames; ++frame) crowd.advance(kQuarter / kManyFrames);
  CHECK(crowd.scene.at(crowd.first_arm).local.scale.y == doctest::Approx((1.0f + kWaveHeight) * kHalfWeight));

  crowd.player.unmix_on(crowd.scene, crowd.first);
  crowd.scene.at(crowd.first_arm).local.scale.y = 1.0f;
  crowd.advance(kQuarter);
  CHECK(crowd.scene.at(crowd.first_arm).local.scale.y == doctest::Approx(1.0f));
  CHECK_FALSE(crowd.player.mix_on(crowd.scene, crowd.first, "Dance", 1.0f, true, 1.0f));
}

TEST_CASE("an object of the level is a character too: itself and what is under it") {
  Crowd crowd;
  crowd.player.play_on(crowd.scene, Crowd::kPerson, "Run", ClipStart{true, 1.0f, 0.0f, false});
  crowd.advance(kQuarter);
  CHECK(crowd.x(Crowd::kArm) == doctest::Approx(kRunX));
  CHECK(crowd.x(crowd.first_arm) == doctest::Approx(kDistance / 4.0f));  // its copies still walk
}

TEST_CASE("a character's own clip can play once, pause and start over") {
  Crowd crowd;
  crowd.player.play_on(crowd.scene, crowd.first, "Walk", ClipStart{false, 1.0f, 0.0f, false});
  crowd.advance(kDuration + kQuarter);
  CHECK(crowd.x(crowd.first_arm) == doctest::Approx(kDistance));  // held at its end
  CHECK_FALSE(crowd.player.playing_on(crowd.scene, crowd.first));
  crowd.player.play_on(crowd.scene, crowd.first, "Walk", ClipStart{false, 1.0f, 0.0f, false});  // ended: starts over
  crowd.advance(kQuarter);
  CHECK(crowd.x(crowd.first_arm) == doctest::Approx(kDistance / 4.0f));
  crowd.player.pause_on(crowd.scene, crowd.first);
  crowd.advance(kQuarter);
  CHECK(crowd.x(crowd.first_arm) == doctest::Approx(kDistance / 4.0f));
}

TEST_CASE("a character spawned into a destroyed one's slot does not inherit its clip") {
  Crowd crowd;
  crowd.player.play_on(crowd.scene, crowd.first, "Run", ClipStart{true, 1.0f, 0.0f, false});
  crowd.scene.request_destroy(crowd.first);
  const std::vector<ObjectId> pending = crowd.scene.pending_destroy_ids();
  for (auto gone = pending.rbegin(); gone != pending.rend(); ++gone) {  // as the engine does
    crowd.player.forget(*gone);
    crowd.scene.finalize_destroy(*gone);
  }
  const ObjectId newcomer = crowd.scene.clone(Crowd::kPerson).front();
  REQUIRE(newcomer == crowd.first);
  CHECK_FALSE(crowd.player.clip_on(crowd.scene, newcomer).has_value());  // it follows the scene's clips
}

TEST_CASE("a destroyed character is forgotten") {
  Crowd crowd;
  crowd.player.play_on(crowd.scene, crowd.first, "Run", ClipStart{true, 1.0f, 0.0f, false});
  crowd.scene.request_destroy(crowd.first);
  for (const ObjectId gone : crowd.scene.pending_destroy_ids()) crowd.scene.finalize_destroy(gone);
  crowd.advance(kQuarter);
  CHECK(crowd.x(crowd.second_arm) == doctest::Approx(kDistance / 4.0f));
}
