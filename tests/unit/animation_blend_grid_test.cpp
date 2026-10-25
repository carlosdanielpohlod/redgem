#include <doctest/doctest.h>

#include <limits>

#include "scene/animation/animation_graph_player.hpp"

using namespace redgem;

namespace {
constexpr float kQuarter = 0.25f;
constexpr float kMiddle = 0.5f;
constexpr float kWeightedCycle = 2.5f;  // mean of the four corner durations

struct GridStage {
  AnimationGraphDefinition graph;
  std::vector<AnimationDesc> clips;
  AnimationGraphPlayer player;
  std::string error;

  GridStage() {
    for (int i = 1; i <= 4; ++i) clips.push_back({std::to_string(i), static_cast<float>(i), {}});
    graph.initial = "Move";
    graph.parameters = {{"x", kMiddle}, {"y", kMiddle}};
    graph.states = {{"Move", "x", {{1, "4", 1}, {0, "1", 0}, {0, "3", 1}, {1, "2", 0}}, true, 1, "y"}};
  }
  void use() { REQUIRE_MESSAGE(player.reset(graph, clips, error), error); }
};
}  // namespace

TEST_CASE("two dimensional animation grids blend four corners and share a normalized phase") {
  GridStage stage;
  stage.use();
  stage.player.advance(kWeightedCycle * kMiddle);
  const auto samples = stage.player.frame().to;
  REQUIRE(samples.size() == 4);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    CHECK(samples[i].clip == i);
    CHECK(samples[i].weight == doctest::Approx(kQuarter));
    CHECK(samples[i].time == doctest::Approx(stage.clips[i].duration * kMiddle));
    CHECK(samples[i].speed == doctest::Approx(stage.clips[i].duration / kWeightedCycle));
  }
}

TEST_CASE("two dimensional animation grids select corners edges and clamp outside both axes") {
  GridStage stage;
  stage.use();
  stage.player.set("x", -1.0f);
  stage.player.set("y", 2.0f);
  REQUIRE(stage.player.frame().to.size() == 1);
  CHECK(stage.player.frame().to.front().clip == 2);
  stage.player.set("y", kMiddle);
  const auto edge = stage.player.frame().to;
  REQUIRE(edge.size() == 2);
  CHECK(edge[0].clip == 0);
  CHECK(edge[1].clip == 2);
  CHECK(edge[0].weight == doctest::Approx(kMiddle));
  stage.player.set("x", kQuarter);
  stage.player.set("y", 1.0f - kQuarter);
  const auto inside = stage.player.frame().to;
  REQUIRE(inside.size() == 4);
  CHECK(inside[0].weight == doctest::Approx((1 - kQuarter) * kQuarter));
  CHECK(inside[3].weight == doctest::Approx(kQuarter * (1 - kQuarter)));
}

TEST_CASE("two dimensional animation grids reject holes duplicates nonfinite coordinates and bad axes") {
  GridStage stage;
  auto& state = stage.graph.states.front();
  SUBCASE("hole") { state.points.pop_back(); }
  SUBCASE("duplicate") { state.points.back() = state.points.front(); }
  SUBCASE("nonfinite second coordinate") { state.points[0].second_threshold = std::numeric_limits<float>::infinity(); }
  SUBCASE("nonfinite first coordinate") { state.points[0].threshold = std::numeric_limits<float>::quiet_NaN(); }
  SUBCASE("missing axis") { state.second_parameter = "missing"; }
  SUBCASE("same axis") { state.second_parameter = state.parameter; }
  SUBCASE("boolean axis") { stage.graph.parameters["y"] = true; }
  SUBCASE("one row") {
    for (auto& point : state.points) point.second_threshold = 0;
  }
  SUBCASE("unknown clip") { state.points[0].clip = "missing"; }
  CHECK_FALSE(stage.player.reset(stage.graph, stage.clips, stage.error));
}

TEST_CASE("two dimensional animation grid interpolation handles the full finite coordinate range") {
  GridStage stage;
  const float bound = std::numeric_limits<float>::max();
  for (auto& point : stage.graph.states[0].points) {
    point.threshold = point.threshold == 0 ? -bound : bound;
    point.second_threshold = point.second_threshold == 0 ? -bound : bound;
  }
  stage.graph.parameters = {{"x", 0.0f}, {"y", 0.0f}};
  stage.use();
  for (const auto& sample : stage.player.frame().to) CHECK(sample.weight == doctest::Approx(kQuarter));
}

TEST_CASE("two dimensional animation grids use only the enclosing cell of a larger grid") {
  GridStage stage;
  auto& points = stage.graph.states[0].points;
  points.clear();
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 3; ++x) points.push_back({static_cast<float>(x), "1", static_cast<float>(y)});
  stage.graph.parameters = {{"x", 1.0f + kQuarter}, {"y", 1.0f + kMiddle}};
  stage.use();
  const auto samples = stage.player.frame().to;
  REQUIRE(samples.size() == 4);
  CHECK(samples[0].weight == doctest::Approx((1 - kQuarter) * kMiddle));
  CHECK(samples[1].weight == doctest::Approx(kQuarter * kMiddle));
}

TEST_CASE("two dimensional animation grids keep four tracks on both sides of a state fade") {
  GridStage stage;
  stage.graph.parameters["change"] = true;
  auto next = stage.graph.states.front();
  next.name = "Next";
  next.speed = 2;
  stage.graph.states.push_back(next);
  stage.graph.transitions = {{"Move", "Next", "change", AnimationComparison::Equal, true, 1}};
  stage.use();
  stage.player.advance(kQuarter);
  const auto frame = stage.player.frame();
  CHECK(frame.from.size() == 4);
  CHECK(frame.to.size() == 4);
  CHECK(frame.blend == doctest::Approx(kQuarter));
  CHECK(frame.to[0].time == doctest::Approx(frame.from[0].time * 2));
}

TEST_CASE("two dimensional animation grids preserve reverse playback pause and nonlooping endpoints") {
  GridStage stage;
  stage.graph.states[0].loop = false;
  stage.graph.states[0].speed = -1;
  stage.use();
  stage.player.advance(kWeightedCycle * kMiddle);
  const auto before = stage.player.frame();
  stage.player.pause(true);
  stage.player.advance(kWeightedCycle);
  CHECK(stage.player.frame().to[0].time == before.to[0].time);
  stage.player.pause(false);
  stage.player.advance(kWeightedCycle);
  CHECK(stage.player.frame().to[0].time == 0);
  CHECK_FALSE(stage.player.playing());
}
