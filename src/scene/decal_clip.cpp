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
constexpr std::size_t kTriangleCorners = 3;
constexpr std::array<float, 2> kSides{-1.0f, 1.0f};  // of the box's center, along each axis

// The polygon a triangle becomes as the box's faces cut it: at most a corner more per face.
using Polygon = std::vector<Vec3>;

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

bool faces_the_decal(const DecalBox& box, const std::array<Vec3, 3>& triangle) {
  const Vec3 facing = cross(triangle[1] - triangle[0], triangle[2] - triangle[0]);
  const float area = length(facing);
  return area > 0.0f && dot(facing, box.normal) > kLeastFacing * area;
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

DecalBox decal_box(Vec3 point, Vec3 normal, float size, float depth, float spin_radians) {
  const Vec3 out_of = normalize(normal);
  const Vec3 upright = std::fabs(out_of.y) > kNearlyVertical ? Vec3::forward() : Vec3::up();
  const Vec3 right = normalize(cross(upright, out_of));
  const Quat spin = quat_from_axis_angle(out_of, spin_radians);
  return {point, rotate(spin, right), rotate(spin, cross(out_of, right)), out_of, size * kHalf, depth * kHalf};
}

void clip_to_decal(const DecalBox& box, const std::array<Vec3, 3>& triangle, std::vector<DecalCorner>& out) {
  if (!faces_the_decal(box, triangle)) return;
  const Polygon polygon = clip_to_box(box, Polygon(triangle.begin(), triangle.end()));
  if (polygon.size() >= kTriangleCorners) append_fan(box, polygon, out);
}

std::array<Vec3, 8> box_corners(const DecalBox& box) {
  std::array<Vec3, 8> corners{};
  std::size_t next = 0;
  for (const float x : kSides) {
    for (const float y : kSides) {
      for (const float z : kSides) {
        corners[next++] = box.center + box.right * (x * box.half_size) + box.up * (y * box.half_size) +
                          box.normal * (z * box.half_depth);
      }
    }
  }
  return corners;
}

}  // namespace redgem
