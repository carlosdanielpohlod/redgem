#include "storage/save_file.hpp"

#include <fstream>
#include <optional>
#include <string>

#include "core/log.hpp"
#include "storage/saved_values.hpp"

namespace redgem {
namespace {

constexpr const char* kPartialSuffix = ".part";     // of a save still being written
constexpr const char* kBackupSuffix = ".bak";       // of the save before the last one
constexpr const char* kDamagedSuffix = ".damaged";  // of a save found damaged, moved aside
// How many damaged saves are kept apart (<name>.damaged, then .damaged.2 and on): past that, the
// last name is written over.
constexpr int kMostDamagedKept = 100;
constexpr int kFirstNumberedDamaged = 2;
constexpr int kIndent = 2;

nlohmann::json as_json(const std::map<std::string, nlohmann::json>& values) {
  nlohmann::json json = nlohmann::json::object();
  for (const auto& [key, value] : values) json[key] = value;
  return json;
}

bool is_there(const std::filesystem::path& file) {
  std::error_code fs_error;
  return std::filesystem::exists(file, fs_error);
}

// What a save file holds; nullopt when it cannot be read or is not a save (damaged).
std::optional<nlohmann::json> read_save(const std::filesystem::path& file) {
  std::ifstream in(file);
  if (!in) return std::nullopt;
  nlohmann::json json = nlohmann::json::parse(in, nullptr, false);
  if (!json.is_object()) return std::nullopt;
  return json;
}

// A name beside `file` for it once damaged that no file has yet: a damaged save moved aside before
// is never written over.
std::filesystem::path damaged_name(const std::filesystem::path& file) {
  const std::string first = file.string() + kDamagedSuffix;
  std::filesystem::path name = first;
  for (int number = kFirstNumberedDamaged; number <= kMostDamagedKept; ++number) {
    if (!is_there(name)) return name;
    name = first + "." + std::to_string(number);
  }
  return name;
}

// Moves a damaged save out of the way of the next one; nullopt when it could not be moved.
std::optional<std::filesystem::path> move_aside(const std::filesystem::path& file) {
  std::filesystem::path aside = damaged_name(file);
  std::error_code fs_error;
  std::filesystem::rename(file, aside, fs_error);
  if (fs_error) return std::nullopt;
  return aside;
}

// Where a damaged save went, for what is said about it.
std::string said_where(const std::optional<std::filesystem::path>& aside) {
  return aside ? "moved to " + aside->string() : "could not be moved aside";
}

// A save written beside the file and flushed to the disk; nothing is left of it when it cannot be.
bool write_partial(const std::filesystem::path& partial, const std::string& text, const durable::Flush& flush) {
  if (durable::write_flushed(partial, text, flush)) return true;
  std::error_code fs_error;
  std::filesystem::remove(partial, fs_error);
  return false;
}

}  // namespace

std::filesystem::path SaveFile::backup_file() const { return file_.string() + kBackupSuffix; }

void SaveFile::load() {
  if (loaded_ || in_memory()) return;
  loaded_ = true;
  if (!is_there(file_)) return;
  if (const auto contents = read_save(file_)) {
    keep(*contents);
    return;
  }
  recover_from_damage();
}

void SaveFile::keep(const nlohmann::json& contents) {
  for (const auto& [key, value] : contents.items()) {
    if (const auto keeping = saved::entry(value)) values_[key] = *keeping;
  }
}

// The damaged save is moved aside, kept for whoever wants to look into it; the backup takes its
// place, or nothing does when the backup is damaged too.
void SaveFile::recover_from_damage() {
  const auto aside = move_aside(file_);
  file_is_good_ = aside.has_value();
  const std::string damaged = "the save file " + file_.string() + " is damaged (" + said_where(aside) + ")";
  if (const auto backup = read_save(backup_file())) {
    keep(*backup);
    log::error(damaged + ": its backup " + backup_file().string() + " was loaded instead");
    return;
  }
  log::error(damaged + " and its backup " + backup_file().string() +
             " cannot be read either: the game starts from nothing");
}

// The save about to be replaced becomes the backup. (Copied, not moved: there is a save in its
// place at every moment.)
bool SaveFile::holds_good_save() const { return file_is_good_ && is_there(file_); }

void SaveFile::back_up_previous() const {
  if (!holds_good_save()) return;  // a damaged save never takes the good backup's place
  std::error_code fs_error;
  std::filesystem::copy_file(file_, backup_file(), std::filesystem::copy_options::overwrite_existing, fs_error);
  if (fs_error) log::warning("the save file " + file_.string() + " could not be backed up: " + fs_error.message());
}

bool SaveFile::write() {
  if (in_memory()) return true;
  std::error_code fs_error;
  std::filesystem::create_directories(file_.parent_path(), fs_error);
  // Written beside the file, flushed to the disk and moved over it: a crash mid-write never leaves
  // half a save.
  const std::filesystem::path partial = file_.string() + kPartialSuffix;
  const std::string text = saved::written(as_json(values_), kIndent) + "\n";
  if (!write_partial(partial, text, flush_)) return false;
  back_up_previous();
  std::filesystem::rename(partial, file_, fs_error);
  if (fs_error) return false;
  file_is_good_ = true;
  return true;
}

std::optional<nlohmann::json> SaveFile::get(const std::string& key) {
  load();
  const auto found = values_.find(key);
  if (found == values_.end()) return std::nullopt;
  return found->second;
}

bool SaveFile::set(const std::string& key, const nlohmann::json& value) {
  if (value.is_null()) return remove(key);
  load();
  values_[key] = saved::kept(value).value_or(nlohmann::json());
  return write();
}

bool SaveFile::remove(const std::string& key) {
  load();
  if (values_.erase(key) == 0) return true;
  return write();
}

bool SaveFile::clear() {
  load();
  values_.clear();
  return write();
}

std::vector<std::string> SaveFile::keys() {
  load();
  std::vector<std::string> names;
  names.reserve(values_.size());
  for (const auto& [key, value] : values_) names.push_back(key);
  return names;
}

}  // namespace redgem
