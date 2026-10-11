#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "app/save_store.hpp"

using namespace redgem;

namespace {

constexpr double kBestScore = 1250.0;
constexpr double kVolume = 0.35;

// A save file in a folder of its own that does not exist yet, removed when the test ends.
struct TempSave {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-save-store-test";
  std::filesystem::path file = dir / "nested" / "game.json";
  TempSave() { std::filesystem::remove_all(dir); }
  ~TempSave() { std::filesystem::remove_all(dir); }
};

}  // namespace

TEST_CASE("what a game saves is there the next time it runs") {
  const TempSave temp;
  {
    SaveStore store(temp.file);
    CHECK_FALSE(store.get("best").has_value());
    CHECK(store.set("best", kBestScore));
    CHECK(store.set("name", std::string("Ana \"the fast\"")));
    CHECK(store.set("muted", true));
    CHECK(store.set("volume", kVolume));
  }
  SaveStore again(temp.file);
  CHECK(std::get<double>(*again.get("best")) == doctest::Approx(kBestScore));
  CHECK(std::get<std::string>(*again.get("name")) == "Ana \"the fast\"");
  CHECK(std::get<bool>(*again.get("muted")));
  CHECK(std::get<double>(*again.get("volume")) == doctest::Approx(kVolume));
  CHECK(again.keys() == std::vector<std::string>{"best", "muted", "name", "volume"});
}

TEST_CASE("saved values can be removed one by one or all at once") {
  const TempSave temp;
  SaveStore store(temp.file);
  store.set("a", 1.0);
  store.set("b", 2.0);
  CHECK(store.remove("a"));
  CHECK_FALSE(store.get("a").has_value());
  CHECK(SaveStore(temp.file).keys() == std::vector<std::string>{"b"});
  CHECK(store.clear());
  CHECK(SaveStore(temp.file).keys().empty());
}

TEST_CASE("a damaged save file starts from nothing instead of stopping the game") {
  const TempSave temp;
  std::filesystem::create_directories(temp.file.parent_path());
  std::ofstream(temp.file) << "{ not json";
  SaveStore store(temp.file);
  CHECK(store.keys().empty());
  CHECK(store.set("best", kBestScore));
  CHECK(std::get<double>(*SaveStore(temp.file).get("best")) == doctest::Approx(kBestScore));
}

TEST_CASE("each game saves in a file named after its folder, and tests save somewhere else") {
  const std::filesystem::path player = save_file_for("/somewhere/games/platformer");
  CHECK(player.filename().string().rfind("platformer-", 0) == 0);
  CHECK(player.extension() == ".json");
  CHECK(player.parent_path().filename() == "saves");
  // A built game's files sit in a folder called "game" next to its executable.
  CHECK(save_file_for("/somewhere/dist/MyGame/game").filename().string().rfind("MyGame-", 0) == 0);
  CHECK(save_file_for("/somewhere/dist/MyGame/game") == save_file_for("/somewhere/dist/MyGame"));
  const std::filesystem::path tested = test_save_file_for("/somewhere/games/platformer");
  CHECK(tested.filename() == player.filename());
  CHECK(tested.parent_path() != player.parent_path());
}

TEST_CASE("two games in folders of the same name do not share a save, and one game always finds its own") {
  const std::filesystem::path mine = save_file_for("/home/ana/games/platformer");
  const std::filesystem::path downloaded = save_file_for("/home/ana/Downloads/platformer");
  CHECK(mine != downloaded);
  CHECK(mine.parent_path() == downloaded.parent_path());
  // The same folder, however it is written.
  CHECK(save_file_for("/home/ana/games/platformer/") == mine);
  CHECK(save_file_for("/home/ana/games/other/../platformer") == mine);
  CHECK(test_save_file_for("/home/ana/games/platformer") != test_save_file_for("/home/ana/Downloads/platformer"));
}

TEST_CASE("test saves are kept in the user's own folder, not where every user of the machine can write") {
  const std::filesystem::path tested = test_save_file_for("/somewhere/games/platformer");
  const std::filesystem::path players = save_file_for("/somewhere/games/platformer");
  CHECK(tested.parent_path().parent_path() == players.parent_path().parent_path());
  const std::string shared = std::filesystem::temp_directory_path().string();
  CHECK(tested.parent_path().parent_path().string().rfind(shared, 0) != 0);
}

TEST_CASE("what a save file holds that a game cannot save is left out") {
  const TempSave temp;
  std::filesystem::create_directories(temp.file.parent_path());
  std::ofstream(temp.file) << R"({"best": 1250.0, "name": "Ana", "muted": true, "list": [1, 2], "nothing": null,
                                  "inner": {"a": 1}})";
  SaveStore store(temp.file);
  CHECK(store.keys() == std::vector<std::string>{"best", "muted", "name"});
  CHECK(std::get<double>(*store.get("best")) == doctest::Approx(kBestScore));
  CHECK(std::get<std::string>(*store.get("name")) == "Ana");
  CHECK(std::get<bool>(*store.get("muted")));
  CHECK_FALSE(store.get("list").has_value());
}

TEST_CASE("a save is written whole: every kind of value, and no half-written file beside it") {
  const TempSave temp;
  SaveStore store(temp.file);
  REQUIRE(store.set("volume", kVolume));
  REQUIRE(store.set("name", std::string("Ana")));
  REQUIRE(store.set("muted", false));
  CHECK_FALSE(std::filesystem::exists(temp.file.string() + ".part"));
  SaveStore again(temp.file);
  CHECK(std::get<double>(*again.get("volume")) == doctest::Approx(kVolume));
  CHECK(std::get<std::string>(*again.get("name")) == "Ana");
  CHECK_FALSE(std::get<bool>(*again.get("muted")));
}

TEST_CASE("a save that cannot be written says so and keeps the value for this run") {
  const TempSave temp;
  // A file where the save's folder should be: nothing can be written under it.
  std::filesystem::create_directories(temp.dir);
  std::ofstream(temp.dir / "nested") << "in the way";
  SaveStore store(temp.file);
  CHECK_FALSE(store.set("best", kBestScore));
  CHECK(std::get<double>(*store.get("best")) == doctest::Approx(kBestScore));
}
