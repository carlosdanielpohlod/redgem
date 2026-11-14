#include "audio/ducking.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

constexpr float kSilentGain = 0.0f;
// Seconds: a shorter attack or release is none (the gain jumps; a denormal one would make the
// rate infinite).
constexpr float kShortestDuckFade = 0.001f;

bool valid_duck_seconds(float seconds) { return std::isfinite(seconds) && seconds >= 0.0f; }
bool valid_ducked_volume(float volume) {
  return std::isfinite(volume) && volume >= kSilentGain && volume <= kNoDucking;
}
// No fade to make: one too short to take time, or no distance between full and ducked.
bool ducks_at_once(float duration, float span) { return duration < kShortestDuckFade || span <= kSilentGain; }

}  // namespace

bool valid_music_ducking(const MusicDucking& ducking) {
  if (!valid_ducked_volume(ducking.volume)) return false;
  return valid_duck_seconds(ducking.attack_seconds) && valid_duck_seconds(ducking.release_seconds);
}

bool MusicDucker::set(const MusicDucking& ducking) {
  if (!valid_music_ducking(ducking)) return false;
  ducking_ = ducking;
  return true;
}

float MusicDucker::update(bool voice_playing, float seconds) {
  const float target = voice_playing ? ducking_.volume : kFullVolume;
  const float change = std::min(step(target, seconds), std::fabs(target - gain_));
  gain_ += target < gain_ ? -change : change;
  return gain_;
}

// How far the gain moves in `seconds`: the whole way from full to the ducked volume over the
// attack (going down) or the release (coming back).
float MusicDucker::step(float target, float seconds) const {
  const float duration = target < gain_ ? ducking_.attack_seconds : ducking_.release_seconds;
  const float span = kFullVolume - ducking_.volume;
  if (ducks_at_once(duration, span)) return kFullVolume;  // (more than any gain can move)
  return span / duration * std::max(seconds, kSilentGain);
}

}  // namespace redgem
