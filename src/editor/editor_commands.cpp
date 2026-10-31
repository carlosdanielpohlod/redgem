// Part of EditorApp: the commands, the same ones the menus, the toolbar and the shortcuts use.

#include "editor/editor_app.hpp"

#include <imgui.h>

#include "core/log.hpp"
#include "scene/prefabs.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/keyboard.hpp"
#include "editor/picking.hpp"
#include "editor/scene_viewport.hpp"
#include "render/render_math.hpp"
#include "render/renderer.hpp"
#include "render/window.hpp"

namespace redgem {
namespace {

using namespace editor_config;
using render_math::to_vec3;

}  // namespace

void EditorApp::save_scripts() {
  std::string error;
  if (!scripts_->save_all(error)) log::error(error);
}

bool EditorApp::interaction_active() const {
  return gizmo_dragging_ || stroke_started_ || ImGui::IsAnyItemActive() ||
         ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

void EditorApp::play() {
  save_scripts();  // like an IDE "save all": the game must run the code on screen
  rescan_files();
  std::string error;
  const bool unchanged_assets = !prefabs::has_references(session_->document().desc());
  game_view_ = std::make_unique<GameView>(*window_, *renderer_, *viewport_, game_input_, unchanged_assets);
  if (!session_->play({game_input_, audio_.get(), game_view_.get()}, error)) {
    log::error("cannot play: " + error);
    game_view_.reset();
    return;
  }
  ImGui::SetWindowFocus(kViewportWindow);  // the game gets the keyboard right away
}

void EditorApp::stop() {
  session_->stop();
  retire_game_view();
}

void EditorApp::toggle_pause() { session_->pause(session_->play_state() == PlayState::Playing); }

void EditorApp::save() {
  std::string error;
  if (!session_->save(error)) log::error("save failed: " + error);
}

void EditorApp::request_exit(EditorExit action) {
  exit_action_ = action;
  if (!editing()) stop();
  if (unsaved_work()) {
    pending_ = PendingAction{PendingAction::Kind::Leave, {}};  // whatever else was waiting
  } else {
    quit_confirmed_ = true;
  }
}

Vec3 EditorApp::spawn_point() const { return camera_.position() + camera_.forward() * kSpawnDistance; }

void EditorApp::frame_selection() {
  std::vector<editor_logic::Sphere> spheres;
  for (const ObjectId id : session_->selected().ids()) {
    editor_logic::Sphere sphere;
    object_extent(session_->scene(), session_->scene_desc(), id, sphere.center, sphere.radius);
    spheres.push_back(sphere);
  }
  if (spheres.empty()) return;
  const editor_logic::Sphere all = editor_logic::enclosing(spheres);
  camera_.frame(all.center, all.radius);
}

std::optional<Vec3> EditorApp::sculpt_at(float x, float y, float seconds) {
  const ObjectId terrain = session_->selection();
  const Ray ray = viewport_->ray_at(x, y);
  const auto point = session_->terrain_hit(terrain, to_vec3(ray.position), normalize(to_vec3(ray.direction)));
  if (!point) return point;
  if (seconds <= 0.0f) return point;  // the brush is over the ground, not held on it
  if (!stroke_started_) {
    session_->document().checkpoint();  // one undo step per stroke
    stroke_started_ = true;
  }
  // Shift digs with Raise and piles up with Lower.
  const SculptMode mode = keyboard::shift_down() ? editor_logic::reversed(terrain_tool_.mode) : terrain_tool_.mode;
  const float amount = editor_logic::brush_amount(mode, terrain_tool_.strength, seconds);
  session_->sculpt_terrain(terrain, *point, terrain_tool_.radius, amount, mode);
  return point;
}

}  // namespace redgem
