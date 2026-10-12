#pragma once

#include <filesystem>

namespace redgem::testing {

// Where tests leave notes for each other: a folder of the tests' own among the temporary files.
// (Scripts cannot touch files, and two games being tested together still have to tell each other
// things before they are connected: a room's code.)
std::filesystem::path notes_folder();
// Keeps the notes in `folder` from here on instead: a run with a folder of its own (see
// SystemTestOptions::work_dir) shares its notes only with the tests run with it.
void keep_notes_in(const std::filesystem::path& folder);
// Where the notes of a run with a folder of its own go.
std::filesystem::path notes_folder_in(const std::filesystem::path& work_dir);

}  // namespace redgem::testing
