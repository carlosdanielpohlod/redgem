#include "project/editor_run.hpp"

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/translations.hpp"
#include "editor/editor_app.hpp"
#include "project/editor_setup.hpp"
#include "project/project_hub.hpp"
#include "project/recent_games.hpp"
#include "project/agent_tools.hpp"
#include "project/editor_bridge_run.hpp"

namespace redgem {
namespace {

// The game to open: the one given, else the one picked in the project hub (which shows why the
// last one did not open). False when the hub was closed instead.
bool pick_game(std::filesystem::path& game, std::string& hub_message, GpuChoice gpu) {
  if (!game.empty()) {
    remember_game(recent_games_file(), game);
    return true;
  }
  ProjectHub hub({default_games_folder(), recent_games_file(), gpu});
  hub.show_error(hub_message);
  hub_message.clear();
  const auto chosen = hub.run();
  if (chosen) game = *chosen;
  return chosen.has_value();
}

EditorOptions options_with_agents(GpuChoice gpu, std::unique_ptr<tools::EditorBridge>& bridge) {
  EditorOptions options = editor_options(gpu);
  options.agent = agent_hooks(bridge);
  return options;
}

// One game in the editor, until the editor is left. The exit code, or nullopt to go (back) to the
// hub: a broken game from the command line exits; from the hub it returns there.
std::optional<int> edit_game(const std::filesystem::path& game, bool from_command_line, std::string& hub_message,
                             GpuChoice gpu, bool bridge_enabled) {
  std::unique_ptr<tools::EditorBridge> bridge;  // (outlives the editor, whose agents' tools reach it)
  auto created = EditorApp::create(game, options_with_agents(gpu, bridge));
  if (auto* error = std::get_if<std::string>(&created)) {
    log::error(*error);
    if (from_command_line) return kExitFailure;
    hub_message = tr("project.hub.cannot_open", {{"game", game.filename().string()}, {"error", *error}});
    return std::nullopt;
  }
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  // (One bridge for the editor: an agent's session may have enabled it already.)
  if (bridge_enabled && !bridge) bridge = enable_editor_bridge(app);
  const int code = app.run();
  if (app.exit_action() != EditorExit::OpenHub) return code;
  return std::nullopt;
}

}  // namespace

int run_editor(const std::filesystem::path& given, GpuChoice gpu, bool bridge) {
  std::filesystem::path game = given;
  bool from_command_line = !game.empty();
  std::string hub_message;  // why the last game did not open, shown in the hub
  while (pick_game(game, hub_message, gpu)) {
    if (const auto code = edit_game(game, from_command_line, hub_message, gpu, bridge)) return *code;
    from_command_line = false;
    game.clear();  // back to the hub
  }
  return kExitSuccess;
}

}  // namespace redgem
