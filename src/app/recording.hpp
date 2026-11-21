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

#include "core/letterbox.hpp"

namespace redgem {

inline constexpr int kDefaultRecordFps = 30;
inline constexpr int kMostRecordFps = 240;
// The encoder, found on the PATH, and what it is asked for: H.264 at high quality, in the pixel
// format every player plays.
inline constexpr const char* kEncoderProgram = "ffmpeg";
// The game's sound in a video: mixed at this rate, in stereo, then encoded as AAC.
inline constexpr int kRecordSampleRate = 48000;
inline constexpr int kRecordChannels = 2;

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
  // Whether the video has the game's sound (--record-audio on, unless off).
  bool sound = true;
};

// `size` with each side rounded down to an even number of pixels (at least two): the video's
// pixel format keeps color for two by two pixels, and its encoder refuses odd sides.
FrameSize even(FrameSize size);

// Where a picture of `picture`'s size is shown in a window of `window`'s (see fit_picture).
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

// "on" or "off", for --record-audio; nothing for anything else.
std::optional<bool> record_sound_from(const std::string& text);

// The frames of sound (a sample on each channel) that go with each recorded picture: on average
// exactly sample_rate / fps, the fraction carried from one picture to the next, so that the sound
// never drifts from the pictures however long the recording.
class SoundClock {
 public:
  SoundClock(int sample_rate, int fps) : sample_rate_(sample_rate), fps_(fps) {}
  // The sound frames of the next picture.
  std::int64_t next();
  // Every sound frame so far: those of `pictures` pictures, to one frame.
  std::int64_t total() const { return total_; }
  std::int64_t pictures() const { return pictures_; }

 private:
  std::int64_t sample_rate_;
  std::int64_t fps_;
  std::int64_t pictures_ = 0;
  std::int64_t total_ = 0;
};
// The most sound frames one picture takes.
int most_sound_frames(int sample_rate, int fps);

// Where a video with sound is made, next to it: the pictures first, then the sound, which are put
// together into the video once both are done (and then deleted).
std::filesystem::path pictures_file(const RecordOptions& options);
std::filesystem::path sound_file(const RecordOptions& options);

// The encoder's arguments: raw RGB frames of `source` size on its standard input, upside down (as
// a framebuffer is read), into the video (into pictures_file, when it has sound).
std::vector<std::string> encoder_arguments(const RecordOptions& options, FrameSize source);
// The encoder's arguments that put the pictures and the sound (raw float samples at
// kRecordSampleRate) together into the video: the pictures as they are, the sound as AAC.
std::vector<std::string> muxer_arguments(const RecordOptions& options);
// Those arguments as one command line for Windows' shell, each quoted. (Elsewhere the encoder is
// started with them as they are, through no shell.)
std::string shell_command(const std::vector<std::string>& arguments);

}  // namespace redgem
