#include "app/json_file.hpp"

#include <fstream>

#include "core/log.hpp"
#include "storage/durable_file.hpp"
#include "storage/save_file.hpp"

namespace redgem {

nlohmann::json read_json_file(const std::filesystem::path& file) {
  std::ifstream in(file);
  if (!in) return nlohmann::json(nlohmann::json::value_t::discarded);
  return nlohmann::json::parse(in, nullptr, false);
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

// What the file holds, to be written again with something changed (nothing yet: an empty object).
}  // namespace

std::optional<nlohmann::json> read_json_for_update(const std::filesystem::path& file, std::string& error) {
  nlohmann::json json = read_json_file(file);
  if (json.is_object()) return json;
  std::error_code problem;
  const bool exists = std::filesystem::exists(file, problem);
  if (problem) {
    error = "cannot inspect " + file.string() + ": " + problem.message();
    return std::nullopt;
  }
  if (!exists) return nlohmann::json::object();
  return set_damaged_aside(file, error);
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
