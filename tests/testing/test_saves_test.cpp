#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/save_location.hpp"
#include "testing/test_saves.hpp"

using namespace redgem;
using namespace redgem::testing;

TEST_CASE("tests save in a file named as the game's own, somewhere else") {
  const std::filesystem::path player = save_file_for("/somewhere/games/platformer");
  const std::filesystem::path tested = test_save_file_for("/somewhere/games/platformer");
  CHECK(tested.filename() == player.filename());
  CHECK(tested.parent_path() != player.parent_path());
  CHECK(test_save_file_for("/home/ana/games/platformer") != test_save_file_for("/home/ana/Downloads/platformer"));
}

TEST_CASE("test saves are kept in the user's own folder, not where every user of the machine can write") {
  const std::filesystem::path tested = test_save_file_for("/somewhere/games/platformer");
  const std::filesystem::path players = save_file_for("/somewhere/games/platformer");
  CHECK(tested.parent_path().parent_path() == players.parent_path().parent_path());
  const std::string shared = std::filesystem::temp_directory_path().string();
  CHECK(tested.parent_path().parent_path().string().rfind(shared, 0) != 0);
}

TEST_CASE("every test starts from no saves at all") {
  const std::filesystem::path game = "/nowhere/redgem-testing-tests/a_game";
  const std::filesystem::path file = test_save_file_for(game);
  std::filesystem::create_directories(file.parent_path());
  std::ofstream(file) << "{\"best\": 3}";
  REQUIRE(std::filesystem::exists(file));
  CHECK(fresh_test_save_file(game) == file);
  CHECK_FALSE(std::filesystem::exists(file));
  CHECK(fresh_test_save_file(game) == file);  // nothing to empty is no error
}
