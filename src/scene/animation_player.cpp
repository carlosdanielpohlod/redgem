#include "scene/animation_player.hpp"

#include <algorithm>
#include <cmath>

#include "scene/animation.hpp"

namespace redgem {

void AnimationPlayer::reset(const std::vector<AnimationDesc>& clips) {
  clips_ = &clips;
  states_.assign(clips.size(), State{});
}

void AnimationPlayer::advance(float dt, Scene& scene) {
  if (clips_ == nullptr) return;
  for (std::size_t i = 0; i < states_.size(); ++i) {
    const AnimationDesc& clip = (*clips_)[i];
    State& state = states_[i];
    if (state.playing) {
      state.time += dt * state.speed;
      const bool past_end = state.time >= clip.duration;
      const bool before_start = state.time < 0.0f;
      if (state.loop) {
        // Kept within the clip so it stays precise however long the game runs.
        if (clip.duration > 0.0f && (past_end || before_start)) {
          state.time = std::fmod(state.time, clip.duration);
          if (state.time < 0.0f) state.time += clip.duration;
        }
      } else if (past_end || before_start) {
        state.time = past_end ? clip.duration : 0.0f;
        state.playing = false;  // holds its last pose
      }
    }
    if (state.posing) apply_animation(clip, state.time, scene, state.loop);
  }
}

std::vector<std::string> AnimationPlayer::names() const {
  std::vector<std::string> out;
  if (clips_ == nullptr) return out;
  for (const AnimationDesc& clip : *clips_) out.push_back(clip.name);
  return out;
}

std::optional<std::size_t> AnimationPlayer::find(const std::string& name) const {
  if (clips_ == nullptr) return std::nullopt;
  for (std::size_t i = 0; i < clips_->size(); ++i) {
    if ((*clips_)[i].name == name) return i;
  }
  return std::nullopt;
}

bool AnimationPlayer::play(const std::string& name, bool loop, float speed, bool restart) {
  const auto index = find(name);
  if (!index) return false;
  State& state = states_[*index];
  const float duration = (*clips_)[*index].duration;
  const bool backwards = speed < 0.0f;
  // A clip played once that already reached its end has nowhere to go: it starts over.
  const bool ended = !loop && (backwards ? state.time <= 0.0f : state.time >= duration);
  if (restart || ended || !state.posing) state.time = backwards ? duration : 0.0f;
  state.loop = loop;
  state.speed = speed;
  state.playing = true;
  state.posing = true;
  return true;
}

bool AnimationPlayer::pause(const std::string& name) {
  const auto index = find(name);
  if (!index) return false;
  states_[*index].playing = false;
  return true;
}

bool AnimationPlayer::stop(const std::string& name) {
  const auto index = find(name);
  if (!index) return false;
  states_[*index].playing = false;
  states_[*index].posing = false;
  return true;
}

bool AnimationPlayer::seek(const std::string& name, float seconds) {
  const auto index = find(name);
  if (!index) return false;
  states_[*index].time = std::clamp(seconds, 0.0f, (*clips_)[*index].duration);
  states_[*index].posing = true;
  return true;
}

std::optional<bool> AnimationPlayer::playing(const std::string& name) const {
  const auto index = find(name);
  return index ? std::optional<bool>(states_[*index].playing) : std::nullopt;
}

std::optional<float> AnimationPlayer::time(const std::string& name) const {
  const auto index = find(name);
  return index ? std::optional<float>(states_[*index].time) : std::nullopt;
}

std::optional<float> AnimationPlayer::duration(const std::string& name) const {
  const auto index = find(name);
  return index ? std::optional<float>((*clips_)[*index].duration) : std::nullopt;
}

}  // namespace redgem
