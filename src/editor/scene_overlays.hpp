#pragma once

// What the Scene view draws over the edited scene: the grid, marks where a picture would show
// nothing (cameras, lights, empty objects, triggers), the selection's box and the terrain brush.

#include <raylib.h>

#include <optional>
#include <vector>

#include "editor/npc_gizmo.hpp"
#include "scene/scene_config.hpp"
#include "physics/debug_geometry.hpp"
#include "render/debug_draw.hpp"
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

// The color of each part of an NPC gizmo.
::Color npc_gizmo_color(NpcGizmoPart part);

// Draws the lines of NPC gizmos (inside a 3D mode).
void draw_npc_gizmos(const std::vector<NpcGizmo>& gizmos);

// The lines of NPC gizmos as the running game's debug pass draws them.
std::vector<DebugDrawItem> npc_gizmo_debug_lines(const std::vector<NpcGizmo>& gizmos);

class DebugDrawBuffer;
// Estimates tessellation from free line capacity; a one-triangle probe distinguishes an empty world.
std::size_t physics_debug_triangle_budget(const DebugDrawBuffer& commands);
// Adds unique collider edges to the renderer's world pass, before HUD/touch controls. False
// when geometry or command limits omitted any edge.
bool append_physics_colliders(DebugDrawBuffer& commands, const PhysicsDebugGeometry& geometry);

}  // namespace redgem
