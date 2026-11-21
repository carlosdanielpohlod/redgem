// Part of EditorApp: the NPC gizmos of the selected objects in the Scene view (editor/npc_gizmo.hpp).
// While editing they come from the objects' properties and are drawn over the edited scene; while
// the game runs, from the parts its scripts gave the objects, drawn in the game's debug pass.

#include "editor/editor_app.hpp"

#include <imgui.h>
#include <raylib.h>

#include "app/engine.hpp"
#include "editor/editor_config.hpp"
#include "editor/scene_overlays.hpp"
#include "editor/scene_viewport.hpp"
#include "render/render_math.hpp"

namespace redgem {
namespace {

using namespace editor_config;
using render_math::to_raylib;

// Whether a point of the world is ahead of the camera (one behind it has no place on the screen).
bool in_front_of(const Camera3D& camera, Vec3 point) {
  const Vec3 eye = render_math::to_vec3(camera.position);
  const Vec3 ahead = render_math::to_vec3(camera.target) - eye;
  return dot(point - eye, ahead) > 0.0f;
}

// Selected objects whose gizmos are drawn, at most: while the game runs, each asks its scripts once a frame.
constexpr std::size_t kMostNpcGizmos = 64;

// Whether the running game left the edited scene for another: the selection names none of its objects.
bool runs_another_scene(Engine* runtime, const EditorSession& session) {
  return runtime != nullptr && runtime->scene_name() != session.scene_name();
}

// An object's NPC settings: the running scripts' while the game runs, else the ones its props give.
NpcSettings npc_settings_of(Engine* runtime, const Scene& scene, ObjectId id) {
  if (runtime != nullptr) return runtime->scripts().npc_settings(id);
  return npc_settings_from_props(scene.at(id).props);
}

ImU32 label_color() {
  const ::Color color = rgba(kNpcLabelColor);
  return IM_COL32(color.r, color.g, color.b, color.a);
}

}  // namespace

std::vector<NpcGizmo> EditorApp::selected_npc_gizmos() {
  std::vector<NpcGizmo> gizmos;
  Engine* runtime = session_->runtime();
  if (!npc_gizmos_ || runs_another_scene(runtime, *session_)) return gizmos;
  const Scene& scene = session_->scene();
  for (const ObjectId id : session_->selected().ids()) {
    if (gizmos.size() == kMostNpcGizmos) break;
    if (!scene.is_alive(id)) continue;
    gizmos.push_back(npc_gizmo(npc_settings_of(runtime, scene, id), scene.world_transform(id)));
  }
  return gizmos;
}

// Each label centered over its point, its last line just above it.
void EditorApp::draw_npc_labels(const std::vector<NpcGizmo>& gizmos) {
  const SceneViewport& view = *viewport_;
  ImDrawList* draw = ImGui::GetWindowDrawList();
  for (const NpcGizmo& gizmo : gizmos) {
    for (const NpcGizmoLabel& label : gizmo.labels) {
      if (!in_front_of(view.camera, label.at)) continue;
      const Vector2 at = GetWorldToScreenEx(to_raylib(label.at), view.camera, view.width, view.height);
      const ImVec2 size = ImGui::CalcTextSize(label.text.c_str());
      draw->AddText(ImVec2(view.screen_x + at.x - size.x * kHalf, view.screen_y + at.y - size.y), label_color(),
                    label.text.c_str());
    }
  }
}

}  // namespace redgem
