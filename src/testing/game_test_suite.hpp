#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "render/gpu_select.hpp"

namespace redgem::testing {

// What a file's line in a suite's answer starts with: whoever runs the suite reads them.
inline constexpr const char* kPassedMark = "PASS ";
inline constexpr const char* kFailedMark = "FAIL ";

// The test files of a game, as paths in its folder ("tests/coin_test.rb"), sorted.
std::vector<std::string> game_test_files(const std::filesystem::path& game_dir);

// Whether a test file is a system test (it calls SystemTest.define): the others are Ruby unit tests.
bool is_system_test(const std::string& code);

// redgem-test --all <game_dir>: every test file of the game, one after the other, each system
// test with no window (in a folder of its own inside `work_dir` when one is given), each file of
// Ruby unit tests against the game's classes. Says a line per file on the standard output:
// "PASS tests/a_test.rb", or "FAIL tests/b_test.rb: " and the first error it said, then how
// many passed. Returns the process exit code: 0 when every file passed.
int run_game_test_suite(const std::filesystem::path& game_dir, const std::filesystem::path& work_dir, GpuChoice gpu);

}  // namespace redgem::testing
