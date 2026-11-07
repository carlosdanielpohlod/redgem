#include "editor/scene_overlays.hpp"

#include <array>
#include <set>
#include <tuple>

#include "render/debug_draw.hpp"

namespace redgem {
namespace {
constexpr Color kStatic{0.63f, 0.71f, 0.78f, 1};
constexpr Color kDynamic{0.31f, 0.94f, 0.47f, 1};
constexpr Color kKinematic{0.39f, 0.63f, 1, 1};
constexpr Color kCharacter{0.31f, 0.9f, 1, 1};
constexpr Color kTrigger{1, 0.71f, 0.24f, 1};
using Point = std::array<float, config::kFloatsPerPosition>;
using Edge = std::tuple<ObjectId, Point, Point>;
using Edges = std::set<Edge>;
Color collider_color(const PhysicsDebugTriangle& triangle) {
  if (triangle.trigger) return kTrigger;
  switch (triangle.type) {
    case BodyType::Dynamic:
    case BodyType::Vehicle:
      return kDynamic;
    case BodyType::Kinematic:
      return kKinematic;
    case BodyType::Character:
      return kCharacter;
    default:
      return kStatic;
  }
}
Edge edge_key(ObjectId object, Vec3 first, Vec3 second) {
  Point a{first.x, first.y, first.z};
  Point b{second.x, second.y, second.z};
  return a < b ? Edge{object, a, b} : Edge{object, b, a};
}
bool append_edge(DebugDrawBuffer& commands, const PhysicsDebugTriangle& triangle, std::size_t corner, Edges& edges) {
  const auto first = triangle.corners[corner];
  const auto second = triangle.corners[(corner + 1) % kDebugTriangleCorners];
  if (!edges.insert(edge_key(triangle.object, first, second)).second) return true;
  return commands.append({DebugDrawItem::Kind::Line, first, second, collider_color(triangle), false});
}
bool append_triangle(DebugDrawBuffer& commands, const PhysicsDebugTriangle& triangle, Edges& edges) {
  for (std::size_t corner = 0; corner < kDebugTriangleCorners; ++corner)
    if (!append_edge(commands, triangle, corner, edges)) return false;
  return true;
}
}  // namespace
bool append_physics_colliders(DebugDrawBuffer& commands, const PhysicsDebugGeometry& geometry) {
  Edges edges;
  for (const auto& triangle : geometry.triangles)
    if (!append_triangle(commands, triangle, edges)) return false;
  return !geometry.truncated;
}
}  // namespace redgem
