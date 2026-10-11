#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "app/engine.hpp"

namespace redgem {

enum class RunMode { Editor, Game, NewGame, BuildGame, Relay, Help };

struct CliOptions {
  RunMode mode = RunMode::Editor;
  std::filesystem::path game_dir;
  std::filesystem::path screenshot_dir;  // where a game's Game.screenshot pictures go
  std::string template_name = "empty";   // RunMode::NewGame
  std::filesystem::path build_dir;       // RunMode::BuildGame
  std::string build_target;              // --target linux|windows (empty: the system this runs on)
  int relay_port = 0;                    // RunMode::Relay (0: the usual port)
  std::optional<std::string> gpu;        // --gpu auto|dedicated|integrated (overrides the saved setting)
};

struct CliResult {
  bool ok = false;
  std::string error;
  CliOptions options;
};

CliResult parse_cli(const std::vector<std::string>& args);
// The engine a game is played with from the command line (--play): the developer's way to play,
// so it comes with what a developer watches (the frames-per-second counter).
EngineOptions developer_play_options(const CliOptions& options, GpuChoice gpu = GpuChoice::Auto);
std::string usage();

}  // namespace redgem
