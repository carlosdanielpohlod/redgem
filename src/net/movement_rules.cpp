// game.json's "movement_validation" section: what the host holds the others' moves to.

#include "net/movement_rules.hpp"

#include <nlohmann/json.hpp>

namespace redgem::net {
namespace {

constexpr const char* kMaxSpeedKey = "max_speed";
constexpr const char* kTeleportDistanceKey = "teleport_distance";
constexpr const char* kToleranceKey = "tolerance";
constexpr const char* kKickAfterKey = "kick_after";
constexpr const char* kUndoKey = "undo";
constexpr const char* kWarningStart = "game.json: movement_validation.";

// The numbers a setting may be, both ends included.
struct Range {
  double low = 0.0;
  double high = 0.0;
};

bool within(const nlohmann::json& value, Range range) {
  if (!value.is_number()) return false;
  const auto number = value.get<double>();
  return number >= range.low && number <= range.high;
}

bool whole_within(const nlohmann::json& value, Range range) {
  return value.is_number_integer() && within(value, range);
}

std::string refusal(const char* key, Range range) {
  return std::string(kWarningStart) + key + " must be a number from " + std::to_string(range.low) + " to " +
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

void read_kick_after(const nlohmann::json& section, MovementRulesReading& reading) {
  if (!section.contains(kKickAfterKey)) return;
  const Range range{0.0, static_cast<double>(kMostKickAfter)};
  if (whole_within(section[kKickAfterKey], range)) {
    reading.rules.kick_after = section[kKickAfterKey].get<int>();
    return;
  }
  reading.warnings.push_back(refusal(kKickAfterKey, range));
}

void read_undo(const nlohmann::json& section, MovementRulesReading& reading) {
  if (!section.contains(kUndoKey)) return;
  if (section[kUndoKey].is_boolean()) {
    reading.rules.undo = section[kUndoKey].get<bool>();
    return;
  }
  reading.warnings.push_back(std::string(kWarningStart) + kUndoKey + " must be true or false");
}

void read_numbers(const nlohmann::json& section, MovementRulesReading& reading) {
  MovementRules& rules = reading.rules;
  std::vector<std::string>& warnings = reading.warnings;
  read_number(section, kMaxSpeedKey, {0.0, kMostMaxSpeed}, rules.max_speed, warnings);
  read_number(section, kTeleportDistanceKey, {0.0, kMostTeleportDistance}, rules.teleport_distance, warnings);
  read_number(section, kToleranceKey, {0.0, kMostMovementTolerance}, rules.tolerance, warnings);
}

}  // namespace

MovementRulesReading movement_rules_from_json(const nlohmann::json& section) {
  MovementRulesReading reading;
  if (!section.is_object()) {
    reading.warnings.emplace_back("game.json: movement_validation must be an object; nothing is checked");
    return reading;
  }
  read_numbers(section, reading);
  read_kick_after(section, reading);
  read_undo(section, reading);
  return reading;
}

}  // namespace redgem::net
