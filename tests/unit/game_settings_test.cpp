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

TEST_CASE("a game's id is read from game.json, and a game without one has none") {
  CHECK(read_game_manifest(game_saying("")).settings.id.empty());
  CHECK(read_game_manifest(game_saying(R"({"title": "Coins!"})")).settings.id.empty());
  const GameManifest manifest = read_game_manifest(game_saying(R"({"id": "com.studio.coins"})"));
  CHECK(manifest.settings.id == "com.studio.coins");
  CHECK(manifest.id_error.empty());
  CHECK(read_game_settings(game_saying(R"({"id": "coin-rush-3fa2c1b0"})")).id == "coin-rush-3fa2c1b0");
}

TEST_CASE("an id is small letters, digits, dots, dashes and underscores, between letters or digits") {
  for (const char* good : {"a", "coins", "coin-rush", "coin_rush", "com.studio.coins", "3d-maze", "a.b-c_d9"}) {
    CAPTURE(good);
    CHECK(valid_game_id(good));
  }
  for (const char* bad : {"", "Coins", "coin rush", "../coins", "a/b", "a\\b", ".coins", "coins.", "-coins", "coins_",
                          "a..b", "coins\n", "c\xc3\xa7"}) {
    CAPTURE(bad);
    CHECK_FALSE(valid_game_id(bad));
  }
  CHECK(valid_game_id(std::string(kMaxGameIdLength, 'a')));
  CHECK_FALSE(valid_game_id(std::string(kMaxGameIdLength + 1, 'a')));
}

TEST_CASE("an id that cannot be used is said, and the game has none: the rest of its settings stand") {
  for (const char* json : {R"({"title": "Coins!", "id": "Coin Rush"})", R"({"title": "Coins!", "id": 7})",
                           R"({"title": "Coins!", "id": "../../other"})"}) {
    CAPTURE(json);
    const GameManifest manifest = read_game_manifest(game_saying(json));
    CHECK(manifest.id_error.find("game.json: id must look like com.mystudio.mygame or my-game") == 0);
    CHECK(manifest.settings.id.empty());
    CHECK(manifest.settings.title == "Coins!");
    CHECK(manifest.android_error.empty());
  }
}

TEST_CASE("saving the settings keeps the game's id, and writes one given to a game that had none") {
  const std::filesystem::path game = game_saying(R"({"id": "com.studio.coins", "title": "Coins!"})");
  GameSettings settings = read_game_settings(game);
  settings.title = "Renamed";
  std::string error;
  REQUIRE_MESSAGE(write_game_settings(game, settings, error), error);
  CHECK(read_game_settings(game).id == "com.studio.coins");
  CHECK(read_game_settings(game).title == "Renamed");
  // Settings that carry no id leave the file's alone.
  settings.id.clear();
  REQUIRE_MESSAGE(write_game_settings(game, settings, error), error);
  CHECK(read_game_settings(game).id == "com.studio.coins");

  const std::filesystem::path plain = game_saying(R"({"title": "Coins!"})");
  settings = read_game_settings(plain);
  settings.id = "coins-3fa2c1b0";
  REQUIRE_MESSAGE(write_game_settings(plain, settings, error), error);
  CHECK(read_game_settings(plain).id == "coins-3fa2c1b0");
  std::filesystem::remove_all(plain.parent_path());
}

TEST_CASE("game.json says how the game is shown until its player chooses otherwise") {
  const GameSettings plain = read_game_settings(game_saying(R"({"window_width": 800, "window_height": 450})"));
  CHECK(plain.display.resolution == SurfaceSize{kWidth, kHeight});
  CHECK(plain.display.mode == WindowMode::Windowed);
  CHECK(plain.display.vsync);
  CHECK(plain.display.fps_cap == config::kTargetFps);
  const GameSettings shown = read_game_settings(
      game_saying(R"({"fullscreen": true, "vsync": false, "fps_cap": 144, "render_scale": 0.75, "quality": "high"})"));
  CHECK(shown.display.mode == WindowMode::Fullscreen);
  CHECK_FALSE(shown.display.vsync);
  CHECK(shown.display.fps_cap == 144);
  CHECK(shown.display.render_scale == doctest::Approx(0.75f));
  CHECK(shown.display.quality == QualityPreset::High);
  CHECK(shown.display.resolution == SurfaceSize{config::kWindowWidth, config::kWindowHeight});
  const GameSettings odd = read_game_settings(game_saying(R"({"fullscreen": "yes", "render_scale": 9})"));
  CHECK(odd.display.mode == WindowMode::Windowed);
  CHECK(odd.display.render_scale == kLargestRenderScale);
}
