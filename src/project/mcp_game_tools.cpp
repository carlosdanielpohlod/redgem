#include "project/mcp_game_tools.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "core/safe_path.hpp"
#include "editor/object_clipboard.hpp"
#include "project/runtime_query.hpp"
#include "project/scene_text.hpp"
#include "project/script_messages.hpp"
#include "project/tool_commands.hpp"
#include "project/tool_play.hpp"

namespace redgem::tools::mcp {
namespace {

constexpr const char* kShotFile = "shot.png";
// The largest picture read back: a 1024-wide PNG of a busy scene stays well below it.
constexpr std::uintmax_t kMostPictureBytes = 4 * 1024 * 1024;
constexpr const char* kPictureWord = "picture";
constexpr int kSecondsDecimals = 2;

std::string scene_of(const Json& args) { return args.value("scene", std::string(config::kMainSceneName)); }

// "512x288": the width asked for, and the height of a 16:9 picture of it (even, as video sizes are).
std::string shot_size(const Json& args) {
  const int width = args.value("width", kDefaultShotWidth);
  const int height = (width * kShotAspectHeight / kShotAspectWidth) / 2 * 2;
  return std::to_string(width) + "x" + std::to_string(height);
}

std::vector<std::string> shot_words(const Options& options, const Json& args, const std::filesystem::path& file) {
  std::vector<std::string> words{"shot",  options.game.string(), file.string(), "--size", shot_size(args), "--at",
                                 number_text(args.value("at", kDefaultShotSeconds), kSecondsDecimals)};
  for (const char* key : {"scene", "camera"}) {
    if (!args.contains(key)) continue;
    words.push_back(std::string("--") + key);
    words.push_back(args[key]);
  }
  return words;
}

// What the shot said, with the scratch file it wrote named only as "picture".
std::string said_without_file(std::string text, const std::filesystem::path& file) {
  const std::string path = file.string();
  for (auto at = text.find(path); at != std::string::npos; at = text.find(path, at)) text.replace(at, path.size(), kPictureWord);
  return text;
}

Json picture_result(const std::vector<std::uint8_t>& png, const std::string& text) {
  return {{"content",
           {{{"type", "image"}, {"data", object_clipboard::base64_encode(png)}, {"mimeType", "image/png"}},
            {{"type", "text"}, {"text", text}}}},
          {"isError", false}};
}

// Plays the saved game `frames` fixed steps (fewer when it ends first).
void play_frames(Engine& engine, int frames) {
  for (int frame = 0; frame < frames && !engine.finished(); ++frame) engine.frame(config::kFixedDeltaSeconds);
}

std::string query_saved(const Options& options, const Json& args, const MessageCollector& messages) {
  const PlayRequest request{options.game, scene_of(args), {}, 0};
  ScratchFolder scratch;
  auto created = engine_for(request, nullptr, scratch);
  if (auto* problem = std::get_if<std::string>(&created)) throw std::runtime_error(messages.lines() + *problem);
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  const int frames = args.value("frames", kDefaultQueryFrames);
  play_frames(engine, frames);
  const auto scene = read_scene(options.game, request.scene);
  const std::string answer = answer_query(engine, scene ? *scene : SceneDesc{}, args);
  return "after " + std::to_string(frames) + " frames: " + answer + "\n" + messages.lines();
}

}  // namespace

Json game_shot(const Options& options, const Json& args) {
  ScratchFolder scratch;
  const auto file = scratch.path() / kShotFile;
  const auto result = run_tool_command(shot_words(options, args, file));
  if (!result) throw std::logic_error("missing shot command");
  const std::string said = said_without_file(result->out + result->err, file);
  if (result->exit_code != kExitSuccess) return tool_result(said, true);
  const auto png = read_file_up_to(file, kMostPictureBytes);
  if (!png) return tool_result("the picture could not be read back; ask for a smaller width", true);
  return picture_result(*png, said);
}

Json game_query(const Options& options, const Json& args) {
  MessageCollector messages(options.game);
  try {
    return tool_result(query_saved(options, args, messages));
  } catch (const std::runtime_error& problem) {
    return tool_result(problem.what(), true);
  }
}

}  // namespace redgem::tools::mcp
