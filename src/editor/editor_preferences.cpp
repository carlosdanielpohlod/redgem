#include "editor/editor_preferences.hpp"

#include <cmath>

#include <nlohmann/json.hpp>

#include "app/json_file.hpp"
#include "core/platform.hpp"
#include "core/recovery_lease.hpp"

namespace redgem {
namespace {

constexpr const char* kPreferencesFile = "editor-preferences.json";
constexpr const char* kFlyKeys = "fly_keys";
constexpr const char* kFly = "fly_speed";
constexpr const char* kTranslate = "translate_snap";
constexpr const char* kRotate = "rotate_snap";
constexpr const char* kScale = "scale_snap";

bool in_range(double value, float min, float max) { return std::isfinite(value) && value >= min && value <= max; }

float number(const nlohmann::json& json, const char* key, float fallback, float min, float max) {
  if (!holds_number(json, key)) return fallback;
  const auto value = json.at(key).get<double>();
  return in_range(value, min, max) ? static_cast<float>(value) : fallback;
}

FlyBindings read_fly_bindings(const nlohmann::json& json) {
  FlyBindings bindings;
  if (!json.is_object()) return bindings;
  for (const auto& field : kFlyBindingFields) {
    if (!holds_text(json, field.setting)) continue;
    if (const auto key = key_from_name(json.at(field.setting).get<std::string>())) bindings.*field.direction = *key;
  }
  return valid_fly_bindings(bindings) ? bindings : FlyBindings{};
}

nlohmann::json fly_bindings_json(const FlyBindings& bindings) {
  auto json = nlohmann::json::object();
  for (const auto& field : kFlyBindingFields) json[field.setting] = key_name(bindings.*field.direction);
  return json;
}

void merge_fly_bindings(nlohmann::json& json, const FlyBindings& previous, const FlyBindings& values) {
  if (same_fly_bindings(previous, values)) return;
  auto keys = json.contains(kFlyKeys) ? json.at(kFlyKeys) : nlohmann::json::object();
  if (!keys.is_object()) keys = nlohmann::json::object();
  keys.update(fly_bindings_json(values));
  json[kFlyKeys] = std::move(keys);
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
  if (json.contains(kFlyKeys)) values.fly_bindings = read_fly_bindings(json.at(kFlyKeys));
  return values;
}

namespace {

bool valid_preferences(const EditorPreferences& values) {
  return in_range(values.fly_speed, kMinFlySpeed, kMaxFlySpeed) &&
         in_range(values.translate_snap, kMinSnapStep, kMaxSnapStep) &&
         in_range(values.rotate_snap, kMinSnapStep, kMaxRotateSnap) &&
         in_range(values.scale_snap, kMinSnapStep, kMaxSnapStep);
}

void merge_changes(nlohmann::json& json, const EditorPreferences& previous, const EditorPreferences& values) {
  if (previous.fly_speed != values.fly_speed) json[kFly] = values.fly_speed;
  if (previous.translate_snap != values.translate_snap) json[kTranslate] = values.translate_snap;
  if (previous.rotate_snap != values.rotate_snap) json[kRotate] = values.rotate_snap;
  if (previous.scale_snap != values.scale_snap) json[kScale] = values.scale_snap;
}

}  // namespace

namespace {

bool lock_preferences(const std::filesystem::path& file, RecoveryLease& lease, std::string& error) {
  std::error_code problem;
  if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), problem);
  if (problem) {
    error = "cannot create " + file.parent_path().string() + ": " + problem.message();
    return false;
  }
  auto lock = file;
  lock += ".lock";
  if (lease.acquire(lock) == RecoveryLease::Result::Acquired) return true;
  error = "cannot lock " + file.string() + "; try saving again";
  return false;
}

}  // namespace

bool write_editor_preferences(const std::filesystem::path& file, const EditorPreferences& previous,
                              const EditorPreferences& values, std::string& error) {
  if (!valid_preferences(values)) {
    error = "Camera speed and snap steps must be finite and within their displayed ranges.";
    return false;
  }
  if (!valid_fly_bindings(values.fly_bindings)) {
    error = "Camera movement needs six distinct letter or arrow keys.";
    return false;
  }
  RecoveryLease lease;
  if (!lock_preferences(file, lease, error)) return false;
  auto json = read_json_for_update(file, error);
  if (!json) return false;
  merge_changes(*json, previous, values);
  merge_fly_bindings(*json, previous.fly_bindings, values.fly_bindings);
  return write_json_file(file, *json, error);
}

}  // namespace redgem
