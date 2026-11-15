// Sounds behind walls: the occlusion math, the pool that follows it, and the level's static bodies
// that answer whether a wall is in the way (a real physics world, the recording audio).

#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>

#include <cmath>
#include <memory>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "app/sound_walls.hpp"
#include "audio/occlusion.hpp"
#include "core/config.hpp"
#include "physics/body_config.hpp"
#include "physics/physics_world.hpp"
#include "test_support.hpp"
#include "testing/recording_audio.hpp"

using namespace redgem;

namespace {

constexpr float kHalfWay = 0.5f;
constexpr float kQuarter = 0.25f;
constexpr float kThreeQuarters = 0.75f;
constexpr float kQuarterFade = kDefaultOcclusionFade * kQuarter;
constexpr float kTwice = 2.0f;
constexpr float kLongFrame = 10.0f;
constexpr float kNoTime = 0.0f;
constexpr Vec3 kListener{0, 1, 0};
constexpr Vec3 kBehind{0, 1, -6};    // past the wall
constexpr Vec3 kBeside{4, 1, 0};     // in the open
constexpr Vec3 kWallAt{0, 1, -3};    // between them
constexpr Vec3 kWallHalf{3, 2, 0.25f};
constexpr Vec3 kCrateHalf{0.5f, 0.5f, 0.5f};

enum Ids : ObjectId { kWall = 1, kCrate = 2 };

// A recording audio whose walls are a flag the test sets.
struct WalledAudio {
  testing::RecordingAudio audio;
  bool wall = false;
  WalledAudio() {
    audio.set_sounds_dir(test::fixture("games/sandbox") / config::kSoundsDirName);
    audio.set_occlusion_query([this](Vec3, Vec3) { return wall; });
  }
};

SpatialSound at(Vec3 position) {
  SpatialSound sound;
  sound.position = position;
  sound.loop = true;
  return sound;
}

BodyDesc box(ObjectId id, PropertyMap props, Vec3 position, Vec3 half) {
  BodyDesc desc;
  desc.object = id;
  desc.config = body_config_from_props(props);
  desc.world.position = position;
  desc.bounds.include(-half);
  desc.bounds.include(half);
  return desc;
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

}  // namespace

TEST_CASE("a sound's occlusion moves toward what was found over the fade, and no further") {
  const OcclusionSettings settings;
  CHECK(occlusion_step(kOpen, true, kQuarterFade, settings) == doctest::Approx(kQuarter));
  CHECK(occlusion_step(kOpen, true, kLongFrame, settings) == doctest::Approx(kBehindWall));
  CHECK(occlusion_step(kBehindWall, false, kQuarterFade, settings) == doctest::Approx(kThreeQuarters));
  CHECK(occlusion_step(kHalfWay, false, kLongFrame, settings) == doctest::Approx(kOpen));
  OcclusionSettings at_once = settings;
  at_once.fade_seconds = kNoTime;
  CHECK(occlusion_step(kOpen, true, kNoTime, at_once) == doctest::Approx(kBehindWall));
}

TEST_CASE("an occluded sound is quieter and muffled, the more the further behind the wall") {
  const OcclusionSettings settings;
  CHECK(occluded_volume(kOpen, settings) == doctest::Approx(kFullVolume));
  CHECK(occluded_volume(kBehindWall, settings) == doctest::Approx(kDefaultOccludedVolume));
  CHECK(occluded_volume(kHalfWay, settings) == doctest::Approx((kFullVolume + kDefaultOccludedVolume) * kHalfWay));
  CHECK_FALSE(occluded_effects(VoiceEffects{}, kOpen, settings).low_pass_hz.has_value());
  CHECK(occluded_effects(VoiceEffects{}, kBehindWall, settings).low_pass_hz == doctest::Approx(kDefaultOccludedCutoff));
  const float halfway = *occluded_effects(VoiceEffects{}, kHalfWay, settings).low_pass_hz;
  CHECK(halfway == doctest::Approx(std::sqrt(kHighestCutoff * kDefaultOccludedCutoff)));  // (halfway in octaves)
  VoiceEffects lower;
  lower.low_pass_hz = kLowestCutoff;
  CHECK(occluded_effects(lower, kBehindWall, settings).low_pass_hz == doctest::Approx(kLowestCutoff));
}

TEST_CASE("occlusion settings are valid within their ranges, and full volume with no filter is none") {
  OcclusionSettings settings;
  CHECK(valid_occlusion(settings));
  CHECK(occludes(settings));
  settings.volume = kFullVolume;
  settings.low_pass_hz = std::nullopt;
  CHECK(valid_occlusion(settings));
  CHECK_FALSE(occludes(settings));
  settings.volume = kFullVolume + kHalfWay;
  CHECK_FALSE(valid_occlusion(settings));
  settings.volume = kHalfWay;
  settings.low_pass_hz = kLowestCutoff * kHalfWay;
  CHECK_FALSE(valid_occlusion(settings));
  settings.low_pass_hz = std::nullopt;
  settings.fade_seconds = -kHalfWay;
  CHECK_FALSE(valid_occlusion(settings));
}

TEST_CASE("a sound behind a wall starts muffled, and fades open when the wall is gone") {
  WalledAudio walled;
  walled.wall = true;
  const auto id = walled.audio.play_spatial_sound("beep", at(kBehind));
  REQUIRE(id != kNoSoundSource);
  const auto& voice = walled.audio.voices().at(id);
  CHECK(voice.effects.low_pass_hz == doctest::Approx(kDefaultOccludedCutoff));
  const float muffled = voice.mix.volume;
  walled.wall = false;
  walled.audio.update(kQuarterFade);
  CHECK(walled.audio.voices().at(id).mix.volume > muffled);
  walled.audio.update(kLongFrame);
  CHECK_FALSE(walled.audio.voices().at(id).effects.low_pass_hz.has_value());
  CHECK(walled.audio.voices().at(id).mix.volume == doctest::Approx(muffled / kDefaultOccludedVolume));
}

TEST_CASE("walls do not muffle flat sounds, sounds that say so, sounds too far to hear, or anything when off") {
  WalledAudio walled;
  walled.wall = true;
  const auto flat = walled.audio.play_sound("beep", FlatSound{});
  SpatialSound free = at(kBehind);
  free.occludable = false;
  const auto open = walled.audio.play_spatial_sound("beep", free);
  const auto far = walled.audio.play_spatial_sound("beep", at({0, 0, -kDefaultSoundReach * kTwice}));
  for (const auto id : {flat, open, far}) CHECK_FALSE(walled.audio.voices().at(id).effects.low_pass_hz.has_value());
  OcclusionSettings off;
  off.volume = kFullVolume;
  off.low_pass_hz = std::nullopt;
  REQUIRE(walled.audio.set_occlusion(off));
  const auto behind = walled.audio.play_spatial_sound("beep", at(kBehind));
  CHECK_FALSE(walled.audio.voices().at(behind).effects.low_pass_hz.has_value());
}

TEST_CASE("a static body between two points muffles; a moving one, or one aside, does not") {
  auto world = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  REQUIRE_FALSE(world->add_object(box(kWall, {{"collider", std::string("box")}}, kWallAt, kWallHalf)));
  CHECK(static_body_between(*world, kListener, kBehind));
  CHECK_FALSE(static_body_between(*world, kListener, kBeside));
  CHECK_FALSE(static_body_between(*world, kListener, kListener));
  REQUIRE_FALSE(world->add_object(box(kCrate, {{"body", std::string("dynamic")}}, kBeside * kHalfWay, kCrateHalf)));
  CHECK_FALSE(static_body_between(*world, kListener, kBeside));
}

TEST_CASE("the recording audio over a real world: a sound behind a static wall is muffled") {
  testing::RecordingAudio audio;
  audio.set_sounds_dir(test::fixture("games/sandbox") / config::kSoundsDirName);
  auto world = create_jolt_physics_world({0.0f, config::kGravityY, 0.0f});
  REQUIRE_FALSE(world->add_object(box(kWall, {{"collider", std::string("box")}}, kWallAt, kWallHalf)));
  audio.set_occlusion_query([&](Vec3 from, Vec3 to) { return static_body_between(*world, from, to); });
  AudioListener listener;
  listener.position = kListener;
  REQUIRE(audio.set_listener(listener));
  const auto behind = audio.play_spatial_sound("beep", at(kBehind));
  const auto beside = audio.play_spatial_sound("beep", at(kBeside));
  CHECK(audio.voices().at(behind).effects.low_pass_hz.has_value());
  CHECK_FALSE(audio.voices().at(beside).effects.low_pass_hz.has_value());
}

TEST_CASE("Audio.occlusion changes how a sound behind a wall is heard, and play_at may turn it off") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  CHECK(engine->scripts().eval("Audio.occlusion(volume: 0.2, low_pass: 500, fade: 0.1)", "occlusion.rb").ok);
  CHECK(engine->scripts().eval("Audio.occlusion(volume: 1, low_pass: nil)", "occlusion.rb").ok);
  CHECK(engine->scripts().eval("Audio.play_at(:beep, [0, 0, 0], occlusion: false)", "occlusion.rb").ok);
  CHECK_FALSE(audio.voices().at(1).sound.occludable);
  for (const char* code : {"Audio.occlusion(volume: 2)", "Audio.occlusion(low_pass: 5)", "Audio.occlusion(fade: -1)",
                           "Audio.play_at(:beep, [0, 0, 0], occlusion: 1)"}) {
    INFO(code);
    CHECK_FALSE(engine->scripts().eval(code, "invalid_occlusion.rb").ok);
  }
}
