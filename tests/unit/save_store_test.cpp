#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "app/save_location.hpp"
#include "core/platform.hpp"
#include "storage/save_store.hpp"

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

TEST_CASE("each game saves in a file named after its folder") {
  const std::filesystem::path player = save_file_for("/somewhere/games/platformer");
  CHECK(player.filename().string().rfind("platformer-", 0) == 0);
  CHECK(player.extension() == ".json");
  CHECK(player.parent_path() == platform::user_data_dir() / "saves");
  // A built game's files sit in a folder called "game" next to its executable.
  CHECK(save_file_for("/somewhere/dist/MyGame/game").filename().string().rfind("MyGame-", 0) == 0);
  CHECK(save_file_for("/somewhere/dist/MyGame/game") == save_file_for("/somewhere/dist/MyGame"));
}

TEST_CASE("two games in folders of the same name do not share a save, and one game always finds its own") {
  const std::filesystem::path mine = save_file_for("/home/ana/games/platformer");
  const std::filesystem::path downloaded = save_file_for("/home/ana/Downloads/platformer");
  CHECK(mine != downloaded);
  CHECK(mine.parent_path() == downloaded.parent_path());
  // The same folder, however it is written.
  CHECK(save_file_for("/home/ana/games/platformer/") == mine);
  CHECK(save_file_for("/home/ana/games/other/../platformer") == mine);
}

TEST_CASE("a game with an id keeps its save wherever its folder goes and whatever it is renamed to") {
  const std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem-save-location-test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "here" / "coins");
  std::ofstream(root / "here" / "coins" / "game.json") << R"({"id": "com.studio.coins"})";
  const std::filesystem::path before = save_file_for(root / "here" / "coins");
  CHECK(before == saves_folder() / "by-id" / "com.studio.coins.json");
  std::filesystem::create_directories(root / "there");
  std::filesystem::rename(root / "here" / "coins", root / "there" / "Coin Rush");
  CHECK(save_file_for(root / "there" / "Coin Rush") == before);
  // A built game carries its game.json in its "game" folder: the same game, the same save.
  std::filesystem::create_directories(root / "dist" / "CoinRush");
  std::filesystem::copy(root / "there" / "Coin Rush", root / "dist" / "CoinRush" / "game");
  CHECK(save_file_for(root / "dist" / "CoinRush" / "game") == before);
  std::filesystem::remove_all(root);
}

TEST_CASE("a save is only ever looked for in one place: a game takes no other's") {
  const std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem-save-location-test";
  std::filesystem::remove_all(root);
  const std::filesystem::path game = root / "coins";
  std::filesystem::create_directories(game);
  const std::filesystem::path by_folder = save_file_for(game);
  CHECK(by_folder.parent_path() == saves_folder());
  // Given an id, the game starts a save of its own: the one named after its folder stays behind.
  std::ofstream(game / "game.json") << R"({"id": "coins-3fa2c1b0"})";
  const std::filesystem::path by_id = save_file_for(game);
  CHECK(by_id != by_folder);
  CHECK(by_id.parent_path() == saves_folder() / "by-id");
  // No folder's name, wherever the folder is, names a file among the saves kept by id.
  std::filesystem::create_directories(root / "by-id");
  CHECK(save_file_for(root / "by-id").parent_path() == saves_folder());
  // An id that cannot be used is no id: the save goes by the folder, as before.
  std::ofstream(game / "game.json") << R"({"id": "../../elsewhere"})";
  CHECK(save_file_for(game) == by_folder);
  std::filesystem::remove_all(root);
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
