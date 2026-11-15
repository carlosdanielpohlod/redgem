#pragma once

// The music goes down while someone speaks: while a sound of the voice group plays, the music
// group falls to a lower volume, and it comes back once every voice is quiet (Audio.ducking).

#include "audio/mix_groups.hpp"

namespace redgem {

inline constexpr float kDefaultDuckedVolume = 0.4f;  // of the music's own, while a voice plays
inline constexpr float kDefaultDuckAttack = 0.15f;   // seconds the music takes to go down
inline constexpr float kDefaultDuckRelease = 0.6f;   // seconds it takes to come back
inline constexpr float kNoDucking = kFullVolume;     // a ducked volume that leaves the music as it is

struct MusicDucking {
  float volume = kDefaultDuckedVolume;  // from 0 (silent) to 1 (no ducking)
  float attack_seconds = kDefaultDuckAttack;
  float release_seconds = kDefaultDuckRelease;
};

bool valid_music_ducking(const MusicDucking& ducking);

// The gain over the music group, moved on every frame: linearly down to the ducked volume over
// the attack while a voice plays, and up to full over the release once none does.
class MusicDucker {
 public:
  // Ducking that is not valid is ignored (the one set stays).
  bool set(const MusicDucking& ducking);
  // Moves the gain on by `seconds` and returns it.
  float update(bool voice_playing, float seconds);
  float gain() const { return gain_; }

 private:
  float step(float target, float seconds) const;

  MusicDucking ducking_;
  float gain_ = kFullVolume;
};

}  // namespace redgem
