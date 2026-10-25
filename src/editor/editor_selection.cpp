// Part of EditorApp: what selects. Clicks and rectangles in the Scene view, and the clipboard's
// commands on what is selected.

#include "editor/editor_app.hpp"

#include <imgui.h>
#include <raylib.h>
#include <ImGuizmo.h>

#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/keyboard.hpp"
#include "editor/object_clipboard.hpp"
#include "editor/picking.hpp"
#include "editor/scene_viewport.hpp"
#include "editor/ui/style.hpp"
#include "render/render_math.hpp"

namespace redgem {
namespace {

using namespace editor_config;
using box_select::ViewPoint;
using render_math::to_vec3;

// The left button went down in the view, away from the gizmo.
bool selecting_click(const SceneViewport& view) {
  return view.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing();
}

// What a rectangle can select: something drawn, a light or a camera.
bool box_selectable(const GameObject& object, const NodeDesc& node) {
  const bool drawn = object.mesh != kNoIndex && object.visible;
  return drawn || node.light.has_value() || node.camera.has_value();
}

Vec3 middle_of(const Scene& scene, const SceneDesc& desc, ObjectId id) {
  Vec3 center;
  float radius = 0.0f;
  object_extent(scene, desc, id, center, radius);
  return center;
}

// Whether a click that met `hit` changes the selection: it met something, or it was a plain
// click (which clears the selection when it meets nothing).
bool changes_selection(ObjectId hit, ClickMode mode) { return hit != kNoObject || mode == ClickMode::Replace; }

// Says in the console why a clipboard command failed, when it says why.
void report(const char* command, const std::string& error) {
  if (!error.empty()) log::error(std::string(command) + " failed: " + error);
}

}  // namespace

void EditorApp::use_selection_clicks() {
  const Vector2 mouse = viewport_->mouse();
  const ViewPoint at{mouse.x, mouse.y};
  if (selecting_click(*viewport_)) box_start_ = at;
  if (!box_start_) return;
  if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) return draw_selection_rect(*box_start_, at);
  finish_selection_click(*box_start_, at);
  box_start_.reset();
}

// A click picks the object under the mouse; a drag selects what its rectangle holds.
void EditorApp::finish_selection_click(ViewPoint from, ViewPoint to) {
  const bool ctrl = keyboard::ctrl_down();
  const bool shift = keyboard::shift_down();
  if (box_select::dragged_far_enough(from, to)) {
    select_in_box(box_select::rect_between(from, to), box_mode(ctrl, shift));
  } else {
    pick_at(to.x, to.y, view_click_mode(ctrl, shift));
  }
}

void EditorApp::draw_selection_rect(ViewPoint from, ViewPoint to) const {
  if (!box_select::dragged_far_enough(from, to)) return;
  const editor_logic::Rect rect = box_select::rect_between(from, to);
  const ImVec2 min(viewport_->screen_x + rect.min_x, viewport_->screen_y + rect.min_y);
  const ImVec2 max(viewport_->screen_x + rect.max_x, viewport_->screen_y + rect.max_y);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  ImVec4 fill = ui::accent_color();
  fill.w *= kSelectionRectFillAlpha;
  draw->AddRectFilled(min, max, ImGui::GetColorU32(fill));
  draw->AddRect(min, max, ImGui::GetColorU32(ui::accent_color()), 0.0f, 0, kSelectionRectBorder);
}

ObjectId EditorApp::object_at(float x, float y) const {
  const Ray ray = viewport_->ray_at(x, y);
  const PickRay pick{to_vec3(ray.position), normalize(to_vec3(ray.direction))};
  return pick_object(session_->scene(), session_->scene_desc(), pick);
}

ObjectId EditorApp::pick_at(float x, float y) { return pick_at(x, y, ClickMode::Replace); }

ObjectId EditorApp::pick_at(float x, float y, ClickMode mode) {
  const ObjectId hit = object_at(x, y);
  if (changes_selection(hit, mode)) session_->click_select(hit, mode);
  return hit;
}

void EditorApp::select_in_box(const editor_logic::Rect& rect, BoxMode mode) {
  const std::vector<int> held = box_select::inside(box_candidates(), rect);
  if (mode == BoxMode::Add) {
    session_->add_to_selection(held);
  } else {
    session_->select_all(held);
  }
}

std::vector<box_select::Candidate> EditorApp::box_candidates() const {
  const Scene& scene = session_->scene();
  const SceneDesc& desc = session_->scene_desc();
  const box_select::ViewCamera camera = view_camera();
  std::vector<box_select::Candidate> candidates;
  for (const ObjectId id : scene.alive_ids()) {
    if (!box_selectable(scene.at(id), desc.nodes[id])) continue;
    candidates.push_back({id, box_select::to_view(camera, middle_of(scene, desc, id))});
  }
  return candidates;
}

box_select::ViewCamera EditorApp::view_camera() const {
  const Camera3D& camera = viewport_->camera;
  return {to_vec3(camera.position),  to_vec3(camera.target),    to_vec3(camera.up), camera.fovy,
          viewport_->width_pixels(), viewport_->height_pixels()};
}

void EditorApp::copy_selection() {
  std::string error;
  const std::optional<std::vector<std::uint8_t>> copied = session_->copy_selection(error);
  if (copied) keep_copied(*copied);
  report("copy", error);
}

void EditorApp::cut_selection() {
  std::string error;
  const std::optional<std::vector<std::uint8_t>> copied = session_->cut_selection(error);
  if (copied) keep_copied(*copied);
  report("cut", error);
}

// On the system's clipboard when it fits there, so that another editor window can paste it.
void EditorApp::keep_copied(const std::vector<std::uint8_t>& copied) {
  if (object_clipboard::fits_system_clipboard(copied.size())) {
    SetClipboardText(object_clipboard::to_text(copied).c_str());
    clipboard_.clear();
  } else {
    clipboard_ = copied;
  }
}

std::optional<std::vector<std::uint8_t>> EditorApp::clipboard_objects() const {
  const char* text = GetClipboardText();
  std::optional<std::vector<std::uint8_t>> copied = object_clipboard::from_text(text != nullptr ? text : "");
  if (copied) return copied;
  if (clipboard_.empty()) return std::nullopt;
  return clipboard_;
}

void EditorApp::paste() { paste_into(session_->paste_parent()); }

void EditorApp::paste_into(ObjectId parent) {
  const std::optional<std::vector<std::uint8_t>> copied = clipboard_objects();
  if (!copied) return;
  std::string error;
  if (!session_->paste(*copied, parent, error)) report("paste", error);
}

}  // namespace redgem
