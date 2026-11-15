// The sample math of the mix groups' effects, and the rack that runs it for each sound with no
// device: buffers in, buffers out.

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <vector>

#include "audio/audio_effects.hpp"
#include "audio/effect_rack.hpp"

using namespace redgem;

namespace {

constexpr float kLowCutoff = 200.0f;
constexpr float kHalfShare = 0.5f;
constexpr std::size_t kFrames = 4800;       // a tenth of a second
constexpr std::size_t kTailFrames = 48000;  // a second
constexpr float kSettledShare = 0.99f;
constexpr float kMuffledShare = 0.05f;
constexpr float kLoudestTail = 1.0f;
constexpr float kHighCutoff = 2000.0f;
constexpr int kPeriods = 20;  // device periods of a tenth of a second: two seconds
constexpr std::size_t kHalves = 2;

// Interleaved stereo frames, both channels the same.
std::vector<float> frames_of(std::size_t count, float value) {
  return std::vector<float>(count * kEffectChannels, value);
}
// The highest frequency there is: every frame the other way.
std::vector<float> alternating(std::size_t count) {
  std::vector<float> frames(count * kEffectChannels);
  for (std::size_t sample = 0; sample < frames.size(); ++sample)
    frames[sample] = (sample / kEffectChannels) % kHalves != 0 ? -1.0f : 1.0f;
  return frames;
}
float loudest(const std::vector<float>& frames, std::size_t from = 0) {
  float most = 0.0f;
  for (std::size_t sample = from; sample < frames.size(); ++sample) most = std::max(most, std::fabs(frames[sample]));
  return most;
}
float energy(const std::vector<float>& frames, std::size_t from, std::size_t to) {
  float sum = 0.0f;
  for (std::size_t sample = from; sample < to; ++sample) sum += frames[sample] * frames[sample];
  return sum;
}

}  // namespace

TEST_CASE("a low-pass lets a steady level through and takes the highest frequencies out") {
  LowPass filter;
  filter.set_cutoff(kLowCutoff);
  auto steady = frames_of(kFrames, kHalfShare);
  filter.process(steady.data(), kFrames);
  CHECK(steady.back() == doctest::Approx(kHalfShare).epsilon(1.0f - kSettledShare));
  filter.reset();
  auto high = alternating(kFrames);
  filter.process(high.data(), kFrames);
  CHECK(loudest(high, high.size() / kHalves) < kMuffledShare);
}

TEST_CASE("a low-pass with no cutoff changes nothing, and a higher cutoff keeps more") {
  LowPass open;
  open.set_cutoff(std::nullopt);
  auto frames = alternating(kFrames);
  open.process(frames.data(), kFrames);
  CHECK(frames == alternating(kFrames));
  LowPass low, high;
  low.set_cutoff(kLowCutoff);
  high.set_cutoff(kHighCutoff);
  CHECK(low.coefficient() < high.coefficient());
  CHECK(high.coefficient() < 1.0f);
}

TEST_CASE("the reverb of silence is silence, and a click rings on after it and dies away") {
  Reverb silent;
  const auto quiet = frames_of(kFrames, 0.0f);
  auto out = frames_of(kFrames, 0.0f);
  silent.process(quiet.data(), out.data(), kFrames);
  CHECK(loudest(out) == 0.0f);
  Reverb room;
  auto click = frames_of(kTailFrames, 0.0f);
  click[0] = click[1] = 1.0f;
  auto tail = frames_of(kTailFrames, 0.0f);
  room.process(click.data(), tail.data(), kTailFrames);
  const std::size_t tenth = kFrames * kEffectChannels;
  CHECK(energy(tail, 0, tenth) > 0.0f);
  CHECK(energy(tail, tail.size() - tenth, tail.size()) < energy(tail, tenth, kHalves * tenth));
  CHECK(loudest(tail) < kLoudestTail);
  CHECK(std::isfinite(energy(tail, 0, tail.size())));
}

TEST_CASE("the reverb adds to what is in the mix already") {
  Reverb room;
  const auto steady = frames_of(kFrames, kHalfShare);
  auto mix = frames_of(kFrames, kHalfShare);
  room.process(steady.data(), mix.data(), kFrames);
  CHECK(mix.front() == doctest::Approx(kHalfShare));  // (nothing came back from the lines yet)
  CHECK(mix.back() != doctest::Approx(kHalfShare));
}

