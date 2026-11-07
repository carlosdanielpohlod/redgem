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
FrameView GameView::physics_view(const FrameView& view) {
  copy_commands(physics_commands_, view.debug_draw);
  const auto geometry = physics_debug_engine_->physics().debug_geometry();
  physics_debug_truncated_ = !append_physics_colliders(physics_commands_, geometry);
  auto combined = view;
  combined.debug_draw = &physics_commands_;
  return combined;
}
void GameView::paint_with_debug(const FrameView& view) {
  physics_debug_truncated_ = false;
  if (physics_debug_engine_)
    painter_.paint(physics_view(view), &view_.target);
  else
    painter_.paint(view, &view_.target);
}
}  // namespace redgem
