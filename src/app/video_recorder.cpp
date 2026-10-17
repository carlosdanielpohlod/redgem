#include "app/video_recorder.hpp"

#include <csignal>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

#include "core/log.hpp"
#include "core/translations.hpp"

namespace redgem {
namespace {

#ifdef _WIN32
constexpr const char* kWriteBinary = "wb";
constexpr const char* kNowhere = " > NUL 2>&1";
// cmd.exe takes off the first and last quotes of a line that starts with one: they are doubled.
std::string for_the_shell(const std::string& command) { return "\"" + command + "\""; }
std::FILE* open_pipe(const std::string& command) {
  return _popen(for_the_shell(command).c_str(), kWriteBinary);  // NOLINT(bugprone-command-processor)
}
int close_pipe(std::FILE* pipe) { return _pclose(pipe); }
void keep_running_when_the_encoder_stops() {}
#else
constexpr const char* kWriteBinary = "w";
constexpr const char* kNowhere = " > /dev/null 2>&1";
std::string for_the_shell(const std::string& command) { return command; }
// (The encoder is a program of its own, given every argument quoted: see shell_command.)
std::FILE* open_pipe(const std::string& command) {
  return popen(command.c_str(), kWriteBinary);  // NOLINT(bugprone-command-processor)
}
int close_pipe(std::FILE* pipe) { return pclose(pipe); }
// An encoder that stops early would otherwise end the game with it (a write to a closed pipe).
void keep_running_when_the_encoder_stops() { std::signal(SIGPIPE, SIG_IGN); }
#endif

constexpr const char* kVersionArgument = "-version";
constexpr int kSucceeded = 0;

// Whether the encoder can be started at all.
bool encoder_found() {
  const std::string command = shell_command({kEncoderProgram, kVersionArgument}) + kNowhere;
  return std::system(for_the_shell(command).c_str()) == kSucceeded;  // NOLINT(bugprone-command-processor)
}

[[noreturn]] void no_encoder(const RecordOptions& options) {
  throw std::runtime_error(
      tr("game.record.no_encoder", {{"file", options.file.string()}, {"program", kEncoderProgram}}));
}

}  // namespace

VideoRecorder::VideoRecorder(RecordOptions options, FrameSize source)
    : options_(std::move(options)), readback_(source.width, source.height) {
  if (!encoder_found()) no_encoder(options_);
  keep_running_when_the_encoder_stops();
  encoder_ = open_pipe(shell_command(encoder_arguments(options_, source)));
  if (encoder_ == nullptr) no_encoder(options_);
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
  const int status = close_pipe(encoder_);
  encoder_ = nullptr;
  if (status != kSucceeded) log::error(tr("game.record.failed", {{"file", options_.file.string()}}));
}

void VideoRecorder::write(const std::uint8_t* pixels, std::size_t bytes) { std::fwrite(pixels, 1, bytes, encoder_); }

}  // namespace redgem
