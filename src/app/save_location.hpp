#pragma once

#include <filesystem>

namespace redgem {

// Where a game's save file lives: saves/<game folder's name>-<digits>.json in the user's RedGem
// folder. The digits come from where the folder is, so that two games in folders of the same
// name keep their own saves; they stay the same for as long as the game stays where it is.
std::filesystem::path save_file_for(const std::filesystem::path& game_dir);

}  // namespace redgem
