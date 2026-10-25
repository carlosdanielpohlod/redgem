#pragma once

#include <optional>

#include "editor/group_transform.hpp"
#include "scene/transform.hpp"

namespace redgem {

struct SceneViewport;

enum class GizmoMode { Translate, Rotate, Scale };

// How far the handles jump while snapping: half a meter, a few degrees, a tenth of the size.
float snap_step(GizmoMode mode);

// The gizmo of the Scene view: handles that move, turn or scale the selected objects around a
// pivot, on the world's axes or the first object's own, snapping to steps or not.
class Gizmo {
 public:
  GizmoMode mode() const { return mode_; }
  // Where several selected objects turn and scale around: their middle, or the first one picked.
  group_transform::PivotMode pivot_mode() const { return pivot_; }
  // W, E and R pick a mode.
  void use_keys();
  // The toolbar's buttons: the mode, the axes, the pivot and snapping.
  void draw_tools();
  // Draws the handles over a pivot placed at `world`, in the Scene view, and lets the mouse
  // drag them. Where the pivot was dragged to, while a drag goes on.
  std::optional<Transform> manipulate(const Transform& world, const SceneViewport& view);

 private:
  // Center or the first object, one click each.
  void draw_pivot_button();
  // Scaling always works on the object's own axes.
  bool own_axes() const { return local_ || mode_ == GizmoMode::Scale; }
  // Snapping is switched on, or Ctrl is held.
  bool snaps() const;
  // Lets the mouse move the handles of `model` (the 16 numbers of a world matrix).
  void drag(float* model, const SceneViewport& view) const;

  GizmoMode mode_ = GizmoMode::Translate;
  bool local_ = false;  // axes of the object instead of the world
  bool snapping_ = false;
  group_transform::PivotMode pivot_ = group_transform::PivotMode::Center;
};

}  // namespace redgem
