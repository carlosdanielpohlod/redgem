// The scene's clips, by name.

#include "scene/animation/animation_player.hpp"

#include <algorithm>

namespace redgem {

void AnimationPlayer::reset(const std::vector<AnimationDesc>& clips, const Scene& scene) {
  clips_ = &clips;
  clocks_.assign(clips.size(), ClipClock{});
  owned_.clear();
  revisions_.clear();
  rest_.clear();
  for (std::size_t id = 0; id < scene.size(); ++id) rest_.push_back(scene.at(static_cast<ObjectId>(id)).local);
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
  start(clocks_[*index], duration_of(*index), loop, speed, restart);
  return true;
}

bool AnimationPlayer::pause(const std::string& name) {
  const auto index = find(name);
  if (!index) return false;
  clocks_[*index].playing = false;
  return true;
}

bool AnimationPlayer::stop(const std::string& name) {
  const auto index = find(name);
  if (!index) return false;
  clocks_[*index].playing = false;
  clocks_[*index].posing = false;
  return true;
}

bool AnimationPlayer::seek(const std::string& name, float seconds) {
  const auto index = find(name);
  if (!index) return false;
  clocks_[*index].time = std::clamp(seconds, 0.0f, duration_of(*index));
  clocks_[*index].posing = true;
  return true;
}

std::optional<bool> AnimationPlayer::playing(const std::string& name) const {
  const auto index = find(name);
  return index ? std::optional<bool>(clocks_[*index].playing) : std::nullopt;
}

std::optional<float> AnimationPlayer::time(const std::string& name) const {
  const auto index = find(name);
  return index ? std::optional<float>(clocks_[*index].time) : std::nullopt;
}

std::optional<float> AnimationPlayer::duration(const std::string& name) const {
  const auto index = find(name);
  return index ? std::optional<float>(duration_of(*index)) : std::nullopt;
}

}  // namespace redgem
