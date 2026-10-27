#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "scene/scene_desc.hpp"

namespace redgem {

using AnimationParameter = std::variant<bool, float>;
using AnimationParameters = std::map<std::string, AnimationParameter>;

struct AnimationBlendPoint {
  float threshold = 0.0f;
  std::string clip;
  float second_threshold = 0.0f;
};

// An empty parameter denotes a clip; a second parameter selects a rectangular two-dimensional grid.
struct AnimationGraphState {
  std::string name;
  std::string parameter;
  std::vector<AnimationBlendPoint> points;
  bool loop = true;
  float speed = 1.0f;
  std::string second_parameter{};
};

enum class AnimationComparison { Equal, Above, Below };

struct AnimationGraphTransition {
  std::string from;
  std::string to;
  std::string parameter;
  AnimationComparison comparison = AnimationComparison::Equal;
  AnimationParameter value = false;
  float fade = 0.0f;
};

struct AnimationGraphDefinition {
  std::string initial;
  AnimationParameters parameters;
  std::vector<AnimationGraphState> states;
  std::vector<AnimationGraphTransition> transitions;  // first matching transition wins
};

struct AnimationGraphPoint {
  float threshold = 0.0f;
  std::size_t clip = 0;
  float duration = 0.0f;
  float second_threshold = 0.0f;
};

struct CompiledAnimationState {
  std::string name;
  std::string parameter;
  std::vector<AnimationGraphPoint> points;
  bool loop = true;
  float speed = 1.0f;
  std::string second_parameter{};
  std::vector<float> first_axis{};
  std::vector<float> second_axis{};
};

struct CompiledAnimationTransition {
  std::size_t from = 0;
  std::size_t to = 0;
  AnimationGraphTransition condition;
};

// Owns all its data. Clip indices refer to the clip list used during compilation.
struct AnimationGraph {
  std::size_t initial = 0;
  AnimationParameters parameters;
  std::vector<CompiledAnimationState> states;
  std::vector<CompiledAnimationTransition> transitions;
};

struct AnimationGraphCompilation {
  std::optional<AnimationGraph> graph;
  std::string error;
};

// Failure returns no graph, including when an invalid entry follows valid entries.
AnimationGraphCompilation compile_animation_graph(const AnimationGraphDefinition& definition,
                                                  const std::vector<AnimationDesc>& clips);

}  // namespace redgem
