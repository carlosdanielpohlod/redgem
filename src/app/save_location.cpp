#include "app/save_location.hpp"

#include <string>

#include "app/bundled_game.hpp"
#include "app/game_settings.hpp"
#include "core/files.hpp"
#include "core/names.hpp"
#include "core/platform.hpp"

namespace redgem {
namespace {

constexpr const char* kSavesFolder = "saves";
// The saves of games with an id, apart from those named after folders: no name of one kind can
// be a name of the other.
constexpr const char* kSavesByIdFolder = "by-id";
constexpr const char* kSaveExtension = ".json";
constexpr char kNameJoiner = '-';
// Before the settings key of a game with no id: no id has a ':', so no id is taken for it.
constexpr const char* kFolderKeyMark = "folder:";

// The folder a game is known by: a built game's files sit in a folder called "game", and it is
// the folder around that one.
std::filesystem::path named_folder(const std::filesystem::path& game_dir) {
  std::error_code fs_error;
  std::filesystem::path named = normalized_folder(std::filesystem::weakly_canonical(game_dir, fs_error));
  if (fs_error) named = normalized_folder(std::filesystem::absolute(game_dir, fs_error));
  return named.filename() == kBundledGameFolder ? named.parent_path() : named;
}

}  // namespace

std::filesystem::path saves_folder() { return platform::user_data_dir() / kSavesFolder; }

namespace {

// What a game with no id goes by: its folder's name, and a few digits made from where the folder
// is (two games in folders of the same name do not share a save).
std::string folder_name(const std::filesystem::path& game_dir) {
  const std::filesystem::path named = named_folder(game_dir);
  return named.filename().string() + kNameJoiner + name_digits(named.generic_string());
}

}  // namespace

std::filesystem::path save_file_for(const std::filesystem::path& game_dir) {
  const std::string id = read_game_settings(game_dir).id;
  if (!id.empty()) return saves_folder() / kSavesByIdFolder / (id + kSaveExtension);
  return saves_folder() / (folder_name(game_dir) + kSaveExtension);
}

std::string game_settings_key(const std::filesystem::path& game_dir) {
  const std::string id = read_game_settings(game_dir).id;
  return id.empty() ? kFolderKeyMark + folder_name(game_dir) : id;
}

}  // namespace redgem
