#include "core/files.hpp"

#include <fstream>
#include <random>
#include <sstream>

#ifndef _WIN32
#include <cerrno>
#include <sys/stat.h>
#endif

#include "core/game_files.hpp"
#include "core/log.hpp"

namespace redgem {

namespace {

// "a/b/" normalizes to a path whose last element is empty. (The root's own separator is its name.)
bool ends_with_separator(const std::filesystem::path& normal) {
  return !normal.has_filename() && normal.has_parent_path() && normal != normal.root_path();
}

constexpr int kReserveAttempts = 8;

std::string folder_seed() {
  std::random_device random;
  return std::to_string(std::uniform_int_distribution<std::uint64_t>{}(random));
}

bool already_exists(const std::error_code& error) { return !error || error == std::errc::file_exists; }

bool occupied_path(const std::filesystem::path& path, const std::error_code& error) {
  if (already_exists(error)) return true;
#ifdef _WIN32
  std::error_code ignored;
  return std::filesystem::exists(std::filesystem::symlink_status(path, ignored));
#else
  static_cast<void>(path);
  return false;
#endif
}

std::filesystem::path reserve_folder(const std::filesystem::path& parent, const std::string& stem, std::string& error) {
  for (int attempt = 0; attempt < kReserveAttempts; ++attempt) {
    const auto suffix = attempt == 0 ? std::string{} : "-" + std::to_string(attempt);
    auto candidate = parent / (stem + suffix);
    std::error_code problem;
    if (make_private_directory(candidate, problem)) return candidate;
    if (occupied_path(candidate, problem)) continue;
    error = "cannot reserve " + candidate.string() + ": " + problem.message();
    return {};
  }
  error = "cannot reserve " + (parent / stem).string() + ": " + kFolderOccupied;
  return {};
}

// A file of the mounted game archive, as text (see game_files.hpp).
std::optional<std::string> read_archived_text(const std::filesystem::path& path, std::string& error) {
  const auto bytes = game_files::read(path, kMaxArchiveEntryBytes);
  if (bytes) return std::string(bytes->begin(), bytes->end());
  error = "cannot read " + path.string();
  return std::nullopt;
}

}  // namespace

bool make_private_directory(const std::filesystem::path& path, std::error_code& error) {
#ifdef _WIN32
  return std::filesystem::create_directory(path, error);
#else
  error.clear();
  if (::mkdir(path.c_str(), S_IRWXU) == 0) return true;
  error = std::error_code(errno, std::generic_category());
  return false;
#endif
}

std::optional<std::string> read_text_file(const std::filesystem::path& path, std::string& error) {
  if (game_files::in_archive(path)) return read_archived_text(path, error);
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
  out.close();  // (what the disk did not take shows here, once the last of it is flushed)
  if (out) return true;
  error = "cannot write " + path.string();
  return false;
}

bool write_bytes_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes, std::string& error) {
  return write_text_file(path, std::string(bytes.begin(), bytes.end()), error);
}

std::filesystem::path normalized_folder(const std::filesystem::path& folder) {
  const std::filesystem::path normal = folder.lexically_normal();
  return ends_with_separator(normal) ? normal.parent_path() : normal;
}

ReservedFolder::ReservedFolder(const std::filesystem::path& parent, std::string_view prefix, std::string& error,
                               std::string_view seed)
    : path_(reserve_folder(parent, std::string(prefix) + (seed.empty() ? folder_seed() : std::string(seed)), error)) {}

ReservedFolder::~ReservedFolder() {
  if (path_.empty()) return;
  std::error_code problem;
  std::filesystem::remove_all(path_, problem);
  if (problem) log::warning("cannot remove temporary folder " + path_.string() + ": " + problem.message());
}

}  // namespace redgem
