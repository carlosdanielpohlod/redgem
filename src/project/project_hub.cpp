#include "project/project_hub.hpp"

#include <algorithm>

#include "app/game_settings.hpp"
#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "project/game_templates.hpp"
#include "project/recent_games.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr const char* kSuggestedTemplate = "platformer";  // the one chosen when the hub opens

// The hub's window: a form, drawn straight into it.
WindowSettings hub_window(GpuChoice gpu) {
  WindowSettings settings;
  settings.width = kHubWidth;
  settings.height = kHubHeight;
  settings.title = kHubTitle;
  settings.resizable = true;
  settings.smooth_edges = true;
  settings.gpu = gpu;
  return settings;
}

}  // namespace

ProjectHub::ProjectHub(Options options)
    : options_(std::move(options)),
      window_(hub_window(options_.gpu)),
      interface_(window_, ui::Interface::Layout::Fixed),
      new_name_(tr("project.hub.default_game_name")),
      template_(kSuggestedTemplate) {
  refresh_games();
}

void ProjectHub::refresh_games() {
  games_ = read_recent_games(options_.recent_file);
  for (const auto& game : find_games(options_.games_folder)) {
    const auto same = [&game](const std::filesystem::path& known) {
      std::error_code error;
      return std::filesystem::equivalent(known, game, error);
    };
    if (std::none_of(games_.begin(), games_.end(), same)) games_.push_back(game);
  }
  titles_.clear();
  for (const auto& game : games_) titles_.push_back(read_game_settings(game).title);
}

bool ProjectHub::waiting_for_choice() const { return !chosen_ && !window_.close_requested(); }

std::string ProjectHub::shown_path(const std::filesystem::path& path) {
  return editor_logic::pretty_path(path.string(), platform::home_dir().string());
}

std::optional<std::filesystem::path> ProjectHub::run() {
  while (waiting_for_choice()) frame();
  return chosen_;
}

bool ProjectHub::create_game(const std::string& name, const std::string& template_name) {
  if (!valid_game_name(name)) {
    error_ = tr("project.hub.bad_name");
    return false;
  }
  const std::filesystem::path dir = options_.games_folder / name;
  std::string error;
  if (!redgem::create_game(dir, template_name, error)) {
    error_ = error;
    return false;
  }
  log::info("created " + dir.string());
  return open_game(dir);
}

bool ProjectHub::open_game(const std::filesystem::path& typed) {
  const std::filesystem::path game_dir = normalized_folder(editor_logic::expand_home(typed, platform::home_dir()));
  if (!is_game_folder(game_dir)) {
    error_ = tr("project.hub.not_a_game", {{"folder", shown_path(game_dir)}, {"scene_file", config::kSceneFileName}});
    return false;
  }
  remember_game(options_.recent_file, game_dir);
  error_.clear();
  chosen_ = game_dir;
  return true;
}

}  // namespace redgem
