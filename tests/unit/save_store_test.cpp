#include <doctest/doctest.h>

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
  CHECK(player.filename() == "platformer.json");
  CHECK(player.parent_path().filename() == "saves");
  // A built game's files sit in a folder called "game" next to its executable.
  CHECK(save_file_for("/somewhere/dist/MyGame/game").filename() == "MyGame.json");
  const std::filesystem::path tested = test_save_file_for("/somewhere/games/platformer");
  CHECK(tested.filename() == "platformer.json");
  CHECK(tested.parent_path() != player.parent_path());
}
