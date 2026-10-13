#pragma once

#include <optional>

#include "scene/transform.hpp"

namespace redgem {

struct SceneViewport;

enum class GizmoMode { Translate, Rotate, Scale };

// How far the handles jump while snapping: half a meter, a few degrees, a tenth of the size.
float snap_step(GizmoMode mode);

// The gizmo of the Scene view: handles that move, turn or scale the selected object, on the
// world's axes or the object's own, snapping to steps or not.
class Gizmo {
 public:
  GizmoMode mode() const { return mode_; }
  // W, E and R pick a mode.
  void use_keys();
  // The toolbar's buttons: the mode, the axes and snapping.
  void draw_tools();
  // Draws the handles over an object placed at `world`, in the Scene view, and lets the mouse
  // drag them. Where the object was dragged to, while a drag goes on.
  std::optional<Transform> manipulate(const Transform& world, const SceneViewport& view);

 private:
  // Scaling always works on the object's own axes.
  bool own_axes() const { return local_ || mode_ == GizmoMode::Scale; }
  // Snapping is switched on, or Ctrl is held.
  bool snaps() const;
  // Lets the mouse move the handles of `model` (the 16 numbers of a world matrix).
  void drag(float* model, const SceneViewport& view) const;

  GizmoMode mode_ = GizmoMode::Translate;
  bool local_ = false;  // axes of the object instead of the world
  bool snapping_ = false;
};

}  // namespace redgem
