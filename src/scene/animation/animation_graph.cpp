#include "scene/animation/animation_graph.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace redgem {
namespace {

bool finite_parameter(const AnimationParameter& value) {
  const auto* number = std::get_if<float>(&value);
  return number == nullptr || std::isfinite(*number);
}

bool valid_parameters(const AnimationParameters& parameters) {
  return std::all_of(parameters.begin(), parameters.end(),
                     [](const auto& entry) { return !entry.first.empty() && finite_parameter(entry.second); });
}

bool numeric_parameter(const AnimationParameters& parameters, const std::string& name) {
  const auto found = parameters.find(name);
  return found != parameters.end() && std::holds_alternative<float>(found->second);
}

std::optional<std::size_t> clip_index(const std::vector<AnimationDesc>& clips, const std::string& name) {
  for (std::size_t index = 0; index < clips.size(); ++index) {
    if (clips[index].name == name) return index;
  }
  return std::nullopt;
}

std::optional<std::size_t> state_index(const AnimationGraph& graph, const std::string& name) {
  for (std::size_t index = 0; index < graph.states.size(); ++index) {
    if (graph.states[index].name == name) return index;
  }
  return std::nullopt;
}

bool valid_duration(float duration) { return std::isfinite(duration) && duration >= 0.0f; }

bool valid_state_shape(const AnimationGraphState& state, const AnimationParameters& parameters) {
  if (state.name.empty() || !std::isfinite(state.speed)) return false;
  if (state.parameter.empty()) return state.points.size() == 1;
  return !state.points.empty() && numeric_parameter(parameters, state.parameter);
}

bool append_point(CompiledAnimationState& state, const AnimationBlendPoint& point,
                  const std::vector<AnimationDesc>& clips) {
  if (point.clip.empty() || !std::isfinite(point.threshold)) return false;
  if (!state.points.empty() && point.threshold <= state.points.back().threshold) return false;
  const auto index = clip_index(clips, point.clip);
  if (!index || !valid_duration(clips[*index].duration)) return false;
  state.points.push_back({point.threshold, *index, clips[*index].duration});
  return true;
}

bool append_state(AnimationGraph& graph, const AnimationGraphState& state, const std::vector<AnimationDesc>& clips) {
  if (!valid_state_shape(state, graph.parameters) || state_index(graph, state.name)) return false;
  CompiledAnimationState compiled{state.name, state.parameter, {}, state.loop, state.speed};
  for (const AnimationBlendPoint& point : state.points) {
    if (!append_point(compiled, point, clips)) return false;
  }
  graph.states.push_back(std::move(compiled));
  return true;
}

bool known_comparison(AnimationComparison comparison) {
  return comparison == AnimationComparison::Equal || comparison == AnimationComparison::Above ||
         comparison == AnimationComparison::Below;
}

bool valid_condition(const AnimationGraphTransition& transition, const AnimationParameters& parameters) {
  const auto found = parameters.find(transition.parameter);
  if (found == parameters.end() || !finite_parameter(transition.value)) return false;
  if (found->second.index() != transition.value.index() || !known_comparison(transition.comparison)) return false;
  return transition.comparison == AnimationComparison::Equal || std::holds_alternative<float>(transition.value);
}

bool append_transition(AnimationGraph& graph, const AnimationGraphTransition& transition) {
  const auto from = state_index(graph, transition.from);
  const auto to = state_index(graph, transition.to);
  if (!from || !to || from == to) return false;
  if (!valid_duration(transition.fade) || !valid_condition(transition, graph.parameters)) return false;
  graph.transitions.push_back({*from, *to, transition});
  return true;
}

bool append_transitions(AnimationGraph& graph, const std::vector<AnimationGraphTransition>& transitions) {
  return std::all_of(transitions.begin(), transitions.end(),
                     [&](const auto& transition) { return append_transition(graph, transition); });
}

bool all_reachable(const AnimationGraph& graph) {
  std::set<std::size_t> reached{graph.initial};
  for (std::size_t pass = 0; pass < graph.states.size(); ++pass) {
    for (const auto& edge : graph.transitions) {
      if (reached.count(edge.from) != 0) reached.insert(edge.to);
    }
  }
  return reached.size() == graph.states.size();
}

bool unique_clip_names(const std::vector<AnimationDesc>& clips) {
  std::set<std::string> names;
  for (const auto& clip : clips) {
    if (!names.insert(clip.name).second) return false;
  }
  return true;
}

AnimationGraphCompilation failure(const std::string& error) { return {std::nullopt, error}; }

AnimationGraphCompilation compile_states(const AnimationGraphDefinition& definition,
                                         const std::vector<AnimationDesc>& clips) {
  AnimationGraph graph;
  graph.parameters = definition.parameters;
  for (const auto& state : definition.states) {
    if (!append_state(graph, state, clips)) return failure("invalid animation state: " + state.name);
  }
  const auto initial = state_index(graph, definition.initial);
  if (!initial) return failure("unknown initial animation state");
  graph.initial = *initial;
  return {std::move(graph), {}};
}

}  // namespace

AnimationGraphCompilation compile_animation_graph(const AnimationGraphDefinition& definition,
                                                  const std::vector<AnimationDesc>& clips) {
  if (!valid_parameters(definition.parameters)) return failure("invalid animation parameter");
  if (!unique_clip_names(clips)) return failure("ambiguous animation clip name");
  auto result = compile_states(definition, clips);
  if (!result.graph) return result;
  if (!append_transitions(*result.graph, definition.transitions)) return failure("invalid animation transition");
  if (!all_reachable(*result.graph)) return failure("unreachable animation state");
  return result;
}

}  // namespace redgem
