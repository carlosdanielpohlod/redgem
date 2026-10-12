#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "app/window_presenter.hpp"

namespace redgem {

enum class RunMode { Editor, Game, NewGame, BuildGame, Help };

struct CliOptions {
  RunMode mode = RunMode::Editor;
  std::filesystem::path game_dir;
  std::string template_name = "empty";  // RunMode::NewGame
  std::filesystem::path build_dir;      // RunMode::BuildGame
  std::string build_target;             // --target linux|windows (empty: the system this runs on)
  std::optional<std::string> gpu;       // --gpu auto|dedicated|integrated (overrides the saved setting)
};

struct CliResult {
  bool ok = false;
  std::string error;
  CliOptions options;
};

CliResult parse_cli(const std::vector<std::string>& args);
// The window a game is played in from the command line (--play): the developer's way to play,
// so it comes with what a developer watches (the frames-per-second counter).
GameWindowOptions developer_play_options(const CliOptions& options, GpuChoice gpu = GpuChoice::Auto);
std::string usage();

}  // namespace redgem
