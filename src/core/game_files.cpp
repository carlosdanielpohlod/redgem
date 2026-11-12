#include "core/game_files.hpp"

#include <fstream>
#include <memory>
#include <mutex>
#include <utility>

#include "core/log.hpp"

namespace redgem::game_files {
namespace {

constexpr const char* kParentStep = "..";
constexpr const char* kSameFolder = ".";

struct Mount {
  std::filesystem::path root;
  GameArchive archive;
};

// The mounted archive, shared with every thread that reads (scenes load in the background).
struct Mounted {
  std::mutex guard;
  std::shared_ptr<const Mount> mount;
};

Mounted& mounted() {
  static Mounted state;
  return state;
}

std::shared_ptr<const Mount> current_mount() {
  Mounted& state = mounted();
  const std::lock_guard<std::mutex> lock(state.guard);
  return state.mount;
}

// A path as the system sees it, ".." and links followed as far as there is something to follow.
std::optional<std::filesystem::path> resolved(const std::filesystem::path& path) {
  std::error_code fs_error;
  std::filesystem::path full = std::filesystem::weakly_canonical(path, fs_error);
  if (fs_error) return std::nullopt;
  return full;
}

bool stays_inside(const std::filesystem::path& inside) { return !inside.empty() && *inside.begin() != kParentStep; }

// The name of `path` inside the mounted folder ("" for the folder itself); nullopt outside it.
std::optional<std::string> name_in(const Mount& mount, const std::filesystem::path& path) {
  const auto full = resolved(path);
  if (!full) return std::nullopt;
  const std::filesystem::path inside = full->lexically_relative(mount.root);
  if (!stays_inside(inside)) return std::nullopt;
  return inside == kSameFolder ? std::string() : inside.generic_u8string();
}

// Where a path is: in the mounted archive (its name there), or not (no name).
struct Place {
  std::shared_ptr<const Mount> mount;
  std::optional<std::string> name;
};

Place place_of(const std::filesystem::path& path) {
  Place place{current_mount(), std::nullopt};
  if (place.mount) place.name = name_in(*place.mount, path);
  return place;
}

const ArchiveEntry* entry_at(const Place& place) {
  return place.name ? place.mount->archive.find(*place.name) : nullptr;
}

std::optional<std::vector<std::uint8_t>> read_from_disk(const std::filesystem::path& file, std::uintmax_t max_bytes) {
  std::error_code fs_error;
  if (!std::filesystem::is_regular_file(file, fs_error)) return std::nullopt;
  const std::uintmax_t size = std::filesystem::file_size(file, fs_error);
  if (fs_error || size > max_bytes) return std::nullopt;
  std::ifstream in(file, std::ios::binary);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  // (Exactly the size it had when asked: a file that grows meanwhile is not followed.)
  if (!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return std::nullopt;
  return bytes;
}

std::optional<std::vector<std::uint8_t>> read_from_archive(const Place& place, std::uintmax_t max_bytes) {
  const ArchiveEntry* entry = entry_at(place);
  if (entry == nullptr) return std::nullopt;
  std::string error;
  auto bytes = place.mount->archive.read(*entry, max_bytes, error);
  if (!bytes) log::warning(error);
  return bytes;
}

std::vector<std::filesystem::path> disk_entries(const std::filesystem::path& folder) {
  std::vector<std::filesystem::path> entries;
  std::error_code fs_error;
  for (const auto& entry : std::filesystem::directory_iterator(folder, fs_error)) entries.push_back(entry.path());
  return entries;
}

}  // namespace

void mount(const std::filesystem::path& root, GameArchive archive) {
  auto mount = std::make_shared<const Mount>(Mount{resolved(root).value_or(root), std::move(archive)});
  Mounted& state = mounted();
  const std::lock_guard<std::mutex> lock(state.guard);
  state.mount = std::move(mount);
}

void unmount() {
  Mounted& state = mounted();
  const std::lock_guard<std::mutex> lock(state.guard);
  state.mount.reset();
}

std::optional<std::filesystem::path> mounted_root() {
  const auto mount = current_mount();
  if (!mount) return std::nullopt;
  return mount->root;
}

bool in_archive(const std::filesystem::path& path) { return place_of(path).name.has_value(); }

bool is_file(const std::filesystem::path& path) {
  const Place place = place_of(path);
  if (place.name) return entry_at(place) != nullptr;
  std::error_code fs_error;
  return std::filesystem::is_regular_file(path, fs_error);
}

bool is_folder(const std::filesystem::path& path) {
  const Place place = place_of(path);
  if (place.name) return place.name->empty() || place.mount->archive.has_folder(*place.name);
  std::error_code fs_error;
  return std::filesystem::is_directory(path, fs_error);
}

std::optional<std::uintmax_t> file_size(const std::filesystem::path& path) {
  const Place place = place_of(path);
  if (place.name) {
    const ArchiveEntry* entry = entry_at(place);
    return entry ? std::optional<std::uintmax_t>(entry->size) : std::nullopt;
  }
  std::error_code fs_error;
  const std::uintmax_t size = std::filesystem::file_size(path, fs_error);
  return fs_error ? std::nullopt : std::optional<std::uintmax_t>(size);
}

std::optional<std::vector<std::uint8_t>> read(const std::filesystem::path& path, std::uintmax_t max_bytes) {
  const Place place = place_of(path);
  return place.name ? read_from_archive(place, max_bytes) : read_from_disk(path, max_bytes);
}

std::vector<std::filesystem::path> entries_in(const std::filesystem::path& folder) {
  const Place place = place_of(folder);
  if (!place.name) return disk_entries(folder);
  std::vector<std::filesystem::path> entries;
  for (const std::string& name : place.mount->archive.files_in(*place.name)) {
    entries.push_back(folder / std::filesystem::u8path(name));
  }
  return entries;
}

}  // namespace redgem::game_files
