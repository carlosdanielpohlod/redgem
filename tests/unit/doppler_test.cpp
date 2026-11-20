// The Doppler effect: the shift of a moving sound or listener, the pool following it, and the same
// through Audio.play_at, Audio.move, Audio.listener and Audio.doppler on the recording audio.

#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>

#include <memory>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "audio/doppler.hpp"
#include "core/config.hpp"
#include "test_support.hpp"
#include "testing/recording_audio.hpp"

using namespace redgem;

namespace {

constexpr float kCarSpeed = 30.0f;  // m/s, about 108 km/h
constexpr float kSoundSpeedShare = 0.5f;
constexpr float kRaisedPitch = 1.5f;
constexpr Vec3 kOrigin{};
constexpr Vec3 kAhead{0, 0, -10};
constexpr Vec3 kApproaching{0, 0, kCarSpeed};  // toward the origin from ahead
constexpr Vec3 kLeaving{0, 0, -kCarSpeed};
constexpr Vec3 kCrossing{kCarSpeed, 0, 0};

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
  const auto result = engine.scripts().eval(code, "doppler_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

SpatialSound moving(Vec3 velocity) {
  SpatialSound sound;
  sound.position = kAhead;
  sound.velocity = velocity;
  sound.loop = true;
  return sound;
}

}  // namespace

TEST_CASE("a sound coming nearer is higher, one going away lower, one crossing unchanged") {
  const float nearer = doppler_shift(kOrigin, {}, kAhead, kApproaching, kRealDoppler);
  const float away = doppler_shift(kOrigin, {}, kAhead, kLeaving, kRealDoppler);
  CHECK(nearer == doctest::Approx(kSpeedOfSound / (kSpeedOfSound - kCarSpeed)));
  CHECK(away == doctest::Approx(kSpeedOfSound / (kSpeedOfSound + kCarSpeed)));
  CHECK(doppler_shift(kOrigin, {}, kAhead, kCrossing, kRealDoppler) == doctest::Approx(kNoShift));
}

TEST_CASE("a listener moving toward a still sound hears it higher") {
  const float toward = doppler_shift(kOrigin, kLeaving, kAhead, {}, kRealDoppler);
  CHECK(toward == doctest::Approx((kSpeedOfSound + kCarSpeed) / kSpeedOfSound));
}

TEST_CASE("the scale strengthens or turns off the shift, and speeds past half of sound's are cut") {
  CHECK(doppler_shift(kOrigin, {}, kAhead, kApproaching, kNoDoppler) == doctest::Approx(kNoShift));
  const float strong = doppler_shift(kOrigin, {}, kAhead, kApproaching, kMostDoppler);
  CHECK(strong == doctest::Approx(1.0f / (1.0f - kSoundSpeedShare)));
  CHECK(doppler_shift(kOrigin, {}, kOrigin, kApproaching, kRealDoppler) == doctest::Approx(kNoShift));
}

TEST_CASE("the pool plays a moving sound at its pitch times its shift, and follows a new velocity") {
  testing::RecordingAudio audio;
  audio.set_sounds_dir(std::filesystem::path(test::fixture("games/sandbox")) / config::kSoundsDirName);
  SpatialSound car = moving(kApproaching);
  car.pitch = kRaisedPitch;
  const auto id = audio.play_spatial_sound("beep", car);
  REQUIRE(id != kNoSoundSource);
  const float nearer = doppler_shift(kOrigin, {}, kAhead, kApproaching, kRealDoppler);
  CHECK(audio.voices().at(id).sound.pitch == doctest::Approx(kRaisedPitch * nearer));
  REQUIRE(audio.move_sound(id, kAhead, kLeaving));
  const float away = doppler_shift(kOrigin, {}, kAhead, kLeaving, kRealDoppler);
  CHECK(audio.voices().at(id).sound.pitch == doctest::Approx(kRaisedPitch * away));
  REQUIRE(audio.set_doppler(kNoDoppler));
  CHECK(audio.voices().at(id).sound.pitch == doctest::Approx(kRaisedPitch));
  CHECK_FALSE(audio.set_doppler(kMostDoppler + kRealDoppler));
}

TEST_CASE("a flat sound has no Doppler shift") {
  testing::RecordingAudio audio;
  audio.set_sounds_dir(std::filesystem::path(test::fixture("games/sandbox")) / config::kSoundsDirName);
  AudioListener running;
  running.velocity = kApproaching;
  REQUIRE(audio.set_listener(running));
  const auto id = audio.play_sound("beep", FlatSound{});
  CHECK(audio.voices().at(id).sound.pitch == doctest::Approx(kAsRecorded));
}

TEST_CASE("Ruby gives velocities to play_at, move and listener, and scales the effect") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  ruby(*engine, "$car = Audio.play_at(:beep, [0, 0, -10], loop: true, velocity: [0, 0, 30])");
  const float nearer = doppler_shift(kOrigin, {}, kAhead, kApproaching, kRealDoppler);
  CHECK(audio.voices().at(1).sound.pitch == doctest::Approx(nearer));
  CHECK(ruby(*engine, "Audio.move($car, [0, 0, -10])") == "true");  // (still: no velocity given)
  CHECK(audio.voices().at(1).sound.pitch == doctest::Approx(kAsRecorded));
  ruby(*engine, "Audio.move($car, [0, 0, -10], [0, 0, 30]); Audio.move($car, [0, 0, -10], nil)");
  CHECK(audio.voices().at(1).sound.pitch == doctest::Approx(kAsRecorded));
  ruby(*engine, "Audio.play_at(:beep, [0, 0, -10], loop: true, velocity: nil)");
  CHECK(audio.voices().at(2).sound.pitch == doctest::Approx(kAsRecorded));
  ruby(*engine, "Audio.listener([0, 0, 0], [0, 0, -1], [0, 1, 0], [0, 0, -30])");
  CHECK(audio.voices().at(1).sound.pitch > kAsRecorded);
  CHECK(ruby(*engine, "Audio.doppler = 0") == "0.0");
  CHECK(audio.voices().at(1).sound.pitch == doctest::Approx(kAsRecorded));
  for (const char* code : {"Audio.doppler = -1", "Audio.doppler = 11", "Audio.play_at(:beep, [0, 0, 0], velocity: 3)",
                           "Audio.listener([0, 0, 0], [0, 0, -1], [0, 1, 0], [0, 0, Float::NAN])"}) {
    INFO(code);
    CHECK_FALSE(engine->scripts().eval(code, "invalid_doppler.rb").ok);
  }
}

TEST_CASE("the game's moving ears end with it") {
  testing::RecordingAudio audio;
  {
    auto engine = sandbox(audio);
    ruby(*engine, "Audio.listener([0, 0, 0], [0, 0, -1], [0, 1, 0], [0, 0, -30])");
  }
  const auto id = audio.play_spatial_sound("beep", moving({}));
  REQUIRE(id != kNoSoundSource);
  CHECK(audio.voices().at(id).sound.pitch == doctest::Approx(kAsRecorded));
}
