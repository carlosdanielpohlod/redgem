#pragma once

#include <filesystem>

namespace redgem::testing {

// Runs a file of Ruby unit tests (RedGemTest::Case subclasses) in a Ruby VM as a game's is, with
// an empty world behind it and what tests are written with. Returns the process exit code: 0
// when every test passed.
int run_ruby_tests(const std::filesystem::path& test_file);

}  // namespace redgem::testing
