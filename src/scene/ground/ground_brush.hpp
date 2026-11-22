#pragma once

// A soft round brush that paints one ground layer into a terrain's weights (see SplatMap): what the
// editor's Paint brush does on a terrain with layers.
//
// Experimental: the brush and its settings may still change.

#include <cstddef>

#include "scene/scene_desc.hpp"

namespace redgem {

inline constexpr float kDefaultBrushFalloff = 0.5f;

struct LayerBrush {
  float x = 0.0f;  // the middle, in the mesh's own space
  float z = 0.0f;
  float radius = 0.0f;  // meters, in the mesh's own space
  // 0 to 1: how far toward the layer the middle goes, this once.
  float strength = 0.0f;
  // 0 to 1: the share of the radius, at the rim, over which the brush fades out (0: a hard edge,
  // 1: it fades from the middle on).
  float falloff = kDefaultBrushFalloff;
  int layer = 0;
};

// How far toward the layer the brush moves a spot `distance` meters from its middle: its strength
// inside, nothing past its radius, a smooth fade between.
float brush_share(const LayerBrush& brush, float distance);

// The brush's strength for a stroke held `seconds`, when `per_second` (0 to 1) is how far toward
// the layer one second goes: the same in a second at any frame rate.
float strength_for(float per_second, float seconds);

// Paints the brush into the weights of `layers`, laid over a mesh of `bounds`. Returns how many
// texels changed; the weights' version goes up when any did.
std::size_t brush_layer(TerrainLayers& layers, const Bounds& bounds, const LayerBrush& brush);

}  // namespace redgem
