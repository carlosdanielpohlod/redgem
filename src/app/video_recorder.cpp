#include "app/video_recorder.hpp"

#include <csignal>
#include <stdexcept>
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
}

VideoRecorder::~VideoRecorder() { finish(); }

void VideoRecorder::add_frame(unsigned int framebuffer) {
  if (finished()) return;
  readback_.read(framebuffer, [this](const std::uint8_t* pixels, std::size_t bytes) { write(pixels, bytes); });
  ++frames_;
}

void VideoRecorder::finish() {
  if (finished()) return;
  readback_.drain([this](const std::uint8_t* pixels, std::size_t bytes) { write(pixels, bytes); });
  if (!finished()) end_encoder();
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
