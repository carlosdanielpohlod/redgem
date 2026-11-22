#include "scene/triangle_index.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace redgem {
namespace {

constexpr std::uint32_t kLeafTriangles = 8;  // at most, in a box of the tree's last level
constexpr std::size_t kTriangleCorners = 3;
constexpr std::size_t kPositionFloats = 3;
constexpr int kAxisX = 0;
constexpr int kAxisY = 1;
constexpr int kAxisZ = 2;

// Whether a box is made of finite numbers (a corner that is not would unsettle the sorting).
bool finite(const Bounds& box) {
  const bool low = std::isfinite(box.min.x) && std::isfinite(box.min.y) && std::isfinite(box.min.z);
  return low && std::isfinite(box.max.x) && std::isfinite(box.max.y) && std::isfinite(box.max.z);
}

float on_axis(Vec3 point, int axis) {
  if (axis == kAxisX) return point.x;
  return axis == kAxisY ? point.y : point.z;
}

int longest_axis(const Bounds& bounds) {
  const Vec3 extent = bounds.max - bounds.min;
  if (extent.x >= extent.y && extent.x >= extent.z) return kAxisX;
  return extent.y >= extent.z ? kAxisY : kAxisZ;
}

}  // namespace

std::size_t listed_corners(const PrimitiveDesc& primitive) {
  return primitive.indices.empty() ? primitive.positions.size() / kPositionFloats : primitive.indices.size();
}

Vec3 listed_corner(const PrimitiveDesc& primitive, std::size_t listed) {
  const std::size_t vertex = primitive.indices.empty() ? listed : primitive.indices[listed];
  return vec3_from(&primitive.positions[vertex * kPositionFloats]);
}

bool boxes_overlap(const Bounds& a, const Bounds& b) {
  const bool across = a.min.x <= b.max.x && b.min.x <= a.max.x;
  const bool along = a.min.z <= b.max.z && b.min.z <= a.max.z;
  return across && along && a.min.y <= b.max.y && b.min.y <= a.max.y;
}

TriangleIndex::TriangleIndex(const PrimitiveDesc& primitive) {
  const std::size_t count = listed_corners(primitive) / kTriangleCorners;
  Sorting sorting;
  sorting.boxes.resize(count);
  sorting.middles.resize(count);
  for (std::size_t triangle = 0; triangle < count; ++triangle) {
    Bounds& box = sorting.boxes[triangle];
    for (std::size_t corner = 0; corner < kTriangleCorners; ++corner) {
      box.include(listed_corner(primitive, triangle * kTriangleCorners + corner));
    }
    sorting.middles[triangle] = box.center();
    if (finite(box)) order_.push_back(static_cast<std::uint32_t>(triangle));
  }
  if (!order_.empty()) build(sorting, 0, static_cast<std::uint32_t>(order_.size()));
}

// The node for the triangles order_[first, first + count), and those under it; returns where it is.
std::uint32_t TriangleIndex::build(Sorting& sorting, std::uint32_t first, std::uint32_t count) {
  const auto at = static_cast<std::uint32_t>(nodes_.size());
  nodes_.push_back({bounds_of(sorting, first, count), first, count, 0});
  if (count <= kLeafTriangles) return at;
  nodes_[at].count = 0;
  split(sorting, nodes_[at].bounds, first, count);
  const std::uint32_t half = count / 2;
  build(sorting, first, half);  // right after this node
  const std::uint32_t second = build(sorting, first + half, count - half);
  nodes_[at].second = second;
  return at;
}

Bounds TriangleIndex::bounds_of(const Sorting& sorting, std::uint32_t first, std::uint32_t count) const {
  Bounds bounds;
  for (std::uint32_t at = first; at < first + count; ++at) {
    bounds.include(sorting.boxes[order_[at]].min);
    bounds.include(sorting.boxes[order_[at]].max);
  }
  return bounds;
}

// Puts the half of the triangles whose middles come first along the box's longest axis first.
void TriangleIndex::split(Sorting& sorting, const Bounds& bounds, std::uint32_t first, std::uint32_t count) {
  const int axis = longest_axis(bounds);
  const auto start = order_.begin() + first;
  const auto earlier = [&](std::uint32_t a, std::uint32_t b) {
    return on_axis(sorting.middles[a], axis) < on_axis(sorting.middles[b], axis);
  };
  std::nth_element(start, start + count / 2, start + count, earlier);
}

void TriangleIndex::find(const Bounds& box, std::vector<std::size_t>& out) const {
  if (nodes_.empty()) return;
  std::vector<std::uint32_t> pending{0};
  while (!pending.empty()) {
    const std::uint32_t at = pending.back();
    pending.pop_back();
    const Node& node = nodes_[at];
    if (!boxes_overlap(node.bounds, box)) continue;
    if (node.count > 0) {
      append_leaf(node, out);
    } else {
      pending.push_back(node.second);
      pending.push_back(at + 1);
    }
  }
}

void TriangleIndex::append_leaf(const Node& leaf, std::vector<std::size_t>& out) const {
  for (std::uint32_t listed = leaf.first; listed < leaf.first + leaf.count; ++listed) {
    out.push_back(order_[listed] * kTriangleCorners);
  }
}

bool TriangleIndexCache::Kept::made_of(const PrimitiveDesc& primitive) const {
  const bool same_positions = primitive.positions.data() == positions && primitive.positions.size() == floats;
  return same_positions && primitive.indices.data() == indices && primitive.indices.size() == corners;
}

const TriangleIndex* TriangleIndexCache::of(const PrimitiveDesc& primitive) {
  if (listed_corners(primitive) / kTriangleCorners < kLeastIndexedTriangles) return nullptr;
  const auto found = kept_.find(&primitive);
  if (found != kept_.end() && found->second.made_of(primitive)) return &found->second.index;
  Kept made{primitive.positions.data(), primitive.positions.size(), primitive.indices.data(), primitive.indices.size(),
            TriangleIndex(primitive)};
  return &kept_.insert_or_assign(&primitive, std::move(made)).first->second.index;
}

}  // namespace redgem
