#include "app/bundled_game.hpp"

#include "app/scene_files.hpp"

namespace redgem {

std::optional<std::filesystem::path> bundled_game(const std::filesystem::path& executable) {
  std::filesystem::path game = executable.parent_path() / kBundledGameFolder;
  if (!is_game_folder(game)) return std::nullopt;
  return game;
}

}  // namespace redgem
