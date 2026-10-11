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
    player.reset(desc.animations);
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
