#include "audio/audio.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>

#include "core/safe_path.hpp"

namespace redgem {
namespace {

struct AudioVector {
  double x, y, z;
};
constexpr double kMinimumAudioAxis = 0.000001;  // reject nearly parallel listener directions
constexpr std::size_t kStereoChannels = 2;
constexpr float kMonoShare = 0.5f;
constexpr double kSmoothFadeCubic = 3.0;
constexpr double kSmoothFadeQuadratic = 2.0;
constexpr std::uint64_t kMaximumSoundHandle = std::numeric_limits<std::int32_t>::max();

bool finite_audio_vector(Vec3 value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
AudioVector audio_vector(Vec3 value) { return {value.x, value.y, value.z}; }
double audio_length(const AudioVector& value) { return std::hypot(value.x, value.y, value.z); }
AudioVector audio_normalized(const AudioVector& value) {
  const double length = audio_length(value);
  return {value.x / length, value.y / length, value.z / length};
}
AudioVector audio_cross(const AudioVector& a, const AudioVector& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double audio_dot(const AudioVector& a, const AudioVector& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
std::optional<AudioVector> audio_right(const AudioListener& listener) {
  if (!finite_audio_vector(listener.forward)) return std::nullopt;
  if (!finite_audio_vector(listener.up)) return std::nullopt;
  const auto forward = audio_vector(listener.forward);
  const auto up = audio_vector(listener.up);
  if (audio_length(forward) == 0.0) return std::nullopt;
  if (audio_length(up) == 0.0) return std::nullopt;
  const auto right = audio_cross(audio_normalized(forward), audio_normalized(up));
  if (audio_length(right) <= kMinimumAudioAxis) return std::nullopt;
  return audio_normalized(right);
}
bool valid_audio_levels(const SpatialSound& sound) {
  if (!std::isfinite(sound.volume)) return false;
  if (!std::isfinite(sound.pitch)) return false;
  const bool safe_pitch = sound.pitch >= kMinSpatialPitch && sound.pitch <= kMaxSpatialPitch;
  return sound.volume >= 0.0f && safe_pitch;
}
bool valid_audio_distances(const SpatialSound& sound) {
  if (!std::isfinite(sound.reference_distance)) return false;
  if (!std::isfinite(sound.max_distance)) return false;
  return sound.reference_distance > 0.0f && sound.max_distance > sound.reference_distance;
}
AudioVector audio_offset(Vec3 source, Vec3 listener) {
  return {static_cast<double>(source.x) - listener.x, static_cast<double>(source.y) - listener.y,
          static_cast<double>(source.z) - listener.z};
}
double audio_attenuation(double distance, const SpatialSound& sound) {
  if (distance <= sound.reference_distance) return 1.0;
  if (distance >= sound.max_distance) return 0.0;
  const double reach = static_cast<double>(sound.max_distance) - sound.reference_distance;
  const double fade = (sound.max_distance - distance) / reach;
  // Inverse-distance amplitude, with a smooth fade to zero at the authored outer radius.
  return sound.reference_distance / distance * fade * fade * (kSmoothFadeCubic - kSmoothFadeQuadratic * fade);
}

}  // namespace

bool valid_audio_listener(const AudioListener& listener) {
  if (!finite_audio_vector(listener.position)) return false;
  return audio_right(listener).has_value();
}
bool valid_spatial_sound(const SpatialSound& sound) {
  if (!finite_audio_vector(sound.position)) return false;
  return valid_audio_levels(sound) && valid_audio_distances(sound);
}
std::optional<SpatialMix> spatial_sound_mix(const AudioListener& listener, const SpatialSound& sound) {
  if (!valid_spatial_sound(sound)) return std::nullopt;
  if (!sound.positional) return SpatialMix{sound.volume};  // (centered)
  if (!finite_audio_vector(listener.position)) return std::nullopt;
  const auto right = audio_right(listener);
  if (!right) return std::nullopt;
  const auto offset = audio_offset(sound.position, listener.position);
  const double distance = audio_length(offset);
  const double pan = distance > 0.0 ? std::clamp(audio_dot(offset, *right) / distance, -1.0, 1.0) : 0.0;
  return SpatialMix{static_cast<float>(sound.volume * audio_attenuation(distance, sound)), static_cast<float>(pan)};
}
void spatial_mono_frames(float* samples, unsigned frames) {
  if (samples == nullptr) return;
  for (unsigned frame = 0; frame < frames; ++frame) {
    float* stereo = samples + static_cast<std::size_t>(frame) * kStereoChannels;
    const float mono = stereo[0] * kMonoShare + stereo[1] * kMonoShare;
    stereo[0] = mono;
    stereo[1] = mono;
  }
}
SpatialSound flat_sound(const FlatSound& sound) {
  SpatialSound flat;
  flat.volume = sound.volume;
  flat.pitch = sound.pitch;
  flat.loop = sound.loop;
  flat.group = sound.group;
  flat.priority = sound.priority;
  flat.positional = false;
  return flat;
}

SoundSourceId SoundPool::play(const std::string& name, const SpatialSound& sound) {
  const auto mix = mix_of(sound);
  if (!mix) return kNoSoundSource;
  if (next_ > kMaximumSoundHandle) return kNoSoundSource;
  reap();  // (a sound that ended gives its place back)
  const auto place = place_for(sound);
  if (place == kNoSoundSource) return kNoSoundSource;
  const auto id = static_cast<SoundSourceId>(next_);
  if (!driver_.start(id, name, sound, *mix)) return kNoSoundSource;  // (the one it would replace goes on)
  if (place) stop(*place);
  sounds_.emplace(id, sound);
  ++next_;
  return id;
}
std::optional<SoundSourceId> SoundPool::place_for(const SpatialSound& sound) const {
  if (sounds_.size() < kMaxSounds) return std::nullopt;
  const SoundSourceId victim = weakest();
  if (sound.priority < sounds_.at(victim).priority) return kNoSoundSource;
  return victim;
}
SoundSourceId SoundPool::weakest() const {
  const auto order = [this](const auto& entry) {
    const auto mix = mix_of(entry.second);
    return std::make_tuple(entry.second.priority, mix ? mix->volume : 0.0f, entry.first);
  };
  const auto found = std::min_element(sounds_.begin(), sounds_.end(),
                                      [&](const auto& a, const auto& b) { return order(a) < order(b); });
  return found->first;
}
bool SoundPool::set_listener(const AudioListener& listener) {
  if (!valid_audio_listener(listener)) return false;
  listener_ = listener;
  refresh();
  return true;
}
bool SoundPool::move(SoundSourceId id, Vec3 position) {
  if (!finite_audio_vector(position)) return false;
  const auto found = sounds_.find(id);
  if (found == sounds_.end()) return false;
  found->second.position = position;
  const auto mix = mix_of(found->second);
  if (mix) driver_.mix(id, *mix);
  return mix.has_value();
}
bool SoundPool::set_levels(SoundSourceId id, const SoundLevels& levels) {
  if (!playing(id)) return false;  // (one that ended is not changed, even before it is reaped)
  const auto found = sounds_.find(id);
  SpatialSound changed = found->second;
  changed.volume = levels.volume.value_or(changed.volume);
  changed.pitch = levels.pitch.value_or(changed.pitch);
  const auto mix = mix_of(changed);
  if (!mix) return false;
  found->second = changed;
  driver_.pitch(id, changed.pitch);
  driver_.mix(id, *mix);
  return true;
}
void SoundPool::set_group_volume(MixGroup group, float volume) {
  group_volumes_[mix_group_index(group)] = volume;
  refresh();
}
void SoundPool::set_group_gain(MixGroup group, float gain) {
  float& current = group_gains_[mix_group_index(group)];
  if (current == gain) return;
  current = gain;
  refresh();
}
bool SoundPool::group_playing(MixGroup group) const {
  for (const auto& [id, sound] : sounds_) {
    if (sound.group != group) continue;
    if (driver_.playing(id)) return true;
  }
  return false;
}
bool SoundPool::playing(SoundSourceId id) const { return sounds_.count(id) != 0 && driver_.playing(id); }
bool SoundPool::stop(SoundSourceId id) {
  const auto found = sounds_.find(id);
  if (found == sounds_.end()) return false;
  driver_.stop(id);
  sounds_.erase(found);
  return true;
}
void SoundPool::clear() {
  while (!sounds_.empty()) stop(sounds_.begin()->first);
}
// A sound's mix, its group's volume and gain applied.
std::optional<SpatialMix> SoundPool::mix_of(const SpatialSound& sound) const {
  auto mix = spatial_sound_mix(listener_, sound);
  const std::size_t group = mix_group_index(sound.group);
  if (mix) mix->volume *= group_volumes_[group] * group_gains_[group];
  return mix;
}
void SoundPool::refresh() {
  for (const auto& [id, sound] : sounds_) {
    const auto mix = mix_of(sound);
    if (mix) driver_.mix(id, *mix);
  }
}
void SoundPool::update() {
  for (const auto& [id, sound] : sounds_) driver_.update(id);
  reap();
  refresh();
}
void SoundPool::reap() {
  for (auto it = sounds_.begin(); it != sounds_.end();) {
    if (driver_.playing(it->first)) {
      ++it;
      continue;
    }
    driver_.stop(it->first);
    it = sounds_.erase(it);
  }
}

namespace {

constexpr const char* kSoundExtensions[] = {".wav", ".ogg", ".mp3", ".flac"};

using GroupName = std::pair<MixGroup, const char*>;
constexpr GroupName kGroupNames[] = {
    {MixGroup::Music, "music"}, {MixGroup::Effects, "effects"}, {MixGroup::Voice, "voice"}};

}  // namespace

const char* mix_group_name(MixGroup group) {
  for (const auto& [known, name] : kGroupNames) {
    if (known == group) return name;
  }
  return kGroupNames[0].second;  // (every group has its name)
}

std::optional<MixGroup> mix_group_from_name(const std::string& name) {
  for (const auto& [group, known] : kGroupNames) {
    if (name == known) return group;
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> find_sound_file(const std::filesystem::path& sounds_dir, const std::string& name) {
  // (A name that reaches outside the sounds folder finds nothing.)
  return find_asset_file(sounds_dir, name, kSoundExtensions);
}

}  // namespace redgem
