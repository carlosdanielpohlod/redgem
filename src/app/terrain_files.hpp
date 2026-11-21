#pragma once

// A terrain's ground layers from the game's folder (see scene/ground/terrain_layers.hpp): its
// layers file, the weights beside it, and its layers' pictures packed for the graphics card. And
// its weights written back, when the editor or a tool changed them.

#include <filesystem>
#include <optional>
#include <string>

#include "scene/scene_desc.hpp"

namespace redgem {

// What of a terrain's files is read: its weights alone (enough for scripts that ask which ground
// is where, or a dedicated server), or its pictures too (for drawing it).
enum class TerrainReading { Weights, WeightsAndPictures };

// Reads terrains/<name>.json and the weights beside it (a map all of the first layer when there are
// none yet), and, as `reading` says, the layers' pictures. nullopt, with `error`, when the layers
// file is missing or wrong. A picture that cannot be read is warned of, and the layer goes without.
std::optional<TerrainLayers> read_terrain_files(const std::filesystem::path& game_dir, const std::string& name,
                                                TerrainReading reading, std::string& error);

// Reads the ground layers of every object of `scene` that names them (props::kTerrainLayers) into
// scene.terrains, replacing what was there. An object whose layers cannot be read is warned of:
// it is drawn with its own material.
void attach_terrain_layers(SceneDesc& scene, const std::filesystem::path& game_dir,
                           TerrainReading reading = TerrainReading::WeightsAndPictures);

// The ground layers of node `node` in `scene`; null when it has none.
const TerrainLayers* terrain_layers_of(const SceneDesc& scene, int node);
TerrainLayers* terrain_layers_of(SceneDesc& scene, int node);

// Writes a terrain's weights into the game's folder: terrains/<name>.splat0.png, and .splat1.png
// when it has more than four layers (an old one is removed when it has no more). False, with
// `error`, when they cannot be written.
bool write_splat_pictures(const std::filesystem::path& game_dir, const TerrainLayers& layers, std::string& error);

// The layers' pictures read and packed (see TerrainPictures). Exposed for the tests.
TerrainPictures pack_terrain_pictures(const std::filesystem::path& game_dir, const TerrainLayers& layers);

}  // namespace redgem
