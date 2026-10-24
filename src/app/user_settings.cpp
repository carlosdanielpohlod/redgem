#include "app/user_settings.hpp"

#include "core/platform.hpp"

#include <nlohmann/json.hpp>

#include "app/json_file.hpp"
#include "core/log.hpp"
#include "storage/durable_file.hpp"
#include "storage/save_file.hpp"

namespace redgem {
namespace {

constexpr const char* kSettingsFileName = "settings.json";
constexpr const char* kGpuKey = "gpu";
constexpr const char* kLanguageKey = "language";
constexpr const char* kGamesKey = "games";          // each game's display settings, by its key
constexpr const char* kGpuVariable = "REDGEM_GPU";  // the user's choice of card for what is started from a shell
constexpr int kJsonIndent = 2;

}  // namespace

std::filesystem::path user_settings_file() { return platform::user_data_dir() / kSettingsFileName; }

UserSettings read_user_settings(const std::filesystem::path& file) {
  UserSettings settings;
  const nlohmann::json json = read_json_file(file);
  if (holds_text(json, kGpuKey)) {
    settings.gpu = gpu_choice_from_name(json[kGpuKey].get<std::string>()).value_or(settings.gpu);
  }
  if (holds_text(json, kLanguageKey)) settings.language = json[kLanguageKey].get<std::string>();
  return settings;
}

namespace {

bool is_there(const std::filesystem::path& file) {
  std::error_code fs_error;
  return std::filesystem::exists(file, fs_error);
}

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
std::optional<nlohmann::json> kept_settings(const std::filesystem::path& file, std::string& error) {
  nlohmann::json json = read_json_file(file);
  if (json.is_object()) return json;
  if (!is_there(file)) return nlohmann::json::object();
  return set_damaged_aside(file, error);
}

// Written whole: beside it first, then moved over it (see durable::replace_file).
bool write_settings_json(const std::filesystem::path& file, const nlohmann::json& json, std::string& error) {
  std::error_code fs_error;
  std::filesystem::create_directories(file.parent_path(), fs_error);
  if (durable::replace_file(file, json.dump(kJsonIndent) + "\n")) return true;
  error = "cannot write " + file.string();
  return false;
}

bool holds_games(const nlohmann::json& json) { return json.contains(kGamesKey) && json[kGamesKey].is_object(); }

bool holds_game(const nlohmann::json& json, const std::string& game_key) {
  return holds_games(json) && json[kGamesKey].contains(game_key);
}

}  // namespace

bool write_user_settings(const std::filesystem::path& file, const UserSettings& settings, std::string& error) {
  std::optional<nlohmann::json> kept = kept_settings(file, error);
  if (!kept) return false;
  nlohmann::json& json = *kept;
  json[kGpuKey] = gpu_choice_name(settings.gpu);
  json.erase(kLanguageKey);
  if (!settings.language.empty()) json[kLanguageKey] = settings.language;
  return write_settings_json(file, json, error);
}

DisplaySettings read_game_display(const std::filesystem::path& file, const std::string& game_key,
                                  const DisplaySettings& defaults) {
  const nlohmann::json json = read_json_file(file);
  if (!holds_game(json, game_key)) return defaults;
  return display_from_json(json[kGamesKey][game_key], defaults);
}

bool write_game_display(const std::filesystem::path& file, const std::string& game_key, const DisplaySettings& display,
                        std::string& error) {
  std::optional<nlohmann::json> kept = kept_settings(file, error);
  if (!kept) return false;
  nlohmann::json& json = *kept;
  if (!holds_games(json)) json[kGamesKey] = nlohmann::json::object();
  json[kGamesKey][game_key] = display_json(display);
  return write_settings_json(file, json, error);
}

GpuChoice resolve_gpu_choice(const std::optional<std::string>& command_line,
                             const std::optional<std::string>& environment, const UserSettings& settings) {
  for (const std::optional<std::string>* source : {&command_line, &environment}) {
    if (!source->has_value()) continue;
    if (const auto choice = gpu_choice_from_name(**source)) return *choice;
  }
  return settings.gpu;
}

GpuChoice gpu_choice_for_run(const std::optional<std::string>& command_line) {
  const std::string variable = platform::environment(kGpuVariable);
  const std::optional<std::string> environment = variable.empty() ? std::nullopt : std::optional(variable);
  return resolve_gpu_choice(command_line, environment, read_user_settings(user_settings_file()));
}

std::string language_for_run() {
  const std::string chosen = read_user_settings(user_settings_file()).language;
  return chosen.empty() ? platform::system_language() : chosen;
}

}  // namespace redgem
