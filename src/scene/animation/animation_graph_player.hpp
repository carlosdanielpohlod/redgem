#pragma once

#include "scene/animation/animation_graph.hpp"

namespace redgem {

struct AnimationGraphSample {
  std::size_t clip = 0;
  float time = 0.0f;
  float weight = 1.0f;
  bool loop = true;
  double speed = 1.0;  // clip seconds per real second, including normalized phase synchronization
};

// Each side has at most four samples. Weights on each side sum to one; blend mixes the sides.
struct AnimationGraphFrame {
  std::vector<AnimationGraphSample> from;
  std::vector<AnimationGraphSample> to;
  float blend = 1.0f;
};

// Evaluates clips and weights only. AnimationPlayer owns scene handles and applies the resulting pose.
class AnimationGraphPlayer {
 public:
  bool reset(const AnimationGraphDefinition& definition, const std::vector<AnimationDesc>& clips, std::string& error);
  bool set(const std::string& parameter, const AnimationParameter& value);
  std::optional<AnimationParameter> parameter(const std::string& name) const;
  float fade_remaining() const { return static_cast<float>(fade_ - faded_); }
  // Invalid time steps do nothing. At most one transition is taken per advance, including dt = 0.
  // A fade finishes before another transition is considered, at the start of the next advance.
  bool advance(float dt);
  void pause(bool paused) { paused_ = paused; }
  bool paused() const { return paused_; }
  bool playing() const;
  std::string state() const;
  AnimationGraphFrame frame() const;

 private:
  struct StateClock {
    std::size_t state = 0;
    double phase = 0.0;
  };

  const AnimationGraph& installed_graph() const;
  const CompiledAnimationState& state_of(const StateClock& clock) const;
  std::vector<AnimationGraphSample> samples(const StateClock& clock) const;
  double duration(const StateClock& clock) const;
  bool representable_speeds() const;
  void tick(StateClock& clock, float dt);
  void choose_transition();
  void take_transition(const CompiledAnimationTransition& transition);
  void finish_fade(float dt);
  StateClock start(std::size_t state) const;

  std::optional<AnimationGraph> graph_;
  AnimationParameters parameters_;
  StateClock current_;
  std::optional<StateClock> source_;
  double fade_ = 0.0;
  double faded_ = 0.0;
  bool paused_ = false;
};

}  // namespace redgem
