#pragma once

// A game's frames into a video (see recording.hpp): each finished frame read back from the
// graphics card without waiting for it, and its pixels piped to the encoder, a program of its own.

#include <cstdint>
#include <cstdio>

#include "app/recording.hpp"
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
  // Hands the frames still being read to the encoder and waits for it to finish the video.
  void finish();
  bool finished() const { return encoder_ == nullptr; }
  // Whether the recording reached the length asked for.
  bool complete() const { return recording_complete(options_, frames_); }
  std::int64_t frames() const { return frames_; }

 private:
  void write(const std::uint8_t* pixels, std::size_t bytes);

  RecordOptions options_;
  FrameReadback readback_;
  std::FILE* encoder_ = nullptr;
  std::int64_t frames_ = 0;
};

}  // namespace redgem
