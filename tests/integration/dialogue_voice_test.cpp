// A dialogue line's voice in a running game, heard through the recording audio: its clip plays
// from the speaker's head in the voice group, the line waits for it and goes on as it ends (or as
// the player skips it), the music ducks under it, and a jaw follows its loudness.

#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <memory>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "test_support.hpp"
#include "testing/recording_audio.hpp"

using namespace redgem;

namespace {

constexpr float kTolerance = 0.01f;
constexpr int kFewFrames = 10;
constexpr int kSecondOfFrames = 60;  // far past the ducking's attack and the envelope's
constexpr float kLoudVoice = 0.5f;   // past VoiceMouth::FULL: the mouth all open
constexpr const char* kVoice = "beep";
const Vec3 kSpeakerAt{4.0f, 0.0f, 0.0f};
const Vec3 kHeadAt{4.0f, 1.7f, 0.0f};

// A speaker (copied from the sandbox's crate) with a head and a jaw under it, and a dialogue of two
// voiced lines: the first goes on to the second, which waits for a choice.
constexpr const char* kSetup = R"(
  $speaker = Scene.spawn(Scene.find('Crate'), physics: false, name: 'Baker', position: Vector3.new(4, 0, 0),
                          rotation: Vector3.zero)
  $head = Scene.spawn(Scene.find('Crate'), physics: false, name: 'Head', parent: $speaker,
                      position: Vector3.new(4, 1.7, 0), rotation: Vector3.zero)
  $jaw = Scene.spawn(Scene.find('Crate'), physics: false, name: 'Jaw', parent: $head,
                     position: Vector3.new(4, 1.6, 0), rotation: Vector3.zero)
  $script = Dialogue::Script.new('baker', { 'start' => 'hello', 'nodes' => {
    'hello' => { 'text' => 'baker.hello', 'voice' => 'beep', 'next' => 'bye' },
    'bye' => { 'text' => 'baker.bye', 'voice' => 'beep', 'choices' => [{ 'text' => 'baker.ok', 'end' => true }] } } })
  $talk = ->(speaker) { Dialogue.start($script, actor: Scene.find('Hero'), speaker: speaker) }
  nil
)";

std::unique_ptr<Engine> sandbox(testing::RecordingAudio& audio) {
  EngineOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.fixed_step = true;
  options.audio = &audio;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

std::string ruby(Engine& engine, const std::string& code) {
  const auto result = engine.scripts().eval(code, "dialogue_voice_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

void frames(Engine& engine, int count) {
  for (int frame = 0; frame < count; ++frame) engine.frame(config::kFixedDeltaSeconds);
}

// The handle of the voice playing, the only one of the voice group; 0 when none plays.
SoundSourceId voice_playing(const testing::RecordingAudio& audio) {
  for (const auto& [id, voice] : audio.voices()) {
    if (voice.sound.group == MixGroup::Voice && voice.playing) return id;
  }
  return kNoSoundSource;
}

void check_at(const Vec3& position, const Vec3& expected) {
  CHECK(position.x == doctest::Approx(expected.x).epsilon(kTolerance));
  CHECK(position.y == doctest::Approx(expected.y).epsilon(kTolerance));
  CHECK(position.z == doctest::Approx(expected.z).epsilon(kTolerance));
}

}  // namespace

TEST_CASE("a voiced line plays its clip from the speaker's head, and waits for it to end") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  ruby(*engine, kSetup);
  ruby(*engine, "$talk.call($speaker)");
  const SoundSourceId line = voice_playing(audio);
  REQUIRE(line != kNoSoundSource);
  const testing::RecordedVoice& voice = audio.voices().at(line);
  CHECK(voice.name == kVoice);
  CHECK(voice.sound.positional);
  check_at(voice.sound.position, kHeadAt);
  frames(*engine, kFewFrames);
  CHECK(ruby(*engine, "Dialogue.current.node_id") == "hello");
  CHECK(ruby(*engine, "Dialogue.current.speaking?") == "true");
  audio.finish(line);
  frames(*engine, 1);
  CHECK(ruby(*engine, "Dialogue.current.node_id") == "bye");
  const SoundSourceId next = voice_playing(audio);
  REQUIRE(next != kNoSoundSource);
  CHECK(next != line);
  // A line with choices waits for one, its clip over.
  audio.finish(next);
  frames(*engine, kFewFrames);
  CHECK(ruby(*engine, "Dialogue.current.node_id") == "bye");
  CHECK(ruby(*engine, "Dialogue.current.speaking?") == "false");
}

TEST_CASE("skipping a voiced line stops its clip, and leaving the conversation stops the next") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  ruby(*engine, kSetup);
  ruby(*engine, "$talk.call($speaker)");
  const SoundSourceId first = voice_playing(audio);
  ruby(*engine, "Dialogue.current.continue");
  CHECK(audio.voices().count(first) == 0);
  const SoundSourceId second = voice_playing(audio);
  REQUIRE(second != kNoSoundSource);
  ruby(*engine, "Dialogue.stop");
  CHECK(audio.voices().count(second) == 0);
}

