// Recording a game to a video: what the command line's values mean, the fixed step every recorded
// frame advances the game by, when a recording is over, and what the encoder is asked for.
#include <doctest/doctest.h>

#include <algorithm>
#include <string>

#include "app/recording.hpp"

using namespace redgem;

namespace {

constexpr int kFps = 30;
constexpr double kSeconds = 2.0;
constexpr std::int64_t kFramesInTwoSeconds = 60;

bool has(const std::vector<std::string>& arguments, const std::string& wanted) {
  return std::find(arguments.begin(), arguments.end(), wanted) != arguments.end();
}

bool mentions(const std::vector<std::string>& arguments, const std::string& part) {
  return std::any_of(arguments.begin(), arguments.end(),
                     [&part](const std::string& argument) { return argument.find(part) != std::string::npos; });
}

}  // namespace

TEST_CASE("sizes are read as <width>x<height>, both above nothing") {
  CHECK(frame_size_from("3840x2160") == FrameSize{3840, 2160});
  for (const char* wrong : {"", "1920", "x1080", "1920x", "0x1080", "-5x10", "19a0x1080", "1920x1080x2"}) {
    CHECK_FALSE(frame_size_from(wrong));
  }
}

TEST_CASE("frames a second and seconds are read within their limits") {
  CHECK(record_fps_from("60") == 60);
  CHECK_FALSE(record_fps_from("0"));
  CHECK_FALSE(record_fps_from(std::to_string(kMostRecordFps + 1)));
  CHECK_FALSE(record_fps_from("30.5"));
  REQUIRE(record_seconds_from("1.5"));
  CHECK(*record_seconds_from("1.5") == doctest::Approx(1.5));
  for (const char* wrong : {"", "0", "-2", "two", "1.5s", "inf"}) CHECK_FALSE(record_seconds_from(wrong));
}

TEST_CASE("every recorded frame advances the game by exactly one frame's time, however long it took") {
  CHECK(recorded_frame_seconds(kFps) == doctest::Approx(1.0 / kFps));
  CHECK(recorded_frame_seconds(60) == doctest::Approx(1.0 / 60.0));
}

TEST_CASE("a recording of some seconds ends after that many frames; one without ends with the game") {
  RecordOptions options;
  options.fps = kFps;
  CHECK_FALSE(frames_to_record(options));
  CHECK_FALSE(recording_complete(options, kFramesInTwoSeconds * 100));
  options.seconds = kSeconds;
  CHECK(frames_to_record(options) == kFramesInTwoSeconds);
  CHECK_FALSE(recording_complete(options, kFramesInTwoSeconds - 1));
  CHECK(recording_complete(options, kFramesInTwoSeconds));
}

TEST_CASE("the encoder takes raw frames of the drawn size, flips them, and scales them when asked") {
  RecordOptions options;
  options.file = "clip.mp4";
  options.fps = kFps;
  const std::vector<std::string> plain = encoder_arguments(options, FrameSize{640, 360});
  CHECK(plain.front() == kEncoderProgram);
  CHECK(plain.back() == "clip.mp4");
  CHECK(has(plain, "640x360"));
  CHECK(has(plain, "rgb24"));
  CHECK(has(plain, "libx264"));
  CHECK(has(plain, "yuv420p"));
  CHECK(has(plain, "vflip"));
  CHECK_FALSE(mentions(plain, "lanczos"));
  options.scale = FrameSize{320, 180};
  CHECK(has(encoder_arguments(options, FrameSize{640, 360}), "vflip,scale=320:180:flags=lanczos"));
}

TEST_CASE("the encoder's command line quotes every argument, so that paths with spaces go through") {
  CHECK(shell_command({"ffmpeg", "-i", "my clip.mp4"}) == "\"ffmpeg\" \"-i\" \"my clip.mp4\"");
}
