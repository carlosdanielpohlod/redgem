#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace redgem::testing {

// What redgem-test was asked to run.
enum class TestMode { SystemTest, RubyTests, Help };

struct TestCliOptions {
  TestMode mode = TestMode::SystemTest;
  std::filesystem::path game_dir;        // TestMode::SystemTest: the game to play
  std::filesystem::path test_file;       // the system test, or the file of Ruby unit tests
  std::filesystem::path scripts_game;    // --game: the game whose classes Ruby unit tests use
  std::filesystem::path screenshot_dir;  // where a system test's pictures go
  bool headless = false;                 // a system test without a window
  std::filesystem::path work_dir;        // --work: a folder of this run's own (see SystemTestOptions)
  std::optional<std::string> gpu;        // --gpu auto|dedicated|integrated (overrides the saved setting)
};

struct TestCliResult {
  bool ok = false;
  std::string error;
  TestCliOptions options;
};

TestCliResult parse_test_cli(const std::vector<std::string>& args);
std::string test_usage();

}  // namespace redgem::testing
