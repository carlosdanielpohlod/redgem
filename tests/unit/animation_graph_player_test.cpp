#include <doctest/doctest.h>

#include <cmath>
#include <limits>

#include "scene/animation/animation_graph_player.hpp"

using namespace redgem;

namespace {

struct GraphStage {
  AnimationGraphDefinition definition;
  std::vector<AnimationDesc> clips;
  AnimationGraphPlayer player;
  std::string error;

  GraphStage() {
    for (const auto& entry :
         std::vector<std::pair<std::string, float>>{{"Idle", 1.0f}, {"Walk", 2.0f}, {"Run", 4.0f}}) {
      AnimationDesc clip;
      clip.name = entry.first;
      clip.duration = entry.second;
      clips.push_back(clip);
    }
    define();
    REQUIRE(player.reset(definition, clips, error));
  }

  void define() {
    definition.initial = "Ground";
    definition.parameters = {{"speed", 0.0f}, {"grounded", true}};
    definition.states = {{"Ground", "speed", {{0.0f, "Idle"}, {1.0f, "Walk"}, {3.0f, "Run"}}},
                         {"Air", "", {{0.0f, "Idle"}}, false}};
    definition.transitions = {{"Ground", "Air", "grounded", AnimationComparison::Equal, false, 0.5f},
                              {"Air", "Ground", "grounded", AnimationComparison::Equal, true, 0.25f}};
  }

  void reset() { REQUIRE(player.reset(definition, clips, error)); }
};

}  // namespace

TEST_CASE("animation graph player is empty before a valid graph is installed") {
  AnimationGraphPlayer player;
  CHECK(player.state().empty());
  CHECK(player.frame().to.empty());
  CHECK_FALSE(player.playing());
  CHECK_FALSE(player.set("speed", 1.0f));
  CHECK_FALSE(player.advance(0.0f));
}

TEST_CASE("one dimensional animation weights clamp at endpoints and select only neighbors") {
  GraphStage stage;
  REQUIRE(stage.player.set("speed", 2.0f));
  const auto frame = stage.player.frame();
  REQUIRE(frame.to.size() == 2);
  CHECK(frame.to[0].clip == 1);
  CHECK(frame.to[1].clip == 2);
  CHECK(frame.to[0].weight == doctest::Approx(0.5f));
  CHECK(frame.to[1].weight == doctest::Approx(0.5f));
  REQUIRE(stage.player.set("speed", -1.0f));
  CHECK(stage.player.frame().to.front().clip == 0);
  REQUIRE(stage.player.set("speed", 9.0f));
  CHECK(stage.player.frame().to.front().clip == 2);
  REQUIRE(stage.player.set("speed", 1.0f));
  CHECK(stage.player.frame().to.size() == 1);
  CHECK(stage.player.frame().to.front().clip == 1);
}

TEST_CASE("animation blend clips share normalized phase at their weighted duration") {
  GraphStage stage;
  REQUIRE(stage.player.set("speed", 2.0f));
  REQUIRE(stage.player.advance(0.75f));  // half walk and half run last three seconds per cycle
  const auto frame = stage.player.frame();
  REQUIRE(frame.to.size() == 2);
  CHECK(frame.to[0].time == doctest::Approx(0.5f));
  CHECK(frame.to[1].time == doctest::Approx(1.0f));
  CHECK(frame.to[0].speed == doctest::Approx(2.0 / 3.0));
  CHECK(frame.to[1].speed == doctest::Approx(4.0 / 3.0));
  REQUIRE(stage.player.set("speed", 3.0f));
  CHECK(stage.player.frame().to.front().time == doctest::Approx(1.0f));
}

TEST_CASE("animation graph looping phase is independent of time step subdivision") {
  GraphStage whole;
  GraphStage divided;
  REQUIRE(whole.player.set("speed", 2.0f));
  REQUIRE(divided.player.set("speed", 2.0f));
  REQUIRE(whole.player.advance(3.75f));
  for (int step = 0; step < 15; ++step) REQUIRE(divided.player.advance(0.25f));
  CHECK(whole.player.frame().to[0].time == doctest::Approx(divided.player.frame().to[0].time));
  CHECK(whole.player.frame().to[1].time == doctest::Approx(divided.player.frame().to[1].time));
}

TEST_CASE("animation graph fades preserve the playing source and finish before another transition") {
  GraphStage stage;
  REQUIRE(stage.player.set("grounded", false));
  REQUIRE(stage.player.advance(0.125f));
  CHECK(stage.player.state() == "Air");
  CHECK(stage.player.frame().blend == doctest::Approx(0.25f));
  CHECK(stage.player.frame().from.front().time == doctest::Approx(0.125f));
  REQUIRE(stage.player.set("grounded", true));
  REQUIRE(stage.player.advance(0.375f));
  CHECK(stage.player.state() == "Air");
  CHECK(stage.player.frame().from.empty());
  CHECK(stage.player.frame().blend == 1.0f);
  REQUIRE(stage.player.advance(0.0f));  // evaluate the queued condition at the next frame boundary
  CHECK(stage.player.state() == "Ground");
  CHECK(stage.player.frame().blend == 0.0f);
  CHECK(stage.player.frame().from.front().time == doctest::Approx(0.5f));
}

TEST_CASE("animation graph pause holds clocks and defers transitions while accepting parameters") {
  GraphStage stage;
  REQUIRE(stage.player.advance(0.25f));
  stage.player.pause(true);
  REQUIRE(stage.player.set("grounded", false));
  REQUIRE(stage.player.advance(1.0f));
  CHECK(stage.player.state() == "Ground");
  CHECK(stage.player.frame().to.front().time == doctest::Approx(0.25f));
  CHECK_FALSE(stage.player.playing());
  stage.player.pause(false);
  REQUIRE(stage.player.advance(0.0f));
  CHECK(stage.player.state() == "Air");
}

