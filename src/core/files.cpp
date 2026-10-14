#include "core/files.hpp"

#include <fstream>
#include <sstream>

namespace redgem {

namespace {

// "a/b/" normalizes to a path whose last element is empty. (The root's own separator is its name.)
bool ends_with_separator(const std::filesystem::path& normal) {
  return !normal.has_filename() && normal.has_parent_path() && normal != normal.root_path();
}

}  // namespace

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
  if (out) out.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (out) return true;
  error = "cannot write " + path.string();
  return false;
}

std::filesystem::path normalized_folder(const std::filesystem::path& folder) {
  const std::filesystem::path normal = folder.lexically_normal();
  return ends_with_separator(normal) ? normal.parent_path() : normal;
}

}  // namespace redgem
