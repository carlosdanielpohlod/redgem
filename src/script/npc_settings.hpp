#pragma once

// What the editor shows of an object's NPC parts in the Scene view: the settings of its
// Interactable, Perception, Schedule and NavAgent. In the edited scene they come from the
// object's properties (the settings each part reads when nothing else gives them); while the game
// runs, from the parts the scripts gave the object (ScriptHost::npc_settings).

#include <optional>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "scene/property.hpp"

namespace redgem {

// The defaults of ruby/engine/interactable.rb and perception.rb (a test checks they agree).
inline constexpr float kInteractableRange = 2.0f;   // meters
inline constexpr float kPerceptionSight = 15.0f;    // meters
inline constexpr float kPerceptionFov = 120.0f;     // degrees, the whole width of the cone
inline constexpr float kPerceptionEyes = 1.6f;      // meters above the origin
inline constexpr float kPerceptionHearing = 10.0f;  // meters, for a noise of loudness 1

struct InteractableSettings {
  float range = kInteractableRange;
  std::optional<std::string> prompt;  // an I18n key; none: the engine's own
};

struct PerceptionSettings {
  float sight = kPerceptionSight;
  float fov = kPerceptionFov;
  float eyes = kPerceptionEyes;
  float hearing = kPerceptionHearing;
};

// Where a NavAgent was sent, and the corners of the path it still has to walk.
struct NavRoute {
  Vec3 destination;
  std::vector<Vec3> waypoints;
};

struct NpcSettings {
  std::optional<InteractableSettings> interactable;
  std::optional<PerceptionSettings> perception;
  std::vector<std::string> schedule;  // its plan, one "HH:MM activity" each
  std::optional<NavRoute> route;
};

// What an object's properties say: an Interactable when it has a `prompt` or a `range`, a
// Perception when it has a `sight`, `fov`, `eyes` or `hearing`, a Schedule when it has a `plan`.
// A setting that is not a number is left at its default (the game reports it as it starts).
NpcSettings npc_settings_from_props(const PropertyMap& props);

// A Schedule's plan as text ("06:00 bake, 12:30 sell") in its entries, trimmed.
std::vector<std::string> schedule_entries(const std::string& plan);

}  // namespace redgem
