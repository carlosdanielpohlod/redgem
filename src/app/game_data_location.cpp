#include "app/game_data_location.hpp"

#include <string>

#include "app/bundled_game.hpp"
#include "app/game_settings.hpp"
#include "core/files.hpp"
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

// The folder a game is known by, however its path is written: a built game's files sit in a folder
// called "game", and it is the folder around that one.
std::filesystem::path named_folder(const std::filesystem::path& game_dir) {
  std::error_code fs_error;
  std::filesystem::path named = normalized_folder(std::filesystem::weakly_canonical(game_dir, fs_error));
  if (fs_error) named = normalized_folder(std::filesystem::absolute(game_dir, fs_error));
  return named.filename() == kBundledGameFolder ? named.parent_path() : named;
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
  const GameManifest manifest = read_game_manifest(game_dir);
  if (manifest.title_given) return game_data_subfolder(manifest.settings);
  GameSettings settings = manifest.settings;
  settings.title = named_folder(game_dir).filename().u8string();
  return game_data_subfolder(settings);
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
