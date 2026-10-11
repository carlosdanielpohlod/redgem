#include "app/save_store.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>

#include "app/game_project.hpp"
#include "core/log.hpp"

namespace redgem {
namespace {

constexpr const char* kSavesFolder = "saves";
constexpr const char* kSaveExtension = ".json";
constexpr const char* kTestSavesFolder = "test-saves";  // beside the players' saves, in the user's own folder
constexpr char kNameJoiner = '-';
// A few digits made from where the game is, so that two games in folders of the same name do not
// share a save.
constexpr std::uint32_t kHashSeed = 2166136261u;  // FNV-1a
constexpr std::uint32_t kHashPrime = 16777619u;
constexpr const char* kHashFormat = "%08x";  // eight hex digits

// The folder a game is known by: a built game's files sit in a folder called "game", and it is
// the folder around that one.
std::filesystem::path named_folder(const std::filesystem::path& game_dir) {
  std::error_code fs_error;
  std::filesystem::path named = normalized_folder(std::filesystem::weakly_canonical(game_dir, fs_error));
  if (fs_error) named = normalized_folder(std::filesystem::absolute(game_dir, fs_error));
  return named.filename() == kBundledGameFolder ? named.parent_path() : named;
}

std::string place_digits(const std::filesystem::path& folder) {
  std::uint32_t hash = kHashSeed;
  for (const char c : folder.generic_string()) hash = (hash ^ static_cast<unsigned char>(c)) * kHashPrime;
  char digits[sizeof("ffffffff")];
  std::snprintf(digits, sizeof(digits), kHashFormat, hash);
  return digits;
}

std::filesystem::path saves_folder() { return recent_games_file().parent_path() / kSavesFolder; }
constexpr const char* kPartialSuffix = ".part";  // of a save still being written
constexpr int kIndent = 2;

// What a save file holds under one key, when it is something a game can save.
std::optional<PropertyValue> saved_value(const nlohmann::json& value) {
  if (value.is_boolean()) return PropertyValue(value.get<bool>());
  if (value.is_number()) return PropertyValue(value.get<double>());
  if (value.is_string()) return PropertyValue(value.get<std::string>());
  return std::nullopt;
}

nlohmann::json as_json(const std::map<std::string, PropertyValue>& values) {
  nlohmann::json json = nlohmann::json::object();
  for (const auto& [key, value] : values) {
    std::visit([&json, &key = key](const auto& held) { json[key] = held; }, value);
  }
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

std::filesystem::path save_file_for(const std::filesystem::path& game_dir) {
  const std::filesystem::path named = named_folder(game_dir);
  return saves_folder() / (named.filename().string() + kNameJoiner + place_digits(named) + kSaveExtension);
}

std::filesystem::path test_save_file_for(const std::filesystem::path& game_dir) {
  // (In the user's own folder, not in one every user of the machine can write to and guess.)
  return recent_games_file().parent_path() / kTestSavesFolder / save_file_for(game_dir).filename();
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
    if (const auto kept = saved_value(value)) values_[key] = *kept;
  }
}

bool SaveStore::write() const {
  std::error_code fs_error;
  std::filesystem::create_directories(file_.parent_path(), fs_error);
  // Written beside the file and moved over it: a crash mid-write never leaves half a save.
  const std::filesystem::path partial = file_.string() + kPartialSuffix;
  // (Text that is not valid UTF-8 is saved with the broken parts replaced, instead of not at all.)
  const std::string text = as_json(values_).dump(kIndent, ' ', false, nlohmann::json::error_handler_t::replace) + "\n";
  if (!write_whole(partial, text)) return false;
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
  names.reserve(values_.size());
  for (const auto& [key, value] : values_) names.push_back(key);
  return names;
}

}  // namespace redgem
