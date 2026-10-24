#include "storage/save_file.hpp"

#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#include "core/log.hpp"
#include "storage/saved_values.hpp"

namespace redgem {

// What reading a save file found.
struct SaveReading {
  enum class Found { Nothing, Save, Damaged, Unreadable };
  Found found = Found::Nothing;
  std::string text;  // as it is on the disk (Save)
  nlohmann::json json;
};

namespace {

using Found = SaveReading::Found;

constexpr const char* kBackupSuffix = ".bak";       // of the save before the last one
constexpr const char* kDamagedSuffix = ".damaged";  // of a save found damaged, moved aside
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

bool is_file(const std::filesystem::path& file) {
  std::error_code fs_error;
  return std::filesystem::is_regular_file(file, fs_error);
}

// The whole of a file; nullopt when it cannot be opened or read (locked by another program, not
// allowed, a disk error, or no file at all but a folder).
std::optional<std::string> text_of(const std::filesystem::path& file) {
  if (!is_file(file)) return std::nullopt;
  std::ifstream in(file, std::ios::binary);
  if (!in) return std::nullopt;
  try {
    std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    if (in.bad()) return std::nullopt;
    return text;
  } catch (const std::ios_base::failure&) {
    return std::nullopt;  // (a read the disk failed)
  }
}

// Only what is read and is no save is damaged: a file that cannot be read may be a good save.
SaveReading read_save(const std::filesystem::path& file) {
  if (!is_there(file)) return {};
  const auto text = text_of(file);
  if (!text) return {Found::Unreadable, {}, {}};
  nlohmann::json json = nlohmann::json::parse(*text, nullptr, false);
  if (!json.is_object()) return {Found::Damaged, {}, {}};
  return {Found::Save, *text, std::move(json)};
}

// A name beside `file` for it once damaged that no file has yet; nullopt when all are taken.
std::optional<std::filesystem::path> damaged_name(const std::filesystem::path& file) {
  const std::string first = file.string() + kDamagedSuffix;
  std::filesystem::path name = first;
  for (int number = kFirstNumberedDamaged; number <= kMostDamagedSavesKept + 1; ++number) {
    if (!is_there(name)) return name;
    name = first + "." + std::to_string(number);
  }
  return std::nullopt;
}

// Moves a damaged save out of the way of the next one; nullopt when it could not be moved.
std::optional<std::filesystem::path> move_aside(const std::filesystem::path& file) {
  auto aside = damaged_name(file);
  if (!aside) return std::nullopt;
  std::error_code fs_error;
  std::filesystem::rename(file, *aside, fs_error);
  if (fs_error) return std::nullopt;
  return aside;
}

std::filesystem::path backup_of(const std::filesystem::path& file) { return file.string() + kBackupSuffix; }

}  // namespace

void remove_save(const std::filesystem::path& file) {
  std::error_code fs_error;
  std::filesystem::remove(file, fs_error);
  std::filesystem::remove(backup_of(file), fs_error);
}

std::filesystem::path SaveFile::backup_file() const { return backup_of(file_); }

void SaveFile::load() {
  if (loaded_ || in_memory()) return;
  loaded_ = true;
  const SaveReading main = read_save(file_);
  const std::string named = "the save file " + file_.string();
  if (main.found == Found::Save) return take(main);
  if (main.found == Found::Unreadable) return refuse_writes(named + " cannot be read");
  if (main.found == Found::Nothing) return recover(named + " is missing", false);
  const auto aside = set_aside(file_);
  if (aside) recover(named + " is damaged (moved to " + aside->string() + ")", true);
}

void SaveFile::take(const SaveReading& reading) {
  for (const auto& [key, value] : reading.json.items()) {
    if (const auto keeping = saved::entry(value)) values_[key] = *keeping;
  }
  on_disk_ = reading.text;
}

// The backup takes the place of a save that is damaged or missing (`trouble` says which; a missing
// save with no backup either is a game's first run).
void SaveFile::recover(const std::string& trouble, bool damaged) {
  const SaveReading backup = read_save(backup_file());
  const std::string named = "its backup " + backup_file().string();
  if (backup.found == Found::Save) return restore_from(backup, trouble, damaged);
  if (backup.found == Found::Unreadable) return refuse_writes(trouble + " and " + named + " cannot be read");
  if (backup.found == Found::Nothing) {
    if (damaged)
      log::error(trouble + " and has no backup (" + backup_file().string() + "): the game starts from nothing");
    return;
  }
  const auto aside = set_aside(backup_file());
  if (aside)
    log::error(trouble + " and " + named + " is damaged too (moved to " + aside->string() +
               "): the game starts from nothing");
}

// The backup is loaded and written back as the save: the next run finds it in its place.
void SaveFile::restore_from(const SaveReading& backup, const std::string& trouble, bool damaged) {
  take(backup);
  on_disk_.clear();  // (the save's file holds nothing yet)
  const std::string said = trouble + ": its backup " + backup_file().string() + " was loaded instead";
  log::write(damaged ? log::Level::Error : log::Level::Warning, said);
  if (durable::replace_file(file_, backup.text, flush_)) {
    on_disk_ = backup.text;
    return;
  }
  log::warning("the backup could not be written back as " + file_.string());
}

// Moves a damaged save aside; when it cannot be moved, nothing is written over it this run.
std::optional<std::filesystem::path> SaveFile::set_aside(const std::filesystem::path& damaged) {
  auto aside = move_aside(damaged);
  if (!aside) refuse_writes("the damaged save " + damaged.string() + " could not be moved aside");
  return aside;
}

void SaveFile::refuse_writes(const std::string& why) {
  writes_refused_ = true;
  log::error(why + ": the game is not saved this run, so that no save is lost");
}

// The save about to be replaced becomes the backup, written as a save is.
void SaveFile::back_up_previous() const {
  if (on_disk_.empty()) return;
  if (!durable::replace_file(backup_file(), on_disk_, flush_)) {
    log::warning("the save file " + file_.string() + " could not be backed up to " + backup_file().string());
  }
}

bool SaveFile::write() {
  if (in_memory()) return true;
  if (writes_refused_) return false;  // (why was said when the save was read)
  std::error_code fs_error;
  std::filesystem::create_directories(file_.parent_path(), fs_error);
  const std::string text = saved::written(as_json(values_), kIndent) + "\n";
  back_up_previous();
  if (!durable::replace_file(file_, text, flush_)) return false;
  on_disk_ = text;
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
