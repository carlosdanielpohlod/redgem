#include "app/json_file.hpp"

#include "core/game_files.hpp"

#include "core/log.hpp"
#include "storage/durable_file.hpp"
#include "storage/save_file.hpp"

namespace redgem {

namespace {

// The most a JSON file of a game or of its players is read into memory.
constexpr std::uintmax_t kMaxJsonFileBytes = 64ull * 1024ull * 1024ull;

}  // namespace

nlohmann::json read_json_file(const std::filesystem::path& file) {
  const auto bytes = game_files::read(file, kMaxJsonFileBytes);
  if (!bytes) return nlohmann::json(nlohmann::json::value_t::discarded);
  return nlohmann::json::parse(bytes->begin(), bytes->end(), nullptr, false);
}

bool holds_text(const nlohmann::json& json, const char* key) { return json.contains(key) && json[key].is_string(); }

bool holds_whole_number(const nlohmann::json& json, const char* key) {
  return json.contains(key) && json[key].is_number_integer();
}

bool holds_number(const nlohmann::json& json, const char* key) {
  return json.contains(key) && json.at(key).is_number();
}

namespace {

constexpr int kJsonIndent = 2;

// A settings file that is no settings, moved aside to <file>.damaged and said: nothing is written
// over it. Empty settings to write anew; nullopt when it could not be moved (it is left alone).
std::optional<nlohmann::json> set_damaged_aside(const std::filesystem::path& file, std::string& error) {
  const auto aside = move_damaged_aside(file);
  if (!aside) {
    error = file.string() + " is damaged and could not be moved aside: it is left as it is";
    return std::nullopt;
  }
  log::error(file.string() + " was damaged: it was moved to " + aside->string() + " and written anew");
  return nlohmann::json::object();
}

bool is_missing(const std::filesystem::file_status& status) {
  return status.type() == std::filesystem::file_type::not_found;
}

std::optional<nlohmann::json> read_settings_object(const std::filesystem::path& file, std::string& error) {
  std::ifstream in(file);
  if (!in) {
    error = "cannot read " + file.string();
    return std::nullopt;
  }
  auto json = nlohmann::json::parse(in, nullptr, false);
  if (in.bad()) {
    error = "cannot finish reading " + file.string();
    return std::nullopt;
  }
  if (json.is_object()) return json;
  in.close();
  return set_damaged_aside(file, error);
}

}  // namespace

std::optional<nlohmann::json> read_json_for_update(const std::filesystem::path& file, std::string& error) {
  std::error_code problem;
  const auto status = std::filesystem::symlink_status(file, problem);
  if (is_missing(status)) return nlohmann::json::object();
  if (!std::filesystem::is_regular_file(status)) {
    error = "cannot update " + file.string() + ": expected a regular settings file";
    return std::nullopt;
  }
  return read_settings_object(file, error);
}

// Written whole: beside it first, then moved over it (see durable::replace_file).
bool write_json_file(const std::filesystem::path& file, const nlohmann::json& json, std::string& error) {
  std::error_code fs_error;
  if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), fs_error);
  const bool folder_ready = !fs_error;
  if (folder_ready && durable::replace_file(file, json.dump(kJsonIndent) + "\n")) return true;
  error = "cannot write " + file.string();
  return false;
}

}  // namespace redgem
