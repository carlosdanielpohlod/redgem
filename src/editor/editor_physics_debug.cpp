#include "editor/editor_app.hpp"

#include <raylib.h>
#include <rlgl.h>

#include "editor/scene_overlays.hpp"
#include "editor/scene_viewport.hpp"

namespace redgem {
namespace {
void draw_geometry(const SceneViewport& viewport, const PhysicsDebugGeometry& geometry) {
  BeginTextureMode(viewport.target);
  BeginMode3D(viewport.camera);
  rlDisableDepthMask();
  rlDisableDepthTest();  // colliders remain visible through opaque meshes
  draw_physics_colliders(geometry);
  rlDrawRenderBatchActive();  // submit the wires before restoring the depth state
  rlEnableDepthTest();
  rlEnableDepthMask();
  EndMode3D();
  EndTextureMode();
}
}  // namespace
void EditorApp::draw_physics_debug() {
  physics_debug_truncated_ = false;
  if (!physics_debug_) return;
  const auto geometry = session_->runtime()->physics().debug_geometry();
  physics_debug_truncated_ = geometry.truncated;
  draw_geometry(*viewport_, geometry);
}
}  // namespace redgem
