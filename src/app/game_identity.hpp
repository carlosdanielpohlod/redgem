#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

// What game.json says a game is when it ships: its version, the pictures it is shown by (an icon
// that builds turn into each system's own, a splash picture shown while it loads) and whether it
// goes online. All of it is optional.
//   { "version": "1.2.0", "icon": "images/icon.png", "splash": "images/splash.png", "network": true }
namespace redgem {

struct GameIdentity {
  std::string version;           // a semantic version ("1.2.0", "2.0.0-beta.1"); empty for none
  std::filesystem::path icon;    // a PNG inside the game folder, as game.json names it; empty for none
  std::filesystem::path splash;  // the same, for the picture shown while the game loads
  bool network = false;          // the game plays online or makes web requests (an app asks for the network)
};

struct GameIdentityReading {
  // Always usable: whatever is missing or wrong is left out (no version, no pictures, offline).
  GameIdentity identity;
  // What is wrong with the file's version, pictures or network flag; empty when nothing is. A game
  // plays all the same; a build refuses it, since what ships would not be what the file says.
  std::string error;
};
GameIdentityReading read_game_identity(const std::filesystem::path& game_dir);

// Whether a text is a semantic version: MAJOR.MINOR.PATCH, numbers with no leading zeros, then
// optionally "-" and pre-release parts and "+" and build parts (letters, digits and '-', joined by
// dots), up to kMaxVersionLength long.
inline constexpr std::size_t kMaxVersionLength = 64;
bool valid_game_version(const std::string& version);

// The three numbers a semantic version starts with ("2.0.0-beta.1" -> "2.0.0"): what systems that
// only take numbers (a Mac app's version) are given.
std::string version_numbers(const std::string& version);

}  // namespace redgem
