#pragma once

// How loud a voice is as it plays, for a mouth that follows it (VoiceMouth): the audio thread
// measures each block of a voice's own samples (before its volume and its place in the world), and
// every frame an envelope follower smooths what was measured into a loudness that rises fast and
// falls slower, as a mouth opens and closes.

#include <atomic>
#include <cstddef>

namespace redgem {

inline constexpr float kSilence = 0.0f;
inline constexpr float kDefaultEnvelopeAttack = 0.02f;   // seconds to rise most of the way to a louder level
inline constexpr float kDefaultEnvelopeRelease = 0.08f;  // seconds to fall most of the way to a quieter one

// The root mean square of `count` samples (0 for none): how loud they are, 0 to 1.
float rms_level(const float* samples, std::size_t count);

// The level of the last block measured, kept for another thread to read (the audio thread writes
// it as it mixes, the game's reads it every frame).
class LevelMeter {
 public:
  void measure(const float* samples, std::size_t count) { level_.store(rms_level(samples, count)); }
  void reset() { level_.store(kSilence); }
  float level() const { return level_.load(); }

 private:
  std::atomic<float> level_{kSilence};
};

// Smooths a level that jumps from block to block: each frame it moves toward the level measured, by
// the share of the way that `attack` (rising) or `release` (falling) seconds take it (one pole:
// after that many seconds, about two thirds of the way). Zero seconds follow the level at once.
class EnvelopeFollower {
 public:
  EnvelopeFollower() = default;
  EnvelopeFollower(float attack_seconds, float release_seconds) : attack_(attack_seconds), release_(release_seconds) {}

  // Moves on by `seconds` toward `level`, and returns the envelope.
  float update(float level, float seconds);
  float value() const { return value_; }

 private:
  float attack_ = kDefaultEnvelopeAttack;
  float release_ = kDefaultEnvelopeRelease;
  float value_ = kSilence;
};

}  // namespace redgem
