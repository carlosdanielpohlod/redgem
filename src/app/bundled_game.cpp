#include "app/bundled_game.hpp"

#include "app/scene_files.hpp"

namespace redgem {
namespace {

// Whether a program is the one of a Mac app: <name>.app/Contents/MacOS/<program>.
bool in_mac_app(const std::filesystem::path& executable) {
  const std::filesystem::path folder = executable.parent_path();
  return folder.filename() == kMacProgramFolder && folder.parent_path().filename() == kMacContentsFolder;
}

// Where the game of a program is kept: beside it, or in the resources of the app it is in.
std::filesystem::path game_place(const std::filesystem::path& executable) {
  if (!in_mac_app(executable)) return executable.parent_path() / kBundledGameFolder;
  return executable.parent_path().parent_path() / kMacResourcesFolder / kBundledGameFolder;
}

}  // namespace

std::optional<std::filesystem::path> bundled_game(const std::filesystem::path& executable) {
  std::filesystem::path game = game_place(executable);
  if (!is_game_folder(game)) return std::nullopt;
  return game;
}

}  // namespace redgem
