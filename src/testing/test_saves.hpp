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
// The same for the players' bindings of a tested game (Input.bind), ending in -bindings.json: a
// test never plays with keys a person bound anew, nor changes them.
std::filesystem::path fresh_bindings_file_for(const std::filesystem::path& game_dir,
                                              const std::filesystem::path& work_dir,
                                              const std::filesystem::path& test_file);

// What a test run keeps between runs of the game: its save, the player's display settings and the
// players' bindings.
struct RunFiles {
  std::filesystem::path save;
  std::filesystem::path settings;
  std::filesystem::path bindings;
};

// The files a run of `test_file` plays with, emptied: it starts from nothing (see the fresh_
// functions above).
RunFiles fresh_run_files(const std::filesystem::path& game_dir, const std::filesystem::path& work_dir,
                         const std::filesystem::path& test_file);
// The files an earlier test's run in the same work folder played with, as it left them: a run that
// goes on from there is the game started again (redgem-test --after).
RunFiles kept_run_files(const std::filesystem::path& game_dir, const std::filesystem::path& work_dir,
                        const std::filesystem::path& earlier_test);
// Whether any of a run's files is there: a run that kept nothing left none.
bool kept_anything(const RunFiles& files);

}  // namespace redgem::testing
