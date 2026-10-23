#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "app/recording.hpp"
#include "app/window_presenter.hpp"
#include "project/game_templates.hpp"

namespace redgem {

enum class RunMode { Editor, Game, NewGame, BuildGame, Help, Version };

struct CliOptions {
  RunMode mode = RunMode::Editor;
  std::filesystem::path game_dir;
  std::string template_name = kDefaultTemplate;  // RunMode::NewGame
  std::filesystem::path build_dir;               // RunMode::BuildGame
  std::string build_target;        // --target linux|windows|macos|android (empty: the system this runs on)
  std::optional<std::string> gpu;  // --gpu auto|dedicated|integrated (overrides the saved setting)
  // --record <file> plays the game into a video (with --record-fps, --record-size, --record-scale
  // and --record-seconds); no file: nothing is recorded.
  RecordOptions record;
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
// What --version says: "RedGem" and the version the program was built as.
std::string version_line();

}  // namespace redgem
