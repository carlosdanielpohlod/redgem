#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace redgem {

enum class RunMode { Editor, Game, RubyTests, NewGame, BuildGame, Relay, Help };

struct CliOptions {
  RunMode mode = RunMode::Editor;
  std::filesystem::path game_dir;
  std::optional<std::filesystem::path> test_script;  // system test to drive the game
  std::filesystem::path ruby_test_file;              // RunMode::RubyTests
  std::filesystem::path screenshot_dir;
  bool headless = false;
  std::string template_name = "empty";  // RunMode::NewGame
  std::filesystem::path build_dir;      // RunMode::BuildGame
  std::string build_target;             // --target linux|windows (empty: the system this runs on)
  int relay_port = 0;                   // RunMode::Relay (0: the usual port)
  std::optional<std::string> gpu;       // --gpu auto|dedicated|integrated (overrides the saved setting)
};

struct CliResult {
  bool ok = false;
  std::string error;
  CliOptions options;
};

CliResult parse_cli(const std::vector<std::string>& args);
std::string usage();

}  // namespace redgem
