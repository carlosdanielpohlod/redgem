#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "testing/game_test_suite.hpp"

using namespace redgem::testing;
namespace fs = std::filesystem;

TEST_CASE("a game's tests are the files of its tests folder that end in _test.rb") {
  const fs::path game = fs::temp_directory_path() / "redgem_suite_files";
  fs::remove_all(game);
  fs::create_directories(game / "tests");
  for (const char* name : {"b_test.rb", "a_test.rb", "helper.rb"}) std::ofstream(game / "tests" / name) << "";
  CHECK(game_test_files(game) == std::vector<std::string>{"tests/a_test.rb", "tests/b_test.rb"});
  CHECK(game_test_files(game / "nothing").empty());
  fs::remove_all(game);
}

TEST_CASE("a system test is a file that defines one") {
  CHECK(is_system_test("SystemTest.define do |t|\n  t.wait(1)\nend\n"));
  CHECK_FALSE(is_system_test("class ScoreTest < RedGemTest::Case\nend\n"));
}
