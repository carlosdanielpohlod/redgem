#include "scene/ground/ground_brush.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "scene/ground/ground_survey.hpp"
#include "scene/ground/splat_map.hpp"

namespace redgem {
namespace {

constexpr float kHalfTexel = 0.5f;
constexpr float kSmoothA = 3.0f;  // of the smoothstep 3t² - 2t³
constexpr float kSmoothB = 2.0f;

using TexelBytes = std::array<std::uint8_t, kMaxTerrainLayers>;

// The texels a brush reaches: columns and rows, both ends in.
struct TexelBox {
  int first_column = 0;
  int last_column = -1;
  int first_row = 0;
  int last_row = -1;
};

// The column (or row) whose middle is at `share` (0 to 1) across a map of `size` texels.
float texel_at(float share, int size) { return share * static_cast<float>(size) - kHalfTexel; }

int clamped(float texel, int size) { return static_cast<int>(std::clamp(texel, 0.0f, static_cast<float>(size - 1))); }

TexelBox box_of(const SplatArea& area, int size, const LayerBrush& brush) {
  const SplatSpot low = splat_spot(area, brush.x - brush.radius, brush.z - brush.radius);
  const SplatSpot high = splat_spot(area, brush.x + brush.radius, brush.z + brush.radius);
  return {clamped(std::floor(texel_at(low.u, size)), size), clamped(std::ceil(texel_at(high.u, size)), size),
          clamped(std::floor(texel_at(low.v, size)), size), clamped(std::ceil(texel_at(high.v, size)), size)};
}

TexelBytes bytes_of(const SplatMap& splat, int column, int row) {
  const std::size_t texel =
      static_cast<std::size_t>(row) * static_cast<std::size_t>(splat.size) + static_cast<std::size_t>(column);
  TexelBytes bytes{};
  const std::size_t from = texel * static_cast<std::size_t>(kMaxTerrainLayers);
  std::copy_n(splat.weights.begin() + static_cast<std::ptrdiff_t>(from), bytes.size(), bytes.begin());
  return bytes;
}

// Paints one texel; whether its bytes changed.
bool brush_texel(SplatMap& splat, const SplatArea& area, const LayerBrush& brush, int column, int row) {
  const GroundPlace place = texel_place(area, splat.size, column, row);
  const float share = brush_share(brush, std::hypot(place.x - brush.x, place.z - brush.z));
  if (share <= 0.0f) return false;
  const TexelBytes before = bytes_of(splat, column, row);
  set_texel(splat, column, row, toward_layer(texel_weights(splat, column, row), brush.layer, share));
  return bytes_of(splat, column, row) != before;
}

std::size_t brush_box(SplatMap& splat, const SplatArea& area, const LayerBrush& brush, const TexelBox& box) {
  std::size_t changed = 0;
  for (int row = box.first_row; row <= box.last_row; ++row) {
    for (int column = box.first_column; column <= box.last_column; ++column)
      changed += brush_texel(splat, area, brush, column, row) ? 1 : 0;
  }
  return changed;
}

// Whether the brush can paint these layers: a layer they have, over weights of the right size.
bool paintable(const TerrainLayers& layers, const LayerBrush& brush) {
  const bool known_layer = brush.layer >= 0 && brush.layer < static_cast<int>(layers.layers.size());
  return known_layer && well_formed(layers.splat) && !layers.splat.empty() && brush.radius > 0.0f;
}

}  // namespace

float brush_share(const LayerBrush& brush, float distance) {
  if (distance >= brush.radius) return 0.0f;
  const float strength = std::clamp(brush.strength, 0.0f, 1.0f);
  const float inner = brush.radius * (1.0f - std::clamp(brush.falloff, 0.0f, 1.0f));
  if (distance <= inner) return strength;
  const float t = (distance - inner) / (brush.radius - inner);
  return strength * (1.0f - t * t * (kSmoothA - kSmoothB * t));
}

float strength_for(float per_second, float seconds) {
  const float kept = 1.0f - std::clamp(per_second, 0.0f, 1.0f);
  return 1.0f - std::pow(kept, std::max(seconds, 0.0f));
}

std::size_t brush_layer(TerrainLayers& layers, const Bounds& bounds, const LayerBrush& brush) {
  if (!paintable(layers, brush)) return 0;
  const SplatArea area = splat_area(bounds);
  const std::size_t changed = brush_box(layers.splat, area, brush, box_of(area, layers.splat.size, brush));
  if (changed > 0) ++layers.splat.version;
  return changed;
}

}  // namespace redgem
