#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/game_settings.hpp"
#include "app/app_config.hpp"

using namespace redgem;

namespace {

constexpr int kWidth = 800;
constexpr int kHeight = 450;
constexpr int kVersionCode = 3;

// A game folder of the test's own, with the game.json given.
std::filesystem::path game_saying(const std::string& json) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem_game_settings_test" / "Coin Rush";
  std::filesystem::remove_all(dir.parent_path());
  std::filesystem::create_directories(dir);
  if (!json.empty()) std::ofstream(dir / kGameSettingsFile) << json;
  return dir;
}

}  // namespace

TEST_CASE("a game that says nothing is named after its folder and gets the engine's window") {
  const GameManifest manifest = read_game_manifest(game_saying(""));
  CHECK(manifest.settings.title == "Coin Rush");
  CHECK(manifest.settings.window_width == config::kWindowWidth);
  CHECK(manifest.settings.window_height == config::kWindowHeight);
  CHECK(manifest.settings.android.package.empty());
  CHECK(manifest.settings.android.version_code == 1);
  CHECK(manifest.android_error.empty());
  CHECK(std::string(kGameSettingsFile) == "game.json");
}

TEST_CASE("one reading of game.json gives the game's settings and its Android app's") {
  const std::filesystem::path game = game_saying(R"({"title": "Coins!", "window_width": 800, "window_height": 450,
      "android": {"package": "com.studio.coins", "version_code": 3, "version_name": "0.3"}})");
  const GameManifest manifest = read_game_manifest(game);
  CHECK(manifest.android_error.empty());
  CHECK(manifest.settings.title == "Coins!");
  CHECK(manifest.settings.window_width == kWidth);
  CHECK(manifest.settings.window_height == kHeight);
  CHECK(manifest.settings.android.package == "com.studio.coins");
  CHECK(manifest.settings.android.version_code == kVersionCode);
  CHECK(manifest.settings.android.version_name == "0.3");
  CHECK(read_game_settings(game).android.package == "com.studio.coins");
}

TEST_CASE("an Android section that cannot be used is said, and the game still has its settings") {
  const GameManifest manifest =
      read_game_manifest(game_saying(R"({"title": "Coins!", "window_width": 800, "android": {"package": "Coins"}})"));
  CHECK(manifest.android_error.find("game.json: android.package") == 0);
  CHECK(manifest.settings.title == "Coins!");
  CHECK(manifest.settings.window_width == kWidth);
}

TEST_CASE("saving the settings leaves the Android section of the file as it was") {
  const std::filesystem::path game = game_saying(R"({"android": {"package": "com.studio.coins"}, "mine": 7})");
  GameSettings settings = read_game_settings(game);
  settings.title = "Renamed";
  settings.android.package = "com.other.app";  // not what this writes
  std::string error;
  REQUIRE_MESSAGE(write_game_settings(game, settings, error), error);
  const GameSettings saved = read_game_settings(game);
  CHECK(saved.title == "Renamed");
  CHECK(saved.android.package == "com.studio.coins");
  std::filesystem::remove_all(game.parent_path());
}
