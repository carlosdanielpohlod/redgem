#pragma once

// A game's own tests (tests/*_test.rb) run for `redgem check --run-tests` by redgem-test, the
// program beside this one, which knows how (`redgem-test --all <game>`): this program has no test
// code in it.

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace redgem::tools {

struct TestsRun {
  std::size_t passed = 0;
  std::vector<std::string> failures;  // a line each: the file, and the first error it said
  std::string problem;                // when the tests could not be run at all
};

// Runs every test of the game with `tester` (redgem-test), keeping what it says in `scratch`.
TestsRun run_game_tests(const std::filesystem::path& game_dir, const std::filesystem::path& tester,
                        const std::filesystem::path& scratch);

// What redgem-test said, read: its lines that say how each file went ("PASS ..." / "FAIL ...").
TestsRun read_tests_run(const std::string& said);

// Where redgem-test is: beside this program.
std::filesystem::path tester_beside_this_program();

}  // namespace redgem::tools