TEST_CASE("a speaker with no head is heard from where it is, and its voice follows it") {
  // (The listener is where it starts, at the origin facing -Z: a voice on its right pans right.)
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  ruby(*engine, kSetup);
  ruby(*engine, "$head.destroy");
  frames(*engine, 1);
  ruby(*engine, "$talk.call($speaker)");
  const SoundSourceId line = voice_playing(audio);
  REQUIRE(line != kNoSoundSource);
  check_at(audio.voices().at(line).sound.position, kSpeakerAt);
  CHECK(audio.voices().at(line).mix.pan > 0.0f);
  ruby(*engine, "$speaker.transform.position = Vector3.new(-4, 0, 0)");
  frames(*engine, 1);
  CHECK(audio.voices().at(line).mix.pan < 0.0f);
}

TEST_CASE("a line whose voice has no sound waits for the player, as one with no voice does") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  ruby(*engine, kSetup);
  ruby(*engine, "$script.node('hello')['voice'] = 'nothing_of_that_name'; $talk.call($speaker)");
  frames(*engine, kFewFrames);
  CHECK(ruby(*engine, "Dialogue.current.node_id") == "hello");
  CHECK(ruby(*engine, "Dialogue.current.speaking?") == "false");
}

TEST_CASE("the music ducks while a line is spoken") {
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  ruby(*engine, kSetup);
  ruby(*engine, "Audio.music(:beep); $talk.call($speaker)");
  frames(*engine, kSecondOfFrames);
  CHECK(audio.music_ducked() < kFullVolume);
  ruby(*engine, "Dialogue.stop");
  frames(*engine, kSecondOfFrames);
  CHECK(audio.music_ducked() == doctest::Approx(kFullVolume));
}

TEST_CASE("a speaker's jaw opens with its voice's loudness and shuts as it ends") {
  constexpr float kJawAngle = 20.0f;
  testing::RecordingAudio audio;
  auto engine = sandbox(audio);
  ruby(*engine, kSetup);
  ruby(*engine, "$speaker.add_component(VoiceMouth, jaw: 'Jaw', angle: 20.0)");
  ruby(*engine, "$rest = $jaw.transform.local_rotation.x; $talk.call($speaker)");
  const SoundSourceId line = voice_playing(audio);
  REQUIRE(line != kNoSoundSource);
  audio.speak(line, kLoudVoice);
  frames(*engine, kSecondOfFrames);
  CHECK(std::stof(ruby(*engine, "Audio.loudness(" + std::to_string(line) + ")")) == doctest::Approx(kLoudVoice));
  CHECK(std::stof(ruby(*engine, "$jaw.transform.local_rotation.x - $rest")) == doctest::Approx(kJawAngle));
  CHECK(ruby(*engine, "$speaker.get_component(VoiceMouth).openness") == "1.0");
  ruby(*engine, "Dialogue.stop");
  frames(*engine, 1);
  CHECK(std::stof(ruby(*engine, "$jaw.transform.local_rotation.x - $rest")) == doctest::Approx(0.0f));
}
