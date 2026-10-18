// System test: a game played into a video, in a real window, through the real encoder (skipped on
// machines without ffmpeg): the video holds exactly the frames asked for.
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

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

// How many frames ffprobe counts in the video's first stream.
long frames_in(const std::filesystem::path& video) {
  const std::string command = shell_command({"ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0",
                                             "-show_entries", "stream=nb_read_frames", "-of", "csv=p=0"}) +
                              " " + quoted(video.string());
  std::FILE* pipe = read_from(command);
  if (pipe == nullptr) return -1;
  std::array<char, kOutputLine> line{};
  const bool read = std::fgets(line.data(), kOutputLine, pipe) != nullptr;
  close(pipe);
  return read ? std::strtol(line.data(), nullptr, 10) : -1;
}

// The empty game, recorded into `video` for kSeconds.
GameWindowOptions recording_into(const std::filesystem::path& video) {
  GameWindowOptions options;
  options.game_dir = std::filesystem::path(REDGEM_TEMPLATES_DIR) / "empty";
  RecordOptions record;
  record.file = video;
  record.fps = kFps;
  record.size = kSmall;
  record.seconds = kSeconds;
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
