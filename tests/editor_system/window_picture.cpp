#include "window_picture.hpp"

#include <raylib.h>

namespace redgem::test {

bool save_window_picture(const std::filesystem::path& path) {
  Image image = LoadImageFromScreen();
  std::filesystem::create_directories(path.parent_path());
  const bool written = ExportImage(image, path.string().c_str());
  UnloadImage(image);
  return written;
}

}  // namespace redgem::test
