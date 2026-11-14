// game.json's "movement_validation" section.

#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "net/movement_rules.hpp"

using redgem::net::kDefaultMovementTolerance;
using redgem::net::movement_rules_from_json;
using redgem::net::MovementRulesReading;

namespace {

constexpr float kSpeed = 9.0f;
constexpr float kTeleport = 4.0f;
constexpr float kTolerance = 0.5f;
constexpr int kKickAfter = 10;
constexpr float kFasterThanAnything = redgem::net::kMostMaxSpeed * 2.0f;

}  // namespace

TEST_CASE("a game without the section checks nothing") {
  CHECK_FALSE(redgem::net::MovementRules{}.enabled());
  const MovementRulesReading reading = movement_rules_from_json(nlohmann::json::object());
  CHECK_FALSE(reading.rules.enabled());
  CHECK(reading.rules.tolerance == doctest::Approx(kDefaultMovementTolerance));
  CHECK(reading.warnings.empty());
}

TEST_CASE("every setting of the section is read") {
  const MovementRulesReading reading = movement_rules_from_json({{"max_speed", kSpeed},
                                                                 {"teleport_distance", kTeleport},
                                                                 {"tolerance", kTolerance},
                                                                 {"kick_after", kKickAfter},
                                                                 {"undo", true}});
  CHECK(reading.warnings.empty());
  CHECK(reading.rules.enabled());
  CHECK(reading.rules.max_speed == doctest::Approx(kSpeed));
  CHECK(reading.rules.teleport_distance == doctest::Approx(kTeleport));
  CHECK(reading.rules.tolerance == doctest::Approx(kTolerance));
  CHECK(reading.rules.kick_after == kKickAfter);
  CHECK(reading.rules.undo);
}

TEST_CASE("a setting that cannot be used keeps its default, with a warning each") {
  const MovementRulesReading reading = movement_rules_from_json(
      {{"max_speed", kFasterThanAnything}, {"tolerance", "lots"}, {"kick_after", kTolerance}, {"undo", 1}});
  CHECK(reading.warnings.size() == 4);
  CHECK_FALSE(reading.rules.enabled());
  CHECK(reading.rules.tolerance == doctest::Approx(kDefaultMovementTolerance));
  CHECK(reading.rules.kick_after == 0);
  CHECK_FALSE(reading.rules.undo);
  CHECK(movement_rules_from_json(true).warnings.size() == 1);
}
