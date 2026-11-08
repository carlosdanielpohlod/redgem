#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "audio/mix_groups.hpp"
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
  // False for a flat sound: heard the same wherever the listener is, centered, in stereo. The
  // position and distances are then not used.
  bool positional = true;
  MixGroup group = MixGroup::Effects;
};

// A sound with no place in the world (Audio.play): a looping hum, an alarm, a voice line.
struct FlatSound {
  float volume = kFullVolume;
  float pitch = kAsRecorded;
  bool loop = false;
  MixGroup group = MixGroup::Effects;
};
SpatialSound flat_sound(const FlatSound& sound);

// What Audio.set changes of a sound that plays; nothing stays as it is.
struct SoundLevels {
  std::optional<float> volume;
  std::optional<float> pitch;
};

struct SpatialMix {
  float volume = 0.0f;
  float pan = 0.0f;  // -1 left, 0 center, +1 right in the listener's orientation
};

bool valid_audio_listener(const AudioListener& listener);
bool valid_spatial_sound(const SpatialSound& sound);
// A flat sound's mix is its own volume, centered.
std::optional<SpatialMix> spatial_sound_mix(const AudioListener& listener, const SpatialSound& sound);
// Raylib processors receive interleaved float stereo after conversion to the device mix format.
void spatial_mono_frames(float* samples, unsigned frames);

class SoundDriver {
 public:
  virtual ~SoundDriver() = default;
  // False must leave no stream behind. Apply the initial mix and pitch before playback begins.
  virtual bool start(SoundSourceId id, const std::string& name, const SpatialSound& sound, SpatialMix mix) = 0;
  virtual void update(SoundSourceId id) = 0;
  virtual bool playing(SoundSourceId id) const = 0;
  virtual void mix(SoundSourceId id, SpatialMix mix) = 0;
  virtual void pitch(SoundSourceId id, float pitch) = 0;
  virtual void stop(SoundSourceId id) = 0;
};

// Every sound a game holds a handle to, flat or positional, in one id space. Logical voice
// ownership is independent of the audio device, so lifecycle controls are testable.
class SoundPool {
 public:
  explicit SoundPool(SoundDriver& driver) : driver_(driver) {}
  ~SoundPool() { clear(); }
  SoundPool(const SoundPool&) = delete;
  SoundPool& operator=(const SoundPool&) = delete;

  SoundSourceId play(const std::string& name, const SpatialSound& sound);
  bool set_listener(const AudioListener& listener);
  bool move(SoundSourceId id, Vec3 position);
  // False for a sound that no longer plays, or levels a sound cannot have (it keeps its own).
  bool set_levels(SoundSourceId id, const SoundLevels& levels);
  // The volume every sound of the group is heard at, times its own.
  void set_group_volume(MixGroup group, float volume);
  bool playing(SoundSourceId id) const;
  bool stop(SoundSourceId id);
  void clear();
  void update();

 private:
  std::optional<SpatialMix> mix_of(const SpatialSound& sound) const;
  void refresh();
  void reap();  // lets go of the sounds that ended
  SoundDriver& driver_;
  AudioListener listener_;
  GroupVolumes group_volumes_ = kFullGroupVolumes;
  std::map<SoundSourceId, SpatialSound> sounds_;
  std::uint64_t next_ = 1;  // never reuse handles, including across game changes
};

}  // namespace redgem
