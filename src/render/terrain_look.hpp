#pragma once

// What the lit shader is told of a terrain's ground layers (see shaders/lit_terrain.hpp): the
// numbers, worked out once when the terrain is uploaded.

#include <array>

#include "scene/ground/terrain_layers.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::render_math {

using LayerNumbers = std::array<float, kMaxTerrainLayers>;
inline constexpr std::size_t kAreaNumbers = 4;  // smallest X and Z, 1 over the size along each

struct TerrainLook {
  int layers = 0;
  // The mesh's smallest X and Z, and 1 over its size along each: where its splat map lies.
  std::array<float, kAreaNumbers> area{};
  LayerNumbers tiling{};  // repeats per meter
  LayerNumbers sharpness{};
  LayerNumbers variation{};
  LayerNumbers stochastic{};  // 1 on, 0 off
  LayerNumbers triplanar{};
  float variation_scale = 0.0f;  // patches per meter
};

// The look of `layers` laid over a mesh of `bounds` (its own space). A mesh flat along X or Z
// gets its map over a meter there, not over nothing.
TerrainLook terrain_look(const TerrainLayers& layers, const Bounds& bounds);

}  // namespace redgem::render_math
