#include "render/terrain_look.hpp"

#include <algorithm>

namespace redgem::render_math {
namespace {

constexpr float kOn = 1.0f;
constexpr float kOff = 0.0f;
constexpr float kSmallestSpan = 1.0f;  // meters: what a flat side is spread over
constexpr std::size_t kMinX = 0;
constexpr std::size_t kMinZ = 1;
constexpr std::size_t kAcross = 2;
constexpr std::size_t kAlong = 3;

float span(float low, float high) { return std::max(high - low, kSmallestSpan); }

void look_of_layer(const TerrainLayer& layer, std::size_t index, TerrainLook& look) {
  look.tiling[index] = 1.0f / layer.tiling;
  look.sharpness[index] = layer.sharpness;
  look.variation[index] = layer.variation;
  look.stochastic[index] = layer.stochastic ? kOn : kOff;
  look.triplanar[index] = layer.triplanar ? kOn : kOff;
}

}  // namespace

TerrainLook terrain_look(const TerrainLayers& layers, const Bounds& bounds) {
  TerrainLook look;
  look.layers = static_cast<int>(std::min(layers.layers.size(), static_cast<std::size_t>(kMaxTerrainLayers)));
  look.area[kMinX] = bounds.min.x;
  look.area[kMinZ] = bounds.min.z;
  look.area[kAcross] = 1.0f / span(bounds.min.x, bounds.max.x);
  look.area[kAlong] = 1.0f / span(bounds.min.z, bounds.max.z);
  for (std::size_t i = 0; i < static_cast<std::size_t>(look.layers); ++i) look_of_layer(layers.layers[i], i, look);
  look.variation_scale = 1.0f / layers.variation_size;
  return look;
}

}  // namespace redgem::render_math
