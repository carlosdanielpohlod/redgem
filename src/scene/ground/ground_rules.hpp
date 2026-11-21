#pragma once

// Ground layers painted by rules (see GroundRule): sand by the sea, grass above it, rock on the
// steep slopes, patches of dirt where a noise says. A terrain's rules are listed in its layers
// file (terrains/<name>.json):
//
//   "rules": [
//     { "layer": "sand" },                                          everywhere first
//     { "layer": "grass", "height": { "min": 2, "soft": 1.5 } },     above 2 m, fading in over 1.5 m
//     { "layer": "rock", "slope": { "min": 35, "soft": 10 } },      steeper than 35 degrees
//     { "layer": "dirt", "noise": { "min": 0.6, "soft": 0.1 },      in patches about 12 m across
//       "noise_size": 12, "seed": 3, "strength": 0.8 }
//   ]
//
// The rules run in order: each moves every texel toward its layer by how much the texel is in its
// ranges, times its strength. A texel the mesh does not cover is left as it was.

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "scene/ground/ground_survey.hpp"
#include "scene/ground/terrain_layers.hpp"

namespace redgem {

// How much of a value is in a range: 1 inside, 0 outside, a smooth fade over `soft` centered on
// each edge.
float range_share(const SoftRange& range, float value);

// A smooth noise of patches about `size` across, 0 to 1, the same for the same place and seed.
float ground_noise(float x, float z, float size, std::uint32_t seed);

// How much a rule paints at a place of the ground (`x`, `z` in the mesh's own space): 0 to its
// strength.
float rule_share(const GroundRule& rule, const GroundPoint& point, float x, float z);

// Reads a "rules" list, naming layers of `layers`, into `into`. Returns what is wrong with it
// ("" when nothing is); `into` is then left as it was.
std::string read_ground_rules(const nlohmann::json& rules, const TerrainLayers& layers, std::vector<GroundRule>& into);

// Paints `rules` into the terrain's weights over the mesh `mesh` it lies on. Returns how many
// texels changed; the weights' version goes up when any did.
std::size_t paint_ground(TerrainLayers& layers, const std::vector<GroundRule>& rules, const MeshDesc& mesh);

}  // namespace redgem
