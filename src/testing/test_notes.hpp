#pragma once

#include <filesystem>

namespace redgem::testing {

// Where tests leave notes for each other: a folder of the tests' own among the temporary files.
// (Scripts cannot touch files, and two games being tested together still have to tell each other
// things before they are connected: a room's code.)
std::filesystem::path notes_folder();

}  // namespace redgem::testing
