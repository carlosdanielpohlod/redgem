#include "app/json_file.hpp"

#include "core/game_files.hpp"

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

}  // namespace redgem
