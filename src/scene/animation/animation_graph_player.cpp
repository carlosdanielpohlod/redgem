#include "scene/animation/animation_graph_player.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace redgem {
namespace {

constexpr double kFullPhase = 1.0;  // normalized time shared by every clip in a state

bool accepts(const AnimationParameter& current, const AnimationParameter& wanted) {
  if (current.index() != wanted.index()) return false;
  const auto* number = std::get_if<float>(&wanted);
  return number == nullptr || std::isfinite(*number);
}

bool valid_time_step(float dt) { return std::isfinite(dt) && dt >= 0.0f; }

bool matches(const CompiledAnimationTransition& transition, const AnimationParameters& parameters) {
  const auto& condition = transition.condition;
  const auto& value = parameters.at(condition.parameter);
  if (condition.comparison == AnimationComparison::Equal) return value == condition.value;
  const float number = std::get<float>(value);
  const float threshold = std::get<float>(condition.value);
  return condition.comparison == AnimationComparison::Above ? number > threshold : number < threshold;
}

AnimationGraphSample sample(const AnimationGraphPoint& point, double phase, float weight, bool loop) {
  return {point.clip, static_cast<float>(phase * point.duration), weight, loop};
}

std::vector<AnimationGraphSample> between(const CompiledAnimationState& state, float value, double phase) {
  const auto& points = state.points;
  const auto upper =
      std::upper_bound(points.begin(), points.end(), value,
                       [](float number, const AnimationGraphPoint& point) { return number < point.threshold; });
  if (upper == points.begin()) return {sample(points.front(), phase, 1.0f, state.loop)};
  if (upper == points.end()) return {sample(points.back(), phase, 1.0f, state.loop)};
  const auto& lower = *(upper - 1);
  const double span = static_cast<double>(upper->threshold) - lower.threshold;
  const float weight = static_cast<float>((static_cast<double>(value) - lower.threshold) / span);
  if (weight == 0.0f) return {sample(lower, phase, 1.0f, state.loop)};
  return {sample(lower, phase, 1.0f - weight, state.loop), sample(*upper, phase, weight, state.loop)};
}

double bounded_phase(double phase, bool loop) {
  if (!loop) return std::clamp(phase, 0.0, kFullPhase);
  const double within = std::fmod(phase, kFullPhase);
  return within < 0.0 ? within + kFullPhase : within;
}

double clip_duration(const CompiledAnimationState& state, std::size_t clip) {
  const auto point =
      std::find_if(state.points.begin(), state.points.end(), [&](const auto& p) { return p.clip == clip; });
  return point->duration;
}

double weighted_duration(const CompiledAnimationState& state, const std::vector<AnimationGraphSample>& samples) {
  double seconds = 0.0;
  for (const auto& entry : samples) seconds += clip_duration(state, entry.clip) * entry.weight;
  return seconds;
}

void synchronize_speeds(const CompiledAnimationState& state, std::vector<AnimationGraphSample>& samples) {
  const double seconds = weighted_duration(state, samples);
  for (auto& entry : samples) {
    entry.speed = seconds > 0.0 ? static_cast<double>(state.speed) * clip_duration(state, entry.clip) / seconds : 0.0;
  }
}

bool representable_speed(const AnimationGraphSample& entry) {
  if (std::abs(entry.speed) > std::numeric_limits<float>::max()) return false;
  return entry.speed == 0.0 || static_cast<float>(entry.speed) != 0.0f;
}

}  // namespace

bool AnimationGraphPlayer::reset(const AnimationGraphDefinition& definition, const std::vector<AnimationDesc>& clips,
                                 std::string& error) {
  auto result = compile_animation_graph(definition, clips);
  error = result.error;
  if (!result.graph) return false;
  AnimationGraphPlayer candidate;
  candidate.graph_ = std::move(result.graph);
  candidate.parameters_ = candidate.graph_->parameters;
  candidate.current_ = candidate.start(candidate.graph_->initial);
  if (!candidate.representable_speeds()) {
    error = "synchronized animation speed is out of range";
    return false;
  }
  *this = std::move(candidate);
  return true;
}

