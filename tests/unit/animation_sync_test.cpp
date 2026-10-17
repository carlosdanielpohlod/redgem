// A character's own clips as another machine sees them (AnimationPlayer::animation_of and
// take_animation): two players over the same scene stand for two machines.

#include <doctest/doctest.h>

#include "scene/animation/animation_player.hpp"

using namespace redgem;

namespace {

constexpr float kDuration = 2.0f;
constexpr float kDistance = 10.0f;  // "Slide" moves its object this far along x; "Lift" as far up y
constexpr float kQuarter = 0.5f;    // seconds
constexpr float kLate = 0.25f;      // seconds a state is taken after it was told
constexpr float kFade = 0.4f;
constexpr float kHalfWeight = 0.5f;
constexpr float kPastWhole = 1.2f;  // a weight the mix takes as 1
constexpr ObjectId kHero = 0;
constexpr const char* kSlide = "Slide";
constexpr const char* kLift = "Lift";

AnimationDesc clip(const char* name, std::size_t axis) {
  AnimationChannel channel;
  channel.node = 0;
  channel.times = {0.0f, kDuration};
  channel.values = {0, 0, 0, 0, 0, 0};
  channel.values[3 + axis] = kDistance;
  return AnimationDesc{name, kDuration, {channel}};
}

// One machine's view of a scene of one character and two clips.
struct Machine {
  SceneDesc desc;
  Scene scene;
  AnimationPlayer player;

  Machine() {
    NodeDesc hero;
    hero.name = "Hero";
    desc.nodes = {hero};
    desc.animations = {clip(kSlide, kXOffset), clip(kLift, kYOffset)};
    scene = Scene::from_desc(desc);
    player.reset(desc.animations, scene);
  }

  void play(const char* name, float fade = 0.0f, bool restart = false) {
    player.play_on(scene, kHero, name, {true, 1.0f, fade, restart});
  }
  CharacterAnimation state() const { return player.animation_of(scene, kHero); }
  std::uint32_t revision() const { return state().revision; }
  Vec3 at() const { return scene.at(kHero).local.position; }
  void advance(float seconds) { player.advance(seconds, scene); }
};

}  // namespace

TEST_CASE("a character's revision moves on when a script changes what it does, not as it plays on") {
  Machine owner;
  const std::uint32_t at_first = owner.revision();
  owner.play(kSlide);
  const std::uint32_t sliding = owner.revision();
  CHECK(sliding != at_first);
  owner.play(kSlide);  // already on it: it carries on
  owner.advance(kQuarter);
  CHECK(owner.revision() == sliding);
  owner.play(kSlide, 0.0f, true);  // starting over is news
  CHECK(owner.revision() != sliding);
  for (const auto& change : {+[](Machine& m) { m.player.pause_on(m.scene, kHero); },
                             +[](Machine& m) { m.player.mix_on(m.scene, kHero, kLift, kHalfWeight, true, 1.0f); },
                             +[](Machine& m) { m.player.unmix_on(m.scene, kHero); },
                             +[](Machine& m) { m.player.follow_scene(m.scene, kHero, 0.0f); }}) {
    const std::uint32_t before = owner.revision();
    change(owner);
    CHECK(owner.revision() != before);
  }
}

TEST_CASE("mixing a clip again with a weight past whole is no change: the weight was taken as whole") {
  Machine owner;
  owner.player.mix_on(owner.scene, kHero, kLift, kPastWhole, true, 1.0f);
  const std::uint32_t mixed = owner.revision();
  owner.player.mix_on(owner.scene, kHero, kLift, kPastWhole, true, 1.0f);  // as a script would, every frame
  CHECK(owner.revision() == mixed);
  CHECK(owner.state().layer_weight == doctest::Approx(1.0f));
}

TEST_CASE("what a character does of its own is its clip, where it is in it, the fade left and what is mixed") {
  Machine owner;
  CHECK_FALSE(owner.state().own.has_value());  // it follows the scene's clips
  owner.play(kSlide, kFade);
  owner.player.mix_on(owner.scene, kHero, kLift, kHalfWeight, false, 1.0f);
  owner.advance(kQuarter / 2.0f);
  const CharacterAnimation state = owner.state();
  REQUIRE(state.own.has_value());
  CHECK(state.own->clip == kSlide);
  CHECK(state.own->time == doctest::Approx(kQuarter / 2.0f));
  CHECK(state.own->loop);
  CHECK(state.fade == doctest::Approx(kFade - kQuarter / 2.0f));
  REQUIRE(state.layer.has_value());
  CHECK(state.layer->clip == kLift);
  CHECK_FALSE(state.layer->loop);
  CHECK(state.layer_weight == doctest::Approx(kHalfWeight));
}

TEST_CASE("another machine plays what the owner's character does, from the same moment, late as it is") {
  Machine owner;
  Machine watcher;
  owner.play(kSlide);
  owner.advance(kQuarter);
  watcher.player.take_animation(watcher.scene, kHero, owner.state(), kLate);
  owner.advance(kLate);  // the owner moved on meanwhile
  watcher.advance(0.0f);
  CHECK(watcher.at().x == doctest::Approx(owner.at().x));
  owner.advance(kQuarter);
  watcher.advance(kQuarter);
  CHECK(watcher.at().x == doctest::Approx(owner.at().x));
}

TEST_CASE("a state of the same clip only catches up; a mixed clip and the scene's clips come back as told") {
  Machine owner;
  Machine watcher;
  owner.play(kSlide);
  owner.player.mix_on(owner.scene, kHero, kLift, 1.0f, true, 1.0f);
  owner.advance(kQuarter);
  watcher.player.take_animation(watcher.scene, kHero, owner.state(), 0.0f);
  const std::uint32_t taken = watcher.revision();
  watcher.player.take_animation(watcher.scene, kHero, owner.state(), 0.0f);
  CHECK(watcher.revision() == taken);  // nothing changed the second time
  watcher.advance(0.0f);
  owner.advance(0.0f);
  CHECK(watcher.at().y == doctest::Approx(owner.at().y));
  owner.player.unmix_on(owner.scene, kHero);
  owner.player.follow_scene(owner.scene, kHero, 0.0f);
  watcher.player.take_animation(watcher.scene, kHero, owner.state(), 0.0f);
  CHECK_FALSE(watcher.state().own.has_value());
  CHECK_FALSE(watcher.state().layer.has_value());
}

TEST_CASE("a clip this machine's scene does not have is left out") {
  Machine watcher;
  CharacterAnimation state;
  state.own = ClipState{"Unknown", kQuarter, 1.0f, true, true};
  watcher.player.take_animation(watcher.scene, kHero, state, 0.0f);
  CHECK_FALSE(watcher.state().own.has_value());
}
