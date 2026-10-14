#include "project/recent_games.hpp"

#include <algorithm>
#include <string>

#include <nlohmann/json.hpp>

#include "app/json_file.hpp"
#include "app/scene_files.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"

namespace redgem {
namespace {

constexpr int kJsonIndent = 2;
constexpr const char* kGamesFolderName = "RedGemGames";
constexpr const char* kRecentFileName = "recent.json";
constexpr std::size_t kMaxRecentGames = 12;

}  // namespace

std::vector<std::filesystem::path> find_games(const std::filesystem::path& folder) {
  std::vector<std::filesystem::path> games;
  std::error_code fs_error;
  for (const auto& entry : std::filesystem::directory_iterator(folder, fs_error)) {
    // Unreadable folders (other users' temp dirs...) are simply skipped.
    if (!entry.is_directory(fs_error)) continue;
    if (is_game_folder(entry.path())) games.push_back(entry.path());
  }
  std::sort(games.begin(), games.end());
  return games;
}

std::filesystem::path default_games_folder() { return platform::home_dir() / kGamesFolderName; }

std::filesystem::path recent_games_file() { return platform::user_data_dir() / kRecentFileName; }

std::vector<std::filesystem::path> read_recent_games(const std::filesystem::path& file) {
  std::vector<std::filesystem::path> games;
  const nlohmann::json json = read_json_file(file);
  if (!json.is_array()) return games;
  for (const nlohmann::json& entry : json) {
    if (!entry.is_string()) continue;
    const std::filesystem::path game = entry.get<std::string>();
    if (is_game_folder(game)) games.push_back(game);
  }
  return games;
}

void remember_game(const std::filesystem::path& file, const std::filesystem::path& game_dir) {
  std::error_code fs_error;
  const std::filesystem::path game = std::filesystem::weakly_canonical(game_dir, fs_error);
  std::vector<std::filesystem::path> games = read_recent_games(file);
  games.erase(std::remove(games.begin(), games.end(), game), games.end());
  games.insert(games.begin(), game);
  if (games.size() > kMaxRecentGames) games.resize(kMaxRecentGames);
  nlohmann::json json = nlohmann::json::array();
  for (const auto& path : games) json.push_back(path.string());
  std::filesystem::create_directories(file.parent_path(), fs_error);
  std::string error;
  write_text_file(file, json.dump(kJsonIndent) + "\n", error);  // best effort: only a convenience
}

}  // namespace redgem
