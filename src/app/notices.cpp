#include "app/notices.hpp"

#include <fstream>

#include "generated/licenses.hpp"

namespace redgem {

bool write_notices(const std::filesystem::path& folder, std::string& error) {
  const std::filesystem::path notices = folder / kNoticesFolder;
  std::error_code fs_error;
  std::filesystem::create_directories(notices, fs_error);
  for (const generated::EmbeddedFile& file : generated::kLicenseFiles) {
    std::ofstream out(notices / file.path, std::ios::binary);
    out << file.contents;
    if (out) continue;
    error = "cannot write " + (notices / file.path).string();
    return false;
  }
  return true;
}

}  // namespace redgem
