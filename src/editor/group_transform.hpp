#pragma once

// Moving, turning and scaling several objects at once with one gizmo. The gizmo holds a pivot;
// each object keeps where it is relative to the pivot while the pivot is dragged. Pure math, so
// it is tested without a window.

#include <vector>

#include "scene/transform.hpp"

namespace redgem::group_transform {

// Where the pivot of a selection sits: in the middle of the objects, or on the first one picked.
enum class PivotMode { Center, First };

// The pivot of objects placed at `worlds` (the first one picked first): at their middle or on the
// first, turned like the first (so the gizmo's own axes are that object's), and unscaled (so what
// the gizmo scales it by is what the group is scaled by). The identity for no object.
Transform pivot_of(const std::vector<Transform>& worlds, PivotMode mode);

// Where an object placed at `start` goes when the pivot moves from `pivot_start` to `pivot_now`.
Transform follow_pivot(const Transform& start, const Transform& pivot_start, const Transform& pivot_now);

// A gizmo drag of a group, from where it began.
struct Drag {
  std::vector<int> objects;       // what is moved
  std::vector<Transform> starts;  // where each one was in the world when the drag began
  Transform pivot_start;
  Transform pivot_now;
};
// A drag of objects placed at `starts`, its pivot where `mode` puts it, not moved yet.
Drag begin_drag(const std::vector<int>& objects, const std::vector<Transform>& starts, PivotMode mode);
// Where each of the drag's objects is in the world now.
std::vector<Transform> placements(const Drag& drag);

}  // namespace redgem::group_transform
