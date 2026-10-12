#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

// game.json: what a game says about itself. This is the one place that reads the file.
namespace redgem {

inline constexpr const char* kGameSettingsFile = "game.json";

// What a game says about its Android app, in the "android" section of its game.json. All of it
// is optional: a game that says nothing gets an id made from its folder name, version 1.0, and
// a key the engine makes on this machine.
//   "android": { "package": "com.mystudio.mygame", "version_code": 3, "version_name": "1.2",
//                "keystore": "release.keystore", "key_alias": "mygame" }
struct AndroidSettings {
  std::string package;               // the app's id; empty for one made from the folder name
  int version_code = 1;              // goes up with every release: Android updates only to higher ones
  std::string version_name = "1.0";  // what players see
  std::filesystem::path
      keystore;           // the developer's own signing key (relative to the game folder); empty for the engine's
  std::string key_alias;  // the key's name inside that keystore
};

struct GameSettings {
  std::string title;
  int window_width = 0;
  int window_height = 0;
  // What the game is known by wherever it is kept and whatever it is called: its save follows it,
  // and so does its Android app unless the app is given a package. Empty for a game that has none.
  //   "id": "com.mystudio.mygame"   or   "id": "my-game-3fa2c1b0"
  std::string id;
  AndroidSettings android;
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
};
GameManifest read_game_manifest(const std::filesystem::path& game_dir);

// Whether a text can be a game's id: small letters, digits, '.', '-' and '_', starting and ending
// with a letter or a digit, with no two dots in a row, up to kMaxGameIdLength long.
inline constexpr std::size_t kMaxGameIdLength = 100;
bool valid_game_id(const std::string& id);
// Whether a text can be an Android app's id: at least two parts joined by dots, each a small
// letter followed by small letters, digits or underscores ("com.mystudio.my_game").
bool valid_android_package(const std::string& package);

// The settings alone, for whoever has no use for what is wrong with the file.
GameSettings read_game_settings(const std::filesystem::path& game_dir);
// Writes the title, the window's size and the id (when the settings have one). What else the file
// holds (its "android" section, an id the settings have none for...) stays as it is.
bool write_game_settings(const std::filesystem::path& game_dir, const GameSettings& settings, std::string& error);

}  // namespace redgem
