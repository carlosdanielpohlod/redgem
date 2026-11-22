#pragma once

// A primitive's triangles sorted into a tree of boxes (a bounding volume hierarchy), to find the few
// a small box touches without walking all of them: a decal on a dense terrain cuts only the
// triangles near it.

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "scene/scene_desc.hpp"

namespace redgem {

// How many corners a primitive's triangles list (three a triangle): its indices, or its positions
// in order for none.
std::size_t listed_corners(const PrimitiveDesc& primitive);
// The position of the corner listed at `listed`, in the primitive's own space.
Vec3 listed_corner(const PrimitiveDesc& primitive, std::size_t listed);

class TriangleIndex {
 public:
  // Sorts the triangles `primitive` lists, in its own space, leaving out any with a corner that is
  // not a finite number. They are not kept: the index stands for the primitive as it was then.
  explicit TriangleIndex(const PrimitiveDesc& primitive);

  // Appends the first listed corner (the index into the primitive's corner list, a multiple of
  // three) of each triangle whose box overlaps `box`. Some of them may only come near it.
  void find(const Bounds& box, std::vector<std::size_t>& out) const;

  std::size_t triangles() const { return order_.size(); }

 private:
  // A box around some triangles: a leaf lists them (order_[first, first + count)), and a branch
  // has its two halves next to it and at `second`.
  struct Node {
    Bounds bounds;
    std::uint32_t first = 0;
    std::uint32_t count = 0;  // 0: a branch
    std::uint32_t second = 0;
  };
  // The boxes and middles of the triangles as they are being sorted.
  struct Sorting {
    std::vector<Bounds> boxes;
    std::vector<Vec3> middles;
  };

  std::uint32_t build(Sorting& sorting, std::uint32_t first, std::uint32_t count);
  Bounds bounds_of(const Sorting& sorting, std::uint32_t first, std::uint32_t count) const;
  void split(Sorting& sorting, const Bounds& bounds, std::uint32_t first, std::uint32_t count);
  void append_leaf(const Node& leaf, std::vector<std::size_t>& out) const;

  std::vector<Node> nodes_;
  std::vector<std::uint32_t> order_;  // the triangles (as numbers: the first is 0) leaf by leaf
};

// A primitive listing fewer triangles than this is walked whole: cheaper than sorting it.
inline constexpr std::size_t kLeastIndexedTriangles = 512;

// The indexes of a scene's primitives, each made the first time it is asked for and kept until
// forgotten (when the scene goes). One is made anew for a primitive whose positions or list of
// corners were replaced; one changed in place is not noticed.
class TriangleIndexCache {
 public:
  // Null for a primitive small enough to walk whole.
  const TriangleIndex* of(const PrimitiveDesc& primitive);
  void forget() { kept_.clear(); }

 private:
  struct Kept {
    const float* positions = nullptr;
    std::size_t floats = 0;
    const std::uint32_t* indices = nullptr;
    std::size_t corners = 0;  // listed by its indices
    TriangleIndex index;
    // Whether it was made of the primitive as it is now (its lists not replaced since).
    bool made_of(const PrimitiveDesc& primitive) const;
  };
  std::unordered_map<const PrimitiveDesc*, Kept> kept_;
};

// Whether two boxes overlap (touching counts).
bool boxes_overlap(const Bounds& a, const Bounds& b);

}  // namespace redgem
