#pragma once

#include <filesystem>

namespace redgem::test {

// Writes what the window shows to a PNG (folders are made as needed), for whoever looks at a run
// afterwards. Call right after a frame was drawn. False when the file cannot be written.
bool save_window_picture(const std::filesystem::path& path);

}  // namespace redgem::test
