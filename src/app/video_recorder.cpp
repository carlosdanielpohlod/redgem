#include "app/video_recorder.hpp"

#include <csignal>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "core/log.hpp"
#include "core/translations.hpp"

namespace redgem {
namespace {

constexpr const char* kVersionArgument = "-version";

#ifdef _WIN32
void keep_running_when_the_encoder_stops() {}
#else
// A write to an encoder that stopped would otherwise end the game with it: the write fails instead.
void keep_running_when_the_encoder_stops() { std::signal(SIGPIPE, SIG_IGN); }
#endif

[[noreturn]] void no_encoder(const RecordOptions& options) {
  throw std::runtime_error(
      tr("game.record.no_encoder", {{"file", options.file.string()}, {"program", kEncoderProgram}}));
}

}  // namespace

VideoRecorder::VideoRecorder(RecordOptions options, FrameSize source)
    : options_(std::move(options)), readback_(source.width, source.height) {
  if (!runs({kEncoderProgram, kVersionArgument})) no_encoder(options_);
  keep_running_when_the_encoder_stops();
  encoder_ = start_process(encoder_arguments(options_, source));
  if (encoder_.input == nullptr) no_encoder(options_);
  if (options_.sound) sound_.emplace(sound_file(options_), options_.fps);
}

VideoRecorder::~VideoRecorder() { finish(); }

void VideoRecorder::add_frame(unsigned int framebuffer) {
  if (finished()) return;
  readback_.read(framebuffer, [this](const std::uint8_t* pixels, std::size_t bytes) { write(pixels, bytes); });
  ++frames_;
}

void VideoRecorder::add_sound(AudioMixdown& mixdown) {
  if (sound_ && !finished()) sound_->add_picture(mixdown);
}

// (An encoder that stopped by itself is done already: the sound still joins, or goes.)
void VideoRecorder::finish() {
  if (!finished()) readback_.drain([this](const std::uint8_t* pixels, std::size_t bytes) { write(pixels, bytes); });
  if (!finished()) end_encoder();
  if (sound_) join_sound(*sound_);
}

// Whether the video can have the sound: its pictures are whole, and so is some sound.
bool VideoRecorder::sound_joinable(const SoundTrack& sound) const {
  return !failed_ && sound.written() && sound.frames() > 0;
}

// The pictures and the sound into the video. Without the sound (it could not be written, or
// putting it in failed), the pictures are the video alone. Neither file stays.
void VideoRecorder::join_sound(SoundTrack& sound) {
  sound.close();
  const bool joined = sound_joinable(sound) && runs(muxer_arguments(options_));
  if (!joined) keep_pictures_alone();
  if (!failed_ && !joined) log::warning(tr("game.record.no_sound", {{"file", options_.file.string()}}));
  std::error_code ignored;
  std::filesystem::remove(sound_file(options_), ignored);
  std::filesystem::remove(pictures_file(options_), ignored);
  sound_.reset();
}

void VideoRecorder::keep_pictures_alone() {
  std::error_code ignored;  // (an encoder that never started wrote no pictures)
  std::filesystem::rename(pictures_file(options_), options_.file, ignored);
}

void VideoRecorder::end_encoder() {
  failed_ = !finish_process(encoder_) || failed_;
  if (failed_) log::error(tr("game.record.failed", {{"file", options_.file.string()}}));
}

// An encoder that stopped (a full disk, a bad file name) takes no more: the recording ends there.
void VideoRecorder::write(const std::uint8_t* pixels, std::size_t bytes) {
  if (finished()) return;
  const std::size_t written = std::fwrite(pixels, 1, bytes, encoder_.input);
  if (written == bytes && std::ferror(encoder_.input) == 0) return;
  failed_ = true;
  end_encoder();
}

}  // namespace redgem
