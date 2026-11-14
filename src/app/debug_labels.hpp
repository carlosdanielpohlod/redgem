#pragma once

// Debug.label: texts a script pinned to points of the world, placed on the screen where the
// scene's main camera shows those points, once the frame's scripts are done (the camera has moved
// for the frame by then). They go over the game's own interface, under the statistics overlay.

#include <optional>

#include "render/debug_draw.hpp"
#include "render/hud.hpp"
#include "scene/camera_ray.hpp"

namespace redgem {
struct ScriptContext;

// Where a camera sees from, what it sees through, and the screen it is shown on.
struct LabelView {
  Transform camera;
  Lens lens;
  ScreenFrame frame;
};

// The text a label is drawn as, centered on where `view` shows its point, `width` pixels wide;
// nothing for a point behind the camera (or nearer than its near plane).
std::optional<HudItem> debug_label_item(const DebugLabel& label, const LabelView& view, int width);
// The labels of the frame as texts over the game: none when the scene has no main camera.
void append_debug_labels(ScriptContext& context, const DebugDrawBuffer& commands);

}  // namespace redgem
