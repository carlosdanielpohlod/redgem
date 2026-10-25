// Part of EditorApp: the Scene window, and what the mouse does in it. Picking, the gizmo, the
// terrain brush, and files dropped on the view.

#include "editor/editor_app.hpp"

#include <imgui.h>
#include <raylib.h>
#include <ImGuizmo.h>

#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/scene_viewport.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"
#include "render/render_math.hpp"

namespace redgem {
namespace {

using namespace editor_config;
using render_math::to_vec3;

constexpr long long kStartupFrames = 2;  // the dock layout exists from the second frame on

// The right button is held over the view: flying takes the keyboard.
bool flying_over(const SceneViewport& view) { return view.hovered && IsMouseButtonDown(MOUSE_BUTTON_RIGHT); }

// The left button is held over the view.
bool brushing(const SceneViewport& view) { return view.hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left); }

}  // namespace

void EditorApp::draw_viewport_window() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  const bool playing = !editing();
  if (frame_count_ <= kStartupFrames) ImGui::SetNextWindowFocus();  // the Scene tab starts in front
  ImGui::Begin(ui::window_title(kViewportTitle, kViewportWindow), nullptr,
               ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  draw_viewport_image();
  if (!playing) draw_viewport_drop_target();
  track_viewport_focus();
  draw_viewport_hint(playing);
  brush_point_.reset();
  playing ? draw_play_border() : use_viewport_tools();
  ImGui::End();
  ImGui::PopStyleVar();
}

// Whether the mouse and the keyboard are in the Scene view. Flying (right mouse) takes the keyboard.
void EditorApp::track_viewport_focus() {
  SceneViewport& view = *viewport_;
  view.hovered = ImGui::IsWindowHovered();
  view.focused = ImGui::IsWindowFocused();
  if (flying_over(view)) ImGui::SetWindowFocus();
}

void EditorApp::draw_viewport_drop_target() {
  if (!ImGui::BeginDragDropTarget()) return;
  const Vector2 mouse = viewport_->mouse();
  if (const char* model = ui::dropped_text(kPayloadModel)) drop_model(model, mouse.x, mouse.y);
  if (const char* script = ui::dropped_text(kPayloadScript)) drop_script(script, mouse.x, mouse.y);
  ImGui::EndDragDropTarget();
}

// A model lands on the ground under the mouse (y = 0), or in front of the camera when aiming at the sky.
void EditorApp::drop_model(const char* model_file, float x, float y) {
  const Ray ray = viewport_->ray_at(x, y);
  const Vec3 at = editor_logic::drop_point(to_vec3(ray.position), to_vec3(ray.direction), spawn_point());
  ui::import_model(*session_, model_file, at);
}

// A script goes to the object under the mouse.
void EditorApp::drop_script(const char* script_file, float x, float y) {
  const ObjectId target = pick_at(x, y);
  if (target == kNoObject) {
    log::warning("drop scripts on an object");
    return;
  }
  session_->attach_script(target, script_file);
}

// What the mouse does in the Scene view while editing: the terrain brush, or the gizmo and picking.
void EditorApp::use_viewport_tools() {
  // Sculpting takes the left button over from the gizmo and from picking.
  if (sculpting()) {
    use_brush();
    return;
  }
  stroke_started_ = false;
  if (!session_->selected().empty()) draw_gizmo();
  use_selection_clicks();
}

bool EditorApp::sculpting() const { return terrain_tool_.sculpting && session_->is_terrain(session_->selection()); }

void EditorApp::use_brush() {
  const SceneViewport& view = *viewport_;
  const Vector2 mouse = view.mouse();
  const bool held = brushing(view);
  if (view.hovered) brush_point_ = sculpt_at(mouse.x, mouse.y, held ? frame_seconds() : 0.0f);
  if (stroke_ends(held)) {
    stroke_started_ = false;
    session_->document().drop_checkpoint_if_unchanged();
  }
}

void EditorApp::draw_gizmo() {
  if (!gizmo_drawable()) return;
  // (While a drag goes on, the gizmo keeps the pivot it moves: the objects follow it.)
  const Transform pivot = gizmo_dragging_ ? group_drag_.pivot_now : selection_pivot();
  const std::optional<Transform> dragged = gizmo_.manipulate(pivot, *viewport_);
  gizmo_drawn_ = true;
  track_gizmo_drag(dragged.has_value());
  if (dragged) apply_gizmo_drag(*dragged);
}

bool EditorApp::gizmo_drawable() const {
  return session_->scene().is_alive(session_->selection()) && viewport_->width > 0 && viewport_->height > 0;
}

Transform EditorApp::selection_pivot() const {
  return group_transform::pivot_of(worlds_of(session_->selected_roots()), gizmo_.pivot_mode());
}

std::vector<Transform> EditorApp::worlds_of(const std::vector<int>& objects) const {
  std::vector<Transform> worlds;
  worlds.reserve(objects.size());
  for (const int object : objects) worlds.push_back(session_->scene().world_transform(object));
  return worlds;
}

void EditorApp::track_gizmo_drag(bool using_gizmo) {
  if (drag_begins(using_gizmo)) {
    session_->document().checkpoint();  // one undo step for the whole group
    const std::vector<int> objects = session_->selected_roots();
    group_drag_ = group_transform::begin_drag(objects, worlds_of(objects), gizmo_.pivot_mode());
  }
  if (drag_ends(using_gizmo)) session_->document().drop_checkpoint_if_unchanged();
  gizmo_dragging_ = using_gizmo;
}

// Moves the selection with the pivot the gizmo was dragged to (a world placement).
void EditorApp::apply_gizmo_drag(const Transform& pivot) {
  group_drag_.pivot_now = pivot;
  const std::vector<Transform> worlds = group_transform::placements(group_drag_);
  for (std::size_t i = 0; i < worlds.size(); ++i) session_->place_in_world(group_drag_.objects[i], worlds[i]);
}

}  // namespace redgem
