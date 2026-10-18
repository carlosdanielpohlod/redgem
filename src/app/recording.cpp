#include "app/recording.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>

namespace redgem {
namespace {

constexpr char kSizeSeparator = 'x';
constexpr int kEvenPixels = 2;
constexpr float kHalfOf = 0.5f;  // of the room left in a window: what goes on each side of a picture
constexpr const char* kQuote = "\"";
constexpr const char* kArgumentSeparator = " ";
constexpr const char* kRawFormat = "rgb24";  // three bytes a pixel, as frames are read back
constexpr const char* kVideoCodec = "libx264";
constexpr const char* kQuality = "16";  // constant rate factor: lower is better, 16 is near lossless to the eye
constexpr const char* kPreset = "medium";
constexpr const char* kPlayablePixels = "yuv420p";
constexpr const char* kFlip = "vflip";  // a framebuffer's rows come bottom first
constexpr const char* kScaleFilter = ",scale={w}:{h}:flags=lanczos";

std::optional<int> whole_number(const std::string& text) {
  int value = 0;
  const char* end = text.data() + text.size();
  const auto [stop, error] = std::from_chars(text.data(), end, value);
  if (error != std::errc() || stop != end) return std::nullopt;
  return value;
}

std::optional<int> positive(std::optional<int> value) {
  if (!value || *value <= 0) return std::nullopt;
  return value;
}

std::string size_text(FrameSize size) {
  return std::to_string(size.width) + kSizeSeparator + std::to_string(size.height);
}

std::string replaced(std::string text, const std::string& name, const std::string& value) {
  const std::size_t at = text.find(name);
  return at == std::string::npos ? text : text.replace(at, name.size(), value);
}

// The filters frames go through: flipped upright, then scaled when asked.
std::string filters(const RecordOptions& options) {
  if (!options.scale) return kFlip;
  const FrameSize size = even(*options.scale);
  const std::string scale = replaced(kScaleFilter, "{w}", std::to_string(size.width));
  return kFlip + replaced(scale, "{h}", std::to_string(size.height));
}

// Raw frames of `source` size, `options.fps` of them a second, on the standard input.
std::vector<std::string> input_arguments(const RecordOptions& options, FrameSize source) {
  return {kEncoderProgram,
          "-y",
          "-loglevel",
          "error",
          "-f",
          "rawvideo",
          "-pix_fmt",
          kRawFormat,
          "-s",
          size_text(source),
          "-r",
          std::to_string(options.fps),
          "-i",
          "-"};
}

// Flipped (and scaled), encoded, into the video.
std::vector<std::string> output_arguments(const RecordOptions& options) {
  return {"-vf",   filters(options), "-c:v",          kVideoCodec,          "-crf", kQuality, "-preset",
          kPreset, "-pix_fmt",       kPlayablePixels, options.file.string()};
}

}  // namespace

FrameSize even(FrameSize size) {
  const auto rounded = [](int side) { return std::max(side - side % kEvenPixels, kEvenPixels); };
  return {rounded(size.width), rounded(size.height)};
}

Letterbox letterbox(FrameSize window, FrameSize picture) {
  const float scale = std::min(static_cast<float>(window.width) / static_cast<float>(picture.width),
                               static_cast<float>(window.height) / static_cast<float>(picture.height));
  const float width = static_cast<float>(picture.width) * scale;
  const float height = static_cast<float>(picture.height) * scale;
  return {(static_cast<float>(window.width) - width) * kHalfOf, (static_cast<float>(window.height) - height) * kHalfOf,
          width, height};
}

PointMapping window_to_picture(FrameSize window, FrameSize picture) {
  const Letterbox box = letterbox(window, picture);
  return {-box.x, -box.y, static_cast<float>(picture.width) / box.width,
          static_cast<float>(picture.height) / box.height};
}

std::optional<FrameSize> frame_size_from(const std::string& text) {
  const std::size_t separator = text.find(kSizeSeparator);
  if (separator == std::string::npos) return std::nullopt;
  const auto width = positive(whole_number(text.substr(0, separator)));
  const auto height = positive(whole_number(text.substr(separator + 1)));
  if (!width || !height) return std::nullopt;
  return FrameSize{*width, *height};
}

std::optional<int> record_fps_from(const std::string& text) {
  const auto fps = positive(whole_number(text));
  if (!fps || *fps > kMostRecordFps) return std::nullopt;
  return fps;
}

std::optional<double> record_seconds_from(const std::string& text) {
  char* end = nullptr;
  const double seconds = std::strtod(text.c_str(), &end);
  const bool whole_text = !text.empty() && end == text.c_str() + text.size();
  if (!whole_text || !std::isfinite(seconds) || seconds <= 0.0) return std::nullopt;
  return seconds;
}

float recorded_frame_seconds(int fps) { return 1.0f / static_cast<float>(fps); }

std::optional<std::int64_t> frames_to_record(const RecordOptions& options) {
  if (!options.seconds) return std::nullopt;
  return static_cast<std::int64_t>(std::llround(*options.seconds * options.fps));
}

bool recording_complete(const RecordOptions& options, std::int64_t frames) {
  const auto wanted = frames_to_record(options);
  return wanted && frames >= *wanted;
}

std::vector<std::string> encoder_arguments(const RecordOptions& options, FrameSize source) {
  std::vector<std::string> arguments = input_arguments(options, source);
  const std::vector<std::string> output = output_arguments(options);
  arguments.insert(arguments.end(), output.begin(), output.end());
  return arguments;
}

std::string shell_command(const std::vector<std::string>& arguments) {
  std::string command;
  for (const std::string& argument : arguments) {
    if (!command.empty()) command += kArgumentSeparator;
    command += kQuote + argument + kQuote;
  }
  return command;
}

}  // namespace redgem
