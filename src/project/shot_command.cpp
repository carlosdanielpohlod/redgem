#include "project/shot_command.hpp"

#include <raylib.h>

#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

#include "app/frame_painter.hpp"
#include "app/recording.hpp"
#include "core/command_line.hpp"
#include "core/config.hpp"
#include "core/translations.hpp"
#include "project/scene_text.hpp"
#include "project/script_messages.hpp"
#include "project/tool_play.hpp"
#include "render/renderer.hpp"
#include "render/window.hpp"

namespace redgem::tools {
namespace {

constexpr const char* kAtOption = "--at";
constexpr const char* kSizeOption = "--size";
constexpr const char* kCameraOption = "--camera";
// What the command takes after its name.
ToolGrammar grammar() { return {{kSceneOption, kAtOption, kSizeOption, kCameraOption}, {command_line::kHelpOption}}; }
constexpr double kDefaultAt = 1.0;  // seconds: what starts moving at once has moved
constexpr FrameSize kDefaultSize{512, 288};
constexpr const char* kPictureExtension = ".png";
constexpr int kSecondsDecimals = 2;
// The words of the line: the game, then the picture's file.
constexpr std::size_t kGameWord = 0;
constexpr std::size_t kPictureWord = 1;
constexpr std::size_t kWords = 2;

// The window a shot is drawn with: hidden, nobody needs to see it.
WindowSettings hidden_window(FrameSize size) {
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  WindowSettings settings;
  settings.width = size.width;
  settings.height = size.height;
  return settings;
}

// Shows a game to nobody, frame by frame off screen, and keeps the picture of one frame.
class ShotPresenter final : public Presenter {
 public:
  ShotPresenter(FrameSize size, long long frame, std::filesystem::path file)
      : window_(hidden_window(size)),
        renderer_(window_),
        painter_(renderer_),
        picture_(LoadRenderTexture(size.width, size.height)),
        frame_(frame),
        file_(std::move(file)) {}
  ~ShotPresenter() override { UnloadRenderTexture(picture_); }
  ShotPresenter(const ShotPresenter&) = delete;
  ShotPresenter& operator=(const ShotPresenter&) = delete;

  SurfaceSize surface_size() const override { return {picture_.texture.width, picture_.texture.height}; }
  int text_width(const std::string& text, int size) const override { return renderer_.text_width(text, size); }
  void capture_cursor(bool) override {}
  bool open() const override { return !taken_; }
  float frame_seconds() const override { return config::kFixedDeltaSeconds; }

  // Every frame is drawn, so that what builds up over frames (smoothed edges, trails) is there.
  void present(const FrameView& view) override {
    renderer_.set_clock(static_cast<double>(drawn_) * config::kFixedDeltaSeconds);
    painter_.paint(view, &picture_);
    if (drawn_++ == frame_) take();
  }

  bool taken() const { return taken_; }
  bool saved() const { return saved_; }

 private:
  void take() {
    Image image = LoadImageFromTexture(picture_.texture);
    ImageFlipVertical(&image);  // (a framebuffer's rows start at the bottom)
    saved_ = ExportImage(image, file_.string().c_str());
    UnloadImage(image);
    taken_ = true;
  }

  Window window_;
  Renderer renderer_;
  FramePainter painter_;
  RenderTexture2D picture_{};
  long long frame_;
  std::filesystem::path file_;
  long long drawn_ = 0;
  bool taken_ = false;
  bool saved_ = false;
};

struct ShotRequest {
  PlayRequest play;
  std::filesystem::path file;
  FrameSize size = kDefaultSize;
  long long frame = 0;
};

ToolResult usage_failure(const std::string& problem) {
  return failed(problem + "\n\n" + tr("project.shot.usage"), kExitUsage);
}

std::string said(const ShotRequest& request, const MessageCollector& messages) {
  const double seconds = static_cast<double>(request.frame) * config::kFixedDeltaSeconds;
  return messages.lines() +
         tr("project.shot.saved", {{"file", request.file.string()},
                                   {"width", std::to_string(request.size.width)},
                                   {"height", std::to_string(request.size.height)},
                                   {"scene", request.play.scene},
                                   {"seconds", number_text(seconds, kSecondsDecimals)}}) +
         "\n";
}

// Plays until the picture is taken, or the game ends first.
void play_to_picture(Engine& engine, const ShotPresenter& shot) {
  while (!shot.taken() && !engine.finished()) engine.frame(config::kFixedDeltaSeconds);
}

ToolResult shoot(const ShotRequest& request) {
  ScratchFolder scratch;
  MessageCollector messages(request.play.game_dir);
  ShotPresenter shot(request.size, request.frame, request.file);
  auto created = engine_for(request.play, &shot, scratch);
  if (auto* problem = std::get_if<std::string>(&created)) return failed(messages.lines() + *problem);
  play_to_picture(*std::get<std::unique_ptr<Engine>>(created), shot);
  if (!shot.taken()) return failed(messages.lines() + tr("project.shot.ended_early"));
  if (!shot.saved()) return failed(tr("project.shot.cannot_write", {{"file", request.file.string()}}));
  return ToolResult{kExitSuccess, said(request, messages), {}};
}

// When and how big the picture is, from the line, into `request`. False when the line says
// something that is no time or no size.
bool read_timing(const ToolLine& line, ShotRequest& request) {
  const auto size_given = line.value(kSizeOption);
  const auto size = size_given ? frame_size_from(*size_given) : kDefaultSize;
  const auto at = number_from(line.value(kAtOption).value_or(std::to_string(kDefaultAt)));
  if (!size || !at || *at < 0) return false;
  request.size = *size;
  request.frame = std::llround(*at / config::kFixedDeltaSeconds);
  return true;
}

// The shot a line asks for; what is wrong with the line otherwise.
std::variant<ShotRequest, std::string> request_of(const ToolLine& line) {
  ShotRequest request;
  if (line.words.size() != kWords) return tr("project.shot.needs_game_and_file");
  request.play = {line.words[kGameWord], line.value(kSceneOption).value_or(config::kMainSceneName),
                  line.value(kCameraOption)};
  request.file = line.words[kPictureWord];
  if (request.file.extension() != kPictureExtension) return tr("project.shot.needs_png");
  if (!read_timing(line, request)) return tr("project.shot.needs_size_and_time");
  return request;
}

}  // namespace

ToolResult run_shot_command(const std::vector<std::string>& args) {
  std::string error;
  const auto line = read_tool_line(args, 0, grammar(), error);
  if (!line) return usage_failure(error);
  if (line->has(command_line::kHelpOption)) return ToolResult{kExitSuccess, tr("project.shot.usage"), {}};
  auto request = request_of(*line);
  if (auto* problem = std::get_if<std::string>(&request)) return usage_failure(*problem);
  try {
    return shoot(std::get<ShotRequest>(request));
  } catch (const std::runtime_error& problem) {  // no window to draw with
    return failed(problem.what());
  }
}

}  // namespace redgem::tools
