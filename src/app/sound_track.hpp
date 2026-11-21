#pragma once

// A recording's sound, as the game plays it: for each recorded picture, exactly its share of sound
// (see SoundClock) is mixed by the game's mixdown and written, raw, to a file the muxer reads once
// the pictures are done.

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "app/recording.hpp"
#include "audio/audio.hpp"

namespace redgem {

class SoundTrack {
 public:
  // Opens the file anew (see written for whether it could be).
  SoundTrack(const std::filesystem::path& file, int fps);
  ~SoundTrack() { close(); }
  SoundTrack(const SoundTrack&) = delete;
  SoundTrack& operator=(const SoundTrack&) = delete;

  // The sound of one more picture, mixed by `mixdown`.
  void add_picture(AudioMixdown& mixdown);
  void close();
  // Whether every frame of sound so far is in the file.
  bool written() const { return !failed_; }
  // The frames of sound so far.
  std::int64_t frames() const { return clock_.total(); }

 private:
  std::FILE* output_ = nullptr;
  SoundClock clock_;
  std::vector<float> samples_;
  bool failed_ = false;
};

}  // namespace redgem
