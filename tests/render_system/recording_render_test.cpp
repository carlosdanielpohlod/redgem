// System test: a game played into a video, in a real window, through the real encoder (skipped on
// machines without ffmpeg): the video holds exactly the frames asked for, and the game's sound for
// as long.
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <raylib.h>

#include "app/recording.hpp"
#include "app/window_presenter.hpp"
#include "core/exit_codes.hpp"

using namespace redgem;

namespace {

constexpr int kFps = 10;
constexpr double kSeconds = 1.0;
constexpr long kFrames = 10;  // kFps x kSeconds
constexpr FrameSize kSmall{160, 90};
constexpr int kBlack = 8;        // a channel no brighter than this is black
constexpr int kOutputLine = 64;  // characters: room for a number
constexpr double kToneSeconds = 2.0;
constexpr double kToneHertz = 440.0;  // the tone game's sounds/tone.wav
constexpr double kHertzOff = 5.0;     // what the encoded tone may be off by
constexpr double kOneFrame = 1.0 / kFps;
constexpr float kHeard = 0.1f;  // a peak at least this loud: the tone is in the sound

#ifdef _WIN32
constexpr const char* kNowhere = " > NUL 2>&1";
std::FILE* read_from(const std::string& command) { return _popen(("\"" + command + "\"").c_str(), "r"); }
std::string quoted(const std::string& path) { return "\"" + path + "\""; }
void close(std::FILE* pipe) { _pclose(pipe); }
#else
constexpr const char* kNowhere = " > /dev/null 2>&1";
std::FILE* read_from(const std::string& command) { return popen(command.c_str(), "r"); }
// The video's path for the shell: single quotes, which take everything as it is but a single quote.
std::string quoted(const std::string& path) {
  std::string text = "'";
  for (const char c : path) text += c == '\'' ? std::string("'\\''") : std::string(1, c);
  return text + "'";
}
void close(std::FILE* pipe) { pclose(pipe); }
#endif

bool has_program(const char* program) {
  return std::system((shell_command({program, "-version"}) + kNowhere).c_str()) == 0;
}

// The first line `arguments` print about the video (its path last); empty when there is none.
std::string first_line(std::vector<std::string> arguments, const std::filesystem::path& video) {
  std::FILE* pipe = read_from(shell_command(arguments) + " " + quoted(video.string()));
  if (pipe == nullptr) return "";
  std::array<char, kOutputLine> line{};
  const bool read = std::fgets(line.data(), kOutputLine, pipe) != nullptr;
  close(pipe);
  return read ? std::string(line.data()) : "";
}

// How many frames ffprobe counts in the video's first stream.
long frames_in(const std::filesystem::path& video) {
  const std::string line = first_line({"ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0",
                                       "-show_entries", "stream=nb_read_frames", "-of", "csv=p=0"},
                                      video);
  return line.empty() ? -1 : std::strtol(line.c_str(), nullptr, 10);
}

// How many seconds the video's first sound stream lasts, by ffprobe; less than nothing without one.
double sound_seconds_in(const std::filesystem::path& video) {
  const std::string line = first_line(
      {"ffprobe", "-v", "error", "-select_streams", "a:0", "-show_entries", "stream=duration", "-of", "csv=p=0"},
      video);
  return line.empty() ? -1.0 : std::strtod(line.c_str(), nullptr);
}

// The video's sound decoded, mono, at the rate it was recorded at.
std::vector<float> sound_in(const std::filesystem::path& video) {
  const std::filesystem::path raw = std::filesystem::temp_directory_path() / "redgem_recorded_sound.f32";
  const std::string command =
      shell_command({kEncoderProgram, "-y", "-v", "error", "-i", video.string(), "-map", "0:a:0", "-f", "f32le", "-ac",
                     "1", "-ar", std::to_string(kRecordSampleRate), raw.string()});
  if (std::system(command.c_str()) != 0) return {};
  std::vector<float> samples(std::filesystem::file_size(raw) / sizeof(float));
  std::ifstream file(raw, std::ios::binary);
  file.read(reinterpret_cast<char*>(samples.data()), static_cast<std::streamsize>(samples.size() * sizeof(float)));
  std::filesystem::remove(raw);
  return samples;
}

// How often a second the sound between `from` and `to` (sample indexes) goes up through nothing.
double rises_a_second(const std::vector<float>& samples, std::size_t from, std::size_t to) {
  int rises = 0;
  for (std::size_t at = from + 1; at < to; ++at) rises += samples[at - 1] < 0.0f && samples[at] >= 0.0f ? 1 : 0;
  return rises * static_cast<double>(kRecordSampleRate) / static_cast<double>(to - from);
}

// A game, recorded into `video` for some seconds (the empty game for kSeconds unless given).
GameWindowOptions recording_into(const std::filesystem::path& video,
                                 const std::filesystem::path& game = std::filesystem::path(REDGEM_TEMPLATES_DIR) /
                                                                     "empty",
                                 double seconds = kSeconds) {
  GameWindowOptions options;
  options.game_dir = game;
  RecordOptions record;
  record.file = video;
  record.fps = kFps;
  record.size = kSmall;
  record.seconds = seconds;
  options.record = record;
  return options;
}

}  // namespace

