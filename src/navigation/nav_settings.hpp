#pragma once

// What game.json says of navigation: the size of the characters that walk the level, and how
// fine the grid they walk on is. Its "navigation" section (every key may be left out):
//   "navigation": {
//     "cell_size": 0.25,       meters a side of a grid cell
//     "agent_radius": 0.4,     how close to a wall a character's center may come
//     "agent_height": 1.8,     the headroom a character needs
//     "max_step": 0.4,         the highest step it climbs (and drops down) without jumping
//     "max_slope": 45,         the steepest ground it walks on, in degrees
//     "bake_on_load": true     bake as each scene loads (true when the section is there at all)
//   }
// A game without the section bakes its grid when a script first asks Navigation something.

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace redgem {

namespace nav {
inline constexpr const char* kSectionKey = "navigation";
inline constexpr float kDefaultCellSize = 0.25f;
inline constexpr float kDefaultAgentRadius = 0.4f;
inline constexpr float kDefaultAgentHeight = 1.8f;
inline constexpr float kDefaultMaxStep = 0.4f;
inline constexpr float kDefaultMaxSlopeDegrees = 45.0f;
// What a setting may be: anything else is left at its default, with a warning.
inline constexpr float kMinCellSize = 0.05f;
inline constexpr float kMaxCellSize = 4.0f;
inline constexpr float kMaxAgentRadius = 10.0f;
inline constexpr float kMinAgentHeight = 0.1f;
inline constexpr float kMaxAgentHeight = 20.0f;
inline constexpr float kMaxStepHeight = 5.0f;
inline constexpr float kMaxSlopeLimitDegrees = 89.0f;
}  // namespace nav

struct NavSettings {
  float cell_size = nav::kDefaultCellSize;
  float agent_radius = nav::kDefaultAgentRadius;
  float agent_height = nav::kDefaultAgentHeight;
  float max_step = nav::kDefaultMaxStep;
  float max_slope_degrees = nav::kDefaultMaxSlopeDegrees;
  bool bake_on_load = false;
};

struct NavSettingsReading {
  NavSettings settings;
  std::vector<std::string> warnings;  // each about a key left at its default
};

// game.json's "navigation" section (`section` is its value).
NavSettingsReading nav_settings_from_json(const nlohmann::json& section);

}  // namespace redgem
