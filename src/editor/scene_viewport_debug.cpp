#include "editor/scene_viewport.hpp"

#include "app/engine.hpp"
#include "editor/scene_overlays.hpp"

namespace redgem {
namespace {
void copy_commands(DebugDrawBuffer& target, const DebugDrawBuffer* source) {
  target.clear();
  if (!source) return;
  for (const auto& item : source->items()) target.append(item);
}
}  // namespace
// The game's own debug lines, then the editor's gizmos, then the colliders in what room is left.
FrameView GameView::debug_view(const FrameView& view) {
  copy_commands(physics_commands_, view.debug_draw);
  for (const auto& item : gizmo_lines_) physics_commands_.append(item);
  if (physics_debug_engine_) {
    const auto triangle_limit = physics_debug_triangle_budget(physics_commands_);
    const auto geometry = physics_debug_engine_->physics().debug_geometry(triangle_limit);
    physics_debug_truncated_ = !append_physics_colliders(physics_commands_, geometry);
  }
  auto combined = view;
  combined.debug_draw = &physics_commands_;
  return combined;
}
void GameView::paint_with_debug(const FrameView& view) {
  physics_debug_truncated_ = false;
  if (adds_debug())
    painter_.paint(debug_view(view), &view_.target);
  else
    painter_.paint(view, &view_.target);
}
}  // namespace redgem
