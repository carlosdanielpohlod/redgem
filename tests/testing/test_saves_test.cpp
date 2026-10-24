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

TEST_CASE("tests run in a folder of their own save there, each in a file of its own, starting from nothing") {
  const std::filesystem::path work = std::filesystem::temp_directory_path() / "redgem-testing-tests-work";
  const std::filesystem::path host = fresh_save_file_in(work, "tests/pairs/co_op_host_test.rb");
  const std::filesystem::path guest = fresh_save_file_in(work, "tests/pairs/co_op_guest_test.rb");
  CHECK(host != guest);
  CHECK(host.string().rfind(work.string(), 0) == 0);
  CHECK(host.filename() == "co_op_host_test.json");
  std::filesystem::create_directories(host.parent_path());
  std::ofstream(host) << "{\"best\": 3}";
  CHECK(fresh_save_file_in(work, "tests/pairs/co_op_host_test.rb") == host);
  CHECK_FALSE(std::filesystem::exists(host));
  std::filesystem::remove_all(work);
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

TEST_CASE("a game with an id is tested with a save of its id's, apart from the saves named after folders") {
  const std::filesystem::path game = std::filesystem::temp_directory_path() / "redgem-testing-tests-id" / "a_game";
  std::filesystem::create_directories(game);
  std::ofstream(game / "game.json") << R"({"id": "com.studio.coins"})";
  const std::filesystem::path tested = test_save_file_for(game);
  CHECK(tested.filename() == "com.studio.coins.json");
  CHECK(tested.lexically_relative(tested.parent_path().parent_path()) ==
        save_file_for(game).lexically_relative(saves_folder()));
  CHECK(tested != save_file_for(game));
  std::filesystem::remove_all(game.parent_path());
}

TEST_CASE("each test of each game keeps its settings in a file of its own, emptied before it runs") {
  const std::filesystem::path one = "/nowhere/redgem-testing-tests/one/game";
  const std::filesystem::path two = "/nowhere/redgem-testing-tests/two/game";
  const std::filesystem::path test = "/tests/system/x/menu_test.rb";
  const std::filesystem::path first = fresh_settings_file_for(one, "", test);
  CHECK(first != fresh_settings_file_for(two, "", test));  // the same test's name, another game
  CHECK(first != fresh_settings_file_for(one, "", "/tests/system/x/other_test.rb"));
  CHECK(first.parent_path() == test_save_file_for(one).parent_path());
  std::filesystem::create_directories(first.parent_path());
  std::ofstream(first) << "{}";
  CHECK(fresh_settings_file_for(one, "", test) == first);
  CHECK_FALSE(std::filesystem::exists(first));
  const std::filesystem::path work = std::filesystem::temp_directory_path() / "redgem-testing-settings-work";
  CHECK(fresh_settings_file_for(one, work, test).parent_path() == work / "saves");
}

TEST_CASE("each test of each game keeps its players' bindings apart from its settings, emptied before it runs") {
  const std::filesystem::path game = "/nowhere/redgem-testing-tests/one/game";
  const std::filesystem::path test = "/tests/system/x/rebind_test.rb";
  const std::filesystem::path bindings = fresh_bindings_file_for(game, "", test);
  CHECK(bindings != fresh_settings_file_for(game, "", test));
  CHECK(bindings.parent_path() == test_save_file_for(game).parent_path());
  std::filesystem::create_directories(bindings.parent_path());
  std::ofstream(bindings) << "{}";
  CHECK(fresh_bindings_file_for(game, "", test) == bindings);
  CHECK_FALSE(std::filesystem::exists(bindings));
}
