#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>

#include <map>
#include <memory>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "audio/audio.hpp"
#include "core/config.hpp"
#include "test_support.hpp"
#include "testing/recording_audio.hpp"

using namespace redgem;

namespace {

constexpr float kHalf = 0.5f;
constexpr float kQuarter = 0.25f;
constexpr float kRaisedPitch = 1.5f;
constexpr float kMusicVolume = 0.7f;  // Audio.music's own default
constexpr float kFadeSeconds = 0.5f;
// Frames that see a fade of kFadeSeconds through, at the engine's fixed step, and one more.
constexpr int kFramesPastFade = static_cast<int>(kFadeSeconds / config::kFixedDeltaSeconds) + 2;

// A sound driver that keeps what it was asked to play (every name is there).
class HandleTestDriver final : public SoundDriver {
 public:
  std::map<SoundSourceId, testing::RecordedVoice> voices;
  bool start(SoundSourceId id, const std::string& name, const SpatialSound& sound, SpatialMix mix) override {
    voices.emplace(id, testing::RecordedVoice{name, sound, mix});
    return true;
  }
  void update(SoundSourceId) override {}
  bool playing(SoundSourceId id) const override { return voices.at(id).playing; }
  void mix(SoundSourceId id, SpatialMix mix) override { voices.at(id).mix = mix; }
  void pitch(SoundSourceId id, float pitch) override { voices.at(id).sound.pitch = pitch; }
  void stop(SoundSourceId id) override { voices.erase(id); }
};

SpatialSound flat_at(float volume, MixGroup group = MixGroup::Effects) {
  FlatSound sound;
  sound.volume = volume;
  sound.group = group;
  return flat_sound(sound);
}

std::unique_ptr<Engine> sandbox(testing::RecordingAudio& audio) {
  EngineOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.fixed_step = true;
  options.audio = &audio;
  auto result = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(result));
  return std::move(std::get<std::unique_ptr<Engine>>(result));
}

std::string ruby(Engine& engine, const std::string& code) {
  const auto result = engine.scripts().eval(code, "sound_handles_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

void frames(Engine& engine, int count) {
  for (int frame = 0; frame < count; ++frame) engine.frame(config::kFixedDeltaSeconds);
}

}  // namespace

TEST_CASE("a flat sound is heard centered at its own volume wherever the listener is") {
  HandleTestDriver driver;
  SoundPool sounds(driver);
  AudioListener listener;
  listener.position = {kDefaultSoundReach * 2, 0, 0};
  REQUIRE(sounds.set_listener(listener));
  const auto id = sounds.play("hum", flat_at(kHalf));
  REQUIRE(id != kNoSoundSource);
  CHECK(driver.voices.at(id).mix.volume == doctest::Approx(kHalf));
  CHECK(driver.voices.at(id).mix.pan == 0.0f);
  CHECK_FALSE(driver.voices.at(id).sound.positional);
}

TEST_CASE("flat and positional sounds share one id space") {
  HandleTestDriver driver;
  SoundPool sounds(driver);
  const auto flat = sounds.play("hum", flat_at(kHalf));
  const auto placed = sounds.play("bird", SpatialSound{});
  CHECK(placed == flat + 1);
  CHECK(sounds.stop(flat));
  CHECK(sounds.playing(placed));
}

TEST_CASE("a sound's levels change while it plays, and levels it cannot have leave it as it was") {
  HandleTestDriver driver;
  SoundPool sounds(driver);
  const auto id = sounds.play("hum", flat_at(kHalf));
  SoundLevels levels;
  levels.pitch = kRaisedPitch;
  REQUIRE(sounds.set_levels(id, levels));
  CHECK(driver.voices.at(id).sound.pitch == doctest::Approx(kRaisedPitch));
  CHECK(driver.voices.at(id).mix.volume == doctest::Approx(kHalf));  // (not asked to change)
  levels.volume = kQuarter;
  levels.pitch = kMaxSpatialPitch * 2;
  CHECK_FALSE(sounds.set_levels(id, levels));
  CHECK(driver.voices.at(id).mix.volume == doctest::Approx(kHalf));
  CHECK_FALSE(sounds.set_levels(id + 1, SoundLevels{}));
  driver.voices.at(id).playing = false;  // it came to its end, and is not reaped yet
  CHECK_FALSE(sounds.set_levels(id, SoundLevels{}));
}

TEST_CASE("a sound that ended gives its place back to the next one played") {
  HandleTestDriver driver;
  SoundPool sounds(driver);
  for (std::size_t count = 0; count < kMaxSpatialSounds; ++count) REQUIRE(sounds.play("step", flat_at(kHalf)));
  CHECK(sounds.play("step", flat_at(kHalf)) == kNoSoundSource);
  driver.voices.begin()->second.playing = false;
  CHECK(sounds.play("step", flat_at(kHalf)) != kNoSoundSource);
}

TEST_CASE("a group's volume is over every sound of that group, those playing and those to come") {
  HandleTestDriver driver;
  SoundPool sounds(driver);
  const auto voice = sounds.play("line", flat_at(kHalf, MixGroup::Voice));
  const auto effect = sounds.play("step", flat_at(kHalf));
  sounds.set_group_volume(MixGroup::Voice, kHalf);
  CHECK(driver.voices.at(voice).mix.volume == doctest::Approx(kQuarter));
  CHECK(driver.voices.at(effect).mix.volume == doctest::Approx(kHalf));
  const auto later = sounds.play("line", flat_at(kFullVolume, MixGroup::Voice));
  CHECK(driver.voices.at(later).mix.volume == doctest::Approx(kHalf));
}

TEST_CASE("mix groups are named as scripts name them") {
  for (const MixGroup group : kMixGroups) CHECK(mix_group_from_name(mix_group_name(group)) == group);
  CHECK(mix_group_from_name("voice") == MixGroup::Voice);
  CHECK_FALSE(mix_group_from_name("master").has_value());
}

TEST_CASE("Audio.play returns a handle that loops, changes its levels, says it plays and stops it") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  CHECK(ruby(*engine, "$hum = Audio.play(:beep, loop: true, volume: 0.5)") == "1");
  const testing::RecordedVoice& hum = audio.voices().at(1);
  CHECK(hum.sound.loop);
  CHECK_FALSE(hum.sound.positional);
  CHECK(hum.sound.group == MixGroup::Effects);
  CHECK(hum.mix.volume == doctest::Approx(kHalf));
  CHECK(ruby(*engine, "Audio.set($hum, volume: 0.25, pitch: 1.5)") == "true");
  CHECK(audio.voices().at(1).mix.volume == doctest::Approx(kQuarter));
  CHECK(audio.voices().at(1).sound.pitch == doctest::Approx(kRaisedPitch));
  CHECK(ruby(*engine, "Audio.playing?($hum)") == "true");
  CHECK(ruby(*engine, "Audio.stop($hum)") == "true");
  CHECK(ruby(*engine, "Audio.playing?($hum)") == "false");
  CHECK(ruby(*engine, "Audio.set($hum, volume: 1)") == "false");
  CHECK(ruby(*engine, "Audio.play(:nope).nil?") == "true");
}

