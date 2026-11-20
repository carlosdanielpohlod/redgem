#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "audio/audio_effects.hpp"
#include "audio/envelope_follower.hpp"
#include "audio/mix_group_tree.hpp"
#include "audio/doppler.hpp"
#include "audio/mix_groups.hpp"
#include "audio/occlusion.hpp"
#include "core/math.hpp"

namespace redgem {

using SoundSourceId = std::uint32_t;
inline constexpr SoundSourceId kNoSoundSource = 0;
// Sounds with ids at once, flat and positional together: it bounds the streams and decoders. Past
// it, a new sound takes the place of the weakest one playing (see SoundPool::play).
inline constexpr std::size_t kMaxSounds = 128;
// Which sound keeps its place when there is no room: the higher priority.
inline constexpr int kLowestSoundPriority = 0;
inline constexpr int kHighestSoundPriority = 255;
inline constexpr int kDefaultSoundPriority = 128;

// Bound the resampling rate before it reaches the audio backend.
inline constexpr float kMinSpatialPitch = 0.01f;
inline constexpr float kMaxSpatialPitch = 8.0f;
inline constexpr float kDefaultSoundReach = 50.0f;  // meters: default outer audible radius

struct AudioListener {
  Vec3 position{};
  Vec3 forward{0.0f, 0.0f, -1.0f};
  Vec3 up{0.0f, 1.0f, 0.0f};
  Vec3 velocity{};  // m/s: how the ears move, for the Doppler effect
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
  int priority = kDefaultSoundPriority;
  // Whether walls between it and the listener muffle it (Audio.play_at's occlusion:).
  bool occludable = true;
  // How far behind a wall it is, from kOpen to kBehindWall, and whether the pool has looked for
  // its walls yet (until it has, the first answer counts at once, with no fade): the pool keeps them.
  float occlusion = kOpen;
  bool occlusion_found = false;
  Vec3 velocity{};  // m/s: how it moves, for the Doppler effect
  // What its pitch is heard at, times, from the Doppler effect: the pool keeps it.
  float doppler = kNoShift;
};

// A sound with no place in the world (Audio.play): a looping hum, an alarm, a voice line.
struct FlatSound {
  float volume = kFullVolume;
  float pitch = kAsRecorded;
  bool loop = false;
  MixGroup group = MixGroup::Effects;
  int priority = kDefaultSoundPriority;
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
inline constexpr std::size_t kDeviceChannels = 2;
void spatial_mono_frames(float* samples, unsigned frames);

class SoundDriver {
 public:
  virtual ~SoundDriver() = default;
  // False must leave no stream behind. Apply the initial mix, pitch and effects (its group's)
  // before playback begins.
  virtual bool start(SoundSourceId id, const std::string& name, const SpatialSound& sound, SpatialMix mix,
                     const VoiceEffects& effects) = 0;
  virtual void update(SoundSourceId id) = 0;
  virtual bool playing(SoundSourceId id) const = 0;
  virtual void mix(SoundSourceId id, SpatialMix mix) = 0;
  virtual void pitch(SoundSourceId id, float pitch) = 0;
  // The effects of its group when they change (a backend with none ignores them).
  virtual void effects(SoundSourceId, const VoiceEffects&) {}
  // Ends every group's reverb and its tail, when the game's groups end.
  virtual void close_reverbs() {}
  virtual void stop(SoundSourceId id) = 0;
  // Measures how loud a sound that started is from now on: one of the voice group or a group
  // under it (a backend that cannot ignores it).
  virtual void measure(SoundSourceId) {}
  // How loud the source's own samples were in the last block it played (0 to 1, before its volume):
  // for the sounds it measures, 0 for the others and where nothing measures.
  virtual float level(SoundSourceId) const { return kSilence; }
};

// Every sound a game holds a handle to, flat or positional, in one id space. Logical voice
// ownership is independent of the audio device, so lifecycle controls are testable.
class SoundPool {
 public:
  // Its own groups, or the audio's, which it reads (after a change to them, refresh and
  // refresh_effects).
  explicit SoundPool(SoundDriver& driver) : driver_(driver), groups_(own_groups_) {}
  SoundPool(SoundDriver& driver, MixGroupTree& groups) : driver_(driver), groups_(groups) {}
  ~SoundPool() { clear(); }
  SoundPool(const SoundPool&) = delete;
  SoundPool& operator=(const SoundPool&) = delete;

