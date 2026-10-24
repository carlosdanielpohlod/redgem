#pragma once

#include <filesystem>

namespace redgem::testing {

// Where a tested game's saves go, so a test never reads or changes a player's progress:
// test-saves/ in the user's RedGem folder, in a file named as the game's own save file is.
std::filesystem::path test_save_file_for(const std::filesystem::path& game_dir);

// The same file, emptied: every test starts from no saves at all.
std::filesystem::path fresh_test_save_file(const std::filesystem::path& game_dir);

// The save file of one test run in a folder of its own (see SystemTestOptions::work_dir), emptied:
// named after the test, so that two tests playing together in one folder each have theirs.
std::filesystem::path fresh_save_file_in(const std::filesystem::path& work_dir, const std::filesystem::path& test_file);

// The settings file of one test of one game (the player's display settings, see game_display.hpp),
// emptied: the game starts with what its game.json says, and what the test keeps stays apart from
// the player's and from other tests'. Named as the game's save file is, with the test's name: in
// the run's own folder, or beside the tests' saves when it has none.
std::filesystem::path fresh_settings_file_for(const std::filesystem::path& game_dir,
                                              const std::filesystem::path& work_dir,
                                              const std::filesystem::path& test_file);

}  // namespace redgem::testing
