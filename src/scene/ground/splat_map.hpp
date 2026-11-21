#pragma once

// The arithmetic of a terrain's layer weights (see SplatMap in terrain_layers.hpp): reading them
// at a spot, writing them so that they always add up to a whole, and moving them toward a layer.

#include <array>
#include <cstdint>
#include <vector>

#include "scene/ground/terrain_layers.hpp"

namespace redgem {

// How much of each layer covers a spot, as shares that add up to 1.
using LayerWeights = std::array<float, kMaxTerrainLayers>;

// A map of `size` texels a side (clamped to kMinSplatSize..kMaxSplatSize) all covered by `layer`.
SplatMap make_splat(int size, int layer = 0);

// Whether a map holds exactly the texels its size says.
bool well_formed(const SplatMap& splat);

// The same shares scaled to add up to 1. Shares that are not finite or below 0 count as 0; when
// nothing is left, the first layer covers the spot.
LayerWeights normalized(const LayerWeights& weights);

// Shares as the bytes a texel stores: they add up to kFullSplatWeight exactly, each rounded so
// that the total error is the smallest (the largest remainders are rounded up).
std::array<std::uint8_t, kMaxTerrainLayers> quantized(const LayerWeights& weights);

// One texel's shares (columns and rows outside the map count as the nearest one in it).
LayerWeights texel_weights(const SplatMap& splat, int column, int row);
// Writes one texel, normalized; nothing for a texel outside the map. Does not change the version.
void set_texel(SplatMap& splat, int column, int row, const LayerWeights& weights);

// The shares at a spot of the map, `u` across its columns and `v` down its rows (0 to 1, clamped),
// mixed from the four texels around it.
LayerWeights splat_weights_at(const SplatMap& splat, float u, float v);

// `weights` moved `share` (0 to 1) of the way toward all of `layer`: what a brush does. The result
// still adds up to 1.
LayerWeights toward_layer(const LayerWeights& weights, int layer, float share);

// The layer with the biggest share (the first of equal ones).
int strongest_layer(const LayerWeights& weights);

// The map's weights of layers 4 * `picture` to 4 * `picture` + 3, as an RGBA picture of `size` x
// `size` (its first row the map's first).
std::vector<std::uint8_t> splat_picture(const SplatMap& splat, int picture);
// Takes those layers' weights from such a picture. False, the map unchanged, when the picture is
// not of the map's size.
bool take_splat_picture(SplatMap& splat, int picture, const std::vector<std::uint8_t>& rgba);
// Every texel scaled to add up to kFullSplatWeight again (after its pictures were read in, or
// edited by hand); a texel of nothing goes to the first layer.
void renormalize(SplatMap& splat);

}  // namespace redgem
