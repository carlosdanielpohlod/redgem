#include "app/bundled_game.hpp"

#include "app/scene_files.hpp"

namespace redgem {

std::optional<std::filesystem::path> bundled_game(const std::filesystem::path& executable) {
  const std::filesystem::path beside = executable.parent_path() / kBundledGameFolder;
  const std::filesystem::path in_app =
      executable.parent_path().parent_path() / kMacResourcesFolder / kBundledGameFolder;
  for (const std::filesystem::path& game : {beside, in_app}) {
    if (is_game_folder(game)) return game;
  }
  return std::nullopt;
}

}  // namespace redgem