TEST_CASE("a game recorded for a second at ten frames a second makes a video of ten frames") {
  if (!has_program(kEncoderProgram) || !has_program("ffprobe")) {
    MESSAGE("ffmpeg is not on this machine: nothing to record with");
    return;
  }
  // (A name a shell would read otherwise: the encoder is given it as it is.)
  const std::filesystem::path video = std::filesystem::temp_directory_path() / "redgem $HOME it's.mp4";
  std::filesystem::remove(video);
  GameWindowOptions options = recording_into(video);
  int shown = 0;  // the brightest channel of the window's middle, as the last frame left it
  options.on_frame_drawn = [&shown] {
    Image screen = LoadImageFromScreen();
    const ::Color middle = GetImageColor(screen, screen.width / 2, screen.height / 2);
    shown = std::max({middle.r, middle.g, middle.b});
    UnloadImage(screen);
  };
  CHECK(play_in_window(options) == kExitSuccess);
  CHECK(
      shown >
      kBlack);  // the window shows what is recorded, not a black screen  // ends by itself once the second is recorded
  REQUIRE(std::filesystem::exists(video));
  CHECK(frames_in(video) == kFrames);
}

TEST_CASE("a recording whose encoder stops ends the run at once, with an error") {
  if (!has_program(kEncoderProgram)) {
    MESSAGE("ffmpeg is not on this machine: nothing to record with");
    return;
  }
  // The encoder cannot write into a folder that is not there: it stops on the first frames.
  const std::filesystem::path nowhere = std::filesystem::temp_directory_path() / "redgem_no_such_folder" / "x.mp4";
  CHECK(play_in_window(recording_into(nowhere)) == kExitFailure);
}

TEST_CASE("a game playing a tone is recorded with its sound: the tone, for as long as the pictures") {
  if (!has_program(kEncoderProgram) || !has_program("ffprobe")) {
    MESSAGE("ffmpeg or ffprobe is not on this machine: the recording's sound is not checked");
    return;
  }
  const std::filesystem::path video = std::filesystem::temp_directory_path() / "redgem_tone.mp4";
  std::filesystem::remove(video);
  const std::filesystem::path game = std::filesystem::path(REDGEM_FIXTURES_DIR) / "games" / "tone";
  REQUIRE(play_in_window(recording_into(video, game, kToneSeconds)) == kExitSuccess);
  REQUIRE(std::filesystem::exists(video));
  RecordOptions record;
  record.file = video;
  CHECK_FALSE(std::filesystem::exists(pictures_file(record)));  // (both parts are gone)
  CHECK_FALSE(std::filesystem::exists(sound_file(record)));
  CHECK(frames_in(video) == static_cast<long>(kToneSeconds * kFps));
  CHECK(std::abs(sound_seconds_in(video) - kToneSeconds) <= kOneFrame);
  const std::vector<float> sound = sound_in(video);
  REQUIRE(sound.size() > static_cast<std::size_t>(kRecordSampleRate));
  // The middle second: past the first frame, before the last.
  const auto quarter = sound.size() / 4;
  CHECK(std::abs(rises_a_second(sound, quarter, sound.size() - quarter) - kToneHertz) <= kHertzOff);
  CHECK(*std::max_element(sound.begin(), sound.end()) > kHeard);
  std::filesystem::remove(video);
}

TEST_CASE("a recording asked for with no sound has none") {
  if (!has_program(kEncoderProgram) || !has_program("ffprobe")) {
    MESSAGE("ffmpeg or ffprobe is not on this machine: nothing to record with");
    return;
  }
  const std::filesystem::path video = std::filesystem::temp_directory_path() / "redgem_silent.mp4";
  std::filesystem::remove(video);
  GameWindowOptions options = recording_into(video, std::filesystem::path(REDGEM_FIXTURES_DIR) / "games" / "tone");
  options.record->sound = false;
  REQUIRE(play_in_window(options) == kExitSuccess);
  CHECK(frames_in(video) == kFrames);
  CHECK(sound_seconds_in(video) < 0.0);
  std::filesystem::remove(video);
}
