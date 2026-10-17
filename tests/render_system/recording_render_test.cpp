// System test: a game played into a video, in a real window, through the real encoder (skipped on
// machines without ffmpeg): the video holds exactly the frames asked for.
#include <doctest/doctest.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "app/recording.hpp"
#include "app/window_presenter.hpp"
#include "core/exit_codes.hpp"

using namespace redgem;

namespace {

constexpr int kFps = 10;
constexpr double kSeconds = 1.0;
constexpr long kFrames = 10;  // kFps x kSeconds
constexpr FrameSize kSmall{160, 90};
constexpr int kOutputLine = 64;  // characters: room for a number

#ifdef _WIN32
constexpr const char* kNowhere = " > NUL 2>&1";
std::FILE* read_from(const std::string& command) { return _popen(("\"" + command + "\"").c_str(), "r"); }
void close(std::FILE* pipe) { _pclose(pipe); }
#else
constexpr const char* kNowhere = " > /dev/null 2>&1";
std::FILE* read_from(const std::string& command) { return popen(command.c_str(), "r"); }
void close(std::FILE* pipe) { pclose(pipe); }
#endif

bool has_program(const char* program) {
  return std::system((shell_command({program, "-version"}) + kNowhere).c_str()) == 0;
}

// How many frames ffprobe counts in the video's first stream.
long frames_in(const std::filesystem::path& video) {
  const std::string command =
      shell_command({"ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0", "-show_entries",
                     "stream=nb_read_frames", "-of", "csv=p=0", video.string()});
  std::FILE* pipe = read_from(command);
  if (pipe == nullptr) return -1;
  std::array<char, kOutputLine> line{};
  const bool read = std::fgets(line.data(), kOutputLine, pipe) != nullptr;
  close(pipe);
  return read ? std::strtol(line.data(), nullptr, 10) : -1;
}

}  // namespace

TEST_CASE("a game recorded for a second at ten frames a second makes a video of ten frames") {
  if (!has_program(kEncoderProgram) || !has_program("ffprobe")) {
    MESSAGE("ffmpeg is not on this machine: nothing to record with");
    return;
  }
  const std::filesystem::path video = std::filesystem::temp_directory_path() / "redgem_recording_test.mp4";
  std::filesystem::remove(video);
  GameWindowOptions options;
  options.game_dir = std::filesystem::path(REDGEM_TEMPLATES_DIR) / "empty";
  RecordOptions record;
  record.file = video;
  record.fps = kFps;
  record.size = kSmall;
  record.seconds = kSeconds;
  options.record = record;
  CHECK(play_in_window(options) == kExitSuccess);  // ends by itself once the second is recorded
  REQUIRE(std::filesystem::exists(video));
  CHECK(frames_in(video) == kFrames);
}