TEST_CASE("group effects are valid within their ranges only") {
  CHECK(valid_group_effects(GroupEffects{}));
  CHECK(valid_group_effects(GroupEffects{kLowestCutoff, kFullReverb}));
  CHECK_FALSE(valid_group_effects(GroupEffects{kLowestCutoff * kHalfShare, kNoReverb}));
  CHECK_FALSE(valid_group_effects(GroupEffects{kHighestCutoff + kLowCutoff, kNoReverb}));
  CHECK_FALSE(valid_group_effects(GroupEffects{std::nullopt, kFullReverb + kHalfShare}));
  CHECK_FALSE(valid_group_effects(GroupEffects{std::numeric_limits<float>::quiet_NaN(), kNoReverb}));
}

TEST_CASE("the rack filters each sound in its slot and leaves the others as they were") {
  EffectRack rack;
  const auto muffled = rack.claim();
  const auto clear = rack.claim();
  REQUIRE(muffled);
  REQUIRE(clear);
  CHECK(*muffled != *clear);
  rack.set_effects(*muffled, VoiceEffects{kLowCutoff, kNoReverb, MixGroup::Effects});
  auto first = alternating(kFrames);
  auto second = alternating(kFrames);
  rack.process_voice(*muffled, first.data(), kFrames);
  rack.process_voice(*clear, second.data(), kFrames);
  CHECK(loudest(first, first.size() / kHalves) < kMuffledShare);
  CHECK(second == alternating(kFrames));
}

TEST_CASE("a sound's send reaches its group's reverb in the mix, at the sound's volume") {
  EffectRack rack;
  const auto slot = rack.claim();
  REQUIRE(slot);
  rack.set_effects(*slot, VoiceEffects{std::nullopt, kFullReverb, MixGroup::Voice});
  auto mix = frames_of(kFrames, 0.0f);
  rack.process_mix(mix.data(), kFrames);  // (no sound sent yet: the bus is silent)
  CHECK(loudest(mix) == 0.0f);
  auto sound = frames_of(kFrames, kHalfShare);
  rack.process_voice(*slot, sound.data(), kFrames);
  rack.process_mix(mix.data(), kFrames);
  const float loud = energy(mix, 0, mix.size());
  CHECK(loud > 0.0f);
  rack.set_volume(*slot, 0.0f);
  auto after = frames_of(kFrames, 0.0f);
  for (int period = 0; period < kPeriods; ++period) {
    rack.process_voice(*slot, sound.data(), kFrames);
    std::fill(after.begin(), after.end(), 0.0f);
    rack.process_mix(after.data(), kFrames);
  }
  CHECK(energy(after, 0, after.size()) < loud);  // (a silent sound sends nothing; the tail dies)
}

TEST_CASE("a low-pass turned on again starts from silence, not from where it was turned off") {
  EffectRack rack;
  const auto slot = rack.claim();
  REQUIRE(slot);
  rack.set_effects(*slot, VoiceEffects{kLowCutoff, kNoReverb, MixGroup::Effects});
  for (int period = 0; period < kPeriods; ++period) {
    auto loud = frames_of(kFrames, kHalfShare);
    rack.process_voice(*slot, loud.data(), kFrames);
  }
  rack.set_effects(*slot, VoiceEffects{});
  rack.set_effects(*slot, VoiceEffects{kLowCutoff, kNoReverb, MixGroup::Effects});
  auto quiet = frames_of(kFrames, 0.0f);
  rack.process_voice(*slot, quiet.data(), kFrames);
  CHECK(loudest(quiet) == 0.0f);
}

TEST_CASE("closing the buses ends the reverbs and their tails") {
  EffectRack rack;
  const auto slot = rack.claim();
  REQUIRE(slot);
  rack.set_effects(*slot, VoiceEffects{std::nullopt, kFullReverb, MixGroup::Voice});
  auto sound = frames_of(kFrames, kHalfShare);
  auto mix = frames_of(kFrames, 0.0f);
  rack.process_voice(*slot, sound.data(), kFrames);
  rack.process_mix(mix.data(), kFrames);
  rack.set_effects(*slot, VoiceEffects{});
  rack.close_buses();
  std::fill(mix.begin(), mix.end(), 0.0f);
  rack.process_mix(mix.data(), kFrames);
  CHECK(loudest(mix) == 0.0f);
}

TEST_CASE("the rack has a slot for every sound at once and one more, given back when released") {
  EffectRack rack;
  std::vector<EffectSlot> slots;
  for (std::size_t count = 0; count < kEffectSlots; ++count) {
    const auto slot = rack.claim();
    REQUIRE(slot);
    slots.push_back(*slot);
  }
  CHECK_FALSE(rack.claim().has_value());
  rack.release(slots.front());
  CHECK(rack.claim() == slots.front());
}
