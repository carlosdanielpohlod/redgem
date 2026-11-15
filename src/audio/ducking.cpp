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

// A frame's seconds as the ramp takes them: none for a time that is not finite or goes back.
float frame_seconds(float seconds) { return std::isfinite(seconds) ? std::max(seconds, kSilentGain) : kSilentGain; }

}  // namespace

bool valid_music_ducking(const MusicDucking& ducking) {
  if (!valid_ducked_volume(ducking.volume)) return false;
  return valid_duck_seconds(ducking.attack_seconds) && valid_duck_seconds(ducking.release_seconds);
}

bool MusicDucker::set(const MusicDucking& ducking) {
  if (!valid_music_ducking(ducking)) return false;
  ducking_ = ducking;
  target_.reset();  // (the ramp under way follows the new settings)
  return true;
}

void MusicDucker::reset() {
  ducking_ = MusicDucking{};
  gain_ = kFullVolume;
  target_.reset();
}

float MusicDucker::update(bool voice_playing, float seconds) {
  const float target = voice_playing ? ducking_.volume : kFullVolume;
  if (target_ != target) start_ramp(target);
  const float gap = std::fabs(target - gain_);
  const float change = instant_ ? gap : std::min(rate_ * frame_seconds(seconds), gap);
  gain_ += target < gain_ ? -change : change;
  return gain_;
}

// A ramp to a new target over the attack (going down) or the release (coming back): the whole way
// from full to the ducked volume in that time, or the way the gain has to go when that is longer
// (the ducked volume changed while ducked).
void MusicDucker::start_ramp(float target) {
  target_ = target;
  const float duration = target < gain_ ? ducking_.attack_seconds : ducking_.release_seconds;
  const float span = std::max(kFullVolume - ducking_.volume, std::fabs(target - gain_));
  instant_ = ducks_at_once(duration, span);
  rate_ = instant_ ? kSilentGain : span / duration;
}

}  // namespace redgem