TEST_CASE("a sound that does not loop lets its handle go when it comes to its end") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  const std::string id = ruby(*engine, "$beep = Audio.play(:beep)");
  CHECK(ruby(*engine, "Audio.playing?($beep)") == "true");
  audio.finish(static_cast<SoundSourceId>(std::stoul(id)));
  frames(*engine, 1);
  CHECK(ruby(*engine, "Audio.playing?($beep)") == "false");
  CHECK(audio.voices().empty());
}

TEST_CASE("a sound picks its group, and the player's volume of that group is over it") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  ruby(*engine, "$line = Audio.play(:beep, group: :voice); $step = Audio.play(:beep)");
  ruby(*engine, "$sting = Audio.play_at(:beep, [0, 0, 0], group: :music)");
  ruby(*engine, "Settings.voice_volume = 0.5; Settings.music_volume = 0.25");
  CHECK(audio.voices().at(1).mix.volume == doctest::Approx(kHalf));
  CHECK(audio.voices().at(2).mix.volume == doctest::Approx(kFullVolume));
  CHECK(audio.voices().at(3).mix.volume == doctest::Approx(kQuarter));
  CHECK(audio.group_volume(MixGroup::Voice) == doctest::Approx(kHalf));
  ruby(*engine, "Settings.effects_volume = 0.5");
  CHECK(audio.voices().at(2).mix.volume == doctest::Approx(kHalf));
}

TEST_CASE("Audio refuses a group, a pitch or a fade it does not know before anything plays") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  const std::string invalid[] = {"Audio.play(:beep, group: :ambience)",
                                 "Audio.play(:beep, pitch: 9)",
                                 "Audio.play(:beep, volume: -1)",
                                 "Audio.play_at(:beep, [0, 0, 0], group: :master)",
                                 "Audio.set(1, pitch: 0)",
                                 "Audio.set(1, volume: -1)",
                                 "Audio.music(:beep, fade: -1)",
                                 "Audio.stop_music(fade: -1)"};
  for (const auto& code : invalid) {
    INFO(code);
    CHECK_FALSE(engine->scripts().eval(code, "invalid_sound_handles.rb").ok);
  }
  CHECK(audio.voices().empty());
  CHECK(audio.tracks().empty());
}

TEST_CASE("Audio.music crossfades from the track playing, and stop_music fades it out") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  CHECK(ruby(*engine, "Audio.music(:beep)") == "true");
  CHECK(ruby(*engine, "Audio.music(:beep, fade: " + std::to_string(kFadeSeconds) + ")") == "true");
  CHECK(audio.tracks().size() == 2);
  frames(*engine, kFramesPastFade);
  REQUIRE(audio.tracks().size() == 1);
  CHECK(audio.tracks().begin()->second.volume == doctest::Approx(kMusicVolume));
  CHECK(ruby(*engine, "Audio.music(:nope, fade: 1)") == "false");
  CHECK(audio.music() == "beep");  // the track playing goes on
  ruby(*engine, "Audio.stop_music(fade: " + std::to_string(kFadeSeconds) + ")");
  CHECK_FALSE(audio.music().has_value());
  CHECK(audio.tracks().size() == 1);  // fading out
  frames(*engine, kFramesPastFade);
  CHECK(audio.tracks().empty());
}

TEST_CASE("the player's music volume is over the music") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  ruby(*engine, "Audio.music(:beep); Settings.music_volume = 0.5");
  REQUIRE(audio.tracks().size() == 1);
  CHECK(audio.tracks().begin()->second.volume == doctest::Approx(kMusicVolume * kHalf));
}

TEST_CASE("a game's sounds and its player's group volumes end with it") {
  testing::RecordingAudio audio;  // made first: the engine plays through it until its end
  {
    auto engine = sandbox(audio);
    ruby(*engine, "Audio.play(:beep, loop: true); Audio.play_at(:beep, [0, 0, 0], loop: true)");
    ruby(*engine, "Settings.voice_volume = 0.5");
    REQUIRE(audio.voices().size() == 2);
  }
  CHECK(audio.voices().empty());
  CHECK(audio.group_volume(MixGroup::Voice) == doctest::Approx(kFullVolume));
}
