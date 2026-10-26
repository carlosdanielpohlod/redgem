#pragma once

// Selecting by dragging a rectangle over the Scene view: where objects of the world are on the
// view, and which of them a rectangle holds. Pure math, so it is tested without a window.

#include <optional>
#include <vector>

#include "core/math.hpp"
#include "editor/editor_logic.hpp"

namespace redgem::box_select {

// A point of the Scene view, in its pixels (origin top-left).
struct ViewPoint {
  float x = 0.0f;
  float y = 0.0f;
};

// How the Scene view looks at the world (a perspective camera).
struct ViewCamera {
  Vec3 position;
  Vec3 target;
  Vec3 up;
  float fov_y_degrees = 0.0f;
  float width = 0.0f;  // the view's size in pixels
  float height = 0.0f;
};

// Where a point of the world shows on the view; nullopt when it is behind the camera (or on its
// plane). Points outside the view's edges come out past them.
std::optional<ViewPoint> to_view(const ViewCamera& camera, Vec3 point);

// The rectangle with these two corners, whichever way it was dragged.
editor_logic::Rect rect_between(ViewPoint a, ViewPoint b);
bool contains(const editor_logic::Rect& rect, ViewPoint point);
// Whether the mouse went far enough from where its button went down for a rectangle, not a click.
bool dragged_far_enough(ViewPoint from, ViewPoint to);

// An object that can be selected with a rectangle, and where it shows on the view (nullopt when
// it is behind the camera).
struct Candidate {
  int id = kNoIndex;
  std::optional<ViewPoint> at;
};
// The candidates the rectangle holds, in their order.
std::vector<int> inside(const std::vector<Candidate>& candidates, const editor_logic::Rect& rect);

}  // namespace redgem::box_select
