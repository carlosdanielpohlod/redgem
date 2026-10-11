#include "core/files.hpp"

#include <fstream>
#include <sstream>

namespace redgem {

std::optional<std::string> read_text_file(const std::filesystem::path& path, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot read " + path.string();
    return std::nullopt;
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

bool write_text_file(const std::filesystem::path& path, const std::string& text, std::string& error) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out || !out.write(text.data(), static_cast<std::streamsize>(text.size()))) {
    error = "cannot write " + path.string();
    return false;
  }
  return true;
}

std::filesystem::path normalized_folder(const std::filesystem::path& folder) {
  std::filesystem::path normal = folder.lexically_normal();
  // "a/b/" normalizes to a path whose last element is empty.
  if (!normal.has_filename() && normal.has_parent_path() && normal != normal.root_path()) normal = normal.parent_path();
  return normal;
}

}  // namespace redgem
