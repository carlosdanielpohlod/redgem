// A game's own mix groups: where they hang, what they are heard at and what effects their sounds
// get; and the same through Audio.group and Audio.effects on an engine with the recording audio.

#include <doctest/doctest.h>
#include <mruby.h>
#include <mruby/string.h>

#include <memory>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "audio/mix_group_tree.hpp"
#include "core/config.hpp"
#include "test_support.hpp"
#include "testing/recording_audio.hpp"

using namespace redgem;

namespace {

constexpr float kHalfShare = 0.5f;
constexpr float kQuarter = 0.25f;
constexpr float kEighth = 0.125f;
constexpr float kCaveCutoff = 1200.0f;
constexpr float kDeepCutoff = 400.0f;
constexpr float kCaveReverb = 0.4f;
constexpr float kDeepReverb = 0.8f;

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
  const auto result = engine.scripts().eval(code, "mix_group_tree_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

}  // namespace

TEST_CASE("a game's group hangs under another and is heard at its volume times theirs") {
  MixGroupTree groups;
  const auto cave = groups.define("cave", MixGroup::Effects, kHalfShare);
  REQUIRE(cave);
  const auto drips = groups.define("drips", *cave, kHalfShare);
  REQUIRE(drips);
  CHECK(groups.find("drips") == drips);
  CHECK(groups.find("effects") == MixGroup::Effects);
  groups.set_player_volume(MixGroup::Effects, kHalfShare);
  CHECK(groups.heard_volume(*drips) == doctest::Approx(kEighth));
  CHECK(groups.within(*drips, MixGroup::Effects));
  CHECK_FALSE(groups.within(*drips, MixGroup::Voice));
  CHECK(groups.set_gain(*cave, kHalfShare));
  CHECK_FALSE(groups.set_gain(*cave, kHalfShare));  // (no change)
  CHECK(groups.heard_volume(*drips) == doctest::Approx(kEighth * kHalfShare));
}

TEST_CASE("a group defined again moves and changes its volume, but never under itself") {
  MixGroupTree groups;
  const auto cave = groups.define("cave", MixGroup::Effects, kFullVolume);
  const auto drips = groups.define("drips", *cave, kFullVolume);
  CHECK(groups.define("cave", MixGroup::Voice, kQuarter) == cave);
  CHECK(groups.within(*drips, MixGroup::Voice));
  CHECK(groups.heard_volume(*drips) == doctest::Approx(kQuarter));
  CHECK_FALSE(groups.define("cave", *drips, kFullVolume).has_value());
  CHECK_FALSE(groups.define("cave", *cave, kFullVolume).has_value());
  CHECK_FALSE(groups.define("voice", MixGroup::Music, kFullVolume).has_value());
}

TEST_CASE("a game has room for its own groups up to the limit, and they go when it forgets them") {
  MixGroupTree groups;
  for (std::size_t count = kMixGroupCount; count < kMaxMixGroups; ++count)
    REQUIRE(groups.define("group" + std::to_string(count), MixGroup::Effects, kFullVolume));
  CHECK_FALSE(groups.define("one_more", MixGroup::Effects, kFullVolume).has_value());
  REQUIRE(groups.set_effects(MixGroup::Music, GroupEffects{kCaveCutoff, kCaveReverb}));
  groups.set_player_volume(MixGroup::Music, kHalfShare);
  groups.forget_game_groups();
  CHECK_FALSE(groups.find("group3").has_value());
  CHECK(groups.define("one_more", MixGroup::Effects, kFullVolume).has_value());
  CHECK(groups.effects_of(MixGroup::Music) == VoiceEffects{});
  CHECK(groups.player_volume(MixGroup::Music) == doctest::Approx(kHalfShare));  // (the player's stays)
}

TEST_CASE("a sound gets the lowest cutoff over it and the reverb of the nearest group with one") {
  MixGroupTree groups;
  const auto cave = groups.define("cave", MixGroup::Effects, kFullVolume);
  const auto deep = groups.define("deep", *cave, kFullVolume);
  REQUIRE(groups.set_effects(*cave, GroupEffects{kDeepCutoff, kCaveReverb}));
  REQUIRE(groups.set_effects(*deep, GroupEffects{kCaveCutoff, kDeepReverb}));
  const VoiceEffects effects = groups.effects_of(*deep);
  CHECK(effects.low_pass_hz == kDeepCutoff);
  CHECK(effects.reverb == doctest::Approx(kDeepReverb));
  CHECK(effects.reverb_bus == *deep);
  REQUIRE(groups.set_effects(*deep, GroupEffects{}));
  CHECK(groups.effects_of(*deep).reverb_bus == *cave);
  CHECK_FALSE(groups.set_effects(*deep, GroupEffects{kCaveCutoff, kFullReverb + kHalfShare}));
}

TEST_CASE("Audio.group and Audio.effects reach the sounds of a game's group and the groups over it") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  ruby(*engine, "Audio.group(:cave, volume: 0.5); $drip = Audio.play(:beep, loop: true, group: :cave)");
  ruby(*engine, "Settings.effects_volume = 0.5");
  CHECK(audio.voices().at(1).mix.volume == doctest::Approx(kQuarter));
  CHECK(ruby(*engine, "Audio.effects(:cave, low_pass: 1200, reverb: 0.4)") == "true");
  const VoiceEffects& effects = audio.voices().at(1).effects;
  CHECK(effects.low_pass_hz == kCaveCutoff);
  CHECK(effects.reverb == doctest::Approx(kCaveReverb));
  CHECK(ruby(*engine, "Audio.effects(:effects, low_pass: 400)") == "true");
  CHECK(audio.voices().at(1).effects.low_pass_hz == kDeepCutoff);  // (the lower one, from over it)
  ruby(*engine, "Audio.effects(:cave); Audio.effects(:effects)");
  CHECK(audio.voices().at(1).effects == VoiceEffects{});
}

TEST_CASE("a game's group under the voice ducks the music, and the game's groups end with it") {
  testing::RecordingAudio audio;
  {
    auto engine = sandbox(audio);
    ruby(*engine, "Audio.ducking(volume: 0.5, attack: 0); Audio.music(:beep, volume: 1)");
    ruby(*engine, "Audio.group(:narrator, parent: :voice); Audio.play(:beep, loop: true, group: :narrator)");
    engine->frame(config::kFixedDeltaSeconds);
    CHECK(audio.tracks().begin()->second.volume == doctest::Approx(kHalfShare));
  }
  CHECK_FALSE(audio.find_group("narrator").has_value());
}

TEST_CASE("Audio.group and Audio.effects refuse what they cannot do") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  for (const char* code :
       {"Audio.group(:music)", "Audio.group(:cave, parent: :nowhere)", "Audio.group(:cave, volume: -1)",
        "Audio.effects(:nowhere, reverb: 0.5)", "Audio.effects(:effects, low_pass: 5)",
        "Audio.effects(:effects, reverb: 2)", "Audio.play(:beep, group: :cave)"}) {
    INFO(code);
    CHECK_FALSE(engine->scripts().eval(code, "invalid_groups.rb").ok);
  }
  CHECK(audio.voices().empty());
}
