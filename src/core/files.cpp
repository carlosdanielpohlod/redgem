#include "core/files.hpp"

#include <fstream>
#include <random>
#include <sstream>

#ifndef _WIN32
#include <cerrno>
#include <sys/stat.h>
#endif

#include "core/log.hpp"

namespace redgem {

namespace {

// "a/b/" normalizes to a path whose last element is empty. (The root's own separator is its name.)
bool ends_with_separator(const std::filesystem::path& normal) {
  return !normal.has_filename() && normal.has_parent_path() && normal != normal.root_path();
}

constexpr int kReserveAttempts = 8;
constexpr const char* kFolderOccupied = "folder already exists";

std::string folder_seed() {
  std::random_device random;
  return std::to_string(std::uniform_int_distribution<std::uint64_t>{}(random));
}

bool reserve_directory(const std::filesystem::path& path, std::error_code& error) {
#ifdef _WIN32
  return std::filesystem::create_directory(path, error);
#else
  constexpr mode_t kOwnerOnly = S_IRWXU;
  if (::mkdir(path.c_str(), kOwnerOnly) == 0) return true;
  error = std::error_code(errno, std::generic_category());
  return false;
#endif
}

std::filesystem::path reserve_folder(const std::filesystem::path& parent, const std::string& stem, std::string& error) {
  for (int attempt = 0; attempt < kReserveAttempts; ++attempt) {
    const auto suffix = attempt == 0 ? std::string{} : "-" + std::to_string(attempt);
    const auto candidate = parent / (stem + suffix);
    std::error_code problem;
    if (reserve_directory(candidate, problem)) return candidate;
    const bool occupied = !problem || problem == std::errc::file_exists;
    if (occupied) continue;
    error = "cannot reserve " + candidate.string() + ": " + problem.message();
    return {};
  }
  error = "cannot reserve " + (parent / stem).string() + ": " + kFolderOccupied;
  return {};
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
