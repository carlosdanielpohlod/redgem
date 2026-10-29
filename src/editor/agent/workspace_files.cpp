#include "editor/agent/workspace_files.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

#include "core/safe_path.hpp"

namespace redgem::agent {
namespace {

namespace fs = std::filesystem;

constexpr char kLineEnd = '\n';
constexpr const char* kParentStep = "..";
constexpr std::uint32_t kFirstLine = 1;

fs::path real_folder(const fs::path& folder) {
  std::error_code error;
  const fs::path real = fs::weakly_canonical(fs::absolute(folder, error), error);
  return error ? folder.lexically_normal() : real;
}

// Whether a path taken relative to the folder stays inside it: it is not the folder itself, and its
// first step does not go up.
bool stays_inside(const fs::path& relative) {
  return !relative.empty() && relative != "." && *relative.begin() != kParentStep;
}

// No line asked for: the whole file.
bool whole_file(const LineRange& range) { return !range.line && !range.limit; }

bool wants_more(const LineRange& range, std::uint32_t taken) { return !range.limit || taken < *range.limit; }

bool too_big(const std::string& text) { return text.size() > kMaxTextFileBytes; }

bool make_folders(const fs::path& file, std::string& error) {
  std::error_code fs_error;
  fs::create_directories(file.parent_path(), fs_error);
  if (fs_error) error = "cannot make the folder of " + file.u8string() + ": " + fs_error.message();
  return !fs_error;
}

}  // namespace

Workspace::Workspace(const fs::path& folder) : folder_(real_folder(folder)) {}

std::optional<fs::path> Workspace::resolve(const std::string& requested, std::string& error) const {
  const fs::path given = fs::u8path(requested);
  fs::path whole = (given.is_absolute() ? given : folder_ / given).lexically_normal();
  const fs::path relative = whole.lexically_relative(folder_);
  if (!stays_inside(relative)) {
    error = "'" + requested + "' is outside the game's folder " + folder_.u8string();
    return std::nullopt;
  }
  if (through_link(folder_, relative)) {
    error = "'" + requested + "' is reached through a symbolic link";
    return std::nullopt;
  }
  return whole;
}

std::optional<std::string> Workspace::read(const std::string& requested, const LineRange& range,
                                           std::string& error) const {
  const auto file = resolve(requested, error);
  if (!file) return std::nullopt;
  const auto bytes = read_file_up_to(*file, kMaxTextFileBytes);
  if (!bytes) {
    error = "cannot read '" + requested + "': no such file, or bigger than the editor reads";
    return std::nullopt;
  }
  return lines_of(std::string(bytes->begin(), bytes->end()), range);
}

bool Workspace::write(const std::string& requested, const std::string& text, std::string& error) const {
  const auto file = resolve(requested, error);
  if (!file) return false;
  if (too_big(text)) {
    error = "'" + requested + "' would be bigger than the editor writes";
    return false;
  }
  if (!make_folders(*file, error)) return false;
  std::ofstream out(*file, std::ios::binary | std::ios::trunc);
  out << text;
  if (!out) error = "cannot write '" + requested + "'";
  return static_cast<bool>(out);
}

std::string lines_of(const std::string& text, const LineRange& range) {
  if (whole_file(range)) return text;
  const std::uint32_t first = std::max(range.line.value_or(kFirstLine), kFirstLine);
  std::istringstream in(text);
  std::string line;
  std::string out;
  std::uint32_t number = 0;
  std::uint32_t taken = 0;
  while (std::getline(in, line) && wants_more(range, taken)) {
    if (++number < first) continue;
    out += line + kLineEnd;
    ++taken;
  }
  return out;
}

}  // namespace redgem::agent
