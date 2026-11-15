#include "audio/audio_effects.hpp"

#include <cmath>

namespace redgem {
namespace {

constexpr float kTwoPi = 6.28318530718f;
constexpr float kPassAll = 1.0f;
// Delays in frames at 48 kHz, apart from each other so their echoes do not line up.
constexpr std::array<std::size_t, kReverbCombs> kCombDelays = {1687, 1601, 2053, 2251};
constexpr std::array<std::size_t, kReverbAllpasses> kAllpassDelays = {556, 441};
constexpr std::size_t kStereoSpread = 23;  // frames the right channel's lines are longer
constexpr float kCombFeedback = 0.82f;     // how long the tail lasts (about a second and a half)
constexpr float kCombDamping = 0.25f;      // how fast its highs die
constexpr float kAllpassFeedback = 0.5f;
// What goes into the combs, so that a sound sent whole comes out about as loud as it went in.
constexpr float kReverbInput = 0.05f;

bool valid_cutoff(std::optional<float> hz) {
  if (!hz) return true;
  return std::isfinite(*hz) && *hz >= kLowestCutoff && *hz <= kHighestCutoff;
}
bool valid_reverb(float reverb) { return std::isfinite(reverb) && reverb >= kNoReverb && reverb <= kFullReverb; }

// Writes `value` where the line is and moves on.
void push(std::vector<float>& samples, std::size_t& at, float value) {
  samples[at] = value;
  at = (at + 1) % samples.size();
}

}  // namespace

bool valid_group_effects(const GroupEffects& effects) {
  return valid_cutoff(effects.low_pass_hz) && valid_reverb(effects.reverb);
}

bool operator==(const VoiceEffects& a, const VoiceEffects& b) {
  return a.low_pass_hz == b.low_pass_hz && a.reverb == b.reverb && a.reverb_bus == b.reverb_bus;
}

void LowPass::set_cutoff(std::optional<float> hz) {
  coefficient_ = hz ? 1.0f - std::exp(-kTwoPi * *hz / kEffectSampleRate) : kPassAll;
}

void LowPass::process(float* frames, std::size_t count) {
  if (coefficient_ >= kPassAll) return;
  for (std::size_t sample = 0; sample < count * kEffectChannels; ++sample) {
    float& memory = memory_[sample % kEffectChannels];
    memory += coefficient_ * (frames[sample] - memory);
    frames[sample] = memory;
  }
}

Reverb::Reverb() {
  for (std::size_t side = 0; side < kEffectChannels; ++side) {
    const std::size_t spread = side * kStereoSpread;
    for (std::size_t line = 0; line < kReverbCombs; ++line)
      combs_[side][line].samples.assign(kCombDelays[line] + spread, 0.0f);
    for (std::size_t line = 0; line < kReverbAllpasses; ++line)
      allpasses_[side][line].samples.assign(kAllpassDelays[line] + spread, 0.0f);
  }
}

float Reverb::combs(float input, std::size_t side) {
  float wet = 0.0f;
  for (Line& comb : combs_[side]) {
    const float delayed = comb.samples[comb.at];
    comb.damped = delayed * (1.0f - kCombDamping) + comb.damped * kCombDamping;
    push(comb.samples, comb.at, input + comb.damped * kCombFeedback);
    wet += delayed;
  }
  return wet;
}

float Reverb::allpasses(float input, std::size_t side) {
  float wet = input;
  for (Line& allpass : allpasses_[side]) {
    const float delayed = allpass.samples[allpass.at];
    push(allpass.samples, allpass.at, wet + delayed * kAllpassFeedback);
    wet = delayed - wet * kAllpassFeedback;
  }
  return wet;
}

float Reverb::channel(float input, std::size_t side) { return allpasses(combs(input * kReverbInput, side), side); }

void Reverb::process(const float* dry, float* out, std::size_t count) {
  for (std::size_t sample = 0; sample < count * kEffectChannels; ++sample)
    out[sample] += channel(dry[sample], sample % kEffectChannels);
}

}  // namespace redgem
