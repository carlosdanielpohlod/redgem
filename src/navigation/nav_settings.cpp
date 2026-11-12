#include "navigation/nav_settings.hpp"

#include <nlohmann/json.hpp>

namespace redgem {
namespace {

constexpr const char* kCellSizeKey = "cell_size";
constexpr const char* kAgentRadiusKey = "agent_radius";
constexpr const char* kAgentHeightKey = "agent_height";
constexpr const char* kMaxStepKey = "max_step";
constexpr const char* kMaxSlopeKey = "max_slope";
constexpr const char* kBakeOnLoadKey = "bake_on_load";

// The numbers a setting may be, both ends included.
struct Range {
  float low = 0.0f;
  float high = 0.0f;
};

bool within(const nlohmann::json& value, Range range) {
  if (!value.is_number()) return false;
  const auto number = value.get<double>();
  return number >= range.low && number <= range.high;
}

std::string refusal(const char* key, Range range) {
  return std::string("game.json: navigation.") + key + " must be a number from " + std::to_string(range.low) + " to " +
         std::to_string(range.high) + "; the default is used";
}

// One number of the section: kept when it is in range, refused with a warning otherwise.
void read_number(const nlohmann::json& section, const char* key, Range range, float& value,
                 std::vector<std::string>& warnings) {
  if (!section.contains(key)) return;
  if (within(section[key], range)) {
    value = section[key].get<float>();
    return;
  }
  warnings.push_back(refusal(key, range));
}

void read_bake_on_load(const nlohmann::json& section, NavSettingsReading& reading) {
  reading.settings.bake_on_load = true;  // a game that has the section uses navigation
  if (!section.contains(kBakeOnLoadKey)) return;
  if (section[kBakeOnLoadKey].is_boolean()) {
    reading.settings.bake_on_load = section[kBakeOnLoadKey].get<bool>();
    return;
  }
  reading.warnings.emplace_back("game.json: navigation.bake_on_load must be true or false");
}

void read_numbers(const nlohmann::json& section, NavSettingsReading& reading) {
  NavSettings& settings = reading.settings;
  std::vector<std::string>& warnings = reading.warnings;
  read_number(section, kCellSizeKey, {nav::kMinCellSize, nav::kMaxCellSize}, settings.cell_size, warnings);
  read_number(section, kAgentRadiusKey, {0.0f, nav::kMaxAgentRadius}, settings.agent_radius, warnings);
  read_number(section, kAgentHeightKey, {nav::kMinAgentHeight, nav::kMaxAgentHeight}, settings.agent_height, warnings);
  read_number(section, kMaxStepKey, {0.0f, nav::kMaxStepHeight}, settings.max_step, warnings);
  read_number(section, kMaxSlopeKey, {0.0f, nav::kMaxSlopeLimitDegrees}, settings.max_slope_degrees, warnings);
}

}  // namespace

NavSettingsReading nav_settings_from_json(const nlohmann::json& section) {
  NavSettingsReading reading;
  if (!section.is_object()) {
    reading.warnings.emplace_back("game.json: navigation must be an object; the defaults are used");
    return reading;
  }
  read_numbers(section, reading);
  read_bake_on_load(section, reading);
  return reading;
}

}  // namespace redgem
