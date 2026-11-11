#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "app/game_data_location.hpp"
#include "core/platform.hpp"
#include "storage/save_file.hpp"
#include "storage/save_slots.hpp"
#include "testing/test_saves.hpp"

using namespace redgem;
using namespace redgem::testing;

namespace {

// A game folder of the test's own saying who made it and what it is called, removed when the test ends.
struct NamedGame {
  std::filesystem::path dir;
  NamedGame(const std::string& folder, const std::string& title)
      : dir(std::filesystem::temp_directory_path() / "redgem-testing-tests-named" / folder) {
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "game.json") << R"({"studio": "Pine Hill", "title": ")" << title << R"("})";
  }
  ~NamedGame() { std::filesystem::remove_all(dir); }
  NamedGame(const NamedGame&) = delete;
  NamedGame& operator=(const NamedGame&) = delete;
};

}  // namespace

TEST_CASE("tests save in a folder named as the game's own, apart from its players' and its developer's") {
  const NamedGame game("coins", "Coin Rush");
  const std::filesystem::path tested = test_save_file_for(game.dir);
  const GameDataFiles players = game_data_files(players_data_folder(game.dir));
  CHECK(tested.filename() == players.save.filename());
  CHECK(tested.parent_path() == test_data_folder(game.dir));
  CHECK(tested != players.save);
  CHECK(tested != game_data_files(developer_data_folder(game.dir)).save);
  CHECK(test_data_folder(game.dir).lexically_relative(platform::user_data_dir() / "test-saves") ==
        game_data_subfolder(game.dir));
}

TEST_CASE("test saves are kept in the user's own folder, not where every user of the machine can write") {
  const NamedGame game("coins", "Coin Rush");
  const std::filesystem::path tested = test_save_file_for(game.dir);
  CHECK(tested.string().rfind(platform::user_data_dir().string(), 0) == 0);
  const std::string shared = std::filesystem::temp_directory_path().string();
  CHECK(tested.string().rfind(shared, 0) != 0);
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

TEST_CASE("each test of each game keeps its settings in a file of its own, emptied before it runs") {
  const NamedGame first_game("one", "First Game");
  const NamedGame second_game("two", "Second Game");
  const std::filesystem::path& one = first_game.dir;
  const std::filesystem::path test = "/tests/system/x/menu_test.rb";
  const std::filesystem::path first = fresh_settings_file_for(one, "", test);
  CHECK(first != fresh_settings_file_for(second_game.dir, "", test));  // the same test's name, another game
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

TEST_CASE("a run that goes on from an earlier one plays with the files that run left, as it left them") {
  const std::filesystem::path game = "/nowhere/redgem-testing-tests/one/game";
  const std::filesystem::path work = std::filesystem::temp_directory_path() / "redgem-testing-kept-work";
  const std::filesystem::path earlier = "/tests/system/x/settings_test.rb";
  const RunFiles first = fresh_run_files(game, work, earlier);
  CHECK(first.save == fresh_save_file_in(work, earlier));
  CHECK(first.settings == fresh_settings_file_for(game, work, earlier));
  CHECK(first.bindings == fresh_bindings_file_for(game, work, earlier));
  std::filesystem::create_directories(first.bindings.parent_path());
  std::ofstream(first.bindings) << "{}";
  const RunFiles kept = kept_run_files(game, work, earlier);
  CHECK(kept.save == first.save);
  CHECK(kept.settings == first.settings);
  CHECK(kept.bindings == first.bindings);
  CHECK(std::filesystem::exists(first.bindings));  // (not emptied)
  CHECK(fresh_run_files(game, "", earlier).save == test_save_file_for(game));
  std::filesystem::remove_all(work);
}

TEST_CASE("a run that goes on from an earlier one plays on copies of its files, the same each time") {
  const std::filesystem::path game = "/nowhere/redgem-testing-tests/one/game";
  const std::filesystem::path work = std::filesystem::temp_directory_path() / "redgem-testing-copied-work";
  const std::filesystem::path earlier = "/tests/system/x/settings_test.rb";
  const std::filesystem::path later = "/tests/system_restarts/x/settings_restarted_test.rb";
  const RunFiles first = fresh_run_files(game, work, earlier);
  std::filesystem::create_directories(first.save.parent_path());
  std::ofstream(first.save) << "{\"best\": 3}";
  std::ofstream(save_backup_of(first.save)) << "{\"best\": 2}";
  std::ofstream(first.bindings) << "{}";
  for (int run = 0; run < 2; ++run) {
    const RunFiles own = copied_run_files(game, work, later, earlier);
    CHECK(own.save != first.save);
    CHECK(std::filesystem::exists(own.save));
    CHECK(std::filesystem::exists(save_backup_of(own.save)));
    CHECK(std::filesystem::exists(own.bindings));
    CHECK_FALSE(std::filesystem::exists(own.settings));    // (the earlier run kept none)
    std::ofstream(own.bindings) << "{\"changed\": true}";  // what this run does is its own
  }
  {
    std::ifstream kept(first.bindings);  // (closed before the folder goes: Windows keeps open files)
    CHECK(std::string(std::istreambuf_iterator<char>(kept), {}) == "{}");
  }
  std::filesystem::remove_all(work);
}

TEST_CASE("a run in a work folder leaves word that it was made there, even having kept nothing") {
  const std::filesystem::path work = std::filesystem::temp_directory_path() / "redgem-testing-marked-work";
  std::filesystem::remove_all(work);
  CHECK_FALSE(ran_in(work, "/tests/system/x/settings_test.rb"));
  mark_run(work, "/tests/system/x/settings_test.rb");
  CHECK(ran_in(work, "settings_test.rb"));
  CHECK_FALSE(ran_in(work, "other_test.rb"));
  std::filesystem::remove_all(work);
}

TEST_CASE("copied game-test runs carry named slots and backups without sharing progress") {
  const auto work = std::filesystem::temp_directory_path() / "redgem-testing-slots-work";
  std::filesystem::remove_all(work);
  const auto first = fresh_run_files("/games/slot-test", work, "first_test.rb");
  SaveFile original(first.save);
  SaveSlots first_slots;
  first_slots.use_default(original);
  REQUIRE(first_slots.select("campaign").set("coins", 1));
  REQUIRE(first_slots.select("campaign").set("coins", 2));
  REQUIRE(first_slots.select("once").set("coins", 41));
  const auto copied = copied_run_files("/games/slot-test", work, "later_test.rb", "first_test.rb");
  SaveFile second(copied.save);
  SaveSlots second_slots;
  second_slots.use_default(second);
  CHECK(second_slots.select("campaign").get("coins") == 2);
  CHECK(second_slots.select("once").get("coins") == 41);
  CHECK_FALSE(std::filesystem::exists(save_backup_of(save_slot_file(copied.save, "once"))));
  CHECK(std::filesystem::exists(save_backup_of(save_slot_file(copied.save, "campaign"))));
  REQUIRE(second_slots.select("campaign").set("coins", 99));
  CHECK(first_slots.select("campaign").get("coins") == 2);
  const auto fresh = fresh_run_files("/games/slot-test", work, "later_test.rb");
  SaveFile reset(fresh.save);
  SaveSlots reset_slots;
  reset_slots.use_default(reset);
  CHECK(reset_slots.select("campaign").get("coins") == std::nullopt);
  CHECK(first_slots.select("campaign").get("coins") == 2);
  std::filesystem::remove_all(work);
}
