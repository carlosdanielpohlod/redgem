#pragma once

// What a terrain's layers file says (terrains/<name>.json in a game's folder):
//
//   {
//     "splat_size": 1024,           texels across the weights (16 to 4096)
//     "picture_size": 1024,         pixels across each layer's pictures once loaded (64 to 2048)
//     "variation_size": 40,         meters across the large patches of variation
//     "layers": [                   1 to 8, the first one covering what no weight says
//       { "name": "sand",
//         "albedo": "textures/sand/Color.jpg", "normal": "textures/sand/NormalGL.jpg",
//         "roughness": "textures/sand/Roughness.jpg", "occlusion": "", "height": "textures/sand/Height.png",
//         "tint": "#ffffff", "roughness_scale": 1, "normal_scale": 1,
//         "tiling": 4, "sharpness": 0.5, "variation": 0.2, "stochastic": true, "triplanar": false }
//     ],
//     "rules": [ { "layer": "sand" }, ... ]   how the layers are painted (see ground_rules.hpp)
//   }
//
// Every key but "layers" and each layer's "name" may be left out. Picture paths are relative to
// the game's folder.

#include <string>

#include <nlohmann/json.hpp>

#include "scene/ground/terrain_layers.hpp"

namespace redgem {

// Where a game keeps the layers file of `name`, and its weights (picture 0 or 1), relative to its
// folder.
std::string terrain_layers_path(const std::string& name);
std::string splat_picture_path(const std::string& name, int picture);

// Reads the settings and the layers of a layers file into `into` (no pictures, no weights). Returns
// what is wrong with it ("" when nothing is); `into` is then left as it was.
std::string read_terrain_layers(const nlohmann::json& file, TerrainLayers& into);

// The index of the layer called `name`; kNoLayer when there is none.
int layer_named(const TerrainLayers& layers, const std::string& name);

}  // namespace redgem
