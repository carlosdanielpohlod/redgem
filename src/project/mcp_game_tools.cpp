#include "project/mcp_game_tools.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "app/encoder_process.hpp"
#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "core/safe_path.hpp"
#include "editor/object_clipboard.hpp"
#include "project/runtime_query.hpp"
#include "project/scene_text.hpp"
#include "project/script_messages.hpp"
#include "project/tool_play.hpp"

namespace redgem::tools::mcp {
namespace {

constexpr const char* kShotFile = "shot.png";
constexpr const char* kShotLog = "shot.txt";  // what the picture's process printed
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

// `redgem shot ...` as a program of its own: a graphics driver that crashes while it opens a
// window or draws (raylib goes on even when it cannot make one) takes only that process down.
std::vector<std::string> shot_words(const Options& options, const Json& args, const std::filesystem::path& file) {
  std::vector<std::string> words{platform::executable_path().string(),
                                 "shot",
                                 options.game.string(),
                                 file.string(),
                                 "--size",
                                 shot_size(args),
                                 "--at",
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
  for (auto at = text.find(path); at != std::string::npos; at = text.find(path, at))
    text.replace(at, path.size(), kPictureWord);
  return text;
}

// What the picture's process printed (nothing when it wrote nothing).
std::string printed(const std::filesystem::path& log) {
  std::string error;
  return read_text_file(log, error).value_or("");
}

// What a picture's process said before the graphics driver or the renderer crashed.
std::string crash_report(const ProcessEnd& end, const std::string& said) {
  return "the renderer crashed while taking the picture (signal " + std::to_string(end.code) +
         "); the graphics driver may not support this display. What it said:\n" + said;
}

Json picture_result(const std::vector<std::uint8_t>& png, const std::string& text) {
  return {{"content",
           {{{"type", "image"}, {"data", object_clipboard::base64_encode(png)}, {"mimeType", "image/png"}},
            {{"type", "text"}, {"text", text}}}},
          {"isError", false}};
}

// Plays the saved game `frames` fixed steps, fewer when it ends first; returns how many it played.
int play_frames(Engine& engine, int frames) {
  int played = 0;
  while (played < frames && !engine.finished()) {
    engine.frame(config::kFixedDeltaSeconds);
    ++played;
  }
  return played;
}

// The objects the runtime's ids name: the requested scene's, unless a script moved the game on
// to another one (whose objects are then named by name alone, as spawned ones are).
SceneDesc runtime_scene(const Engine& engine, const PlayRequest& request) {
  if (engine.scene_name() != request.scene) return {};
  auto scene = read_scene(request.game_dir, request.scene);
  return scene ? std::move(*scene) : SceneDesc{};
}

std::string query_saved(const Options& options, const Json& args, const MessageCollector& messages) {
  const PlayRequest request{options.game, scene_of(args), {}, 0};
  ScratchFolder scratch;
  auto created = engine_for(request, nullptr, scratch);
  if (auto* problem = std::get_if<std::string>(&created)) throw std::runtime_error(messages.lines() + *problem);
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  const int played = play_frames(engine, args.value("frames", kDefaultQueryFrames));
  const std::string answer = answer_query(engine, runtime_scene(engine, request), args);
  return "after " + std::to_string(played) + " frames: " + answer + "\n" + messages.lines();
}

}  // namespace

Json game_shot(const Options& options, const Json& args) {
  ScratchFolder scratch;
  const auto file = scratch.path() / kShotFile;
  const auto log = scratch.path() / kShotLog;
  const ProcessEnd end = run_to_end(shot_words(options, args, file), log);
  if (!end.started) return tool_result("cannot start the program that takes pictures", true);
  const std::string said = said_without_file(printed(log), file);
  if (!end.exited) return tool_result(crash_report(end, said), true);
  if (end.code != kExitSuccess) return tool_result(said, true);
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
