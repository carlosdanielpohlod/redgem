#pragma once

// Recording a game to a video (redgem <game_dir> --record out.mp4): what is asked for, read from
// the command line, and what follows from it: the fixed time each frame advances the game by, how
// many frames are recorded, and how the encoder (ffmpeg) is asked to make the video. No window and
// no encoder needed, so it can be tested.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace redgem {

inline constexpr int kDefaultRecordFps = 30;
inline constexpr int kMostRecordFps = 240;
// The encoder, found on the PATH, and what it is asked for: H.264 at high quality, in the pixel
// format every player plays.
inline constexpr const char* kEncoderProgram = "ffmpeg";

// Width and height of a recorded picture, in pixels.
struct FrameSize {
  int width = 0;
  int height = 0;
  bool operator==(const FrameSize& other) const { return width == other.width && height == other.height; }
};

struct RecordOptions {
  std::filesystem::path file;  // the video; empty: nothing is recorded
  int fps = kDefaultRecordFps;
  // The size the game is drawn at (unless given, the window's), whatever the window's or the
  // desktop's size.
  std::optional<FrameSize> size;
  // The size of the video, when it is not `size`: the frames are scaled to it (lanczos).
  std::optional<FrameSize> scale;
  std::optional<double> seconds;  // the recording stops after this long (else when the game quits)
};

// `size` with each side rounded down to an even number of pixels (at least two): the video's
// pixel format keeps color for two by two pixels, and its encoder refuses odd sides.
FrameSize even(FrameSize size);

// Where a picture of `picture`'s size is shown in a window of `window`'s: as large as fits, in
// its middle (bars fill the rest).
struct Letterbox {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};
Letterbox letterbox(FrameSize window, FrameSize picture);
// What takes a point of the window to the same point of the picture: picture = (window + offset) * scale.
struct PointMapping {
  float offset_x = 0.0f;
  float offset_y = 0.0f;
  float scale_x = 1.0f;
  float scale_y = 1.0f;
};
PointMapping window_to_picture(FrameSize window, FrameSize picture);

// "1920x1080" as a size; nothing for anything else (or a side that is not a positive number).
std::optional<FrameSize> frame_size_from(const std::string& text);
// A whole number of frames a second, 1 to kMostRecordFps.
std::optional<int> record_fps_from(const std::string& text);
// A positive number of seconds.
std::optional<double> record_seconds_from(const std::string& text);

// What every recorded frame advances the game by, whatever the real time it took.
float recorded_frame_seconds(int fps);
// How many frames a recording of `options.seconds` holds; nothing when it has no end.
std::optional<std::int64_t> frames_to_record(const RecordOptions& options);
// Whether a recording that has `frames` frames is over.
bool recording_complete(const RecordOptions& options, std::int64_t frames);

// The encoder's arguments: raw RGB frames of `source` size on its standard input, upside down (as
// a framebuffer is read), into the video.
std::vector<std::string> encoder_arguments(const RecordOptions& options, FrameSize source);
// Those arguments as one command line for Windows' shell, each quoted. (Elsewhere the encoder is
// started with them as they are, through no shell.)
std::string shell_command(const std::vector<std::string>& arguments);

}  // namespace redgem
