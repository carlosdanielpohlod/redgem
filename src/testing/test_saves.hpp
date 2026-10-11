#pragma once

#include <filesystem>

namespace redgem::testing {

// Where a tested game's saves go, so a test never reads or changes a player's progress:
// test-saves/ in the user's RedGem folder, in a file named as the game's own save file is.
std::filesystem::path test_save_file_for(const std::filesystem::path& game_dir);

// The same file, emptied: every test starts from no saves at all.
std::filesystem::path fresh_test_save_file(const std::filesystem::path& game_dir);

}  // namespace redgem::testing
