#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace redgem {

// game.json: what a game says about itself.
struct GameSettings {
  std::string title;
  int window_width = 0;
  int window_height = 0;
};

// Defaults (folder name as title, engine window size) for anything missing.
GameSettings read_game_settings(const std::filesystem::path& game_dir);
bool write_game_settings(const std::filesystem::path& game_dir, const GameSettings& settings, std::string& error);

// Starting points for new games: "empty", "platformer".
const std::vector<std::string>& game_templates();
// One line about what a template starts with.
std::string template_description(const std::string& template_name);
// A name usable as a game folder: letters, digits, spaces, '-' and '_' (not starting with a space).
bool valid_game_name(const std::string& name);

// Creates a playable game folder (scene.glb, scripts/, sounds/, scenes/, models/, game.json).
// Fails if the folder exists and is not empty.
bool create_game(const std::filesystem::path& game_dir, const std::string& template_name, std::string& error);

// A folder path as the rest of the code expects it: no "." or "..", no trailing separator
// ("games/foo/" -> "games/foo", whose filename is the game's name).
std::filesystem::path normalized_folder(const std::filesystem::path& folder);

// Makes a standalone game in `out_dir`: an executable named after the game (a copy of the engine)
// next to a folder with the game's files. Started with no arguments, that executable plays the
// game beside it. Refuses an `out_dir` that is the game folder or inside it (the copy would
// contain itself).
bool build_game(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                const std::filesystem::path& engine_binary, std::string& error);

// The file name of a game's executable: its folder name in lowercase, with anything that is not a
// letter or a digit turned into '_' ("My Game" -> "my_game").
std::string executable_name(const std::filesystem::path& game_dir);
// The game a built executable ships with: the game folder next to it, when there is one.
std::optional<std::filesystem::path> bundled_game(const std::filesystem::path& executable);

// Games found in a folder (subfolders with a scene.glb), sorted by name.
std::vector<std::filesystem::path> find_games(const std::filesystem::path& folder);

// Where the project hub creates new games: ~/RedGemGames.
std::filesystem::path default_games_folder();
// Games opened recently, most recent first, in a small JSON file (~/.config/redgem/recent.json).
std::filesystem::path recent_games_file();
// Only games that still exist are returned.
std::vector<std::filesystem::path> read_recent_games(const std::filesystem::path& file);
void remember_game(const std::filesystem::path& file, const std::filesystem::path& game_dir);

}  // namespace redgem
