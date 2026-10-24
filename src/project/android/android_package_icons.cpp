// Packing an Android app: its launcher icon, at each screen density.

#include <vector>

#include "core/files.hpp"
#include "project/android/android_package.hpp"
#include "project/android/android_package_names.hpp"
#include "project/icons/icon_set.hpp"

namespace redgem {
namespace {

constexpr const char* kDensityFolderPrefix = "mipmap-";       // res/mipmap-<density>/: launcher icons
constexpr const char* kLauncherIconFile = "ic_launcher.png";  // what the manifest calls @mipmap/ic_launcher

std::vector<int> launcher_sides() {
  std::vector<int> sides;
  sides.reserve(kLauncherIconSides.size());
  for (const auto& [density, side] : kLauncherIconSides) sides.push_back(side);
  return sides;
}

bool write_icon(const std::filesystem::path& file, const bytes::Bytes& png, std::string& error) {
  std::error_code fs_error;
  std::filesystem::create_directories(file.parent_path(), fs_error);
  return write_bytes_file(file, png, error);
}

}  // namespace

bool stage_android_icon(const std::filesystem::path& icon, const std::filesystem::path& stage_dir, std::string& error) {
  const std::vector<int> sides = launcher_sides();
  const auto icons = scaled_icons(icon, sides.data(), sides.size(), error);
  if (!icons) return false;
  const std::filesystem::path resources = stage_dir / android_names::kResourcesFolder;
  for (const auto& [density, side] : kLauncherIconSides) {
    const std::filesystem::path file = resources / (std::string(kDensityFolderPrefix) + density) / kLauncherIconFile;
    if (!write_icon(file, icons->at(side), error)) return false;
  }
  return true;
}

}  // namespace redgem
