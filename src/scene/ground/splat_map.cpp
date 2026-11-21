#include "scene/ground/splat_map.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace redgem {
namespace {

constexpr float kWhole = 1.0f;
constexpr auto kLayers = static_cast<std::size_t>(kMaxTerrainLayers);
constexpr auto kChannels = static_cast<std::size_t>(kLayersPerSplatPicture);

bool counts(float share) { return std::isfinite(share) && share > 0.0f; }

std::size_t texel_offset(const SplatMap& splat, int column, int row) {
  const int last = splat.size - 1;
  const auto at = static_cast<std::size_t>(std::clamp(row, 0, last)) * static_cast<std::size_t>(splat.size) +
                  static_cast<std::size_t>(std::clamp(column, 0, last));
  return at * kLayers;
}

bool inside(const SplatMap& splat, int column, int row) {
  return column >= 0 && row >= 0 && column < splat.size && row < splat.size;
}

std::size_t texel_count(const SplatMap& splat) {
  return static_cast<std::size_t>(splat.size) * static_cast<std::size_t>(splat.size);
}

LayerWeights mixed(const LayerWeights& from, const LayerWeights& to, float share) {
  LayerWeights result{};
  for (std::size_t layer = 0; layer < kLayers; ++layer) result[layer] = from[layer] + (to[layer] - from[layer]) * share;
  return result;
}

// The order in which the shares' remainders are rounded up: the largest first.
std::array<std::size_t, kLayers> by_remainder(const LayerWeights& scaled) {
  std::array<std::size_t, kLayers> order{};
  std::iota(order.begin(), order.end(), 0);
  const auto larger_remainder = [&scaled](std::size_t a, std::size_t b) {
    return scaled[a] - std::floor(scaled[a]) > scaled[b] - std::floor(scaled[b]);
  };
  std::stable_sort(order.begin(), order.end(), larger_remainder);
  return order;
}

}  // namespace

SplatMap make_splat(int size, int layer) {
  SplatMap splat;
  splat.size = std::clamp(size, kMinSplatSize, kMaxSplatSize);
  splat.weights.assign(texel_count(splat) * kLayers, 0);
  const auto covering = static_cast<std::size_t>(std::clamp(layer, 0, kMaxTerrainLayers - 1));
  for (std::size_t at = covering; at < splat.weights.size(); at += kLayers) splat.weights[at] = kFullSplatWeight;
  return splat;
}

bool well_formed(const SplatMap& splat) {
  return splat.size > 0 && splat.weights.size() == texel_count(splat) * kLayers;
}

LayerWeights normalized(const LayerWeights& weights) {
  LayerWeights result{};
  float total = 0.0f;
  for (std::size_t layer = 0; layer < kLayers; ++layer) {
    result[layer] = counts(weights[layer]) ? weights[layer] : 0.0f;
    total += result[layer];
  }
  if (!(total > 0.0f) || !std::isfinite(total)) return LayerWeights{kWhole};
  for (float& share : result) share /= total;
  return result;
}

std::array<std::uint8_t, kMaxTerrainLayers> quantized(const LayerWeights& weights) {
  LayerWeights scaled = normalized(weights);
  int left = kFullSplatWeight;
  std::array<std::uint8_t, kMaxTerrainLayers> bytes{};
  for (std::size_t layer = 0; layer < kLayers; ++layer) {
    scaled[layer] *= kFullSplatWeight;
    bytes[layer] = static_cast<std::uint8_t>(std::floor(scaled[layer]));
    left -= bytes[layer];
  }
  for (const std::size_t layer : by_remainder(scaled)) {
    if (left <= 0) break;
    ++bytes[layer];
    --left;
  }
  return bytes;
}

LayerWeights texel_weights(const SplatMap& splat, int column, int row) {
  if (!well_formed(splat)) return LayerWeights{kWhole};
  const std::uint8_t* texel = &splat.weights[texel_offset(splat, column, row)];
  LayerWeights weights{};
  for (std::size_t layer = 0; layer < kLayers; ++layer) weights[layer] = static_cast<float>(texel[layer]);
  return normalized(weights);
}

void set_texel(SplatMap& splat, int column, int row, const LayerWeights& weights) {
  if (!well_formed(splat) || !inside(splat, column, row)) return;
  const auto bytes = quantized(weights);
  std::copy(bytes.begin(), bytes.end(),
            splat.weights.begin() + static_cast<std::ptrdiff_t>(texel_offset(splat, column, row)));
}

LayerWeights splat_weights_at(const SplatMap& splat, float u, float v) {
  if (!well_formed(splat)) return LayerWeights{kWhole};
  // Texel centers sit half a texel in from the map's edges.
  const float x = std::clamp(u, 0.0f, kWhole) * static_cast<float>(splat.size) - kHalf;
  const float y = std::clamp(v, 0.0f, kWhole) * static_cast<float>(splat.size) - kHalf;
  const int column = static_cast<int>(std::floor(x));
  const int row = static_cast<int>(std::floor(y));
  const float across = x - static_cast<float>(column);
  const LayerWeights near_row = mixed(texel_weights(splat, column, row), texel_weights(splat, column + 1, row), across);
  const LayerWeights far_row =
      mixed(texel_weights(splat, column, row + 1), texel_weights(splat, column + 1, row + 1), across);
  return normalized(mixed(near_row, far_row, y - static_cast<float>(row)));
}

LayerWeights toward_layer(const LayerWeights& weights, int layer, float share) {
  const bool known_layer = layer >= 0 && layer < kMaxTerrainLayers;
  if (!known_layer || !std::isfinite(share)) return normalized(weights);
  LayerWeights target{};
  target[static_cast<std::size_t>(layer)] = kWhole;
  return normalized(mixed(normalized(weights), target, std::clamp(share, 0.0f, kWhole)));
}

int strongest_layer(const LayerWeights& weights) {
  return static_cast<int>(std::max_element(weights.begin(), weights.end()) - weights.begin());
}

std::vector<std::uint8_t> splat_picture(const SplatMap& splat, int picture) {
  if (!well_formed(splat)) return {};
  std::vector<std::uint8_t> rgba(texel_count(splat) * kChannels);
  const auto first = static_cast<std::size_t>(picture) * kChannels;
  for (std::size_t texel = 0; texel < texel_count(splat); ++texel) {
    const auto from = splat.weights.begin() + static_cast<std::ptrdiff_t>(texel * kLayers + first);
    std::copy(from, from + static_cast<std::ptrdiff_t>(kChannels),
              rgba.begin() + static_cast<std::ptrdiff_t>(texel * kChannels));
  }
  return rgba;
}

bool take_splat_picture(SplatMap& splat, int picture, const std::vector<std::uint8_t>& rgba) {
  const bool fits = well_formed(splat) && rgba.size() == texel_count(splat) * kChannels;
  if (!fits) return false;
  const auto first = static_cast<std::size_t>(picture) * kChannels;
  for (std::size_t texel = 0; texel < texel_count(splat); ++texel) {
    const auto from = rgba.begin() + static_cast<std::ptrdiff_t>(texel * kChannels);
    std::copy(from, from + static_cast<std::ptrdiff_t>(kChannels),
              splat.weights.begin() + static_cast<std::ptrdiff_t>(texel * kLayers + first));
  }
  return true;
}

void renormalize(SplatMap& splat) {
  if (!well_formed(splat)) return;
  for (int row = 0; row < splat.size; ++row) {
    for (int column = 0; column < splat.size; ++column)
      set_texel(splat, column, row, texel_weights(splat, column, row));
  }
}

}  // namespace redgem
