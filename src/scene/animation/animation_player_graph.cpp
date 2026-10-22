#include "scene/animation/animation_player.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "scene/animation/animation.hpp"
#include "scene/animation/animation_blend.hpp"

namespace redgem {
namespace {

constexpr float kWeightTolerance = 0.0001f;

bool unit_weight(float weight) { return std::isfinite(weight) && weight >= 0.0f && weight <= 1.0f; }

bool graph_shape(const GraphAnimation& graph) {
  if (!unit_weight(graph.blend) || !std::isfinite(graph.remaining)) return false;
  if (graph.to.empty()) return false;
  if (graph.from.empty()) return graph.blend == 1.0f && graph.remaining == 0.0f;
  return graph.blend < 1.0f && graph.remaining > 0.0f;
}

float wrapped_clip_time(double time, float duration) {
  const double within = std::fmod(time, duration);
  return static_cast<float>(within < 0.0 ? within + duration : within);
}

void hold_zero_clip(ClipState& clip) {
  clip.time = 0.0f;
  clip.playing = clip.loop;
}

void advance_clip(ClipState& clip, float duration, float dt) {
  if (!clip.playing) return;
  const double next = clip.time + static_cast<double>(dt) * clip.speed;
  if (duration <= 0.0f) {
    hold_zero_clip(clip);
    return;
  }
  if (clip.loop) {
    clip.time = wrapped_clip_time(next, duration);
    return;
  }
  clip.time = static_cast<float>(std::clamp(next, 0.0, static_cast<double>(duration)));
  clip.playing = clip.speed < 0.0f ? next > 0.0 : next < duration;
}

void append_samples(const std::vector<WeightedClipState>& side, float weight, const std::vector<AnimationDesc>& clips,
                    std::vector<AnimationGraphSample>& samples) {
  for (const auto& entry : side) {
    const auto found =
        std::find_if(clips.begin(), clips.end(), [&](const auto& clip) { return clip.name == entry.clip.clip; });
    if (found == clips.end()) continue;
    samples.push_back({static_cast<std::size_t>(found - clips.begin()), entry.clip.time, weight * entry.weight,
                       entry.clip.loop, entry.clip.speed});
  }
}

bool sample_playing(const AnimationGraphSample& sample, float duration, bool paused) {
  if (paused) return false;
  if (sample.loop) return true;
  if (duration <= 0.0f) return false;
  return sample.speed < 0.0 ? sample.time > 0.0f : sample.time < duration;
}

bool side_playing(const std::vector<WeightedClipState>& side) {
  return std::any_of(side.begin(), side.end(), [](const auto& entry) { return entry.clip.playing; });
}

std::vector<WeightedClipState> clip_states(const std::vector<AnimationGraphSample>& samples,
                                           const std::vector<AnimationDesc>& clips, bool paused) {
  std::vector<WeightedClipState> states;
  for (const auto& sample : samples) {
    const auto& clip = clips[sample.clip];
    states.push_back({{clip.name, sample.time, static_cast<float>(sample.speed), sample.loop,
                       sample_playing(sample, clip.duration, paused)},
                      sample.weight});
  }
  return states;
}

}  // namespace

void AnimationPlayer::detach_graph(Own& own) {
  own.graph.reset();
  own.received_graph.reset();
}

bool AnimationPlayer::leave_graph(const Scene& scene, ObjectId object) {
  Own* own = own_of(scene, object);
  if (own == nullptr) return false;
  if (!has_graph(*own)) return false;
  detach_graph(*own);
  changed(scene, object);
  return true;
}

bool AnimationPlayer::pause_graph_on(const Scene& scene, ObjectId object) {
  Own* own = own_of(scene, object);
  if (own == nullptr) return false;
  if (!has_graph(*own)) return false;
  if (graph_paused(*own)) return true;
  if (own->graph) own->graph->pause(true);
  if (own->received_graph) own->received_graph->playing = false;
  changed(scene, object);
  return true;
}

bool AnimationPlayer::graph_paused(const Own& own) {
  const bool local_paused = own.graph && own.graph->paused();
  const bool remote_paused = own.received_graph && !own.received_graph->playing;
  return local_paused || remote_paused;
}

std::optional<ClipState> AnimationPlayer::dominant_graph_clip(const Scene& scene, ObjectId object) const {
  const Own* own = own_of(scene, object);
  if (own == nullptr) return std::nullopt;
  if (!has_graph(*own)) return std::nullopt;
  const auto clips = graph_animation(*own).to;
  const auto dominant =
      std::max_element(clips.begin(), clips.end(), [](const auto& a, const auto& b) { return a.weight < b.weight; });
  return dominant == clips.end() ? std::nullopt : std::optional<ClipState>(dominant->clip);
}

bool AnimationPlayer::use_graph(const Scene& scene, ObjectId object, const AnimationGraphDefinition& graph,
                                std::string& error) {
  if (clips_ == nullptr) {
    error = "animation is unavailable";
    return false;
  }
  AnimationGraphPlayer player;
  if (!player.reset(graph, *clips_, error)) return false;
  Own& own = owned_[scene.handle(character_of(scene, object))];
  own = Own{};
  own.graph = std::move(player);
  changed(scene, object);
  return true;
}

bool AnimationPlayer::set_parameter(const Scene& scene, ObjectId object, const std::string& name,
                                    const AnimationParameter& value) {
  Own* own = own_of(scene, object);
  if (own == nullptr || !own->graph) return false;
  const auto previous = own->graph->parameter(name);
  if (!own->graph->set(name, value)) return false;
  if (previous != std::optional<AnimationParameter>(value)) changed(scene, object);
  return true;
}

std::optional<std::string> AnimationPlayer::graph_state(const Scene& scene, ObjectId object) const {
  const Own* own = own_of(scene, object);
  if (own == nullptr || !own->graph) return std::nullopt;
  return own->graph->state();
}

GraphAnimation AnimationPlayer::graph_animation(const Own& own) const {
  if (own.received_graph) return *own.received_graph;
  if (!own.graph) throw std::logic_error("character has no animation graph");
  const auto& player = *own.graph;
  const auto frame = player.frame();
  GraphAnimation result;
  result.playing = player.playing();
  result.from = clip_states(frame.from, *clips_, player.paused());
  result.to = clip_states(frame.to, *clips_, player.paused());
  result.blend = frame.blend;
  result.remaining = player.fade_remaining();
  return result;
}

void AnimationPlayer::pose_graph(ObjectId character, const GraphAnimation& graph, Scene& scene) {
  std::vector<AnimationGraphSample> samples;
  append_samples(graph.from, 1.0f - graph.blend, *clips_, samples);
  append_samples(graph.to, graph.blend, *clips_, samples);
  const AnimationDesc pose = blended_animation(*clips_, samples, rest_, blend_layout_);
  PoseOptions options = character_options(character, kWhole, scene);
  options.offsets = false;
  apply_animation(pose, 0.0f, scene, options);
}

void AnimationPlayer::advance_graph(ObjectId character, Own& own, float dt, Scene& scene) {
  if (own.graph) {
    const auto state = own.graph->state();
    own.graph->advance(dt);
    if (state != own.graph->state()) changed(scene, character);
  } else {
    if (!own.received_graph) throw std::logic_error("character has no received animation graph");
    tick_graph(*own.received_graph, dt);
  }
  pose_graph(character, graph_animation(own), scene);
  advance_track(own.layer, dt);
  if (own.layer) pose_character(character, own.layer, own.layer_weight, scene);
}

void AnimationPlayer::tick_graph_side(std::vector<WeightedClipState>& clips, float dt) const {
  for (auto& entry : clips) {
    const auto index = find(entry.clip.clip);
    if (!index) continue;
    advance_clip(entry.clip, duration_of(*index), dt);
  }
}

void AnimationPlayer::tick_graph(GraphAnimation& graph, float dt) const {
  if (!graph.playing) return;
  tick_graph_side(graph.from, dt);
  tick_graph_side(graph.to, dt);
  const float along = graph.remaining > 0.0f ? std::min(dt / graph.remaining, 1.0f) : 1.0f;
  graph.blend += (1.0f - graph.blend) * along;
  graph.remaining = std::max(graph.remaining - dt, 0.0f);
  if (graph.remaining > 0.0f) graph.blend = std::min(graph.blend, std::nextafter(1.0f, 0.0f));
  if (graph.remaining == 0.0f) {
    graph.from.clear();
    graph.playing = side_playing(graph.to);
  }
}

bool AnimationPlayer::known_graph_clip(const WeightedClipState& entry) const {
  const auto index = find(entry.clip.clip);
  if (!index || !unit_weight(entry.weight)) return false;
  const auto& clip = entry.clip;
  if (!std::isfinite(clip.speed) || !std::isfinite(clip.time)) return false;
  return clip.time >= 0.0f && clip.time <= duration_of(*index);
}

bool AnimationPlayer::known_graph_side(const std::vector<WeightedClipState>& side) const {
  if (side.size() > kMaxGraphSideClips) return false;
  double total = 0.0;
  for (const auto& entry : side) {
    if (!known_graph_clip(entry)) return false;
    total += entry.weight;
  }
  return side.empty() || std::abs(total - 1.0) <= kWeightTolerance;
}

bool AnimationPlayer::take_graph(const Scene& scene, ObjectId object, const GraphAnimation& graph, float late_by) {
  if (!std::isfinite(late_by) || late_by < 0.0f) return false;
  if (!graph_shape(graph)) return false;
  if (!known_graph_side(graph.from) || !known_graph_side(graph.to)) return false;
  Own& own = owned_[scene.handle(character_of(scene, object))];
  own.from.reset();
  own.to.reset();
  own.graph.reset();
  own.received_graph = graph;
  tick_graph(*own.received_graph, late_by);
  return true;
}

}  // namespace redgem
