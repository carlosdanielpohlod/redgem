#include "app/input_bindings_store.hpp"

#include <algorithm>
#include <system_error>

#include "app/game_settings.hpp"
#include "app/json_file.hpp"
#include "core/platform.hpp"
#include "input/input_config.hpp"
#include "storage/durable_file.hpp"

namespace redgem {
namespace {

constexpr const char* kInputKey = "input";  // game.json's section
constexpr const char* kPadsKey = "pads";    // the bindings file's list, one entry per player
constexpr const char* kBindingsFolder = "bindings";
constexpr const char* kBindingsExtension = ".json";
constexpr int kJsonIndent = 2;

using Json = nlohmann::json;

bool holds_list(const Json& json, const char* key) {
  return json.is_object() && json.contains(key) && json[key].is_array();
}

}  // namespace

InputMap read_game_input(const std::filesystem::path& game_dir, std::vector<std::string>& problems) {
  const Json game = read_json_file(game_dir / kGameSettingsFile);
  const bool declares_input = game.is_object() && game.contains(kInputKey);
  if (!declares_input) return {};
  return input_map_from_json(game[kInputKey], problems);
}

std::filesystem::path bindings_file_for(const std::filesystem::path& game_dir) {
  const std::string id = read_game_settings(game_dir).id;
  if (id.empty()) return {};
  return platform::user_data_dir() / kBindingsFolder / (id + kBindingsExtension);
}

void read_player_bindings(const std::filesystem::path& file, PlayerBindings& bindings) {
  const Json json = read_json_file(file);
  if (!holds_list(json, kPadsKey)) return;
  const Json& pads = json[kPadsKey];
  std::vector<std::string> ignored;                              // (what the engine did not write there is left out)
  const std::size_t kept = std::min(kMaxGamepads, pads.size());  // (more than there are gamepads: ignored)
  for (std::size_t pad = 0; pad < kept; ++pad) {
    bindings.set_changes(pad, input_map_from_json(pads[pad], ignored));
  }
}

bool write_player_bindings(const std::filesystem::path& file, const PlayerBindings& bindings) {
  Json pads = Json::array();
  for (std::size_t pad = 0; pad < kMaxGamepads; ++pad) pads.push_back(input_map_json(bindings.changes(pad)));
  std::error_code fs_error;
  std::filesystem::create_directories(file.parent_path(), fs_error);
  return durable::replace_file(file, Json{{kPadsKey, pads}}.dump(kJsonIndent) + "\n");
}

}  // namespace redgem
