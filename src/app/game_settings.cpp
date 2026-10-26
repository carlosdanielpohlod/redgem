#include "app/game_settings.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "app/app_config.hpp"
#include "app/json_file.hpp"
#include "core/files.hpp"
#include "core/names.hpp"
#include "core/safe_path.hpp"

namespace redgem {
namespace {

// What game.json says.
constexpr const char* kIdKey = "id";
constexpr const char* kTitleKey = "title";
constexpr const char* kWindowWidthKey = "window_width";
constexpr const char* kWindowHeightKey = "window_height";
constexpr const char* kFullscreenKey = "fullscreen";
constexpr const char* kVsyncKey = "vsync";
constexpr const char* kFpsCapKey = "fps_cap";
constexpr const char* kRenderScaleKey = "render_scale";
constexpr const char* kQualityKey = "quality";
constexpr const char* kScriptBudgetKey = config::kScriptBudgetKey;
constexpr const char* kPhysicsKey = "physics";
constexpr int kJsonIndent = 2;
constexpr std::string_view kIdMarks = ".-_";  // what an id may hold besides small letters and digits
constexpr std::string_view kTwoSeparators = "..";

// A side of the game's window as game.json gives it, kept to what a window can be; `fallback`
// when the file gives none, or something that is not a whole number.
int window_size(const nlohmann::json& json, const char* key, int fallback) {
  if (!holds_whole_number(json, key)) return fallback;
  const auto asked = json[key].get<std::int64_t>();
  return static_cast<int>(std::clamp<std::int64_t>(asked, config::kMinWindowSide, config::kMaxWindowSide));
}

bool id_letter(unsigned char letter) {
  return small_letter_or_digit(letter) || kIdMarks.find(static_cast<char>(letter)) != std::string_view::npos;
}

bool holds_usable_id(const nlohmann::json& json) {
  return holds_text(json, kIdKey) && valid_game_id(json[kIdKey].get<std::string>());
}

// The game's id, when the file has one that can be used; what is wrong with it otherwise.
std::string read_id(const nlohmann::json& json, std::string& id) {
  if (!json.contains(kIdKey)) return {};
  if (holds_usable_id(json)) {
    id = json[kIdKey].get<std::string>();
    return {};
  }
  return std::string(kGameSettingsFile) +
         ": id must look like com.mystudio.mygame or my-game (small letters, digits, '.', '-' and '_')";
}

// The title and the window's size: each is used only when it is what it should be.
void read_window(const nlohmann::json& json, GameSettings& settings) {
  if (holds_text(json, kTitleKey)) settings.title = printable(json[kTitleKey].get<std::string>());
  settings.window_width = window_size(json, kWindowWidthKey, settings.window_width);
  settings.window_height = window_size(json, kWindowHeightKey, settings.window_height);
}

// Whether game.json asks for the game to start in fullscreen.
bool asks_fullscreen(const nlohmann::json& json) {
  return json.contains(kFullscreenKey) && json[kFullscreenKey].is_boolean() && json[kFullscreenKey].get<bool>();
}

// What the game shows itself as until its player says otherwise: game.json's keys are those of the
// player's settings (display_json), under other names for some.
void read_display(const nlohmann::json& json, GameSettings& settings) {
  DisplaySettings& display = settings.display;
  display.resolution = {settings.window_width, settings.window_height};
  if (asks_fullscreen(json)) display.mode = WindowMode::Fullscreen;
  nlohmann::json same_keys = nlohmann::json::object();
  for (const char* key : {kVsyncKey, kFpsCapKey, kRenderScaleKey, kQualityKey}) {
    if (json.contains(key)) same_keys[key] = json[key];
  }
  display = display_from_json(same_keys, display);
}

// Whether a text is as long as an id may be, with no two dots in a row.
bool sized_as_id(const std::string& id) {
  return !id.empty() && id.size() <= kMaxGameIdLength && id.find(kTwoSeparators) == std::string::npos;
}

bool within_budget_range(std::int64_t asked) { return asked > 0 && asked <= config::kMaxScriptBudgetMs; }

bool holds_usable_budget(const nlohmann::json& json) {
  return holds_whole_number(json, kScriptBudgetKey) && within_budget_range(json[kScriptBudgetKey].get<std::int64_t>());
}

// The scripts' budget, when the file gives one that can be used; what is wrong with it otherwise.
std::string read_script_budget(const nlohmann::json& json, GameSettings& settings) {
  if (!json.contains(kScriptBudgetKey)) return {};
  if (holds_usable_budget(json)) {
    settings.script_budget_ms = json[kScriptBudgetKey].get<int>();
    return {};
  }
  return std::string(kGameSettingsFile) + ": " + kScriptBudgetKey + " must be a whole number from 1 to " +
         std::to_string(config::kMaxScriptBudgetMs) + "; the scripts get " +
         std::to_string(config::kDefaultScriptBudgetMs) + " ms";
}

void read_physics(const nlohmann::json& json, GameManifest& manifest) {
  if (!json.contains(kPhysicsKey)) return;
  CollisionLayersReading reading = collision_layers_from_json(json[kPhysicsKey]);
  manifest.settings.collision_layers = std::move(reading.layers);
  manifest.physics_warnings = std::move(reading.warnings);
}

}  // namespace

GameManifest read_game_manifest(const std::filesystem::path& game_dir) {
  GameManifest manifest;
  manifest.settings.title = game_dir.filename().string();
  manifest.settings.window_width = config::kWindowWidth;
  manifest.settings.window_height = config::kWindowHeight;
  manifest.settings.display.resolution = {config::kWindowWidth, config::kWindowHeight};
  const nlohmann::json json = read_json_file(game_dir / kGameSettingsFile);
  if (!json.is_object()) return manifest;
  read_window(json, manifest.settings);
  read_display(json, manifest.settings);
  manifest.id_error = read_id(json, manifest.settings.id);
  manifest.android_error = read_android_section(json, manifest.settings.android);
  manifest.script_budget_error = read_script_budget(json, manifest.settings);
  read_physics(json, manifest);
  return manifest;
}

bool valid_game_id(const std::string& id) {
  if (!sized_as_id(id)) return false;
  const auto first = static_cast<unsigned char>(id.front());
  const auto last = static_cast<unsigned char>(id.back());
  return small_letter_or_digit(first) && small_letter_or_digit(last) && std::all_of(id.begin(), id.end(), id_letter);
}

GameSettings read_game_settings(const std::filesystem::path& game_dir) { return read_game_manifest(game_dir).settings; }

bool write_game_settings(const std::filesystem::path& game_dir, const GameSettings& settings, std::string& error) {
  // What else the file holds (its "android" section, an id these settings have none for...) stays
  // as it is.
  nlohmann::json json = read_json_file(game_dir / kGameSettingsFile);
  if (!json.is_object()) json = nlohmann::json::object();
  if (!settings.id.empty()) json[kIdKey] = settings.id;
  json[kTitleKey] = settings.title;
  json[kWindowWidthKey] = settings.window_width;
  json[kWindowHeightKey] = settings.window_height;
  return write_text_file(game_dir / kGameSettingsFile, json.dump(kJsonIndent) + "\n", error);
}

}  // namespace redgem
