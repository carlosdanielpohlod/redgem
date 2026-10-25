#include "editor/group_transform.hpp"

namespace redgem::group_transform {
namespace {

Vec3 middle_of(const std::vector<Transform>& worlds) {
  Vec3 sum = Vec3::zero();
  for (const Transform& world : worlds) sum = sum + world.position;
  return sum * (1.0f / static_cast<float>(worlds.size()));
}

}  // namespace

Transform pivot_of(const std::vector<Transform>& worlds, PivotMode mode) {
  Transform pivot;
  if (worlds.empty()) return pivot;
  pivot.rotation = worlds.front().rotation;
  pivot.position = mode == PivotMode::First ? worlds.front().position : middle_of(worlds);
  return pivot;
}

Transform follow_pivot(const Transform& start, const Transform& pivot_start, const Transform& pivot_now) {
  // Where the object is as seen from the pivot does not change; the pivot does.
  return combine(pivot_now, relative_to(pivot_start, start));
}

Drag begin_drag(const std::vector<int>& objects, const std::vector<Transform>& starts, PivotMode mode) {
  const Transform pivot = pivot_of(starts, mode);
  return Drag{objects, starts, pivot, pivot};
}

std::vector<Transform> placements(const Drag& drag) {
  std::vector<Transform> out;
  out.reserve(drag.starts.size());
  for (const Transform& start : drag.starts) out.push_back(follow_pivot(start, drag.pivot_start, drag.pivot_now));
  return out;
}

}  // namespace redgem::group_transform
