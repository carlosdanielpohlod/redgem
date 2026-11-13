// game.json's "navigation" section: defaults, values taken, values refused.

#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "navigation/nav_settings.hpp"

using namespace redgem;

TEST_CASE("navigation settings default to a person-sized character on a quarter-meter grid") {
  const NavSettings settings;
  CHECK(settings.cell_size == nav::kDefaultCellSize);
  CHECK(settings.agent_radius == nav::kDefaultAgentRadius);
  CHECK(settings.agent_height == nav::kDefaultAgentHeight);
  CHECK(settings.max_step == nav::kDefaultMaxStep);
  CHECK(settings.max_slope_degrees == nav::kDefaultMaxSlopeDegrees);
  CHECK_FALSE(settings.bake_on_load);  // a game without the section bakes when first asked
}

TEST_CASE("the navigation section sets what it gives, and asks for a bake as each scene loads") {
  const NavSettingsReading reading = nav_settings_from_json(nlohmann::json::parse(R"({
    "cell_size": 0.5, "agent_radius": 0.3, "agent_height": 2.0, "max_step": 0.25, "max_slope": 30
  })"));
  CHECK(reading.warnings.empty());
  CHECK(reading.settings.cell_size == 0.5f);
  CHECK(reading.settings.agent_radius == 0.3f);
  CHECK(reading.settings.agent_height == 2.0f);
  CHECK(reading.settings.max_step == 0.25f);
  CHECK(reading.settings.max_slope_degrees == 30.0f);
  CHECK(reading.settings.bake_on_load);
  CHECK_FALSE(nav_settings_from_json(nlohmann::json::parse(R"({"bake_on_load": false})")).settings.bake_on_load);
}

TEST_CASE("a navigation setting out of range keeps its default, with a warning") {
  const NavSettingsReading reading = nav_settings_from_json(nlohmann::json::parse(R"({
    "cell_size": 0, "agent_height": "tall", "max_slope": 90, "agent_radius": 0.5, "bake_on_load": 1
  })"));
  CHECK(reading.warnings.size() == 4);
  CHECK(reading.settings.cell_size == nav::kDefaultCellSize);
  CHECK(reading.settings.agent_height == nav::kDefaultAgentHeight);
  CHECK(reading.settings.max_slope_degrees == nav::kDefaultMaxSlopeDegrees);
  CHECK(reading.settings.agent_radius == 0.5f);
  CHECK(reading.settings.bake_on_load);
  CHECK(nav_settings_from_json(nlohmann::json::array()).warnings.size() == 1);
}
