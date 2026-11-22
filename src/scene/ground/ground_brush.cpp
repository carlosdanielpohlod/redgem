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
constexpr float kFullByte = static_cast<float>(kFullSplatWeight);
// A hash of a texel and a stroke step (a 32-bit mix), as a share from 0 to 1.
constexpr std::uint32_t kPrimeColumn = 0x8da6b343U;
constexpr std::uint32_t kPrimeRow = 0xd8163841U;
constexpr std::uint32_t kPrimeStep = 0xcb1ab31fU;
constexpr std::uint32_t kMixShift = 15;
constexpr std::uint32_t kMixFactor = 0x2c1b3c6dU;
constexpr float kLargestHash = 4294967295.0f;

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

// A share from 0 to 1, the same for the same texel and step of the brush.
float dice(int column, int row, std::uint32_t step) {
  std::uint32_t hash = static_cast<std::uint32_t>(column) * kPrimeColumn ^ static_cast<std::uint32_t>(row) * kPrimeRow ^
                       step * kPrimeStep;
  hash ^= hash >> kMixShift;
  hash *= kMixFactor;
  hash ^= hash >> kMixShift;
  return static_cast<float>(hash) / kLargestHash;
}

// A share too small to move the layer's byte at all (a held brush, a frame at a time, or its soft
// rim) moves it one byte as often as that share would on average: whole bytes, as the weights
// keep them, without the stroke stalling short of the layer.
float stored_share(float share, std::uint8_t layer_byte, float roll) {
  const float room = kFullByte - static_cast<float>(layer_byte);
  const float bytes = share * room;
  if (bytes >= 1.0f || room <= 0.0f) return share;
  return roll < bytes ? 1.0f / room : 0.0f;
}

// Paints one texel; whether its bytes changed.
bool brush_texel(SplatMap& splat, const SplatArea& area, const LayerBrush& brush, int column, int row) {
  const GroundPlace place = texel_place(area, splat.size, column, row);
  const float wanted = brush_share(brush, std::hypot(place.x - brush.x, place.z - brush.z));
  if (wanted <= 0.0f) return false;
  const TexelBytes before = bytes_of(splat, column, row);
  const float share =
      stored_share(wanted, before[static_cast<std::size_t>(brush.layer)], dice(column, row, brush.step));
  if (share <= 0.0f) return false;
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
