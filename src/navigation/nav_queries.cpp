#include "navigation/nav_queries.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace redgem {
namespace {

using Node = NavGrid::Node;

// Looks for the node nearest a point ring by ring of columns around the point's own.
class NearestSearch {
 public:
  NearestSearch(const NavGrid& grid, Vec3 point) : grid_(grid), point_(point) {}
  std::optional<Node> run(int rings);

 private:
  void consider(NavCell cell);
  void scan_ring(NavCell center, int ring);
  // Whether no column past this ring can hold a nearer node.
  bool settled(int ring) const;

  const NavGrid& grid_;
  Vec3 point_;
  std::optional<Node> best_;
  float best_distance_ = std::numeric_limits<float>::max();  // squared
};

void NearestSearch::consider(NavCell cell) {
  const std::optional<int> column = grid_.column_of_cell(cell);
  if (!column) return;
  const auto [first, last] = grid_.nodes_in(*column);
  for (Node node = first; node < last; ++node) {
    const Vec3 offset = grid_.position(node) - point_;
    const float distance = dot(offset, offset);
    if (distance >= best_distance_) continue;
    best_ = node;
    best_distance_ = distance;
  }
}

void NearestSearch::scan_ring(NavCell center, int ring) {
  if (ring == 0) return consider(center);
  for (int side = -ring; side <= ring; ++side) {
    consider({center.x + side, center.z - ring});
    consider({center.x + side, center.z + ring});
  }
  for (int side = 1 - ring; side < ring; ++side) {
    consider({center.x - ring, center.z + side});
    consider({center.x + ring, center.z + side});
  }
}

bool NearestSearch::settled(int ring) const {
  const float closest_beyond = (static_cast<float>(ring) + kHalf) * grid_.shape().cell_size;
  return best_.has_value() && best_distance_ <= closest_beyond * closest_beyond;
}

std::optional<Node> NearestSearch::run(int rings) {
  const NavCell center = grid_.nearest_cell(point_.x, point_.z);
  for (int ring = 0; ring <= rings; ++ring) {
    scan_ring(center, ring);
    if (settled(ring)) break;
  }
  if (best_distance_ > nav::kNearestReach * nav::kNearestReach) return std::nullopt;
  return best_;
}

int rings_within_reach(const NavGrid& grid) {
  const auto reach = static_cast<int>(std::ceil(nav::kNearestReach / grid.shape().cell_size));
  return std::min(reach, nav::kMaxNearestRings);
}

// Whether a point stands on ground at `height`: at most `headroom` above it, at most a step below.
bool stands_on(float height, Vec3 point, float headroom, float max_step) {
  return height <= point.y + max_step && height >= point.y - headroom;
}

}  // namespace

std::optional<NavGrid::Node> nearest_nav_node(const NavGrid& grid, Vec3 point) {
  if (grid.empty()) return std::nullopt;
  return NearestSearch(grid, point).run(rings_within_reach(grid));
}

bool on_nav_grid(const NavGrid& grid, Vec3 point, float headroom) {
  const std::optional<int> column = grid.column_at(point.x, point.z);
  if (!column) return false;
  const auto [first, last] = grid.nodes_in(*column);
  for (Node node = first; node < last; ++node) {
    if (stands_on(grid.height(node), point, headroom, grid.shape().max_step)) return true;
  }
  return false;
}

}  // namespace redgem
