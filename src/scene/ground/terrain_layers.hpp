#pragma once

// The ground materials of a terrain, blended by a splat map: up to eight layers (sand, grass,
// rock, dirt...), each a full set of pictures, and a picture of weights, independent of the
// mesh's vertices, saying how much of each layer covers each spot of the ground.
//
// A terrain object names its layers with the `terrain_layers` property: the name of a file of
// the game's terrains/ folder (terrains/<name>.json, see terrain_layers_file.hpp). Its weights
// are kept beside it, as pictures: terrains/<name>.splat0.png holds the first four layers' weights
// in its red, green, blue and alpha, terrains/<name>.splat1.png the next four.
//
// Experimental: the files and the API around them may still change.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/math.hpp"

namespace redgem {

inline constexpr int kMaxTerrainLayers = 8;
inline constexpr int kLayersPerSplatPicture = 4;  // red, green, blue and alpha
inline constexpr int kSplatPictures = kMaxTerrainLayers / kLayersPerSplatPicture;
inline constexpr int kDefaultSplatSize = 1024;  // texels across the whole terrain
inline constexpr int kMinSplatSize = 16;
inline constexpr int kMaxSplatSize = 4096;
inline constexpr int kDefaultLayerPictureSize = 1024;  // pixels across each layer's pictures, once loaded
inline constexpr int kMinLayerPictureSize = 64;
inline constexpr int kMaxLayerPictureSize = 2048;
inline constexpr float kDefaultLayerTiling = 4.0f;     // meters one repeat of a layer's pictures covers
inline constexpr float kDefaultLayerSharpness = 0.5f;  // 0: weights mix evenly; 1: the highest texel wins
inline constexpr float kDefaultVariationSize = 40.0f;  // meters across the large patches of variation
inline constexpr std::uint8_t kFullSplatWeight = 255;
inline constexpr int kNoTerrainNode = -1;
inline constexpr int kNoLayer = -1;

// One ground material. Its pictures are files of the game's folder ("" when it has none). Its
// color picture is sRGB; its normal map is OpenGL style (green up); roughness, occlusion and
// height are grey pictures (their red channel is read).
struct TerrainLayer {
  std::string name;
  std::string albedo;
  std::string normal;
  std::string roughness;
  std::string occlusion;
  std::string height;
  Vec3 tint{1.0f, 1.0f, 1.0f};   // linear RGB, multiplies the color picture
  float roughness_scale = 1.0f;  // multiplies the roughness picture (the roughness itself without one)
  float normal_scale = 1.0f;
  float tiling = kDefaultLayerTiling;
  // How sharply this layer meets its neighbours where its height picture is high (sand filling the
  // cracks between rocks): 0 blends by weight alone, 1 lets the higher texel take the spot.
  float sharpness = kDefaultLayerSharpness;
  // Large patches of the layer a little lighter, darker or shifted in hue (0: none, 1: strong),
  // which hide how its pictures repeat.
  float variation = 0.0f;
  bool stochastic = false;  // its pictures repeated with random turns and shifts, so no repeat shows
  bool triplanar = false;   // projected along the three axes on steep slopes, not stretched there
};

// The weights of a terrain's layers: `size` x `size` texels, rows along the terrain's Z (the
// first row at its smallest Z), columns along X. Each texel has kMaxTerrainLayers bytes, one a
// layer, which add up to kFullSplatWeight (or are all 0 on a blank map).
struct SplatMap {
  int size = 0;
  std::vector<std::uint8_t> weights;
  // Changed every time the weights do, so whoever copies them (the renderer) knows to copy again.
  std::uint64_t version = 0;

  bool empty() const { return size <= 0; }
};

// The layers' pictures, read and packed for the graphics card: every layer `side` x `side`, the
// layers one after the other, RGBA bytes. `color` holds the color picture (times the tint) with the
// height in its alpha; `surface` the normal's X and Y, the roughness and the occlusion.
struct TerrainPictures {
  int side = 0;
  int layers = 0;
  std::vector<std::uint8_t> color;
  std::vector<std::uint8_t> surface;
  // Which layers have a height picture (the others blend by weight alone).
  std::array<bool, kMaxTerrainLayers> has_height{};

  bool empty() const { return layers == 0; }
};

// A terrain's ground layers and their weights, for the scene's node `node` (whose mesh they cover:
// the splat map is laid over the mesh's own X and Z bounds).
struct TerrainLayers {
  std::string name;  // of its file in the terrains/ folder
  int node = kNoTerrainNode;
  int splat_size = kDefaultSplatSize;
  int picture_size = kDefaultLayerPictureSize;
  float variation_size = kDefaultVariationSize;
  std::vector<TerrainLayer> layers;
  SplatMap splat;
  TerrainPictures pictures;
};

}  // namespace redgem
