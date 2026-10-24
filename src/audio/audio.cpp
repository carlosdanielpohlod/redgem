#include "audio/audio.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <variant>

#include "core/log.hpp"
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
SoundSourceId SpatialSoundPool::play(const std::string& name, const SpatialSound& sound) {
  const auto mix = spatial_sound_mix(listener_, sound);
  if (!mix) return kNoSoundSource;
  if (next_ > kMaximumSoundHandle) return kNoSoundSource;
  update();
  if (sounds_.size() >= kMaxSpatialSounds) return kNoSoundSource;
  const auto id = static_cast<SoundSourceId>(next_);
  if (!driver_.start(id, name, sound, *mix)) return kNoSoundSource;
  sounds_.emplace(id, sound);
  ++next_;
  return id;
}
bool SpatialSoundPool::set_listener(const AudioListener& listener) {
  if (!valid_audio_listener(listener)) return false;
  listener_ = listener;
  refresh();
  return true;
}
bool SpatialSoundPool::move(SoundSourceId id, Vec3 position) {
  if (!finite_audio_vector(position)) return false;
  const auto found = sounds_.find(id);
  if (found == sounds_.end()) return false;
  found->second.position = position;
  const auto mix = spatial_sound_mix(listener_, found->second);
  if (mix) driver_.mix(id, *mix);
  return mix.has_value();
}
bool SpatialSoundPool::playing(SoundSourceId id) const { return sounds_.count(id) != 0 && driver_.playing(id); }
bool SpatialSoundPool::stop(SoundSourceId id) {
  const auto found = sounds_.find(id);
  if (found == sounds_.end()) return false;
  driver_.stop(id);
  sounds_.erase(found);
  return true;
}
void SpatialSoundPool::clear() {
  while (!sounds_.empty()) stop(sounds_.begin()->first);
}
void SpatialSoundPool::refresh() {
  for (const auto& [id, sound] : sounds_) {
    const auto mix = spatial_sound_mix(listener_, sound);
    if (mix) driver_.mix(id, *mix);
  }
}
void SpatialSoundPool::update() {
  for (auto it = sounds_.begin(); it != sounds_.end();) {
    driver_.update(it->first);
    if (driver_.playing(it->first)) {
      ++it;
      continue;
    }
    driver_.stop(it->first);
    it = sounds_.erase(it);
  }
  refresh();
}

namespace {

constexpr const char* kSoundExtensions[] = {".wav", ".ogg", ".mp3", ".flac"};

using SpatialVoice = std::variant<Sound, Music>;
AudioStream spatial_stream(const SpatialVoice& voice) {
  return std::visit([](const auto& source) { return source.stream; }, voice);
}
void unload_spatial_voice(Sound sound) { UnloadSound(sound); }
void unload_spatial_voice(Music music) { UnloadMusicStream(music); }
std::optional<SpatialVoice> load_spatial_loop(const std::filesystem::path& file) {
  Music music = LoadMusicStream(file.string().c_str());
  const bool valid = IsMusicValid(music) && IsAudioStreamValid(music.stream);
  if (valid) return SpatialVoice{music};
  UnloadMusicStream(music);
  return std::nullopt;
}
std::optional<SpatialVoice> load_spatial_voice(const std::filesystem::path& file, bool loop) {
  if (loop) return load_spatial_loop(file);
  // Buffered effects play through their final frame; a short nonlooping music stream may end while refilling.
  Sound sound = LoadSound(file.string().c_str());
  if (IsSoundValid(sound)) return SpatialVoice{sound};
  UnloadSound(sound);
  return std::nullopt;
}
void spatial_mono_callback(void* buffer, unsigned frames) { spatial_mono_frames(static_cast<float*>(buffer), frames); }
void apply_spatial_mix(AudioStream stream, SpatialMix mix) {
  constexpr float kPanCenter = 0.5f;  // raylib uses 0 for right and 1 for left
  SetAudioStreamVolume(stream, mix.volume);
  SetAudioStreamPan(stream, kPanCenter * (1.0f - mix.pan));
}
void prepare_spatial_stream(AudioStream stream, const SpatialSound& sound, SpatialMix mix) {
  AttachAudioStreamProcessor(stream, spatial_mono_callback);
  SetAudioStreamPitch(stream, sound.pitch);
  apply_spatial_mix(stream, mix);
}
class RaylibSpatialAudio final : public SpatialAudioDriver {
 public:
  RaylibSpatialAudio(const std::filesystem::path& dir, const bool& ready) : dir_(dir), ready_(ready) {}
  bool start(SoundSourceId id, const std::string& name, const SpatialSound& sound, SpatialMix mix) override {
    if (!ready_) return false;
    const auto file = find_sound_file(dir_, name);
    if (!file) return false;
    auto voice = load_spatial_voice(*file, sound.loop);
    if (!voice) return false;
    const auto stream = spatial_stream(*voice);
    prepare_spatial_stream(stream, sound, mix);
    streams_.emplace(id, *voice);
    PlayAudioStream(stream);
    return true;
  }
  void update(SoundSourceId id) override {
    if (auto* music = std::get_if<Music>(&streams_.at(id))) UpdateMusicStream(*music);
  }
  bool playing(SoundSourceId id) const override { return IsAudioStreamPlaying(spatial_stream(streams_.at(id))); }
  void mix(SoundSourceId id, SpatialMix mix) override { apply_spatial_mix(spatial_stream(streams_.at(id)), mix); }
  void stop(SoundSourceId id) override {
    const auto found = streams_.find(id);
    if (found == streams_.end()) return;
    const auto stream = spatial_stream(found->second);
    StopAudioStream(stream);
    DetachAudioStreamProcessor(stream, spatial_mono_callback);
    std::visit([](const auto& voice) { unload_spatial_voice(voice); }, found->second);
    streams_.erase(found);
  }

