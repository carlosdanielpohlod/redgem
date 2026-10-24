#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "core/math.hpp"

namespace redgem {

using SoundSourceId = std::uint32_t;
inline constexpr SoundSourceId kNoSoundSource = 0;
inline constexpr std::size_t kMaxSpatialSounds = 64;  // bound simultaneous stream/decoder resources

// Bound the resampling rate before it reaches the audio backend.
inline constexpr float kMinSpatialPitch = 0.01f;
inline constexpr float kMaxSpatialPitch = 8.0f;
inline constexpr float kDefaultSoundReach = 50.0f;  // meters: default outer audible radius

struct AudioListener {
  Vec3 position{};
  Vec3 forward{0.0f, 0.0f, -1.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
};

struct SpatialSound {
  Vec3 position{};
  float volume = 1.0f;
  float pitch = 1.0f;
  float reference_distance = 1.0f;          // meters: full-volume radius
  float max_distance = kDefaultSoundReach;  // meters: smooth fade reaches silence here
  bool loop = false;
};

struct SpatialMix {
  float volume = 0.0f;
  float pan = 0.0f;  // -1 left, 0 center, +1 right in the listener's orientation
};

bool valid_audio_listener(const AudioListener& listener);
bool valid_spatial_sound(const SpatialSound& sound);
std::optional<SpatialMix> spatial_sound_mix(const AudioListener& listener, const SpatialSound& sound);
// Raylib processors receive interleaved float stereo after conversion to the device mix format.
void spatial_mono_frames(float* samples, unsigned frames);

class SpatialAudioDriver {
 public:
  virtual ~SpatialAudioDriver() = default;
  // False must leave no stream behind. Apply the initial mix before playback begins.
  virtual bool start(SoundSourceId id, const std::string& name, const SpatialSound& sound, SpatialMix mix) = 0;
  virtual void update(SoundSourceId id) = 0;
  virtual bool playing(SoundSourceId id) const = 0;
  virtual void mix(SoundSourceId id, SpatialMix mix) = 0;
  virtual void stop(SoundSourceId id) = 0;
};

// Logical voice ownership is independent of the audio device, so lifecycle controls are testable.
class SpatialSoundPool {
 public:
  explicit SpatialSoundPool(SpatialAudioDriver& driver) : driver_(driver) {}
  ~SpatialSoundPool() { clear(); }
  SpatialSoundPool(const SpatialSoundPool&) = delete;
  SpatialSoundPool& operator=(const SpatialSoundPool&) = delete;

  SoundSourceId play(const std::string& name, const SpatialSound& sound);
  bool set_listener(const AudioListener& listener);
  bool move(SoundSourceId id, Vec3 position);
  bool playing(SoundSourceId id) const;
  bool stop(SoundSourceId id);
  void clear();
  void update();

 private:
  void refresh();
  SpatialAudioDriver& driver_;
  AudioListener listener_;
  std::map<SoundSourceId, SpatialSound> sounds_;
  std::uint64_t next_ = 1;  // never reuse handles, including across game changes
};

}  // namespace redgem
