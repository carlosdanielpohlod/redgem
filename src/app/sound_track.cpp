#include "app/sound_track.hpp"

namespace redgem {
namespace {

constexpr const char* kWriteBinary = "wb";
static_assert(kDeviceChannels == static_cast<std::size_t>(kRecordChannels), "the muxer reads the mix as it is");

}  // namespace

SoundTrack::SoundTrack(const std::filesystem::path& file, int fps)
    : output_(std::fopen(file.string().c_str(), kWriteBinary)), clock_(kRecordSampleRate, fps) {
  failed_ = output_ == nullptr;
}

// A file that takes no more (a full disk) ends the sound there: the pictures go on.
void SoundTrack::add_picture(AudioMixdown& mixdown) {
  const auto frames = static_cast<std::size_t>(clock_.next());
  samples_.resize(frames * kDeviceChannels);
  mixdown.render(samples_.data(), frames);
  if (failed_) return;
  const std::size_t written = std::fwrite(samples_.data(), sizeof(float), samples_.size(), output_);
  failed_ = written != samples_.size();
}

void SoundTrack::close() {
  if (output_ == nullptr) return;
  failed_ = std::fclose(output_) != 0 || failed_;
  output_ = nullptr;
}

}  // namespace redgem