 private:
  const std::filesystem::path& dir_;
  const bool& ready_;
  std::map<SoundSourceId, SpatialVoice> streams_;
};

class RaylibAudio final : public AudioSystem {
 public:
  RaylibAudio() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) log::warning("no audio device: the game runs silently");
  }

  ~RaylibAudio() override {
    spatial_.clear();
    stop_music();
    for (auto& [name, sound] : sounds_) UnloadSound(sound);
    if (ready_) CloseAudioDevice();
  }

  bool play_sound(const std::string& name, float volume, float pitch) override {
    if (!ready_) return false;
    auto it = sounds_.find(name);
    if (it == sounds_.end()) {
      const auto file = resolve(name);
      if (!file) return false;
      Sound sound = LoadSound(file->string().c_str());
      if (!IsSoundValid(sound)) return false;
      it = sounds_.emplace(name, sound).first;
    }
    SetSoundVolume(it->second, volume);
    SetSoundPitch(it->second, pitch);
    PlaySound(it->second);
    return true;
  }

  SoundSourceId play_spatial_sound(const std::string& name, const SpatialSound& sound) override {
    return spatial_.play(name, sound);
  }
  bool set_listener(const AudioListener& listener) override { return spatial_.set_listener(listener); }
  bool move_sound(SoundSourceId id, Vec3 position) override { return spatial_.move(id, position); }
  bool sound_playing(SoundSourceId id) const override { return spatial_.playing(id); }
  bool stop_sound(SoundSourceId id) override { return spatial_.stop(id); }
  void stop_spatial_sounds() override { spatial_.clear(); }

  bool play_music(const std::string& name, bool loop, float volume) override {
    if (!ready_) return false;
    stop_music();
    const auto file = resolve(name);
    if (!file) return false;
    music_ = LoadMusicStream(file->string().c_str());
    if (!IsMusicValid(music_)) return false;
    music_.looping = loop;
    SetMusicVolume(music_, volume);
    PlayMusicStream(music_);
    has_music_ = true;
    return true;
  }

  void stop_music() override {
    if (!has_music_) return;
    StopMusicStream(music_);
    UnloadMusicStream(music_);
    has_music_ = false;
  }

  void set_music_pitch(float pitch) override {
    if (has_music_) SetMusicPitch(music_, pitch);
  }

  void set_music_volume(float volume) override {
    if (has_music_) SetMusicVolume(music_, volume);
  }

  void set_master_volume(float volume) override {
    if (ready_) SetMasterVolume(volume);
  }

  void update() override {
    if (has_music_) UpdateMusicStream(music_);
    spatial_.update();
  }

  void set_sounds_dir(const std::filesystem::path& dir) override {
    if (dir == sounds_dir_) return;
    spatial_.clear();
    for (auto& [name, sound] : sounds_) UnloadSound(sound);
    sounds_.clear();
    missing_.clear();
    sounds_dir_ = dir;
  }

 private:
  std::optional<std::filesystem::path> resolve(const std::string& name) {
    auto file = find_sound_file(sounds_dir_, name);
    if (file) return file;
    // (Said once for each name: a game asks for the same sound every time it plays it.)
    if (missing_.insert(name).second) log::warning("sound '" + name + "' not found in " + sounds_dir_.string());
    return std::nullopt;
  }

  bool ready_ = false;
  std::filesystem::path sounds_dir_;
  std::map<std::string, Sound> sounds_;
  std::set<std::string> missing_;
  Music music_{};
  bool has_music_ = false;
  RaylibSpatialAudio spatial_driver_{sounds_dir_, ready_};
  SpatialSoundPool spatial_{spatial_driver_};
};

}  // namespace

std::optional<std::filesystem::path> find_sound_file(const std::filesystem::path& sounds_dir, const std::string& name) {
  // (A name that reaches outside the sounds folder finds nothing.)
  return find_asset_file(sounds_dir, name, kSoundExtensions);
}

std::unique_ptr<AudioSystem> create_raylib_audio() { return std::make_unique<RaylibAudio>(); }

}  // namespace redgem
