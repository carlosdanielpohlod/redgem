#include "render/ui_scale.hpp"

#include <algorithm>
#include <utility>

namespace redgem {
namespace {

using MatchName = std::pair<UiMatch, const char*>;
constexpr MatchName kMatchNames[] = {{UiMatch::Width, "width"}, {UiMatch::Height, "height"}, {UiMatch::Fit, "fit"}};
constexpr float kUnscaled = 1.0f;  // a unit is a pixel

bool has_area(SurfaceSize size) { return size.width > 0 && size.height > 0; }

float side_scale(int screen, int reference) { return static_cast<float>(screen) / static_cast<float>(reference); }

// The reference's own scale to the screen, before the player's.
float reference_scale(SurfaceSize screen, const UiReference& reference) {
  if (!has_area(screen) || !has_area(reference.size)) return kUnscaled;
  const float across = side_scale(screen.width, reference.size.width);
  const float down = side_scale(screen.height, reference.size.height);
  if (reference.match == UiMatch::Width) return across;
  return reference.match == UiMatch::Height ? down : std::min(across, down);
}

}  // namespace

const char* ui_match_name(UiMatch match) {
  for (const auto& [known, name] : kMatchNames) {
    if (known == match) return name;
  }
  return kMatchNames[1].second;  // (every match has its name)
}

std::optional<UiMatch> ui_match_from_name(const std::string& name) {
  for (const auto& [match, known] : kMatchNames) {
    if (name == known) return match;
  }
  return std::nullopt;
}

float ui_scale(SurfaceSize screen, const UiReference& reference, float player_scale) {
  return reference_scale(screen, reference) * held_ui_scale(player_scale);
}

}  // namespace redgem
