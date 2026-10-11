#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "render/gpu_select.hpp"

namespace redgem {

// The window shown when the editor starts without a game: recent games, a new game from a
// template, or any game folder. Owns its own window (closed when destroyed).
class ProjectHub {
 public:
  struct Options {
    std::filesystem::path games_folder;  // where new games go (and games are listed from)
    std::filesystem::path recent_file;   // recently opened games
    GpuChoice gpu = GpuChoice::Auto;     // the graphics card the window opens on
  };
  explicit ProjectHub(Options options);
  ~ProjectHub();
  ProjectHub(const ProjectHub&) = delete;
  ProjectHub& operator=(const ProjectHub&) = delete;

  // Runs until a game is chosen or the window is closed (nullopt).
  std::optional<std::filesystem::path> run();
  // One frame of the hub's window: what run() is a loop of.
  void frame();

  // Commands, the same ones the buttons use. Both choose the game on success.
  bool create_game(const std::string& name, const std::string& template_name);
  bool open_game(const std::filesystem::path& game_dir);

  const std::optional<std::filesystem::path>& chosen() const { return chosen_; }
  // Recent games first, then the other games of the games folder.
  const std::vector<std::filesystem::path>& games() const { return games_; }
  const std::string& error() const { return error_; }
  // Shows a message in the hub's error area (e.g. why the game picked last time did not open).
  void show_error(const std::string& message) { error_ = message; }

 private:
  void refresh_games();
  void draw();
  void draw_title();
  void draw_new_game(float width);
  void draw_templates();
  void draw_error();
  void draw_games();
  void draw_game_list();
  void draw_open_folder();

  Options options_;
  std::vector<std::filesystem::path> games_;
  std::vector<std::string> titles_;  // game.json titles, read once
  std::optional<std::filesystem::path> chosen_;
  std::string error_;
  std::string new_name_;
  std::string template_;
  std::string open_path_;
};

}  // namespace redgem