bool AnimationGraphPlayer::set(const std::string& parameter, const AnimationParameter& value) {
  const auto found = parameters_.find(parameter);
  if (found == parameters_.end()) return false;
  if (!accepts(found->second, value)) return false;
  const auto previous = found->second;
  found->second = value;
  if (representable_speeds()) return true;
  found->second = previous;
  return false;
}

std::optional<AnimationParameter> AnimationGraphPlayer::parameter(const std::string& name) const {
  const auto found = parameters_.find(name);
  return found == parameters_.end() ? std::nullopt : std::optional<AnimationParameter>(found->second);
}

const AnimationGraph& AnimationGraphPlayer::installed_graph() const {
  if (!graph_) throw std::logic_error("animation graph is not installed");
  return *graph_;
}

const CompiledAnimationState& AnimationGraphPlayer::state_of(const StateClock& clock) const {
  return installed_graph().states[clock.state];
}

AnimationGraphPlayer::StateClock AnimationGraphPlayer::start(std::size_t state) const {
  return {state, installed_graph().states[state].speed < 0.0f ? kFullPhase : 0.0};
}

std::vector<AnimationGraphSample> AnimationGraphPlayer::samples(const StateClock& clock) const {
  const auto& state = state_of(clock);
  auto result = state.parameter.empty()
                    ? std::vector<AnimationGraphSample>{sample(state.points.front(), clock.phase, 1.0f, state.loop)}
                    : between(state, std::get<float>(parameters_.at(state.parameter)), clock.phase);
  synchronize_speeds(state, result);
  return result;
}

bool AnimationGraphPlayer::representable_speeds() const {
  // Check every state so a later transition cannot produce an unsendable snapshot.
  for (std::size_t state = 0; state < installed_graph().states.size(); ++state) {
    const auto evaluated = samples({state, 0.0});
    if (!std::all_of(evaluated.begin(), evaluated.end(), representable_speed)) return false;
  }
  return true;
}

double AnimationGraphPlayer::duration(const StateClock& clock) const {
  return weighted_duration(state_of(clock), samples(clock));
}

void AnimationGraphPlayer::tick(StateClock& clock, float dt) {
  const double seconds = duration(clock);
  if (seconds <= 0.0) return;
  const auto& state = state_of(clock);
  clock.phase = bounded_phase(clock.phase + static_cast<double>(dt) * state.speed / seconds, state.loop);
}

void AnimationGraphPlayer::take_transition(const CompiledAnimationTransition& transition) {
  source_ = current_;
  current_ = start(transition.to);
  fade_ = transition.condition.fade;
  faded_ = 0.0;
}

void AnimationGraphPlayer::choose_transition() {
  if (faded_ < fade_) return;
  for (const auto& transition : installed_graph().transitions) {
    if (transition.from != current_.state) continue;
    if (!matches(transition, parameters_)) continue;
    take_transition(transition);
    return;
  }
}

void AnimationGraphPlayer::finish_fade(float dt) {
  faded_ = std::min(fade_, faded_ + dt);
  if (faded_ < fade_) return;
  source_.reset();
}

bool AnimationGraphPlayer::advance(float dt) {
  if (!valid_time_step(dt)) return false;
  if (!graph_) return false;
  if (paused_) return true;
  choose_transition();
  tick(current_, dt);
  if (source_) tick(*source_, dt);
  finish_fade(dt);
  return true;
}

bool AnimationGraphPlayer::playing() const {
  if (!graph_ || paused_) return false;
  if (faded_ < fade_) return true;
  const auto& state = state_of(current_);
  if (state.loop) return true;
  if (duration(current_) <= 0.0) return false;
  return state.speed < 0.0f ? current_.phase > 0.0 : current_.phase < kFullPhase;
}

std::string AnimationGraphPlayer::state() const { return graph_ ? state_of(current_).name : std::string{}; }

AnimationGraphFrame AnimationGraphPlayer::frame() const {
  AnimationGraphFrame result;
  if (!graph_) return result;
  result.to = samples(current_);
  if (source_) result.from = samples(*source_);
  result.blend = fade_ > 0.0 ? static_cast<float>(faded_ / fade_) : 1.0f;
  // Keep an unfinished fade distinguishable after rounding the ratio to float.
  if (source_) result.blend = std::min(result.blend, std::nextafter(1.0f, 0.0f));
  return result;
}

}  // namespace redgem
