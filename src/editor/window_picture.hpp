#pragma once

#include <filesystem>

namespace redgem {

// Writes the window as it is drawn so far this frame to a PNG (folders are made as needed).
// Call between the last drawing and the end of the frame. False when the file cannot be written.
bool save_window_picture(const std::filesystem::path& path);

}  // namespace redgem
