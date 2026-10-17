#pragma once

// A field of grass ringed by a forest, built in code for the renderer benchmark: a ground, a
// grass tuft and a tree (both cut out of a texture made here, so they sway in the wind), a sun,
// and a camera standing in the field.

#include <vector>

#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::benchmark {

inline constexpr int kGrassTufts = 50000;
inline constexpr int kTrees = 2000;
// The names of the two templates in the scene: hidden objects that every copy is made from.
inline constexpr const char* kGrassName = "GrassTuft";
inline constexpr const char* kTreeName = "Tree";
// Their meshes.
inline constexpr int kGrassMesh = 1;
inline constexpr int kTreeMesh = 2;

// Where the copies go: grass over a square around the middle, trees in a ring around it.
inline constexpr float kFieldSize = 160.0f;    // meters across
inline constexpr float kForestInner = 85.0f;   // meters from the middle
inline constexpr float kForestOuter = 140.0f;  // meters from the middle
inline constexpr unsigned int kFieldSeed = 7;  // the same field every run

// A place and a turn around the vertical, for one copy.
struct Placement {
  Vec3 position;
  float yaw_degrees = 0.0f;
  float scale = 1.0f;
};
std::vector<Placement> grass_placements();
std::vector<Placement> tree_placements();

// The scene with its two templates and nothing placed yet.
SceneDesc field_templates();
// The same scene with every tuft and tree as an object of its own.
SceneDesc field_of_objects();
// Puts every tuft and tree in a scene of the templates' as scattered copies (Scatter) instead.
void scatter_field(Scene& scene);

}  // namespace redgem::benchmark
