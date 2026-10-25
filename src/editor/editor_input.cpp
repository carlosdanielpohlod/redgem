// Part of EditorApp: the keyboard and mouse outside the panels. Shortcuts, and flying the camera.

#include "editor/editor_app.hpp"

#include <imgui.h>
#include <raylib.h>

#include "editor/keyboard.hpp"
#include "editor/scene_viewport.hpp"

namespace redgem {
namespace {

using namespace keyboard;

// WASD fly the camera while Ctrl or the right mouse button is held.
bool keys_fly_camera() { return ctrl_down() || IsMouseButtonDown(MOUSE_BUTTON_RIGHT); }

}  // namespace

void EditorApp::handle_shortcuts() {
  if (shortcuts_blocked()) return;
  const bool was_editing = editing();  // Play and Stop below take effect on the next frame's shortcuts
  if (ctrl_down()) handle_window_shortcuts(was_editing);
  if (was_editing) {
    handle_editing_shortcuts();
  } else if (IsKeyPressed(KEY_ESCAPE)) {
    stop();  // the only key the game does not own while playing
  }
}

// Shortcuts stay out of what is typed into a field, and of a drag or a dialog.
bool EditorApp::shortcuts_blocked() const { return ImGui::GetIO().WantTextInput || editing_blocked(); }

// Ctrl + a key, whether a game runs or not: Play / Stop and the panel toggles (as in VS Code).
void EditorApp::handle_window_shortcuts(bool was_editing) {
  if (IsKeyPressed(KEY_P)) was_editing ? play() : stop();
  if (IsKeyPressed(KEY_B)) (alt_down() ? panels_.inspector : panels_.hierarchy) ^= true;
  if (IsKeyPressed(KEY_J)) panels_.bottom ^= true;
}

void EditorApp::handle_editing_shortcuts() {
  if (ctrl_down()) handle_document_shortcuts();
  if (IsKeyPressed(KEY_DELETE)) session_->delete_selection();
  if (keys_fly_camera()) return;
  if (IsKeyPressed(KEY_F)) frame_selection();
  gizmo_.use_keys();
}

// Ctrl + a key, while editing.
void EditorApp::handle_document_shortcuts() {
  if (IsKeyPressed(KEY_S)) save();
  if (IsKeyPressed(KEY_Z)) shift_down() ? session_->redo() : session_->undo();
  if (IsKeyPressed(KEY_Y)) session_->redo();
  if (IsKeyPressed(KEY_D)) session_->duplicate_selection();
  handle_clipboard_shortcuts();
}

// Ctrl + X, C or V; with Shift, V pastes under the selected object.
void EditorApp::handle_clipboard_shortcuts() {
  if (IsKeyPressed(KEY_C)) copy_selection();
  if (IsKeyPressed(KEY_X)) cut_selection();
  if (IsKeyPressed(KEY_V)) shift_down() ? paste_into(session_->selection()) : paste();
}

void EditorApp::update_camera() {
  if (!mouse_steers_camera()) return;
  FlyInput input;
  input.dt = frame_seconds();
  input.zoom = GetMouseWheelMove();
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    const Vector2 delta = GetMouseDelta();
    input.look_x = delta.x;
    input.look_y = delta.y;
    steer(input, fly_keys());
  }
  camera_.update(input);
}

bool EditorApp::mouse_steers_camera() const { return editing() && viewport_->hovered; }

}  // namespace redgem