TEST_CASE("invalid graph replacement and parameter updates leave the installed player intact") {
  GraphStage stage;
  REQUIRE(stage.player.advance(0.25f));
  stage.definition.states.back().points[0].clip = "Missing";
  CHECK_FALSE(stage.player.reset(stage.definition, stage.clips, stage.error));
  CHECK_FALSE(stage.error.empty());
  CHECK(stage.player.state() == "Ground");
  CHECK(stage.player.frame().to.front().time == doctest::Approx(0.25f));
  CHECK_FALSE(stage.player.set("speed", true));
  CHECK_FALSE(stage.player.set("grounded", 1.0f));
  CHECK_FALSE(stage.player.set("missing", 1.0f));
  CHECK_FALSE(stage.player.set("speed", std::numeric_limits<float>::quiet_NaN()));
  CHECK(stage.player.frame().to.front().clip == 0);
}

TEST_CASE("invalid graph time steps preserve clocks and transitions") {
  GraphStage stage;
  REQUIRE(stage.player.set("grounded", false));
  CHECK_FALSE(stage.player.advance(-1.0f));
  CHECK_FALSE(stage.player.advance(std::numeric_limits<float>::infinity()));
  CHECK_FALSE(stage.player.advance(std::numeric_limits<float>::quiet_NaN()));
  CHECK(stage.player.state() == "Ground");
  CHECK(stage.player.frame().to.front().time == 0.0f);
}

TEST_CASE("animation graph nonlooping clips hold their endpoints in either direction") {
  GraphStage stage;
  stage.definition.states[0].loop = false;
  SUBCASE("forward") {
    stage.reset();
    REQUIRE(stage.player.advance(2.0f));
    CHECK(stage.player.frame().to.front().time == 1.0f);
  }
  SUBCASE("backward") {
    stage.definition.states[0].speed = -1.0f;
    stage.reset();
    CHECK(stage.player.frame().to.front().time == 1.0f);
    REQUIRE(stage.player.advance(0.25f));
    CHECK(stage.player.frame().to.front().time == doctest::Approx(0.75f));
    REQUIRE(stage.player.advance(2.0f));
    CHECK(stage.player.frame().to.front().time == 0.0f);
  }
  CHECK_FALSE(stage.player.playing());
}

TEST_CASE("animation graph zero length clips and extreme finite inputs remain finite") {
  GraphStage stage;
  SUBCASE("zero length") {
    stage.clips[0].duration = 0.0f;
    stage.definition.states[0].loop = false;
    stage.reset();
    REQUIRE(stage.player.advance(1.0f));
    CHECK(stage.player.frame().to.front().time == 0.0f);
    CHECK_FALSE(stage.player.playing());
  }
  SUBCASE("large blend span and time step") {
    stage.definition.states[0].points[0].threshold = -std::numeric_limits<float>::max();
    stage.definition.states[0].points[2].threshold = std::numeric_limits<float>::max();
    stage.reset();
    REQUIRE(stage.player.advance(std::numeric_limits<float>::max()));
    for (const auto& sample : stage.player.frame().to) {
      CHECK(std::isfinite(sample.time));
      CHECK(std::isfinite(sample.weight));
      CHECK(std::isfinite(sample.speed));
    }
  }
}

TEST_CASE("animation graph numeric transitions follow declaration priority and take only one edge") {
  GraphStage stage;
  stage.definition.transitions[0] = {"Ground", "Air", "speed", AnimationComparison::Above, 1.0f, 0.0f};
  stage.definition.transitions[1] = {"Air", "Ground", "speed", AnimationComparison::Above, 1.0f, 0.0f};
  stage.reset();
  REQUIRE(stage.player.set("speed", 2.0f));
  REQUIRE(stage.player.advance(0.0f));
  CHECK(stage.player.state() == "Air");
  CHECK(stage.player.frame().from.empty());
  REQUIRE(stage.player.advance(0.0f));
  CHECK(stage.player.state() == "Ground");
}

TEST_CASE("animation graph transitions use the first matching edge in declaration order") {
  GraphStage stage;
  stage.definition.states.push_back({"Other", "", {{0.0f, "Walk"}}});
  stage.definition.transitions.insert(stage.definition.transitions.begin(),
                                      {"Ground", "Other", "grounded", AnimationComparison::Equal, false, 0.0f});
  stage.reset();
  REQUIRE(stage.player.set("grounded", false));
  REQUIRE(stage.player.advance(0.0f));
  CHECK(stage.player.state() == "Other");
}

TEST_CASE("animation graph numeric threshold comparisons are strict") {
  GraphStage stage;
  stage.definition.transitions[0] = {"Ground", "Air", "speed", AnimationComparison::Below, 1.0f, 0.0f};
  stage.reset();
  REQUIRE(stage.player.set("speed", 1.0f));
  REQUIRE(stage.player.advance(0.0f));
  CHECK(stage.player.state() == "Ground");
  REQUIRE(stage.player.set("speed", 0.5f));
  REQUIRE(stage.player.advance(0.0f));
  CHECK(stage.player.state() == "Air");
}

TEST_CASE("animation graph players own independent clocks and parameter values") {
  GraphStage first;
  GraphStage second;
  REQUIRE(first.player.set("grounded", false));
  REQUIRE(first.player.advance(0.25f));
  CHECK(first.player.state() == "Air");
  CHECK(second.player.state() == "Ground");
  CHECK(second.player.frame().to.front().time == 0.0f);
  first.player.pause(true);
  first.reset();
  CHECK_FALSE(first.player.paused());
  CHECK(first.player.state() == "Ground");
  CHECK(first.player.frame().to.front().time == 0.0f);
}