  // With kMaxSounds playing, the new sound takes the place of the weakest one (the lowest
  // priority, then the quietest as heard, then the oldest) when its priority is at least that
  // one's: that one stops and its handle goes. Otherwise it does not play.
  SoundSourceId play(const std::string& name, const SpatialSound& sound);
  bool set_listener(const AudioListener& listener);
  // A new velocity too, when given.
  bool move(SoundSourceId id, Vec3 position, std::optional<Vec3> velocity = std::nullopt);
  // How strong the Doppler effect is: 0 off, 1 real, up to kMostDoppler; false out of that range.
  bool set_doppler(float scale);
  // False for a sound that no longer plays, or levels a sound cannot have (it keeps its own).
  bool set_levels(SoundSourceId id, const SoundLevels& levels);
  // The player's volume of one of the engine's groups: every sound of it and under it is heard at
  // it, times its own.
  void set_group_volume(MixGroup group, float volume);
  // A gain the engine puts over a group, apart from the player's volume: the music ducked under a
  // voice.
  void set_group_gain(MixGroup group, float gain);
  // Whether any sound of the group, or of a group under it, still plays and is heard (not at
  // volume 0, out of reach or in a group the player muted).
  bool group_heard(MixGroup group) const;
  // Every sound mixed again, and given its effects again: after the groups changed.
  void refresh();
  void refresh_effects();
  void close_reverbs() { driver_.close_reverbs(); }
  // How a sound behind a wall is heard (false for settings that are not valid), and what says
  // whether a wall is in the way: none, and no sound is ever behind one.
  bool set_occlusion(const OcclusionSettings& settings);
  void set_occlusion_query(OcclusionQuery query);
  bool playing(SoundSourceId id) const;
  // How loud a sound of the voice group is as it plays, smoothed (see EnvelopeFollower); 0 for
  // other sounds, and once it ended.
  float loudness(SoundSourceId id) const;
  // The loudness of every voice moves on by `seconds` toward what its driver measured.
  void follow(float seconds);
  bool stop(SoundSourceId id);
  void clear();
  // Feeds the sounds, lets go of those that ended, moves their occlusions on by `seconds` and
  // mixes them again.
  void update(float seconds);

 private:
  std::optional<SpatialMix> mix_of(const SpatialSound& sound) const;
  bool heard(SoundSourceId id, const SpatialSound& sound) const;
  // The sound that gives its place to `sound` when the pool is full: nullopt when there is room,
  // and kNoSoundSource when every sound playing is stronger.
  std::optional<SoundSourceId> place_for(const SpatialSound& sound) const;
  SoundSourceId start_sound(const std::string& name, const SpatialSound& sound, SpatialMix mix,
                            std::optional<SoundSourceId> place);
  SoundSourceId weakest() const;
  VoiceEffects effects_of(const SpatialSound& sound) const;
  std::optional<bool> blocked(const SpatialSound& sound) const;
  bool looks_for_walls() const;
  float shift_of(const SpatialSound& sound) const;
  void follow_doppler(SoundSourceId id, SpatialSound& sound);
  void occlude(float seconds);
  void reap();  // lets go of the sounds that ended
  void forget(std::map<SoundSourceId, SpatialSound>::iterator sound);
  void keep(SoundSourceId id, const SpatialSound& sound);  // a sound that started
  SoundDriver& driver_;
  MixGroupTree own_groups_;
  MixGroupTree& groups_;
  AudioListener listener_;
  OcclusionSettings occlusion_;
  OcclusionQuery walls_;
  float doppler_scale_ = kRealDoppler;
  std::map<SoundSourceId, SpatialSound> sounds_;
  std::map<SoundSourceId, EnvelopeFollower> loudness_;  // of the sounds of the voice group
  std::uint64_t next_ = 1;                              // never reuse handles, including across game changes
};

}  // namespace redgem
