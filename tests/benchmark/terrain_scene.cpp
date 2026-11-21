#include "benchmark/terrain_scene.hpp"

#include <cstdint>
#include <random>

#include "scene/ground/splat_map.hpp"
#include "scene/props.hpp"
#include "scene/shapes/terrain.hpp"

namespace redgem::benchmark {
namespace {

constexpr Vec3 kCameraAt{0.0f, 4.0f, 60.0f};
constexpr Vec3 kCameraLooksAt{0.0f, 0.0f, 0.0f};
constexpr TerrainBrush kHill{-20.0f, -10.0f, 40.0f, 12.0f, SculptMode::Raise};
constexpr unsigned int kSeed = 11;  // the same pictures every run
constexpr int kChannels = 4;
constexpr std::uint8_t kFlat = 128;  // a normal picture's "straight up", a mid height
constexpr std::uint8_t kFull = 255;
constexpr int kTerrainNode = 1;
constexpr const char* kLayersName = "benchmark";

// Noisy pictures: every texel differs, as a real ground's do (what the texture cache sees).
void add_noisy_layer(TerrainPictures& pictures, std::mt19937& random) {
  std::uniform_int_distribution<int> byte(0, kFull);
  const std::size_t texels = static_cast<std::size_t>(kBenchmarkPictureSide) * kBenchmarkPictureSide;
  for (std::size_t i = 0; i < texels; ++i) {
    pictures.color.insert(pictures.color.end(),
                          {static_cast<std::uint8_t>(byte(random)), static_cast<std::uint8_t>(byte(random)),
                           static_cast<std::uint8_t>(byte(random)), static_cast<std::uint8_t>(byte(random))});
    pictures.surface.insert(pictures.surface.end(), {kFlat, kFlat, kFull, kFull});
  }
  pictures.has_height[static_cast<std::size_t>(pictures.layers)] = true;
  ++pictures.layers;
}

// Every layer an even share of every texel.
SplatMap even_weights(int layers) {
  SplatMap splat = make_splat(kBenchmarkSplatSide);
  LayerWeights even{};
  for (int i = 0; i < layers; ++i) even[static_cast<std::size_t>(i)] = 1.0f;
  for (int row = 0; row < splat.size; ++row) {
    for (int column = 0; column < splat.size; ++column) set_texel(splat, column, row, even);
  }
  return splat;
}

TerrainLayer layer_with(const TerrainOptions& options) {
  TerrainLayer layer;
  layer.stochastic = options.stochastic;
  layer.triplanar = options.triplanar;
  layer.variation = kHalf;
  return layer;
}

TerrainLayers layers_for(const TerrainOptions& options) {
  TerrainLayers layers;
  layers.name = kLayersName;
  layers.node = kTerrainNode;
  layers.pictures.side = kBenchmarkPictureSide;
  std::mt19937 random(kSeed);  // NOLINT(bugprone-random-generator-seed): the same pictures every run
  for (int i = 0; i < options.layers; ++i) {
    add_noisy_layer(layers.pictures, random);
    layers.layers.push_back(layer_with(options));
  }
  layers.splat = even_weights(options.layers);
  return layers;
}

NodeDesc terrain_node(const TerrainGrid& grid) {
  NodeDesc node;
  node.name = "Terrain";
  node.mesh = 0;
  node.props[props::kTerrain] = true;
  node.props[props::kTerrainSize] = static_cast<double>(grid.size);
  node.props[props::kTerrainCells] = static_cast<double>(grid.cells);
  return node;
}

NodeDesc camera() {
  NodeDesc made;
  made.name = "Camera";
  made.camera = CameraDesc{};
  made.local.position = kCameraAt;
  made.local.rotation = quat_look_rotation(kCameraLooksAt - kCameraAt, Vec3::up());
  return made;
}

}  // namespace

SceneDesc terrain_scene(const TerrainOptions& options) {
  const TerrainGrid grid{kTerrainSize, kTerrainCells};
  PrimitiveDesc ground = make_terrain(grid);
  sculpt_terrain(ground, grid, kHill);
  ground.material = 0;
  SceneDesc desc;
  desc.materials = {MaterialDesc{}};
  desc.meshes = {MeshDesc{"Terrain", {ground}, mesh_bounds_of(ground), {}, {}}};
  desc.nodes = {camera(), terrain_node(grid)};
  if (options.layers > 0) desc.terrains = {layers_for(options)};
  return desc;
}

}  // namespace redgem::benchmark
