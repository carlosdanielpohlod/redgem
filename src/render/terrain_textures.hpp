#pragma once

// A terrain's ground layers on the graphics card (see scene/ground/terrain_layers.hpp): its
// layers' pictures as two texture arrays, a layer each, and its weights as a third of two layers.
// Raw OpenGL, as gl_extras: rlgl has no texture arrays.

#include <cstdint>

#include "scene/ground/terrain_layers.hpp"

namespace redgem::render_detail {

struct TerrainTextures {
  unsigned int colors = 0;    // RGBA8 array: color and height
  unsigned int surfaces = 0;  // RGBA8 array: normal X and Y, roughness, occlusion
  unsigned int splat = 0;     // RGBA8 array of kSplatPictures layers: the weights
  int splat_size = 0;
  std::uint64_t splat_version = 0;  // of the weights it holds

  bool loaded() const { return colors != 0 && surfaces != 0 && splat != 0; }
};

// The pictures and the weights of `layers` on the graphics card, smaller copies of the pictures
// included. Nothing (all 0) when it has no pictures or no weights.
TerrainTextures upload_terrain_textures(const TerrainLayers& layers);

// Sends the weights again when they changed since they were sent (a brush, a script).
void update_terrain_splat(TerrainTextures& textures, const SplatMap& splat);

void release(TerrainTextures& textures);

// Binds the three arrays to `first_slot` and the two slots after it.
void bind_terrain_textures(const TerrainTextures& textures, int first_slot);

}  // namespace redgem::render_detail
