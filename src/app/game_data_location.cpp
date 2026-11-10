#include "app/game_data_location.hpp"

#include <string>

#include "app/game_settings.hpp"
#include "core/folder_names.hpp"
#include "core/platform.hpp"

namespace redgem {
namespace {

constexpr const char* kSaveFile = "save.json";
constexpr const char* kSettingsFile = "settings.json";
constexpr const char* kBindingsFile = "bindings.json";
constexpr const char* kCrashesFolder = "crashes";
// In the user's RedGem folder: the games its user makes keep what they keep while they are played
// there (the editor's Play), apart from what the same games keep for their players.
constexpr const char* kDeveloperFolder = "game-data";

// The folder set_players_data_folder gave; empty until it gives one.
std::filesystem::path& given_players_folder() {
  static std::filesystem::path folder;
  return folder;
}

// A name as a folder's (in UTF-8 on every system), or `unnamed` when nothing of it can be used.
std::filesystem::path folder_named(const std::string& name, const char* unnamed) {
  const std::string safe = safe_folder_name(name);
  return std::filesystem::u8path(safe.empty() ? std::string(unnamed) : safe);
}

}  // namespace

GameDataFiles game_data_files(const std::filesystem::path& folder) {
  return {folder / kSaveFile, folder / kSettingsFile, folder / kBindingsFile, folder / kCrashesFolder};
}

std::filesystem::path game_data_subfolder(const GameSettings& settings) {
  return folder_named(settings.studio, kUnnamedStudioFolder) / folder_named(settings.title, kUnnamedGameFolder);
}

std::filesystem::path game_data_subfolder(const std::filesystem::path& game_dir) {
  return game_data_subfolder(read_game_settings(game_dir));
}

std::filesystem::path players_data_folder(const std::filesystem::path& game_dir) {
  if (!given_players_folder().empty()) return given_players_folder();
  return platform::programs_data_dir() / game_data_subfolder(game_dir);
}

void set_players_data_folder(const std::filesystem::path& folder) { given_players_folder() = folder; }

std::filesystem::path developer_data_folder(const std::filesystem::path& game_dir) {
  return platform::user_data_dir() / kDeveloperFolder / game_data_subfolder(game_dir);
}

}  // namespace redgem
