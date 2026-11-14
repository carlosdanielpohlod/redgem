#pragma once

// How big the interface a game's scripts draw (UI) is on a screen: its sizes are given for a
// reference screen the game names, and on any other screen each of those units is as many pixels
// as the reference's side that is matched takes there, times the player's own scale (Settings.ui_scale).
//   game.json: "ui": {"reference": [1920, 1080], "match": "height"}

#include <optional>
#include <string>

#include "render/display_settings.hpp"

namespace redgem {

// Which of the reference's sides follows the screen's: its width, its height, or whichever keeps
// all of the reference on the screen (the smaller of the two scales).
enum class UiMatch { Width, Height, Fit };

struct UiReference {
  SurfaceSize size;  // the screen the game's interface is laid out for; none: a unit is a pixel
  UiMatch match = UiMatch::Height;
};

const char* ui_match_name(UiMatch match);  // "width", "height", "fit"
std::optional<UiMatch> ui_match_from_name(const std::string& name);

// Pixels a unit of the interface takes on a screen of `screen`'s size: the reference's scale to
// it (1 when either has no area), times the player's scale (held to its range).
float ui_scale(SurfaceSize screen, const UiReference& reference, float player_scale);

}  // namespace redgem
