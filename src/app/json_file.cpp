#include "app/json_file.hpp"

#include <fstream>

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

}  // namespace redgem
