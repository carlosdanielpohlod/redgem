#pragma once

// The effects a mix group puts on its sounds, as sample math with no device: a low-pass filter (a
// muffled radio, a sound behind a wall) and a reverb (a cave, a hall). The backend runs them on the
// audio thread (audio/effect_rack.hpp); the tests run them on buffers of their own.

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "audio/mix_groups.hpp"

namespace redgem {

inline constexpr std::size_t kEffectChannels = 2;  // interleaved stereo, as the device mixes
// The rate the filters are tuned for. The device mixes at its own rate, most often this one; at
// 44.1 kHz a cutoff is heard about 9% higher.
inline constexpr float kEffectSampleRate = 48000.0f;
inline constexpr float kLowestCutoff = 20.0f;      // Hz: about the lowest a person hears
inline constexpr float kHighestCutoff = 20000.0f;  // Hz: about the highest
inline constexpr float kNoReverb = 0.0f;
inline constexpr float kFullReverb = 1.0f;

// What a group does to its sounds (Audio.effects). No cutoff is no filter; a reverb of 0 is none.
struct GroupEffects {
  std::optional<float> low_pass_hz;
  float reverb = kNoReverb;  // how much of each sound goes to the group's reverb, 0 to 1
};
bool valid_group_effects(const GroupEffects& effects);

// What one sound is played with, from its group and the groups over it: the lowest cutoff of them
// all, and the reverb of the nearest one that has one (the bus its sound goes to).
struct VoiceEffects {
  std::optional<float> low_pass_hz;
  float reverb = kNoReverb;
  MixGroup reverb_bus = MixGroup::Effects;
};
bool operator==(const VoiceEffects& a, const VoiceEffects& b);

// A one-pole low-pass over interleaved stereo frames, each channel with its own memory.
class LowPass {
 public:
  void set_cutoff(std::optional<float> hz);
  void reset() { memory_ = {}; }
  void process(float* frames, std::size_t count);
  float coefficient() const { return coefficient_; }

 private:
  float coefficient_ = 1.0f;  // (1 lets everything through)
  std::array<float, kEffectChannels> memory_{};
};

inline constexpr std::size_t kReverbCombs = 4;
inline constexpr std::size_t kReverbAllpasses = 2;

// A small Schroeder reverb: parallel damped combs into allpasses on each channel, the right
// channel's delays a little longer so the tail is wide. About 60 KB of delay lines.
class Reverb {
 public:
  Reverb();
  // Adds the reverb of `dry` (interleaved stereo, `count` frames) into `out`.
  void process(const float* dry, float* out, std::size_t count);

 private:
  struct Line {
    std::vector<float> samples;
    std::size_t at = 0;
    float damped = 0.0f;
  };
  float channel(float input, std::size_t side);
  float combs(float input, std::size_t side);
  float allpasses(float input, std::size_t side);
  std::array<std::array<Line, kReverbCombs>, kEffectChannels> combs_;
  std::array<std::array<Line, kReverbAllpasses>, kEffectChannels> allpasses_;
};

}  // namespace redgem
