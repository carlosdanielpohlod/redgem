#pragma once

// A game's frames into a video (see recording.hpp): each finished frame read back from the
// graphics card without waiting for it, and its pixels piped to the encoder, a program of its own.
// With its sound, the pictures and the sound are each written to a file of their own, put together
// into the video once the recording ends.

#include <cstdint>
#include <optional>

#include "app/encoder_process.hpp"
#include "app/recording.hpp"
#include "app/sound_track.hpp"
#include "render/frame_readback.hpp"

namespace redgem {

class VideoRecorder {
 public:
  // Starts the encoder on frames of `source` size. Throws std::runtime_error, saying what to do in
  // the user's language, when there is no encoder to start.
  VideoRecorder(RecordOptions options, FrameSize source);
  ~VideoRecorder();
  VideoRecorder(const VideoRecorder&) = delete;
  VideoRecorder& operator=(const VideoRecorder&) = delete;

  // Adds the frame drawn into `framebuffer` (of the source size).
  void add_frame(unsigned int framebuffer);
  // Adds the sound of the frame just added, mixed by `mixdown`: once a frame, when the video has
  // its sound (nothing otherwise).
  void add_sound(AudioMixdown& mixdown);
  bool records_sound() const { return sound_.has_value(); }
  // Hands the frames still being read to the encoder and waits for it to finish the video.
  void finish();
  bool finished() const { return encoder_.input == nullptr; }
  // Whether the encoder stopped before it was asked to (the video is then incomplete, or none).
  bool failed() const { return failed_; }
  // Whether the recording reached the length asked for.
  bool complete() const { return recording_complete(options_, frames_); }
  std::int64_t frames() const { return frames_; }

 private:
  void write(const std::uint8_t* pixels, std::size_t bytes);
  void end_encoder();
  void join_sound();
  void keep_pictures_alone();

  RecordOptions options_;
  FrameReadback readback_;
  EncoderProcess encoder_;
  std::optional<SoundTrack> sound_;
  bool failed_ = false;
  std::int64_t frames_ = 0;
};

}  // namespace redgem
