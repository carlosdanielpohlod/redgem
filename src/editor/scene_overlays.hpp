#pragma once

// What the Scene view draws over the edited scene: the grid, marks where a picture would show
// nothing (cameras, lights, empty objects, triggers), the selection's box and the terrain brush.

#include <raylib.h>

#include <optional>
#include <vector>

#include "scene/scene_config.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// A color of editor_config (four numbers, 0-255) as raylib takes it.
::Color rgba(const unsigned char (&channels)[config::kChannelsPerColor]);

// The box drawn around an object: its mesh's, or a small one for objects without a mesh.
Bounds bounds_of(const GameObject& object, const SceneDesc& desc);

// The terrain brush where it touches the ground.
struct BrushMark {
  Vec3 center;
  float radius = 0.0f;
};

// Draws the overlays (inside a 3D mode) for the scene as it is, around each of `selected`.
void draw_scene_overlays(const Scene& scene, const SceneDesc& desc, const std::vector<ObjectId>& selected,
                         const std::optional<BrushMark>& brush);

}  // namespace redgem
