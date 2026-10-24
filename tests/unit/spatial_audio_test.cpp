#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#include <cmath>
#include <limits>
#include <map>
#include <string>

#include "audio/audio.hpp"
#include "app/engine.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr float kSpatialNear = 1.0f;
constexpr float kSpatialFar = 10.0f;
constexpr float kSpatialHalf = 0.5f;

struct RecordedSpatialVoice {
  std::string name;
  SpatialSound sound;
  SpatialMix mix;
  bool active = true;
};
class SpatialTestDriver final : public SpatialAudioDriver {
 public:
  std::map<SoundSourceId, RecordedSpatialVoice> voices;
  bool accept = true;
  unsigned stopped = 0;
  unsigned started = 0;
  bool start(SoundSourceId id, const std::string& name, const SpatialSound& sound, SpatialMix mix) override {
    if (!accept) return false;
    voices.emplace(id, RecordedSpatialVoice{name, sound, mix});
    ++started;
    return true;
  }
  void update(SoundSourceId) override {}
  bool playing(SoundSourceId id) const override { return voices.at(id).active; }
  void mix(SoundSourceId id, SpatialMix mix) override { voices.at(id).mix = mix; }
  void stop(SoundSourceId id) override {
    voices.erase(id);
    ++stopped;
  }
};

SpatialSound spatial_at(Vec3 position) {
  SpatialSound sound;
  sound.position = position;
  sound.reference_distance = kSpatialNear;
  sound.max_distance = kSpatialFar;
  return sound;
}

}  // namespace

TEST_CASE("spatial audio resolves left right center and listener rotation in three dimensions") {
  AudioListener listener;
  CHECK(spatial_sound_mix(listener, spatial_at({1, 0, 0}))->pan == doctest::Approx(1.0f));
  CHECK(spatial_sound_mix(listener, spatial_at({-1, 0, 0}))->pan == doctest::Approx(-1.0f));
  CHECK(spatial_sound_mix(listener, spatial_at({0, 0, -1}))->pan == doctest::Approx(0.0f));
  CHECK(spatial_sound_mix(listener, spatial_at({0, 1, 0}))->pan == doctest::Approx(0.0f));
  CHECK(spatial_sound_mix(listener, spatial_at({0, 0, 0}))->volume == doctest::Approx(1.0f));
  listener.forward = {1, 0, 0};
  CHECK(spatial_sound_mix(listener, spatial_at({0, 0, 1}))->pan == doctest::Approx(1.0f));
  listener.forward = {0, 0, -1};
  listener.up = {1, 0, 0};
  CHECK(spatial_sound_mix(listener, spatial_at({0, -1, 0}))->pan == doctest::Approx(1.0f));
}

TEST_CASE("spatial sound amplitude decreases continuously with three dimensional distance") {
  SpatialSound sound = spatial_at({0, 0, 0});
  sound.volume = kSpatialHalf;
  float previous = sound.volume;
  for (int distance = 0; distance <= static_cast<int>(kSpatialFar); ++distance) {
    sound.position = {0, static_cast<float>(distance), 0};
    const auto mix = spatial_sound_mix({}, sound);
    REQUIRE(mix.has_value());
    CHECK(mix->volume <= previous);
    CHECK(mix->volume >= 0.0f);
    previous = mix->volume;
  }
  CHECK(previous == 0.0f);
  sound.max_distance = std::numeric_limits<float>::max();
  sound.volume = 1.0f;
  sound.position = {0, 0, 2};
  CHECK(spatial_sound_mix({}, sound)->volume == doctest::Approx(kSpatialHalf));
  sound.position = {0, 3, 4};
  CHECK(spatial_sound_mix({}, sound)->volume == doctest::Approx(0.2f));
}

TEST_CASE("spatial math remains finite for extreme finite coordinates and scaled listener directions") {
  const float large = std::numeric_limits<float>::max();
  AudioListener listener;
  listener.position = {-large, 0, 0};
  listener.forward = {0, 0, -large};
  listener.up = {0, large, 0};
  const auto mix = spatial_sound_mix(listener, spatial_at({large, 0, 0}));
  REQUIRE(mix.has_value());
  CHECK(mix->volume == 0.0f);
  CHECK(mix->pan == doctest::Approx(1.0f));
}

TEST_CASE("spatial audio rejects invalid geometry levels and falloff radii") {
  AudioListener listener;
  SpatialSound sound;
  const float invalid = std::numeric_limits<float>::quiet_NaN();
  SUBCASE("zero forward") { listener.forward = {}; }
  SUBCASE("parallel axes") { listener.forward = listener.up; }
  SUBCASE("nonfinite listener") { listener.position.x = invalid; }
  SUBCASE("nonfinite source") { sound.position.y = invalid; }
  SUBCASE("negative gain") { sound.volume = -1.0f; }
  SUBCASE("zero pitch") { sound.pitch = 0.0f; }
  SUBCASE("unsafe high pitch") { sound.pitch = kMaxSpatialPitch + 1.0f; }
  SUBCASE("unsafe low pitch") { sound.pitch = kMinSpatialPitch * kSpatialHalf; }
  SUBCASE("infinite pitch") { sound.pitch = std::numeric_limits<float>::infinity(); }
  SUBCASE("zero reference") { sound.reference_distance = 0.0f; }
  SUBCASE("inverted radii") { sound.max_distance = sound.reference_distance; }
  SUBCASE("nonfinite radius") { sound.max_distance = invalid; }
  CHECK_FALSE(spatial_sound_mix(listener, sound).has_value());
}

