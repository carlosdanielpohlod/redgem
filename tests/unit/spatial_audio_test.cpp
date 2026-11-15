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
#include "testing/recording_audio.hpp"
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
class SpatialTestDriver final : public SoundDriver {
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
  void pitch(SoundSourceId id, float pitch) override { voices.at(id).sound.pitch = pitch; }
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
  SoundPool sounds(driver);
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
  SoundPool sounds(driver);
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
  SoundPool sounds(driver);
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
  SoundPool sounds(driver);
  driver.accept = false;
  CHECK(sounds.play("missing", {}) == kNoSoundSource);
  CHECK(driver.voices.empty());
  driver.accept = true;
  for (std::size_t count = 0; count < kMaxSounds; ++count) REQUIRE(sounds.play("ambient", {}) != kNoSoundSource);
  SpatialSound minor;
  minor.priority = kLowestSoundPriority;
  CHECK(sounds.play("overflow", minor) == kNoSoundSource);
  CHECK(driver.voices.size() == kMaxSounds);
  CHECK(sounds.stop(driver.voices.begin()->first));
  CHECK(sounds.play("replacement", {}) != kNoSoundSource);
}

TEST_CASE("spatial pool destruction releases every active loop and silent backends refuse playback") {
  SpatialTestDriver driver;
  {
    SoundPool sounds(driver);
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

std::unique_ptr<Engine> spatial_engine(testing::RecordingAudio& audio) {
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
  testing::RecordingAudio audio;
  auto engine = spatial_engine(audio);
  CHECK(spatial_ruby(*engine, "$voice = Audio.play_at(:beep, [1, 0, 0], volume: 0.5, pitch: 1.5, loop: true)") == "1");
  REQUIRE(audio.voices().size() == 1);
  CHECK(audio.voices().at(1).sound.loop);
  CHECK(audio.voices().at(1).sound.pitch == doctest::Approx(1.5f));
  CHECK(audio.voices().at(1).mix.volume == kSpatialHalf);
  CHECK(spatial_ruby(*engine, "Audio.listener([0, 0, 0], [1, 0, 0], [0, 1, 0])") == "true");
  CHECK(audio.voices().at(1).mix.pan == 0.0f);
  CHECK(spatial_ruby(*engine, "Audio.move($voice, Vector3.new(0, 0, 1))") == "true");
  CHECK(audio.voices().at(1).mix.pan == 1.0f);
  CHECK(spatial_ruby(*engine, "Audio.playing?($voice)") == "true");
  CHECK(spatial_ruby(*engine, "Audio.stop($voice)") == "true");
  CHECK(spatial_ruby(*engine, "Audio.playing?($voice)") == "false");
}

TEST_CASE("Ruby positional audio refuses invalid input before creating a voice") {
  testing::RecordingAudio audio;
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
  CHECK(audio.voices().empty());
}

TEST_CASE("Ruby spatial sources report playback failures and stop with the flat sounds") {
  testing::RecordingAudio audio;
  auto engine = spatial_engine(audio);
  CHECK(spatial_ruby(*engine, "Audio.play_at(:missing, [0, 0, 0]).nil?") == "true");
  CHECK(spatial_ruby(*engine, "Audio.play_at(:beep, [0, 0, 0]); Audio.play(:beep)") == "2");
  CHECK(audio.voices().size() == 2);
  spatial_ruby(*engine, "Audio.stop_sources");
  CHECK(audio.voices().empty());
  CHECK(spatial_ruby(*engine, "Audio.move(1, [0, 0, 0])") == "false");
}

namespace {

constexpr int kMinorPriority = kDefaultSoundPriority - 1;
constexpr int kMajorPriority = kDefaultSoundPriority + 1;

SpatialSound with_priority(int priority, float volume = 1.0f) {
  SpatialSound sound;
  sound.positional = false;
  sound.volume = volume;
  sound.priority = priority;
  return sound;
}

// Fills the pool with sounds of the default priority; returns the first one's id.
SoundSourceId fill(SoundPool& sounds) {
  const SoundSourceId first = sounds.play("crowd", with_priority(kDefaultSoundPriority));
  for (std::size_t count = 1; count < kMaxSounds; ++count) sounds.play("crowd", with_priority(kDefaultSoundPriority));
  return first;
}

}  // namespace

TEST_CASE("a full pool lets a new sound take the place of the oldest of the weakest ones") {
  SpatialTestDriver driver;
  SoundPool sounds(driver);
  const SoundSourceId oldest = fill(sounds);
  const SoundSourceId shot = sounds.play("shot", with_priority(kDefaultSoundPriority));
  REQUIRE(shot != kNoSoundSource);
  CHECK_FALSE(sounds.playing(oldest));
  CHECK(driver.voices.count(oldest) == 0);
  CHECK(driver.voices.size() == kMaxSounds);
  CHECK(sounds.playing(oldest + 1));
}

TEST_CASE("a lower priority is taken before a quieter sound, and the quietest before the oldest") {
  SpatialTestDriver driver;
  SoundPool sounds(driver);
  const SoundSourceId oldest = fill(sounds);
  REQUIRE(sounds.stop(oldest + 2));
  REQUIRE(sounds.stop(oldest + 3));
  const SoundSourceId quiet = sounds.play("whisper", with_priority(kDefaultSoundPriority, kSpatialHalf));
  const SoundSourceId minor = sounds.play("bird", with_priority(kMinorPriority));
  CHECK(sounds.play("shot", with_priority(kMajorPriority)) != kNoSoundSource);
  CHECK_FALSE(sounds.playing(minor));
  CHECK(sounds.playing(quiet));
  CHECK(sounds.play("shot", with_priority(kMajorPriority)) != kNoSoundSource);
  CHECK_FALSE(sounds.playing(quiet));
  CHECK(sounds.playing(oldest));
}

TEST_CASE("a sound weaker than every one playing does not play, and a failed start steals nothing") {
  SpatialTestDriver driver;
  SoundPool sounds(driver);
  const SoundSourceId oldest = fill(sounds);
  CHECK(sounds.play("bird", with_priority(kMinorPriority)) == kNoSoundSource);
  driver.accept = false;
  CHECK(sounds.play("missing", with_priority(kHighestSoundPriority)) == kNoSoundSource);
  CHECK(sounds.playing(oldest));
  CHECK(driver.voices.size() == kMaxSounds);
}

TEST_CASE("the quietest as heard is the one taken: a sound far away gives its place first") {
  SpatialTestDriver driver;
  SoundPool sounds(driver);
  const SoundSourceId oldest = fill(sounds);
  REQUIRE(sounds.stop(oldest + 1));
  SpatialSound far = spatial_at({kSpatialFar * kSpatialHalf, 0, 0});
  const SoundSourceId distant = sounds.play("river", far);
  CHECK(sounds.play("shot", with_priority(kDefaultSoundPriority)) != kNoSoundSource);
  CHECK_FALSE(sounds.playing(distant));
  CHECK(sounds.playing(oldest));
}

TEST_CASE("Ruby sounds past the limit take the place of a lower priority, and a weaker one gets nil") {
  testing::RecordingAudio audio;
  auto engine = spatial_engine(audio);
  spatial_ruby(*engine, "$bird = Audio.play(:beep, loop: true, priority: 1)");
  spatial_ruby(*engine, std::to_string(kMaxSounds - 1) + ".times { Audio.play_at(:beep, [0, 0, 0], loop: true) }");
  CHECK(audio.voices().size() == kMaxSounds);
  CHECK(spatial_ruby(*engine, "Audio.play(:beep, priority: 0).nil?") == "true");
  CHECK(spatial_ruby(*engine, "Audio.play_at(:beep, [0, 0, 0], priority: 255).nil?") == "false");
  CHECK(spatial_ruby(*engine, "Audio.playing?($bird)") == "false");
  CHECK(audio.voices().size() == kMaxSounds);
}

TEST_CASE("Ruby refuses a priority that is not a whole number from 0 to 255") {
  testing::RecordingAudio audio;
  auto engine = spatial_engine(audio);
  for (const char* code :
       {"Audio.play(:beep, priority: 256)", "Audio.play(:beep, priority: -1)", "Audio.play(:beep, priority: 1.5)",
        "Audio.play_at(:beep, [0, 0, 0], priority: 300)", "Audio.play_at(:beep, [0, 0, 0], priority: :high)"}) {
    INFO(code);
    CHECK_FALSE(engine->scripts().eval(code, "invalid_priority.rb").ok);
  }
  CHECK(audio.voices().empty());
}
