#include "app/bundled_game.hpp"

#include "core/config.hpp"

namespace redgem {

std::optional<std::filesystem::path> bundled_game(const std::filesystem::path& executable) {
  std::filesystem::path game = executable.parent_path() / kBundledGameFolder;
  std::error_code fs_error;
  if (!std::filesystem::exists(game / config::kSceneFileName, fs_error)) return std::nullopt;
  return game;
}

}  // namespace redgem
