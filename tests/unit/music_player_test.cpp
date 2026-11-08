#include <doctest/doctest.h>

#include <map>
#include <string>

#include "audio/music_player.hpp"

using namespace redgem;

namespace {

constexpr float kFadeSeconds = 2.0f;
constexpr float kHalfFade = kFadeSeconds / 2.0f;
constexpr float kHalf = 0.5f;
constexpr float kQuarter = 0.25f;
constexpr float kRaisedPitch = 1.5f;

struct TestTrack {
  std::string name;
  bool loop = true;
  float volume = 0.0f;
  float pitch = kAsRecorded;
  int updates = 0;
};

// Streams kept instead of played; a name it is told is missing does not start.
class MusicTestDriver final : public MusicDriver {
 public:
  std::map<MusicTrackId, TestTrack> tracks;
  std::string missing = "missing";
  bool start(MusicTrackId id, const std::string& name, bool loop, float volume) override {
    if (name == missing) return false;
    tracks.emplace(id, TestTrack{name, loop, volume});
    return true;
  }
  void update(MusicTrackId id) override { ++tracks.at(id).updates; }
  void volume(MusicTrackId id, float volume) override { tracks.at(id).volume = volume; }
  void pitch(MusicTrackId id, float pitch) override { tracks.at(id).pitch = pitch; }
  void stop(MusicTrackId id) override { tracks.erase(id); }

  const TestTrack* named(const std::string& name) const {
    for (const auto& [id, track] : tracks) {
      if (track.name == name) return &track;
    }
    return nullptr;
  }
};

MusicRequest at_volume(float volume, float fade_seconds = kNoFade) {
  MusicRequest request;
  request.volume = volume;
  request.fade_seconds = fade_seconds;
  return request;
}

}  // namespace

TEST_CASE("a track with no fade is heard at its volume at once, and replaces the one playing at once") {
  MusicTestDriver driver;
  MusicPlayer music(driver);
  REQUIRE(music.play("day", at_volume(kHalf)));
  CHECK(driver.named("day")->volume == doctest::Approx(kHalf));
  CHECK(driver.named("day")->loop);
  REQUIRE(music.play("night", at_volume(kFullVolume)));
  CHECK(driver.tracks.size() == 1);
  CHECK(driver.named("night") != nullptr);
}

TEST_CASE("a crossfade brings the new track in while the old one goes, then lets the old one go") {
  MusicTestDriver driver;
  MusicPlayer music(driver);
  REQUIRE(music.play("day", at_volume(kFullVolume)));
  REQUIRE(music.play("night", at_volume(kHalf, kFadeSeconds)));
  CHECK(driver.named("night")->volume == 0.0f);  // it starts silent
  CHECK(driver.tracks.size() == 2);
  music.update(kHalfFade);
  CHECK(driver.named("night")->volume == doctest::Approx(kQuarter));
  CHECK(driver.named("day")->volume == doctest::Approx(kHalf));
  CHECK(driver.named("day")->updates == 1);  // both streams are fed while they play
  music.update(kHalfFade);
  CHECK(driver.named("night")->volume == doctest::Approx(kHalf));
  CHECK(driver.named("day") == nullptr);
  music.update(kFadeSeconds);
  CHECK(driver.named("night")->volume == doctest::Approx(kHalf));  // never past its own volume
}

TEST_CASE("a track that cannot play leaves the one playing as it was") {
  MusicTestDriver driver;
  MusicPlayer music(driver);
  REQUIRE(music.play("day", at_volume(kHalf)));
  const auto day = music.current();
  CHECK_FALSE(music.play(driver.missing, at_volume(kFullVolume, kFadeSeconds)));
  CHECK(music.current() == day);
  CHECK(driver.named("day")->volume == doctest::Approx(kHalf));
}

TEST_CASE("stopping with a fade lets the track go over the fade, and without one at once") {
  MusicTestDriver driver;
  MusicPlayer music(driver);
  REQUIRE(music.play("day", at_volume(kFullVolume)));
  music.stop(kFadeSeconds);
  CHECK_FALSE(music.current().has_value());
  music.update(kHalfFade);
  CHECK(driver.named("day")->volume == doctest::Approx(kHalf));
  music.update(kHalfFade);
  CHECK(driver.tracks.empty());
  REQUIRE(music.play("night", at_volume(kFullVolume)));
  music.stop(kNoFade);
  CHECK(driver.tracks.empty());
}

TEST_CASE("at most two streams play: a third track drops the one already fading out") {
  MusicTestDriver driver;
  MusicPlayer music(driver);
  REQUIRE(music.play("day", at_volume(kFullVolume)));
  REQUIRE(music.play("dusk", at_volume(kFullVolume, kFadeSeconds)));
  music.update(kHalfFade);
  REQUIRE(music.play("night", at_volume(kFullVolume, kFadeSeconds)));
  CHECK(driver.tracks.size() == 2);
  CHECK(driver.named("day") == nullptr);
  // Dusk was halfway in: it goes from there to silence over the whole fade.
  music.update(kHalfFade);
  CHECK(driver.named("dusk")->volume == doctest::Approx(kQuarter));
}

TEST_CASE("a track replaced before it was heard at all goes at the next update") {
  MusicTestDriver driver;
  MusicPlayer music(driver);
  REQUIRE(music.play("dusk", at_volume(kFullVolume, kFadeSeconds)));
  REQUIRE(music.play("night", at_volume(kFullVolume, kFadeSeconds)));
  music.update(0.0f);
  CHECK(driver.named("dusk") == nullptr);
  CHECK(driver.named("night") != nullptr);
}

TEST_CASE("the music group's volume is over both tracks; the volume and pitch set are the playing one's") {
  MusicTestDriver driver;
  MusicPlayer music(driver);
  REQUIRE(music.play("day", at_volume(kFullVolume)));
  REQUIRE(music.play("night", at_volume(kFullVolume, kFadeSeconds)));
  music.update(kHalfFade);
  music.set_group_volume(kHalf);
  CHECK(driver.named("day")->volume == doctest::Approx(kQuarter));
  CHECK(driver.named("night")->volume == doctest::Approx(kQuarter));
  music.set_volume(kHalf);
  music.set_pitch(kRaisedPitch);
  CHECK(driver.named("night")->volume == doctest::Approx(kHalf * kHalf * kHalf));
  CHECK(driver.named("night")->pitch == doctest::Approx(kRaisedPitch));
  CHECK(driver.named("day")->volume == doctest::Approx(kQuarter));
  CHECK(driver.named("day")->pitch == doctest::Approx(kAsRecorded));
}

TEST_CASE("a player going away stops what it plays, and changes with nothing playing do nothing") {
  MusicTestDriver driver;
  {
    MusicPlayer music(driver);
    music.set_volume(kHalf);
    music.set_pitch(kRaisedPitch);
    music.update(kHalfFade);
    REQUIRE(music.play("day", at_volume(kFullVolume)));
    REQUIRE(music.play("night", at_volume(kFullVolume, kFadeSeconds)));
  }
  CHECK(driver.tracks.empty());
}
