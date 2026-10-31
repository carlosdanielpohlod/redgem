#include "editor/agent/workspace_files.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
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

// The names Windows keeps for devices, whatever their extension ("nul.txt" is the device too).
constexpr const char* kDeviceNames[] = {"CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
                                        "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
                                        "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
constexpr char kStreamMark = ':';  // an alternate data stream, after a file's name
constexpr const char* kTemporarySuffix = ".redgem-writing";

bool is_device_name(const fs::path& part) {
  std::string stem = part.stem().u8string();
  std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return std::toupper(c); });
  return std::any_of(std::begin(kDeviceNames), std::end(kDeviceNames), [&](const char* name) { return stem == name; });
}

bool plain_name(const fs::path& part) {
  return part.u8string().find(kStreamMark) == std::string::npos && !is_device_name(part);
}

// Every step of a path below the folder names a file or folder: no device, no stream.
bool plain_names(const fs::path& relative) {
  return std::all_of(relative.begin(), relative.end(), [](const fs::path& part) { return plain_name(part); });
}

// Where the path leads, once every link and junction that exists on it is followed, is still inside.
bool really_inside(const fs::path& folder, const fs::path& relative) {
  std::error_code error;
  const fs::path real = fs::weakly_canonical(folder / relative, error);
  return !error && stays_inside(real.lexically_relative(folder));
}

// A path that is missing (made new) or is a regular file: never a folder, a pipe or a device.
bool fits_a_file(const fs::path& file) {
  std::error_code error;
  const fs::file_status status = fs::symlink_status(file, error);
  return !fs::exists(status) || fs::is_regular_file(status);
}

// Writes the text beside the file, then puts it in the file's place: a reader never sees half of it.
bool replace_file(const fs::path& file, const std::string& text, std::string& error) {
  fs::path written = file;
  written += kTemporarySuffix;
  {
    std::ofstream out(written, std::ios::binary | std::ios::trunc);
    out << text;
    if (!out) error = "cannot write " + file.u8string();
  }
  std::error_code fs_error;
  if (error.empty()) fs::rename(written, file, fs_error);
  if (fs_error) error = "cannot write " + file.u8string() + ": " + fs_error.message();
  if (!error.empty()) fs::remove(written, fs_error);
  return error.empty();
}

}  // namespace

Workspace::Workspace(const fs::path& folder) : folder_(real_folder(folder)) {}

std::optional<fs::path> Workspace::resolve(const std::string& requested, std::string& error) const {
  const fs::path given = fs::u8path(requested);
  fs::path whole = (given.is_absolute() ? given : folder_ / given).lexically_normal();
  const fs::path relative = whole.lexically_relative(folder_);
  error = refusal(requested, relative);
  if (!error.empty()) return std::nullopt;
  return whole;
}

// Why the file at `relative` (from the folder) is out of the agent's reach; empty when it is not.
std::string Workspace::refusal(const std::string& requested, const fs::path& relative) const {
  if (!stays_inside(relative)) return "'" + requested + "' is outside the game's folder " + folder_.u8string();
  if (!plain_names(relative)) return "'" + requested + "' names a device or a stream, not a file";
  if (through_link(folder_, relative)) return "'" + requested + "' is reached through a symbolic link";
  // (Junctions on Windows are no symbolic links to the check above: where the path really leads is
  // checked too.)
  if (!really_inside(folder_, relative)) return "'" + requested + "' leads outside the game's folder";
  return "";
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
  if (!fits_a_file(*file)) {
    error = "'" + requested + "' is not a regular file";
    return false;
  }
  return make_folders(*file, error) && replace_file(*file, text, error);
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