TEST_CASE("positional stereo downmix removes source channel bias without crossing frame bounds") {
  constexpr unsigned kFrames = 3;
  constexpr float kSentinel = 0.123f;
  const float large = std::numeric_limits<float>::max();
  float samples[] = {1.0f, 0.0f, -1.0f, 1.0f, large, large, kSentinel};
  spatial_mono_frames(samples, kFrames);
  CHECK(samples[0] == kSpatialHalf);
  CHECK(samples[1] == kSpatialHalf);
  CHECK(samples[2] == 0.0f);
  CHECK(samples[3] == 0.0f);
  CHECK(samples[4] == large);
  CHECK(samples[5] == large);
  CHECK(samples[6] == kSentinel);
  spatial_mono_frames(nullptr, 0);
}

TEST_CASE("two spatial sources of one asset keep independent position and playback ownership") {
  SpatialTestDriver driver;
  SpatialSoundPool sounds(driver);
  const auto left = sounds.play("step", spatial_at({-1, 0, 0}));
  const auto right = sounds.play("step", spatial_at({1, 0, 0}));
  REQUIRE(left != kNoSoundSource);
  REQUIRE(right != left);
  CHECK(driver.voices.at(left).mix.pan == -1.0f);
  CHECK(driver.voices.at(right).mix.pan == 1.0f);
  CHECK(sounds.move(left, {0, 0, -1}));
  CHECK(driver.voices.at(left).mix.pan == 0.0f);
  CHECK(driver.voices.at(right).mix.pan == 1.0f);
  CHECK(sounds.stop(left));
  CHECK_FALSE(sounds.playing(left));
  CHECK(sounds.playing(right));
}

TEST_CASE("listener changes refresh active voices while rejected input leaves their mix unchanged") {
  SpatialTestDriver driver;
  SpatialSoundPool sounds(driver);
  const auto id = sounds.play("bird", spatial_at({1, 0, 0}));
  REQUIRE(id != kNoSoundSource);
  AudioListener listener;
  listener.forward = {1, 0, 0};
  CHECK(sounds.set_listener(listener));
  CHECK(driver.voices.at(id).mix.pan == 0.0f);
  listener.up = listener.forward;
  CHECK_FALSE(sounds.set_listener(listener));
  CHECK_FALSE(sounds.move(id, {std::numeric_limits<float>::infinity(), 0, 0}));
  CHECK(driver.voices.at(id).mix.pan == 0.0f);
}

TEST_CASE("spatial voice completion and clear release resources without reusing stale handles") {
  SpatialTestDriver driver;
  SpatialSoundPool sounds(driver);
  const auto finished = sounds.play("impact", {});
  driver.voices.at(finished).active = false;
  sounds.update();
  CHECK(driver.voices.empty());
  CHECK_FALSE(sounds.stop(finished));
  const auto next = sounds.play("impact", {});
  CHECK(next > finished);
  sounds.clear();
  CHECK_FALSE(sounds.move(next, {}));
  CHECK(sounds.play("impact", {}) > next);
}

TEST_CASE("spatial voice resources are bounded and failed starts do not leave handles behind") {
  SpatialTestDriver driver;
  SpatialSoundPool sounds(driver);
  driver.accept = false;
  CHECK(sounds.play("missing", {}) == kNoSoundSource);
  CHECK(driver.voices.empty());
  driver.accept = true;
  for (std::size_t count = 0; count < kMaxSpatialSounds; ++count) REQUIRE(sounds.play("ambient", {}) != kNoSoundSource);
  CHECK(sounds.play("overflow", {}) == kNoSoundSource);
  CHECK(driver.voices.size() == kMaxSpatialSounds);
  CHECK(sounds.stop(driver.voices.begin()->first));
  CHECK(sounds.play("replacement", {}) != kNoSoundSource);
}

TEST_CASE("spatial pool destruction releases every active loop and silent backends refuse playback") {
  SpatialTestDriver driver;
  {
    SpatialSoundPool sounds(driver);
    SpatialSound loop;
    loop.loop = true;
    REQUIRE(sounds.play("wind", loop) != kNoSoundSource);
    CHECK(driver.voices.begin()->second.sound.loop);
  }
  CHECK(driver.voices.empty());
  CHECK(driver.stopped == 1);
  NullAudio silent;
  CHECK(silent.play_spatial_sound("wind", {}) == kNoSoundSource);
  CHECK_FALSE(silent.stop_sound(1));
}

