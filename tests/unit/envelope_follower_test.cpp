// How loud a voice is as it plays: the level of a block of samples, the envelope follower that
// smooths it, and the sound pool following the voices of the voice group (Audio.loudness).

#include <doctest/doctest.h>

#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "audio/envelope_follower.hpp"
#include "audio/mix_group_tree.hpp"
#include "audio/spatial_audio.hpp"

using namespace redgem;

namespace {

constexpr float kHalfLevel = 0.5f;
constexpr float kQuarterLevel = 0.25f;
constexpr float kAttackSeconds = 0.1f;
constexpr float kReleaseSeconds = 0.4f;
constexpr float kNoTime = 0.0f;
constexpr float kLongWhile = 10.0f;  // seconds: far past any attack or release
constexpr std::size_t kBlock = 64;
constexpr std::size_t kEveryOther = 2;
// After one time constant, a one-pole follower is 1 - 1/e of the way.
float one_time_constant() { return 1.0f - std::exp(-1.0f); }

// A driver whose sounds are as loud as it is told.
class LevelDriver final : public SoundDriver {
 public:
  std::map<SoundSourceId, float> levels;
  std::map<SoundSourceId, bool> playing_ones;
  std::vector<SoundSourceId> measured;  // what the pool asked it to measure, in order
  bool start(SoundSourceId id, const std::string&, const SpatialSound&, SpatialMix, const VoiceEffects&) override {
    levels[id] = kSilence;
    playing_ones[id] = true;
    return true;
  }
  void measure(SoundSourceId id) override { measured.push_back(id); }
  void update(SoundSourceId) override {}
  bool playing(SoundSourceId id) const override { return playing_ones.at(id); }
  void mix(SoundSourceId, SpatialMix) override {}
  void pitch(SoundSourceId, float) override {}
  void stop(SoundSourceId id) override {
    levels.erase(id);
    playing_ones.erase(id);
  }
  float level(SoundSourceId id) const override { return levels.at(id); }
};

constexpr const char* kLinesGroup = "lines";

SpatialSound in_group(MixGroup group) {
  FlatSound sound;
  sound.group = group;
  return flat_sound(sound);
}

}  // namespace

TEST_CASE("the level of a block is the root mean square of its samples") {
  const std::vector<float> steady(kBlock, kHalfLevel);
  CHECK(rms_level(steady.data(), steady.size()) == doctest::Approx(kHalfLevel));
  std::vector<float> square(kBlock);
  for (std::size_t index = 0; index < square.size(); ++index) square[index] = index % kEveryOther == 0 ? 1.0f : -1.0f;
  CHECK(rms_level(square.data(), square.size()) == doctest::Approx(1.0f));
  const std::vector<float> quiet(kBlock, kSilence);
  CHECK(rms_level(quiet.data(), quiet.size()) == doctest::Approx(kSilence));
  CHECK(rms_level(nullptr, 0) == doctest::Approx(kSilence));
}

TEST_CASE("a meter keeps the level of the last block, until it is reset") {
  LevelMeter meter;
  CHECK(meter.level() == doctest::Approx(kSilence));
  const std::vector<float> block(kBlock, kQuarterLevel);
  meter.measure(block.data(), block.size());
  CHECK(meter.level() == doctest::Approx(kQuarterLevel));
  meter.reset();
  CHECK(meter.level() == doctest::Approx(kSilence));
}

TEST_CASE("the envelope rises over the attack and falls over the release") {
  EnvelopeFollower envelope(kAttackSeconds, kReleaseSeconds);
  CHECK(envelope.update(kHalfLevel, kAttackSeconds) == doctest::Approx(kHalfLevel * one_time_constant()));
  CHECK(envelope.update(kHalfLevel, kLongWhile) == doctest::Approx(kHalfLevel));
  // Falling takes the release: one attack's time goes a smaller share of the way down.
  const float after_attack = envelope.update(kSilence, kAttackSeconds);
  CHECK(after_attack > kHalfLevel * (1.0f - one_time_constant()));
  CHECK(after_attack < kHalfLevel);
  EnvelopeFollower falling(kAttackSeconds, kReleaseSeconds);
  falling.update(kHalfLevel, kLongWhile);
  CHECK(falling.update(kSilence, kReleaseSeconds) == doctest::Approx(kHalfLevel * (1.0f - one_time_constant())));
}

TEST_CASE("no time moves the envelope nowhere, and no attack or release follows the level at once") {
  EnvelopeFollower envelope(kAttackSeconds, kReleaseSeconds);
  CHECK(envelope.update(kHalfLevel, kNoTime) == doctest::Approx(kSilence));
  EnvelopeFollower instant(kNoTime, kNoTime);
  CHECK(instant.update(kHalfLevel, kNoTime) == doctest::Approx(kHalfLevel));
  CHECK(instant.update(kQuarterLevel, kNoTime) == doctest::Approx(kQuarterLevel));
  CHECK(instant.value() == doctest::Approx(kQuarterLevel));
}

TEST_CASE("the pool follows the loudness of its voices, and of nothing else") {
  LevelDriver driver;
  SoundPool sounds(driver);
  const SoundSourceId voice = sounds.play("line", in_group(MixGroup::Voice));
  const SoundSourceId effect = sounds.play("step", in_group(MixGroup::Effects));
  driver.levels[voice] = kHalfLevel;
  driver.levels[effect] = kHalfLevel;
  CHECK(sounds.loudness(voice) == doctest::Approx(kSilence));  // (nothing followed yet)
  sounds.follow(kLongWhile);
  CHECK(sounds.loudness(voice) == doctest::Approx(kHalfLevel));
  CHECK(sounds.loudness(effect) == doctest::Approx(kSilence));
  driver.levels[voice] = kQuarterLevel;
  sounds.follow(kLongWhile);
  CHECK(sounds.loudness(voice) == doctest::Approx(kQuarterLevel));
}

TEST_CASE("a voice that ended or was stopped is silent") {
  LevelDriver driver;
  SoundPool sounds(driver);
  const SoundSourceId stopped = sounds.play("line", in_group(MixGroup::Voice));
  const SoundSourceId ended = sounds.play("line", in_group(MixGroup::Voice));
  driver.levels[stopped] = kHalfLevel;
  driver.levels[ended] = kHalfLevel;
  sounds.follow(kLongWhile);
  CHECK(sounds.stop(stopped));
  CHECK(sounds.loudness(stopped) == doctest::Approx(kSilence));
  driver.playing_ones[ended] = false;
  sounds.update();  // (it lets go of the sounds that ended)
  CHECK(sounds.loudness(ended) == doctest::Approx(kSilence));
  CHECK(sounds.loudness(kNoSoundSource) == doctest::Approx(kSilence));
}

TEST_CASE("the pool measures and follows the voices of a game's group under the voice group") {
  LevelDriver driver;
  MixGroupTree groups;
  SoundPool sounds(driver, groups);
  const auto lines = groups.define(kLinesGroup, MixGroup::Voice, std::nullopt);
  REQUIRE(lines);
  const SoundSourceId line = sounds.play("line", in_group(*lines));
  const SoundSourceId effect = sounds.play("step", in_group(MixGroup::Effects));
  CHECK(driver.measured == std::vector<SoundSourceId>{line});
  driver.levels[line] = kHalfLevel;
  driver.levels[effect] = kHalfLevel;
  sounds.follow(kLongWhile);
  CHECK(sounds.loudness(line) == doctest::Approx(kHalfLevel));
  CHECK(sounds.loudness(effect) == doctest::Approx(kSilence));
}
