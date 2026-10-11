#include "scene/animation_player.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "scene/animation.hpp"

namespace redgem {

namespace {

constexpr float kWhole = 1.0f;                  // a clip laid on with all its weight
constexpr std::uint64_t kAnimationPaths = 3;    // translation, rotation, scale

}  // namespace

void AnimationPlayer::reset(const std::vector<AnimationDesc>& clips, const Scene& scene) {
  clips_ = &clips;
  states_.assign(clips.size(), State{});
  owned_.clear();
  rest_.clear();
  for (std::size_t id = 0; id < scene.size(); ++id) rest_.push_back(scene.at(static_cast<ObjectId>(id)).local);
}

void AnimationPlayer::steady_base(const Scene& scene, ObjectId object, AnimationPath path, Transform& local) {
  const std::uint64_t part = static_cast<std::uint64_t>(object) * kAnimationPaths + static_cast<std::uint64_t>(path);
  if (!posed_.insert(part).second) return;  // already posed this frame: blend from that
  // A copy rests as the object it copies does.
  const ObjectId original = scene.at(object).copy_of != kNoObject ? scene.at(object).copy_of : object;
  if (original < 0 || static_cast<std::size_t>(original) >= rest_.size()) return;
  const Transform& rest = rest_[static_cast<std::size_t>(original)];
  switch (path) {
    case AnimationPath::Translation: local.position = rest.position; break;
    case AnimationPath::Rotation: local.rotation = rest.rotation; break;
    case AnimationPath::Scale: local.scale = rest.scale; break;
  }
}

void AnimationPlayer::advance_state(State& state, float duration, float dt) const {
  if (!state.playing) return;
  state.time += dt * state.speed;
  const bool past_end = state.time >= duration;
  const bool before_start = state.time < 0.0f;
  if (state.loop) {
    // Kept within the clip so it stays precise however long the game runs.
    if (duration > 0.0f && (past_end || before_start)) {
      state.time = std::fmod(state.time, duration);
      if (state.time < 0.0f) state.time += duration;
    }
  } else if (past_end || before_start) {
    state.time = past_end ? duration : 0.0f;
    state.playing = false;  // holds its last pose
  }
}

void AnimationPlayer::advance(float dt, Scene& scene) {
  if (clips_ == nullptr) return;
  posed_.clear();
  for (std::size_t i = 0; i < states_.size(); ++i) advance_state(states_[i], (*clips_)[i].duration, dt);

  // Characters with a clip of their own are left out of the scene's.
  PoseOptions everyone_else;
  if (!owned_.empty()) {
    everyone_else.wanted = [this, &scene](ObjectId object) { return owner_of(scene, object) == kNoObject; };
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    if (!states_[i].posing) continue;
    everyone_else.loop = states_[i].loop;
    apply_animation((*clips_)[i], states_[i].time, scene, everyone_else);
  }

  for (auto entry = owned_.begin(); entry != owned_.end();) {
    const ObjectId character = entry->first;
    Own& own = entry->second;
    if (!scene.is_alive(character)) {
      entry = owned_.erase(entry);
      continue;
    }
    for (std::optional<Track>* track : {&own.from, &own.to, &own.layer}) {
      if (*track) advance_state((*track)->state, (*clips_)[(*track)->clip].duration, dt);
    }
    own.faded += dt;
    const bool arrived = own.fade <= 0.0f || own.faded >= own.fade;
    if (arrived) {
      pose_character(character, own.to, kWhole, scene);
    } else {
      pose_character(character, own.from, kWhole, scene);
      pose_character(character, own.to, own.faded / own.fade, scene);
    }
    if (own.layer) pose_character(character, own.layer, own.layer_weight, scene);
    if (arrived) own.from = own.to;
    // Back with the scene's clips and nothing laid over them: nothing of its own is left.
    entry = arrived && !own.to && !own.layer ? owned_.erase(entry) : std::next(entry);
  }
}

void AnimationPlayer::pose_character(ObjectId character, const std::optional<Track>& track, float weight,
                                     Scene& scene) {
  PoseOptions options;
  options.weight = weight;
  options.wanted = [this, &scene, character](ObjectId object) { return owner_of(scene, object) == character; };
  options.before = [this, &scene, weight](ObjectId object, AnimationPath path, Transform& local) {
    if (weight < kWhole) {
      steady_base(scene, object, path, local);
    } else {
      posed_.insert(static_cast<std::uint64_t>(object) * kAnimationPaths + static_cast<std::uint64_t>(path));
    }
  };
  if (track) {
    options.loop = track->state.loop;
    options.offsets = false;  // its own clip, at its own time
    apply_animation((*clips_)[track->clip], track->state.time, scene, options);
    return;
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    if (!states_[i].posing) continue;
    options.loop = states_[i].loop;
    apply_animation((*clips_)[i], states_[i].time, scene, options);
  }
}

ObjectId AnimationPlayer::character_of(const Scene& scene, ObjectId object) {
  const ObjectId root = scene.at(object).copy_root;
  return root != kNoObject ? root : object;
}

ObjectId AnimationPlayer::owner_of(const Scene& scene, ObjectId object) const {
  const ObjectId root = scene.at(object).copy_root;
  if (root != kNoObject) return owned_.count(root) != 0 ? root : kNoObject;
  for (ObjectId walk = object; walk != kNoObject; walk = scene.at(walk).parent) {
    if (owned_.count(walk) != 0) return walk;
  }
  return kNoObject;
}

const AnimationPlayer::Track* AnimationPlayer::track_on(const Scene& scene, ObjectId object) const {
  const auto found = owned_.find(character_of(scene, object));
  return found != owned_.end() && found->second.to ? &*found->second.to : nullptr;
}

bool AnimationPlayer::play_on(const Scene& scene, ObjectId object, const std::string& name, bool loop, float speed,
                              float fade, bool restart) {
  const auto index = find(name);
  if (!index) return false;
  Own& own = owned_[character_of(scene, object)];
  const float duration = (*clips_)[*index].duration;
  if (own.to && own.to->clip == *index) {
    // Already on this clip: carry on (or start over), with no fade to itself.
    start(own.to->state, duration, loop, speed, restart);
    return true;
  }
  Track next;
  next.clip = *index;
  start(next.state, duration, loop, speed, true);
  // Mid-fade, the change starts from where the character was heading.
  own.from = own.to;
  own.to = next;
  own.fade = std::max(fade, 0.0f);
  own.faded = 0.0f;
  return true;
}

void AnimationPlayer::pause_on(const Scene& scene, ObjectId object) {
  const auto found = owned_.find(character_of(scene, object));
  if (found != owned_.end() && found->second.to) found->second.to->state.playing = false;
}

bool AnimationPlayer::mix_on(const Scene& scene, ObjectId object, const std::string& name, float weight, bool loop,
                             float speed) {
  const auto index = find(name);
  if (!index) return false;
  Own& own = owned_[character_of(scene, object)];
  // The same clip again only changes how it is mixed; another one starts from its beginning.
  const bool another = !own.layer || own.layer->clip != *index;
  if (another) {
    own.layer = Track{};
    own.layer->clip = *index;
  }
  start(own.layer->state, (*clips_)[*index].duration, loop, speed, another);
  own.layer_weight = std::clamp(weight, 0.0f, kWhole);
  return true;
}

void AnimationPlayer::unmix_on(const Scene& scene, ObjectId object) {
  const auto found = owned_.find(character_of(scene, object));
  if (found != owned_.end()) found->second.layer.reset();
}

void AnimationPlayer::follow_scene(const Scene& scene, ObjectId object, float fade) {
  const auto found = owned_.find(character_of(scene, object));
  if (found == owned_.end() || !found->second.to) return;
  Own& own = found->second;
  own.from = own.to;
  own.to.reset();
  own.fade = std::max(fade, 0.0f);
  own.faded = 0.0f;
}

std::optional<std::string> AnimationPlayer::clip_on(const Scene& scene, ObjectId object) const {
  const Track* track = track_on(scene, object);
  return track != nullptr ? std::optional<std::string>((*clips_)[track->clip].name) : std::nullopt;
}

bool AnimationPlayer::playing_on(const Scene& scene, ObjectId object) const {
  const Track* track = track_on(scene, object);
  return track != nullptr && track->state.playing;
}

float AnimationPlayer::time_on(const Scene& scene, ObjectId object) const {
  const Track* track = track_on(scene, object);
  return track != nullptr ? track->state.time : 0.0f;
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

void AnimationPlayer::start(State& state, float duration, bool loop, float speed, bool restart) {
  const bool backwards = speed < 0.0f;
  // A clip played once that already reached its end has nowhere to go: it starts over.
  const bool ended = !loop && (backwards ? state.time <= 0.0f : state.time >= duration);
  if (restart || ended || !state.posing) state.time = backwards ? duration : 0.0f;
  state.loop = loop;
  state.speed = speed;
  state.playing = true;
  state.posing = true;
}

bool AnimationPlayer::play(const std::string& name, bool loop, float speed, bool restart) {
  const auto index = find(name);
  if (!index) return false;
  start(states_[*index], (*clips_)[*index].duration, loop, speed, restart);
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
