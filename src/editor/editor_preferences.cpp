#include "editor/editor_preferences.hpp"

#include <cmath>

#include <nlohmann/json.hpp>

#include "app/json_file.hpp"
#include "core/platform.hpp"
#include "storage/durable_file.hpp"

namespace redgem {
namespace {

constexpr const char* kPreferencesFile = "editor-preferences.json";
constexpr const char* kFly = "fly_speed";
constexpr const char* kTranslate = "translate_snap";
constexpr const char* kRotate = "rotate_snap";
constexpr const char* kScale = "scale_snap";
constexpr int kJsonIndent = 2;

bool in_range(double value, float min, float max) { return std::isfinite(value) && value >= min && value <= max; }

bool has_number(const nlohmann::json& json, const char* key) { return json.contains(key) && json.at(key).is_number(); }

float number(const nlohmann::json& json, const char* key, float fallback, float min, float max) {
  if (!has_number(json, key)) return fallback;
  const auto value = json.at(key).get<double>();
  return in_range(value, min, max) ? static_cast<float>(value) : fallback;
}

}  // namespace

std::filesystem::path editor_preferences_file() { return platform::user_data_dir() / kPreferencesFile; }

EditorPreferences read_editor_preferences(const std::filesystem::path& file) {
  const auto json = read_json_file(file);
  EditorPreferences values;
  values.fly_speed = number(json, kFly, values.fly_speed, kMinFlySpeed, kMaxFlySpeed);
  values.translate_snap = number(json, kTranslate, values.translate_snap, kMinSnapStep, kMaxSnapStep);
  values.rotate_snap = number(json, kRotate, values.rotate_snap, kMinSnapStep, kMaxRotateSnap);
  values.scale_snap = number(json, kScale, values.scale_snap, kMinSnapStep, kMaxSnapStep);
  return values;
}

static bool save_json(const std::filesystem::path& file, const nlohmann::json& json) {
  std::error_code problem;
  if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), problem);
  return !problem && durable::replace_file(file, json.dump(kJsonIndent) + "\n");
}

bool write_editor_preferences(const std::filesystem::path& file, const EditorPreferences& values, std::string& error) {
  const nlohmann::json json{{kFly, values.fly_speed},
                            {kTranslate, values.translate_snap},
                            {kRotate, values.rotate_snap},
                            {kScale, values.scale_snap}};
  if (save_json(file, json)) return true;
  error = "cannot write " + file.string();
  return false;
}

}  // namespace redgem
