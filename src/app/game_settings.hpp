#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "app/android/android_settings.hpp"
#include "render/display_settings.hpp"
#include "script/script_config.hpp"

// game.json: what a game says about itself. This is the one place that reads the file.
namespace redgem {

inline constexpr const char* kGameSettingsFile = "game.json";

struct GameSettings {
  std::string title;
  int window_width = 0;
  int window_height = 0;
  // What the game is known by wherever it is kept and whatever it is called: its save follows it,
  // and so does its Android app unless the app is given a package. Empty for a game that has none.
  //   "id": "com.mystudio.mygame"   or   "id": "my-game-3fa2c1b0"
  std::string id;
  AndroidSettings android;
  // How the game is shown until its player chooses otherwise: the window's size above, and
  //   "fullscreen": true, "vsync": false, "fps_cap": 144 (0: none), "render_scale": 0.75,
  //   "quality": "medium"
  DisplaySettings display;
  // How long one call into the game's scripts may run before it is stopped (see
  // script/script_budget.hpp), in milliseconds, 1 to config::kMaxScriptBudgetMs:
  //   "script_budget_ms": 2000
  int script_budget_ms = config::kDefaultScriptBudgetMs;
};

// Everything game.json says, read once.
struct GameManifest {
  // Always usable: the defaults (folder name as title, the engine's window size, an app with
  // nothing of its own) stand in for whatever is missing or is not what it should be.
  GameSettings settings;
  // What is wrong with the "android" section, for whoever packs the app; empty when it can be
  // used. (A game plays all the same: the section only matters to a build for Android.)
  std::string android_error;
  // What is wrong with the "id"; empty when it can be used or the game has none. (A game plays
  // all the same, as one with no id does.)
  std::string id_error;
  // What is wrong with "script_budget_ms"; empty when it can be used or the file has none. (The
  // scripts get the engine's default instead.)
  std::string script_budget_error;
};
GameManifest read_game_manifest(const std::filesystem::path& game_dir);

// Whether a text can be a game's id: small letters, digits, '.', '-' and '_', starting and ending
// with a letter or a digit, with no two dots in a row, up to kMaxGameIdLength long.
inline constexpr std::size_t kMaxGameIdLength = 100;
bool valid_game_id(const std::string& id);
// The settings alone, for whoever has no use for what is wrong with the file.
GameSettings read_game_settings(const std::filesystem::path& game_dir);
// Writes the title, the window's size and the id (when the settings have one). What else the file
// holds (its "android" section, an id the settings have none for...) stays as it is.
bool write_game_settings(const std::filesystem::path& game_dir, const GameSettings& settings, std::string& error);

}  // namespace redgem
