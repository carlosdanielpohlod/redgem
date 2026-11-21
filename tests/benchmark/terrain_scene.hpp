#pragma once

// A large terrain built in code for the renderer benchmark, seen from a camera standing on it:
// without ground layers (its vertex tint alone), or with some layers, every one of them under
// every pixel (the most the shader can be asked to do).

#include "scene/scene_desc.hpp"

namespace redgem::benchmark {

inline constexpr float kTerrainSize = 200.0f;  // meters across
inline constexpr int kTerrainCells = 200;
inline constexpr int kBenchmarkPictureSide = 1024;  // pixels across each layer's pictures
inline constexpr int kBenchmarkSplatSide = 1024;

// What the layers are asked for, beyond blending by height.
struct TerrainOptions {
  int layers = 0;  // 0: no ground layers
  bool stochastic = false;
  bool triplanar = false;
};

SceneDesc terrain_scene(const TerrainOptions& options);

}  // namespace redgem::benchmark