namespace {

class SpatialTestAudio final : public AudioSystem {
 public:
  SpatialTestDriver driver;
  SpatialSoundPool pool{driver};
  bool play_sound(const std::string&, float, float) override { return true; }
  bool play_music(const std::string&, bool, float) override { return true; }
  void stop_music() override {}
  void set_music_pitch(float) override {}
  void set_music_volume(float) override {}
  void set_master_volume(float) override {}
  void update() override { pool.update(); }
  void set_sounds_dir(const std::filesystem::path&) override { pool.clear(); }
  SoundSourceId play_spatial_sound(const std::string& name, const SpatialSound& sound) override {
    return pool.play(name, sound);
  }
  bool set_listener(const AudioListener& listener) override { return pool.set_listener(listener); }
  bool move_sound(SoundSourceId id, Vec3 position) override { return pool.move(id, position); }
  bool sound_playing(SoundSourceId id) const override { return pool.playing(id); }
  bool stop_sound(SoundSourceId id) override { return pool.stop(id); }
  void stop_spatial_sounds() override { pool.clear(); }
};
std::unique_ptr<Engine> spatial_engine(SpatialTestAudio& audio) {
  EngineOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.fixed_step = true;
  options.audio = &audio;
  auto result = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(result));
  return std::move(std::get<std::unique_ptr<Engine>>(result));
}
std::string spatial_ruby(Engine& engine, const std::string& code) {
  const auto result = engine.scripts().eval(code, "spatial_audio_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

}  // namespace

TEST_CASE("Ruby positional audio exposes independent handles explicit listener orientation and movement") {
  SpatialTestAudio audio;
  auto engine = spatial_engine(audio);
  CHECK(spatial_ruby(*engine, "$voice = Audio.play_at(:beep, [1, 0, 0], volume: 0.5, pitch: 1.5, loop: true)") == "1");
  REQUIRE(audio.driver.voices.size() == 1);
  CHECK(audio.driver.voices.at(1).sound.loop);
  CHECK(audio.driver.voices.at(1).sound.pitch == doctest::Approx(1.5f));
  CHECK(audio.driver.voices.at(1).mix.volume == kSpatialHalf);
  CHECK(spatial_ruby(*engine, "Audio.listener([0, 0, 0], [1, 0, 0], [0, 1, 0])") == "true");
  CHECK(audio.driver.voices.at(1).mix.pan == 0.0f);
  CHECK(spatial_ruby(*engine, "Audio.move($voice, Vector3.new(0, 0, 1))") == "true");
  CHECK(audio.driver.voices.at(1).mix.pan == 1.0f);
  CHECK(spatial_ruby(*engine, "Audio.playing?($voice)") == "true");
  CHECK(spatial_ruby(*engine, "Audio.stop($voice)") == "true");
  CHECK(spatial_ruby(*engine, "Audio.playing?($voice)") == "false");
}

TEST_CASE("Ruby positional audio refuses invalid input before creating a voice") {
  SpatialTestAudio audio;
  auto engine = spatial_engine(audio);
  auto* vm = engine->scripts().vm().state();
  mrb_gv_set(vm, mrb_intern_lit(vm, "$audio_nan"), mrb_float_value(vm, std::numeric_limits<mrb_float>::quiet_NaN()));
  const std::string invalid[] = {"Audio.play_at(:beep, [0, 0, 0], volume: -1)",
                                 "Audio.play_at(:beep, [0, 0, 0], pitch: 0)",
                                 "Audio.play_at(:beep, [0, 0, 0], pitch: 1e-300)",
                                 "Audio.play_at(:beep, [0, 0, 0], reference_distance: 2, max_distance: 1)",
                                 "Audio.play_at(:beep, [0, 0, 0], loop: 1)",
                                 "Audio.play_at(:beep, [0, 0, 0], unknown: true)",
                                 "Audio.play_at(:beep, [0, $audio_nan, 0])",
                                 "Audio.listener([0, 0, 0], [0, 1, 0], [0, 1, 0])",
                                 "Audio.stop(-1)",
                                 "Audio.stop(1.5)"};
  for (const auto& code : invalid) {
    INFO(code);
    CHECK_FALSE(engine->scripts().eval(code, "invalid_spatial_audio.rb").ok);
  }
  CHECK(audio.driver.voices.empty());
}

TEST_CASE("Ruby spatial sources report playback failures and stop without interrupting flat audio") {
  SpatialTestAudio audio;
  auto engine = spatial_engine(audio);
  audio.driver.accept = false;
  CHECK(spatial_ruby(*engine, "Audio.play_at(:missing, [0, 0, 0]).nil?") == "true");
  audio.driver.accept = true;
  CHECK(spatial_ruby(*engine, "Audio.play_at(:beep, [0, 0, 0]); Audio.play(:beep)") == "true");
  CHECK(audio.driver.voices.size() == 1);
  spatial_ruby(*engine, "Audio.stop_sources");
  CHECK(audio.driver.voices.empty());
  CHECK(spatial_ruby(*engine, "Audio.move(1, [0, 0, 0])") == "false");
}
