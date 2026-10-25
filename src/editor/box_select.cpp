#include "editor/box_select.hpp"

#include <algorithm>
#include <cmath>

namespace redgem::box_select {
namespace {

constexpr float kDegreesToRadians = kPi / 180.0f;
constexpr float kNearestDepth = 1e-4f;  // meters in front of the camera: anything closer is on its plane
constexpr float kMinBoxPixels = 4.0f;   // a smaller move of the mouse is a shaky click

// The camera's own axes: where it looks, its right and its up.
struct Axes {
  Vec3 forward;
  Vec3 right;
  Vec3 up;
};

Axes axes_of(const ViewCamera& camera) {
  const Vec3 forward = normalize(camera.target - camera.position);
  const Vec3 right = normalize(cross(forward, camera.up));
  return {forward, right, cross(right, forward)};
}

// The view has a size to put points on.
bool has_area(const ViewCamera& camera) { return camera.width > 0.0f && camera.height > 0.0f; }

}  // namespace

std::optional<ViewPoint> to_view(const ViewCamera& camera, Vec3 point) {
  const Axes axes = axes_of(camera);
  const Vec3 offset = point - camera.position;
  const float depth = dot(offset, axes.forward);
  if (depth < kNearestDepth || !has_area(camera)) return std::nullopt;
  const float half_height = depth * std::tan(camera.fov_y_degrees * kDegreesToRadians * kHalf);
  const float half_width = half_height * camera.width / camera.height;
  const float across = dot(offset, axes.right) / half_width;  // -1 at the left edge, 1 at the right
  const float upward = dot(offset, axes.up) / half_height;    // -1 at the bottom, 1 at the top
  return ViewPoint{(across + 1.0f) * kHalf * camera.width, (1.0f - upward) * kHalf * camera.height};
}

editor_logic::Rect rect_between(ViewPoint a, ViewPoint b) {
  return {std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
}

bool contains(const editor_logic::Rect& rect, ViewPoint point) {
  const bool across = point.x >= rect.min_x && point.x <= rect.max_x;
  return across && point.y >= rect.min_y && point.y <= rect.max_y;
}

bool dragged_far_enough(ViewPoint from, ViewPoint to) {
  return std::fabs(to.x - from.x) >= kMinBoxPixels || std::fabs(to.y - from.y) >= kMinBoxPixels;
}

std::vector<int> inside(const std::vector<Candidate>& candidates, const editor_logic::Rect& rect) {
  std::vector<int> ids;
  for (const Candidate& candidate : candidates) {
    if (candidate.at && contains(rect, *candidate.at)) ids.push_back(candidate.id);
  }
  return ids;
}

}  // namespace redgem::box_select
