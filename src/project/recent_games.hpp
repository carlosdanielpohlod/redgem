#pragma once

#include <filesystem>
#include <vector>

// The games on this machine that the project hub offers: the ones in the games folder and the
// ones opened lately.
namespace redgem {

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
