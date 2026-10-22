#include <doctest/doctest.h>

#include <limits>

#include "scene/animation/animation_graph.hpp"

using namespace redgem;

namespace {

std::vector<AnimationDesc> graph_clips() {
  std::vector<AnimationDesc> clips(3);
  clips[0].name = "Idle";
  clips[0].duration = 1.0f;
  clips[1].name = "Walk";
  clips[1].duration = 2.0f;
  clips[2].name = "Run";
  clips[2].duration = 4.0f;
  return clips;
}

AnimationGraphDefinition graph_definition() {
  AnimationGraphDefinition graph;
  graph.initial = "Ground";
  graph.parameters = {{"speed", 0.0f}, {"grounded", true}};
  graph.states = {{"Ground", "speed", {{0.0f, "Idle"}, {1.0f, "Walk"}, {3.0f, "Run"}}},
                  {"Air", "", {{0.0f, "Idle"}}, false}};
  graph.transitions = {{"Ground", "Air", "grounded", AnimationComparison::Equal, false, 0.5f},
                       {"Air", "Ground", "grounded", AnimationComparison::Equal, true, 0.25f}};
  return graph;
}

void rejects(const AnimationGraphDefinition& definition, const std::vector<AnimationDesc>& clips) {
  const auto result = compile_animation_graph(definition, clips);
  CHECK_FALSE(result.graph.has_value());
  CHECK_FALSE(result.error.empty());
}

}  // namespace

TEST_CASE("animation graph compilation resolves clips and states without retaining input references") {
  auto definition = graph_definition();
  auto clips = graph_clips();
  const auto result = compile_animation_graph(definition, clips);
  REQUIRE(result.graph.has_value());
  CHECK(result.error.empty());
  definition.states.clear();
  clips.clear();
  CHECK(result.graph->states.size() == 2);
  CHECK(result.graph->states[0].points[2].clip == 2);
  CHECK(result.graph->states[0].points[2].duration == 4.0f);
  CHECK(result.graph->transitions[0].to == 1);
}

TEST_CASE("animation graph rejects invalid states atomically") {
  auto graph = graph_definition();
  SUBCASE("missing initial") { graph.initial = "Unknown"; }
  SUBCASE("empty graph") { graph.states.clear(); }
  SUBCASE("duplicate names") { graph.states[1].name = "Ground"; }
  SUBCASE("empty name") { graph.states[1].name.clear(); }
  SUBCASE("no points") { graph.states[0].points.clear(); }
  SUBCASE("missing clip") { graph.states[1].points[0].clip = "Absent"; }
  SUBCASE("unknown numeric parameter") { graph.states[0].parameter = "unknown"; }
  SUBCASE("boolean blend parameter") { graph.states[0].parameter = "grounded"; }
  SUBCASE("multiple clips without parameter") { graph.states[0].parameter.clear(); }
  SUBCASE("duplicate thresholds") { graph.states[0].points[1].threshold = 0.0f; }
  SUBCASE("descending thresholds") { graph.states[0].points[1].threshold = -1.0f; }
  SUBCASE("nonfinite threshold") { graph.states[0].points[1].threshold = std::numeric_limits<float>::infinity(); }
  SUBCASE("nonfinite speed") { graph.states[1].speed = std::numeric_limits<float>::quiet_NaN(); }
  rejects(graph, graph_clips());
}

TEST_CASE("animation graph rejects invalid transitions and defaults") {
  auto graph = graph_definition();
  SUBCASE("unknown source") { graph.transitions[0].from = "missing"; }
  SUBCASE("unknown destination") { graph.transitions[0].to = "missing"; }
  SUBCASE("self edge") { graph.transitions[0].to = "Ground"; }
  SUBCASE("unreachable state") { graph.transitions.clear(); }
  SUBCASE("unknown parameter") { graph.transitions[0].parameter = "missing"; }
  SUBCASE("mismatched parameter types") { graph.transitions[0].value = 1.0f; }
  SUBCASE("ordered comparison of booleans") { graph.transitions[0].comparison = AnimationComparison::Above; }
  SUBCASE("invalid comparison") { graph.transitions[0].comparison = static_cast<AnimationComparison>(99); }
  SUBCASE("negative fade") { graph.transitions[0].fade = -1.0f; }
  SUBCASE("infinite fade") { graph.transitions[0].fade = std::numeric_limits<float>::infinity(); }
  SUBCASE("nonfinite default") { graph.parameters["speed"] = std::numeric_limits<float>::quiet_NaN(); }
  SUBCASE("empty parameter name") { graph.parameters[""] = false; }
  SUBCASE("nonfinite condition") {
    graph.transitions[0].parameter = "speed";
    graph.transitions[0].value = std::numeric_limits<float>::infinity();
  }
  rejects(graph, graph_clips());
}

TEST_CASE("animation graph rejects ambiguous clips and invalid durations") {
  auto clips = graph_clips();
  SUBCASE("duplicate clip name") { clips[2].name = "Walk"; }
  SUBCASE("negative duration") { clips[1].duration = -1.0f; }
  SUBCASE("nonfinite duration") { clips[1].duration = std::numeric_limits<float>::quiet_NaN(); }
  rejects(graph_definition(), clips);
}

TEST_CASE("animation graph accepts zero duration clips and one point blend states") {
  auto graph = graph_definition();
  auto clips = graph_clips();
  graph.states[0].points.resize(1);
  clips[0].duration = 0.0f;
  CHECK(compile_animation_graph(graph, clips).graph.has_value());
}
