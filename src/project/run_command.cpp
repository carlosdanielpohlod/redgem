#include "project/run_command.hpp"

#include <memory>
#include <optional>
#include <utility>

#include "core/command_line.hpp"
#include "core/config.hpp"
#include "core/translations.hpp"
#include "project/script_messages.hpp"
#include "project/scene_text.hpp"
#include "project/tool_play.hpp"

namespace redgem::tools {
namespace {

// (A run never opens a window: playing in one is `redgem <game> --play`.)
// What the command takes after its name.
ToolGrammar grammar() { return {{kSceneOption, kSecondsOption, kFramesOption}, {command_line::kHelpOption}}; }
constexpr double kDefaultSeconds = 5.0;
constexpr int kSecondsDecimals = 2;

// What a run did: how far it got, and the script errors the game counted.
struct Played {
  long long frames = 0;
  int script_errors = 0;
};

Played play(Engine& engine, long long frames) {
  Played played;
  while (played.frames < frames && !engine.finished()) {
    engine.frame(config::kFixedDeltaSeconds);
    ++played.frames;
  }
  played.script_errors = engine.scripts().error_count();
  return played;
}

std::string summary(const std::string& scene, const Played& played, const MessageCollector& messages) {
  const double seconds = static_cast<double>(played.frames) * config::kFixedDeltaSeconds;
  return tr("project.run.summary", {{"scene", scene},
                                    {"frames", std::to_string(played.frames)},
                                    {"seconds", number_text(seconds, kSecondsDecimals)},
                                    {"errors", std::to_string(messages.errors())},
                                    {"warnings", std::to_string(messages.warnings())}});
}

// Whether the run went wrong: a script failed, or the engine said something was broken.
bool went_wrong(const Played& played, const MessageCollector& messages) {
  return played.script_errors > 0 || messages.errors() > 0;
}

ToolResult run_game(const PlayRequest& request, long long frames) {
  ScratchFolder scratch;
  MessageCollector messages(request.game_dir);
  auto created = engine_for(request, nullptr, scratch);
  if (auto* problem = std::get_if<std::string>(&created)) return failed(messages.lines() + *problem);
  const Played played = play(*std::get<std::unique_ptr<Engine>>(created), frames);
  ToolResult result;
  result.out = messages.lines() + summary(request.scene, played, messages) + "\n";
  result.exit_code = went_wrong(played, messages) ? kExitFailure : kExitSuccess;
  return result;
}

ToolResult usage_failure(const std::string& problem) {
  return failed(problem + "\n\n" + tr("project.run.usage"), kExitUsage);
}

}  // namespace

ToolResult run_run_command(const std::vector<std::string>& args) {
  std::string error;
  const auto line = read_tool_line(args, 0, grammar(), error);
  if (!line) return usage_failure(error);
  if (line->has(command_line::kHelpOption)) return ToolResult{kExitSuccess, tr("project.run.usage"), {}};
  if (line->words.size() != 1) return usage_failure(tr("project.tools.needs_game"));
  const auto frames = frames_asked(*line, kDefaultSeconds);
  if (!frames) return usage_failure(tr("project.run.needs_length"));
  const PlayRequest request{line->words.front(), line->value(kSceneOption).value_or(config::kMainSceneName), {}};
  return run_game(request, *frames);
}

}  // namespace redgem::tools
