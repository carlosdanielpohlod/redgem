#include "editor/scene_overlays.hpp"

#include <array>
#include <cmath>
#include <algorithm>
#include <vector>
#include <tuple>

#include "render/debug_draw.hpp"

namespace redgem {
namespace {
// Allow shared mesh edges to fill the buffer; append trims disconnected edges at capacity.
constexpr std::size_t kTrianglesPerCommand = 2;
constexpr Color kStatic{0.63f, 0.71f, 0.78f, 1};
constexpr Color kDynamic{0.31f, 0.94f, 0.47f, 1};
constexpr Color kKinematic{0.39f, 0.63f, 1, 1};
constexpr Color kCharacter{0.31f, 0.9f, 1, 1};
constexpr Color kTrigger{1, 0.71f, 0.24f, 1};
using Point = std::array<float, config::kFloatsPerPosition>;
using Edge = std::tuple<ObjectId, Point, Point>;
using ColoredEdge = std::pair<Edge, Color>;
using Edges = std::vector<ColoredEdge>;
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
bool finite_corner(Vec3 corner) {
  return std::isfinite(corner.x) && std::isfinite(corner.y) && std::isfinite(corner.z);
}
bool finite_triangle(const PhysicsDebugTriangle& triangle) {
  return std::all_of(triangle.corners.begin(), triangle.corners.end(), finite_corner);
}
void collect_triangle(Edges& edges, const PhysicsDebugTriangle& triangle) {
  if (!finite_triangle(triangle)) return;
  for (std::size_t corner = 0; corner < kDebugTriangleCorners; ++corner) {
    const auto first = triangle.corners[corner];
    const auto second = triangle.corners[(corner + 1) % kDebugTriangleCorners];
    edges.emplace_back(edge_key(triangle.object, first, second), collider_color(triangle));
  }
}
bool edge_before(const ColoredEdge& a, const ColoredEdge& b) { return a.first < b.first; }
bool same_edge(const ColoredEdge& a, const ColoredEdge& b) { return a.first == b.first; }
Edges unique_edges(const PhysicsDebugGeometry& geometry) {
  Edges edges;
  edges.reserve(geometry.triangles.size() * kDebugTriangleCorners);
  for (const auto& triangle : geometry.triangles) collect_triangle(edges, triangle);
  std::sort(edges.begin(), edges.end(), edge_before);
  edges.erase(std::unique(edges.begin(), edges.end(), same_edge), edges.end());
  return edges;
}
Vec3 vector_of(const Point& point) {
  const auto& [x, y, z] = point;
  return {x, y, z};
}
bool append_edge(DebugDrawBuffer& commands, const ColoredEdge& edge) {
  const auto& [object, first, second] = edge.first;
  return commands.append({DebugDrawItem::Kind::Line, vector_of(first), vector_of(second), edge.second, false});
}
}  // namespace
std::size_t physics_debug_triangle_budget(const DebugDrawBuffer& commands) {
  const auto remaining = kMaxDebugDrawCommands - commands.items().size();
  return std::min(kMaxPhysicsDebugTriangles, std::max(std::size_t{1}, remaining * kTrianglesPerCommand));
}
bool append_physics_colliders(DebugDrawBuffer& commands, const PhysicsDebugGeometry& geometry) {
  for (const auto& edge : unique_edges(geometry))
    if (!append_edge(commands, edge)) return false;
  return !geometry.truncated;
}
}  // namespace redgem
