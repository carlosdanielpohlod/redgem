#pragma once

// What game.json says of movement validation (see movement_validator.hpp), on the machine hosting
// the game and on a dedicated server's rooms. Its "movement_validation" section (every key may be
// left out; a game without it checks nothing, and Network.validate_movement changes it at any time):
//   "movement_validation": {
//     "max_speed": 9.0,           meters a second an object of the others may move (0: not checked)
//     "teleport_distance": 4.0,   meters one pose may move it at most (0: not checked)
//     "tolerance": 0.25,          the share of the limits a move may go past and still be believed
//     "kick_after": 10,           violations after which the machine is let go (0: never)
//     "undo": false               a move too fast is undone whole, rather than cut down
//   }

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "net/movement_validator.hpp"

namespace redgem::net {

inline constexpr const char* kMovementSectionKey = "movement_validation";
// Its keys, which are also what Network.validate_movement and Network.movement_rules call them.
inline constexpr const char* kMaxSpeedKey = "max_speed";
inline constexpr const char* kTeleportDistanceKey = "teleport_distance";
inline constexpr const char* kToleranceKey = "tolerance";
inline constexpr const char* kKickAfterKey = "kick_after";
inline constexpr const char* kUndoKey = "undo";
// What a setting may be: anything else is left at its default, with a warning (also what
// Network.validate_movement takes).
inline constexpr float kMostMaxSpeed = 1000.0f;           // meters a second
inline constexpr float kMostTeleportDistance = 10000.0f;  // meters
inline constexpr float kMostMovementTolerance = 10.0f;    // ten times over
inline constexpr int kMostKickAfter = 1000;

struct MovementRulesReading {
  MovementRules rules;
  std::vector<std::string> warnings;  // each about a key left at its default
};

// game.json's "movement_validation" section (`section` is its value).
MovementRulesReading movement_rules_from_json(const nlohmann::json& section);

}  // namespace redgem::net
