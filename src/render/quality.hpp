#pragma once

// How much the renderer does for a level, as the player of a game chooses it: a preset (low to
// ultra) or what the player set one by one (custom).

#include <optional>
#include <string>

#include "render/render_config.hpp"

namespace redgem {

enum class QualityPreset { Low, Medium, High, Ultra, Custom };

// The most a quality preset lets a level's environment ask for. A level gets what it asked for
// or less, never more: shares multiply what it asked for, the others hold it under them.
struct QualityCaps {
  int msaa = config::kMostSamples;              // samples per pixel
  bool temporal_antialias = true;               // edges smoothed with the frames before
  int shadow_cascades = config::kMostCascades;  // pictures of the sun's shadow map
  float shadow_distance = 1.0f;                 // share of the meters the level's shadows reach
  float scatter_density = 1.0f;                 // share of a scattered set's copies that are drawn
  float scatter_distance = 1.0f;                // share of how far away scattered copies are drawn
  float sky_detail = 1.0f;                      // share of the sky picture's width, and of its light's
  float ambient_occlusion = 1.0f;               // share of how dark creases get
  float bloom = 1.0f;                           // share of the glow around bright things
};

bool operator==(const QualityCaps& a, const QualityCaps& b);
inline bool operator!=(const QualityCaps& a, const QualityCaps& b) { return !(a == b); }

const char* quality_name(QualityPreset quality);  // "low", "medium", "high", "ultra", "custom"
std::optional<QualityPreset> quality_from_name(const std::string& name);

// What a preset allows. Ultra (and Custom, until the player says otherwise) allows everything a
// level asks for.
QualityCaps preset_caps(QualityPreset quality);

}  // namespace redgem
