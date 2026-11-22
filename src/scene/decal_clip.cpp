#include "scene/decal_clip.hpp"

#include <cmath>
#include <cstddef>
#include <utility>

namespace redgem {
namespace {

// A surface this near to facing straight up or down has no upright for a picture: the world's
// forward is its up then.
constexpr float kNearlyVertical = 0.99f;
// A triangle turned further than this from the decal (the cosine of the angle between them) is
// left out: a wall's side at a hole in it, or the back of a thin board.
constexpr float kLeastFacing = 0.1f;
constexpr float kPictureMiddle = 0.5f;
// A box corner's number says which side of the center it is on along each axis: past it along
// `right` for this bit set, along `up` for the next, along `normal` for the one after.
constexpr std::size_t kRightBit = 1;
constexpr std::size_t kUpBit = 2;
constexpr std::size_t kNormalBit = 4;

// The polygon a triangle becomes as the box's faces cut it: at most a corner more per face.
using Polygon = std::vector<Vec3>;

// 1 for a corner past the center along the axis its bit stands for, -1 for one short of it.
float side_of(std::size_t corner, std::size_t bit) { return (corner & bit) != 0 ? 1.0f : -1.0f; }

Vec3 box_corner(const DecalBox& box, std::size_t corner) {
  return box.center + box.right * (side_of(corner, kRightBit) * box.half_size) +
         box.up * (side_of(corner, kUpBit) * box.half_size) +
         box.normal * (side_of(corner, kNormalBit) * box.half_depth);
}

// Where a point is along an axis of the box, from its center.
float along(const DecalBox& box, Vec3 point, Vec3 axis) { return dot(point - box.center, axis); }

// Keeps the part of a polygon on the inner side of one face of the box: where it is at most
// `limit` along `axis`.
Polygon clip_against(const Polygon& polygon, const DecalBox& box, Vec3 axis, float limit) {
  Polygon kept;
  for (std::size_t i = 0; i < polygon.size(); ++i) {
    const Vec3 from = polygon[i];
    const Vec3 to = polygon[(i + 1) % polygon.size()];
    const float from_out = along(box, from, axis) - limit;
    const float to_out = along(box, to, axis) - limit;
    if (from_out <= 0.0f) kept.push_back(from);
    if ((from_out <= 0.0f) != (to_out <= 0.0f)) kept.push_back(mix(from, to, from_out / (from_out - to_out)));
  }
  return kept;
}

Polygon clip_to_box(const DecalBox& box, Polygon polygon) {
  const std::array<std::pair<Vec3, float>, 6> faces{{{box.right, box.half_size},
                                                     {-box.right, box.half_size},
                                                     {box.up, box.half_size},
                                                     {-box.up, box.half_size},
                                                     {box.normal, box.half_depth},
                                                     {-box.normal, box.half_depth}}};
  for (const auto& [axis, limit] : faces) {
    if (polygon.empty()) break;
    polygon = clip_against(polygon, box, axis, limit);
  }
  return polygon;
}

bool faces_the_decal(const DecalBox& box, const DecalTriangle& triangle, bool either_side) {
  const Vec3 facing = triangle_facing(triangle);
  const float area = length(facing);
  const float toward = dot(facing, box.normal);
  return area > 0.0f && (either_side ? std::fabs(toward) : toward) > kLeastFacing * area;
}

DecalCorner corner_of(const DecalBox& box, Vec3 point) {
  const float across = box.half_size + box.half_size;
  return {point, along(box, point, box.right) / across + kPictureMiddle,
          along(box, point, box.up) / across + kPictureMiddle};
}

// A convex polygon as a fan of triangles from its first corner.
void append_fan(const DecalBox& box, const Polygon& polygon, std::vector<DecalCorner>& out) {
  for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
    for (const Vec3 point : {polygon[0], polygon[i], polygon[i + 1]}) out.push_back(corner_of(box, point));
  }
}

}  // namespace

Vec3 triangle_facing(const DecalTriangle& triangle) {
  return cross(triangle[kSecondCorner] - triangle[0], triangle[kThirdCorner] - triangle[0]);
}

DecalBox decal_box(Vec3 point, Vec3 normal, float size, float depth, float spin_radians) {
  const Vec3 out_of = normalize(normal);
  const Vec3 upright = std::fabs(out_of.y) > kNearlyVertical ? Vec3::forward() : Vec3::up();
  const Vec3 right = normalize(cross(upright, out_of));
  const Quat spin = quat_from_axis_angle(out_of, spin_radians);
  return {point, rotate(spin, right), rotate(spin, cross(out_of, right)), out_of, size * kHalf, depth * kHalf};
}

void clip_to_decal(const DecalBox& box, const DecalTriangle& triangle, std::vector<DecalCorner>& out,
                   bool either_side) {
  if (!faces_the_decal(box, triangle, either_side)) return;
  const Polygon polygon = clip_to_box(box, Polygon(triangle.begin(), triangle.end()));
  if (polygon.size() >= kTriangleCorners) append_fan(box, polygon, out);
}

std::array<Vec3, kBoxCorners> box_corners(const DecalBox& box) {
  std::array<Vec3, kBoxCorners> corners{};
  for (std::size_t corner = 0; corner < kBoxCorners; ++corner) corners[corner] = box_corner(box, corner);
  return corners;
}

}  // namespace redgem
