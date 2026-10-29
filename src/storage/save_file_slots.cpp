#include "storage/save_file.hpp"
#include <stdexcept>
#include <set>
#include "storage/save_slots.hpp"
#include "storage/save_view.hpp"
namespace redgem {
namespace {

bool exists_as_link(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::is_symlink(std::filesystem::symlink_status(path, error));
}
void refuse_link(const std::filesystem::path& path) {
  if (exists_as_link(path)) throw std::runtime_error("save slot paths must not be symbolic links");
}
std::optional<std::string> slot_file_name(const std::filesystem::directory_entry& entry) {
  auto path = entry.path();
  if (path.extension() == ".bak") path = path.parent_path() / path.stem();
  if (path.extension() != kSaveSlotExtension || entry.is_symlink()) return std::nullopt;
  const auto stem = path.stem().u8string();
  if (stem.compare(0, kSaveSlotPrefix.size(), kSaveSlotPrefix) != 0) return std::nullopt;
  auto name = stem.substr(kSaveSlotPrefix.size());
  if (!entry.is_regular_file() || !valid_save_slot(name) || name == kDefaultSaveSlot) return std::nullopt;
  return name;
}
}  // namespace
std::unique_ptr<SaveFile> SaveFile::make_slot_file(const std::string& name) const {
  require_named_save_slot(name);
  if (in_memory()) return std::make_unique<SaveFile>();
  const auto folder = save_slots_folder(file_);
  const auto path = save_slot_file(file_, name);
  refuse_link(folder);
  refuse_link(path);
  refuse_link(save_backup_of(path));
  return std::make_unique<SaveFile>(path, flush_);
}
SaveStore& SaveFile::slot_store(const std::string& name) {
  require_named_save_slot(name);
  const auto found = named_slots_.find(name);
  if (found != named_slots_.end()) return *found->second;
  if (named_slots_.size() == kMaxOpenSaveSlots) throw std::runtime_error("too many named file saves opened");
  return *named_slots_.emplace(name, make_slot_file(name)).first->second;
}
std::unique_ptr<SaveStore> SaveFile::open_slot(const std::string& name) {
  return std::make_unique<SaveView>(slot_store(name));
}
namespace {
std::set<std::string> disk_names(const std::filesystem::path& folder) {
  refuse_link(folder);
  if (!std::filesystem::exists(folder)) return {};
  std::set<std::string> names;
  for (const auto& entry : std::filesystem::directory_iterator(folder)) {
    if (const auto name = slot_file_name(entry)) names.insert(*name);
    if (names.size() > kMaxListedSaveSlots) throw std::runtime_error("too many save slots to list");
  }
  return names;
}
}  // namespace
std::vector<std::string> SaveFile::slot_names() {
  std::set<std::string> names;
  for (const auto& entry : named_slots_) names.insert(entry.first);
  if (!in_memory()) {
    const auto stored = disk_names(save_slots_folder(file_));
    names.insert(stored.begin(), stored.end());
  }
  return {names.begin(), names.end()};
}
namespace {
void remove_slot_entry(const std::filesystem::directory_entry& entry) {
  std::error_code error;
  try {
    if (slot_file_name(entry)) std::filesystem::remove(entry.path(), error);
  } catch (const std::filesystem::filesystem_error&) {
    // Resetting a test's save is best effort, as removing its default file already is.
    return;
  }
}
void remove_slot_entries(const std::filesystem::path& folder) {
  std::error_code error;
  auto entries = std::filesystem::directory_iterator(folder, error);
  const auto end = std::filesystem::directory_iterator();
  for (; entries != end && !error; entries.increment(error)) remove_slot_entry(*entries);
}
}  // namespace
void remove_named_save_files(const std::filesystem::path& file) {
  if (file.empty()) return;
  const auto folder = save_slots_folder(file);
  if (!exists_as_link(folder)) return remove_slot_entries(folder);
  std::error_code error;
  std::filesystem::remove(folder, error);
}
}  // namespace redgem
