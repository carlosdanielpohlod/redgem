#pragma once

#include <filesystem>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace redgem {

// What a game says about its Android app, in the "android" section of its game.json. All of it
// is optional: a game that says nothing gets an id made from its folder name, the version its
// game.json gives the game (1.0 when it gives none), and a key the engine makes on this machine.
//   "android": { "package": "com.mystudio.mygame", "version_code": 3, "version_name": "1.2",
//                "keystore": "release.keystore", "key_alias": "mygame" }
struct AndroidSettings {
  std::string package;       // the app's id; empty for one made from the folder name
  int version_code = 1;      // goes up with every release: Android updates only to higher ones
  std::string version_name;  // what players see; empty for the game's own version (or 1.0)
  std::filesystem::path
      keystore;           // the developer's own signing key (relative to the game folder); empty for the engine's
  std::string key_alias;  // the key's name inside that keystore
};

// Whether a text can be an Android app's id: at least two parts joined by dots, each a small
// letter followed by small letters, digits or underscores ("com.mystudio.my_game").
bool valid_android_package(const std::string& package);

// Reads the "android" section of a game.json into `settings`, when the file has one. What is
// wrong with it, said as game.json's own mistake; empty when nothing is.
std::string read_android_section(const nlohmann::json& game_json, AndroidSettings& settings);

}  // namespace redgem
