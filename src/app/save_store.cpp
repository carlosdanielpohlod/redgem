#include "app/save_store.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

#include "app/game_project.hpp"
#include "core/log.hpp"

namespace redgem {
namespace {

constexpr const char* kSavesFolder = "saves";
constexpr const char* kSaveExtension = ".json";
constexpr const char* kTestSavesFolder = "redgem-test-saves";
constexpr int kIndent = 2;

}  // namespace

std::filesystem::path save_file_for(const std::filesystem::path& game_dir) {
  // A built game's files sit in a folder called "game": its name is the folder around it.
  std::filesystem::path named = normalized_folder(std::filesystem::absolute(game_dir));
  if (named.filename() == kBundledGameFolder) named = named.parent_path();
  return recent_games_file().parent_path() / kSavesFolder / (named.filename().string() + kSaveExtension);
}

std::filesystem::path test_save_file_for(const std::filesystem::path& game_dir) {
  return std::filesystem::temp_directory_path() / kTestSavesFolder / save_file_for(game_dir).filename();
}

void SaveStore::load() {
  if (loaded_) return;
  loaded_ = true;
  std::ifstream in(file_);
  if (!in) return;
  const nlohmann::json json = nlohmann::json::parse(in, nullptr, false);
  if (!json.is_object()) {
    log::warning("the save file " + file_.string() + " is damaged: starting from nothing");
    return;
  }
  for (const auto& [key, value] : json.items()) {
    if (value.is_boolean()) {
      values_[key] = value.get<bool>();
    } else if (value.is_number()) {
      values_[key] = value.get<double>();
    } else if (value.is_string()) {
      values_[key] = value.get<std::string>();
    }
  }
}

bool SaveStore::write() const {
  nlohmann::json json = nlohmann::json::object();
  for (const auto& [key, value] : values_) {
    if (const auto* flag = std::get_if<bool>(&value)) {
      json[key] = *flag;
    } else if (const auto* number = std::get_if<double>(&value)) {
      json[key] = *number;
    } else {
      json[key] = std::get<std::string>(value);
    }
  }
  std::error_code fs_error;
  std::filesystem::create_directories(file_.parent_path(), fs_error);
  // Written beside the file and moved over it: a crash mid-write never leaves half a save.
  const std::filesystem::path partial = file_.string() + ".part";
  {
    std::ofstream out(partial);
    if (!out) return false;
    out << json.dump(kIndent) << "\n";
    out.close();  // a full disk shows here: the file being replaced must not be lost to half a one
    if (!out) return false;
  }
  std::filesystem::rename(partial, file_, fs_error);
  return !fs_error;
}

std::optional<PropertyValue> SaveStore::get(const std::string& key) {
  load();
  const auto found = values_.find(key);
  if (found == values_.end()) return std::nullopt;
  return found->second;
}

bool SaveStore::set(const std::string& key, const PropertyValue& value) {
  load();
  values_[key] = value;
  return write();
}

bool SaveStore::remove(const std::string& key) {
  load();
  if (values_.erase(key) == 0) return true;
  return write();
}

bool SaveStore::clear() {
  load();
  values_.clear();
  return write();
}

std::vector<std::string> SaveStore::keys() {
  load();
  std::vector<std::string> names;
  for (const auto& [key, value] : values_) names.push_back(key);
  return names;
}

}  // namespace redgem
