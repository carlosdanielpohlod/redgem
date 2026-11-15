// The music goes down under a voice: the ducker's gain, and the music group following it in the
// sound pool and the music player.

#include <doctest/doctest.h>

#include <limits>
#include <map>
#include <string>

#include "audio/ducking.hpp"
#include "audio/music_player.hpp"
#include "audio/spatial_audio.hpp"

using namespace redgem;

namespace {

constexpr float kHalfLevel = 0.5f;
constexpr float kQuarterLevel = 0.25f;
constexpr float kAttackSeconds = 0.2f;
constexpr float kReleaseSeconds = 0.8f;
constexpr float kHalfAttack = kAttackSeconds / 2.0f;
constexpr float kQuarterRelease = kReleaseSeconds / 4.0f;
constexpr float kLongFrame = 10.0f;
constexpr float kNoTime = 0.0f;
constexpr float kHalfwayDown = 0.75f;   // half an attack from full to half
constexpr float kQuarterBack = 0.625f;  // a quarter of a release from half to full

MusicDucking ducking_to(float volume, float attack = kAttackSeconds, float release = kReleaseSeconds) {
  MusicDucking ducking;
  ducking.volume = volume;
  ducking.attack_seconds = attack;
  ducking.release_seconds = release;
  return ducking;
}

class PoolDriver final : public SoundDriver {
 public:
  std::map<SoundSourceId, float> volumes;
  std::map<SoundSourceId, bool> playing_ones;
  bool start(SoundSourceId id, const std::string&, const SpatialSound&, SpatialMix mix) override {
    volumes[id] = mix.volume;
    playing_ones[id] = true;
    return true;
  }
  void update(SoundSourceId) override {}
  bool playing(SoundSourceId id) const override { return playing_ones.at(id); }
  void mix(SoundSourceId id, SpatialMix mix) override { volumes[id] = mix.volume; }
  void pitch(SoundSourceId, float) override {}
  void stop(SoundSourceId id) override {
    volumes.erase(id);
    playing_ones.erase(id);
  }
};

class TrackDriver final : public MusicDriver {
 public:
  std::map<MusicTrackId, float> volumes;
  bool start(MusicTrackId id, const std::string&, bool, float volume) override {
    volumes[id] = volume;
    return true;
  }
  void update(MusicTrackId) override {}
  void volume(MusicTrackId id, float volume) override { volumes[id] = volume; }
  void pitch(MusicTrackId, float) override {}
  void stop(MusicTrackId id) override { volumes.erase(id); }
};

SpatialSound flat_in(MixGroup group) {
  FlatSound sound;
  sound.volume = kHalfLevel;
  sound.group = group;
  return flat_sound(sound);
}

}  // namespace

TEST_CASE("the ducker goes down over the attack while a voice plays, and back over the release") {
  MusicDucker ducker;
  REQUIRE(ducker.set(ducking_to(kHalfLevel)));
  CHECK(ducker.gain() == doctest::Approx(kFullVolume));
  CHECK(ducker.update(true, kHalfAttack) == doctest::Approx(kHalfwayDown));
  CHECK(ducker.update(true, kHalfAttack) == doctest::Approx(kHalfLevel));
  CHECK(ducker.update(true, kLongFrame) == doctest::Approx(kHalfLevel));  // (no lower than ducked)
  CHECK(ducker.update(false, kQuarterRelease) == doctest::Approx(kQuarterBack));
  CHECK(ducker.update(false, kLongFrame) == doctest::Approx(kFullVolume));  // (no higher than full)
}

TEST_CASE("no attack or release moves the gain at once, and a ducked volume of 1 is no ducking") {
  MusicDucker ducker;
  REQUIRE(ducker.set(ducking_to(kQuarterLevel, kNoTime, kNoTime)));
  CHECK(ducker.update(true, kNoTime) == doctest::Approx(kQuarterLevel));
  CHECK(ducker.update(false, kNoTime) == doctest::Approx(kFullVolume));
  REQUIRE(ducker.set(ducking_to(kNoDucking)));
  CHECK(ducker.update(true, kLongFrame) == doctest::Approx(kFullVolume));
}

TEST_CASE("a ducked volume raised while ducked brings the gain up over the release") {
  MusicDucker ducker;
  REQUIRE(ducker.set(ducking_to(kQuarterLevel, kNoTime)));
  ducker.update(true, kNoTime);
  REQUIRE(ducker.set(ducking_to(kHalfLevel, kNoTime)));
  CHECK(ducker.update(true, kReleaseSeconds) == doctest::Approx(kHalfLevel));
}

TEST_CASE("ducking that is not valid is refused and the one set stays") {
  MusicDucker ducker;
  CHECK_FALSE(ducker.set(ducking_to(-kHalfLevel)));
  CHECK_FALSE(ducker.set(ducking_to(kFullVolume * 2)));
  CHECK_FALSE(ducker.set(ducking_to(kHalfLevel, -kAttackSeconds)));
  CHECK_FALSE(ducker.set(ducking_to(kHalfLevel, kAttackSeconds, std::numeric_limits<float>::infinity())));
  CHECK_FALSE(valid_music_ducking(ducking_to(std::numeric_limits<float>::quiet_NaN())));
  CHECK(ducker.update(true, kLongFrame) == doctest::Approx(kDefaultDuckedVolume));
}

TEST_CASE("a group's gain is over its sounds apart from the player's volume") {
  PoolDriver driver;
  SoundPool sounds(driver);
  const auto sting = sounds.play("sting", flat_in(MixGroup::Music));
  const auto step = sounds.play("step", flat_in(MixGroup::Effects));
  sounds.set_group_volume(MixGroup::Music, kHalfLevel);
  sounds.set_group_gain(MixGroup::Music, kHalfLevel);
  CHECK(driver.volumes.at(sting) == doctest::Approx(kHalfLevel * kQuarterLevel));
  CHECK(driver.volumes.at(step) == doctest::Approx(kHalfLevel));
}

TEST_CASE("a group plays while any of its sounds still plays") {
  PoolDriver driver;
  SoundPool sounds(driver);
  CHECK_FALSE(sounds.group_playing(MixGroup::Voice));
  const auto line = sounds.play("line", flat_in(MixGroup::Voice));
  sounds.play("step", flat_in(MixGroup::Effects));
  CHECK(sounds.group_playing(MixGroup::Voice));
  driver.playing_ones.at(line) = false;  // it ended, and is not reaped yet
  CHECK_FALSE(sounds.group_playing(MixGroup::Voice));
  CHECK(sounds.group_playing(MixGroup::Effects));
}

TEST_CASE("the music player's ducked gain is over both tracks, apart from the group's volume") {
  TrackDriver driver;
  MusicPlayer music(driver);
  MusicRequest request;
  REQUIRE(music.play("theme", request));
  music.set_group_volume(kHalfLevel);
  music.set_ducked(kHalfLevel);
  CHECK(driver.volumes.begin()->second == doctest::Approx(kQuarterLevel));
  MusicRequest next;
  REQUIRE(music.play("night", next));
  CHECK(driver.volumes.at(*music.current()) == doctest::Approx(kQuarterLevel));
  music.set_ducked(kFullVolume);
  CHECK(driver.volumes.at(*music.current()) == doctest::Approx(kHalfLevel));
}
