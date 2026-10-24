#include "render/quality.hpp"

#include <tuple>
#include <utility>

namespace redgem {
namespace {

using QualityName = std::pair<QualityPreset, const char*>;
constexpr QualityName kQualityNames[] = {{QualityPreset::Low, "low"},
                                         {QualityPreset::Medium, "medium"},
                                         {QualityPreset::High, "high"},
                                         {QualityPreset::Ultra, "ultra"},
                                         {QualityPreset::Custom, "custom"}};

// What each preset below ultra allows: a card that draws high runs a level at its best but for
// the farthest shadows and grass; medium halves what costs most; low draws a plain picture.
constexpr int kHighSamples = 4;
constexpr int kHighCascades = 3;
constexpr float kHighReach = 0.75f;  // of the shadows' and the scattered copies' distance
constexpr float kHighDensity = 0.75f;
constexpr int kMediumSamples = 2;
constexpr int kMediumCascades = 2;
constexpr float kMediumReach = 0.5f;
constexpr float kMediumDensity = 0.5f;
constexpr float kMediumSky = 0.5f;
constexpr float kMediumOcclusion = 0.5f;
constexpr float kLowReach = 0.35f;
constexpr float kLowDensity = 0.25f;
constexpr float kLowSky = 0.25f;

QualityCaps high_caps() {
  QualityCaps caps;
  caps.msaa = kHighSamples;
  caps.shadow_cascades = kHighCascades;
  caps.shadow_distance = kHighReach;
  caps.scatter_density = kHighDensity;
  caps.scatter_distance = kHighReach;
  return caps;
}

QualityCaps medium_caps() {
  QualityCaps caps;
  caps.msaa = kMediumSamples;
  caps.shadow_cascades = kMediumCascades;
  caps.shadow_distance = kMediumReach;
  caps.scatter_density = kMediumDensity;
  caps.scatter_distance = kMediumReach;
  caps.sky_detail = kMediumSky;
  caps.ambient_occlusion = kMediumOcclusion;
  return caps;
}

QualityCaps low_caps() {
  QualityCaps caps;
  caps.msaa = 0;
  caps.temporal_antialias = false;
  caps.shadow_cascades = 1;
  caps.shadow_distance = kLowReach;
  caps.scatter_density = kLowDensity;
  caps.scatter_distance = kLowReach;
  caps.sky_detail = kLowSky;
  caps.ambient_occlusion = 0.0f;
  caps.bloom = 0.0f;
  return caps;
}

}  // namespace

bool operator==(const QualityCaps& a, const QualityCaps& b) {
  const auto fields = [](const QualityCaps& caps) {
    return std::make_tuple(caps.msaa, caps.temporal_antialias, caps.shadow_cascades, caps.shadow_distance,
                           caps.scatter_density, caps.scatter_distance, caps.sky_detail, caps.ambient_occlusion,
                           caps.bloom);
  };
  return fields(a) == fields(b);
}

const char* quality_name(QualityPreset quality) {
  for (const auto& [preset, name] : kQualityNames) {
    if (preset == quality) return name;
  }
  return kQualityNames[0].second;  // (every preset has its name)
}

std::optional<QualityPreset> quality_from_name(const std::string& name) {
  for (const auto& [preset, known] : kQualityNames) {
    if (name == known) return preset;
  }
  return std::nullopt;
}

QualityCaps preset_caps(QualityPreset quality) {
  switch (quality) {
    case QualityPreset::Low:
      return low_caps();
    case QualityPreset::Medium:
      return medium_caps();
    case QualityPreset::High:
      return high_caps();
    default:
      return {};
  }
}

}  // namespace redgem
