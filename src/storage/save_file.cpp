#include "storage/save_file.hpp"

#include <fstream>

#include "core/log.hpp"
#include "storage/saved_values.hpp"

namespace redgem {
namespace {

constexpr const char* kPartialSuffix = ".part";  // of a save still being written
constexpr int kIndent = 2;

nlohmann::json as_json(const std::map<std::string, nlohmann::json>& values) {
  nlohmann::json json = nlohmann::json::object();
  for (const auto& [key, value] : values) json[key] = value;
  return json;
}

// False when not all of the text reached the disk.
bool write_whole(const std::filesystem::path& file, const std::string& text) {
  std::ofstream out(file);
  if (!out) return false;
  out << text;
  out.close();  // a full disk shows here: the file being replaced must not be lost to half a one
  return static_cast<bool>(out);
}

}  // namespace

void SaveFile::load() {
  if (loaded_ || in_memory()) return;
  loaded_ = true;
  std::ifstream in(file_);
  if (!in) return;
  const nlohmann::json json = nlohmann::json::parse(in, nullptr, false);
  if (!json.is_object()) {
    log::warning("the save file " + file_.string() + " is damaged: starting from nothing");
    return;
  }
  for (const auto& [key, value] : json.items()) {
    if (const auto keeping = saved::entry(value)) values_[key] = *keeping;
  }
}

bool SaveFile::write() const {
  if (in_memory()) return true;
  std::error_code fs_error;
  std::filesystem::create_directories(file_.parent_path(), fs_error);
  // Written beside the file and moved over it: a crash mid-write never leaves half a save.
  const std::filesystem::path partial = file_.string() + kPartialSuffix;
  const std::string text = saved::written(as_json(values_), kIndent) + "\n";
  if (!write_whole(partial, text)) return false;
  std::filesystem::rename(partial, file_, fs_error);
  return !fs_error;
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
