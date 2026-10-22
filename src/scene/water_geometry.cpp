#include "scene/water_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

constexpr std::size_t kWaterBoundsCorners = 8;  // all combinations of each axis's minimum and maximum
constexpr std::array<Vec3, kWaterBoundsCorners> kBoundsCornerDirections = {
    Vec3{-1.0f, -1.0f, -1.0f}, Vec3{-1.0f, -1.0f, 1.0f}, Vec3{-1.0f, 1.0f, -1.0f}, Vec3{-1.0f, 1.0f, 1.0f},
    Vec3{1.0f, -1.0f, -1.0f},  Vec3{1.0f, -1.0f, 1.0f},  Vec3{1.0f, 1.0f, -1.0f},  Vec3{1.0f, 1.0f, 1.0f}};
constexpr std::size_t kWaterBvhLeafTriangles = 8;  // narrowphase only visits small overlapping leaves
constexpr std::size_t kPositionChannels = 3;
constexpr float kFootprintEpsilon = 1e-6f;  // squared meters: degenerate edges have no useful direction

Vec3 horizontal(Vec3 p) { return {p.x, 0.0f, p.z}; }
Vec3 world_point(Vec3 p, const Transform& world) {
  return world.position + rotate(world.rotation, hadamard(p, world.scale));
}
Vec3 local_corner(const Bounds& bounds, Vec3 direction) {
  return {direction.x > 0.0f ? bounds.max.x : bounds.min.x, direction.y > 0.0f ? bounds.max.y : bounds.min.y,
          direction.z > 0.0f ? bounds.max.z : bounds.min.z};
}

float cross_xz(Vec3 a, Vec3 b) { return a.x * b.z - a.z * b.x; }

bool water_primitive(const PrimitiveDesc& primitive, const SceneDesc& desc) {
  if (!is_index_of(primitive.material, desc.materials.size())) return false;
  return desc.materials[static_cast<std::size_t>(primitive.material)].water.enabled;
}

bool eligible_surface(const Scene& scene, const GameObject& object, const SceneDesc& desc) {
  return object.visible && !object.pending_destroy && !scene.is_prefab(object.id) && object.skin == kNoIndex &&
         is_index_of(object.mesh, desc.meshes.size());
}

std::size_t triangle_indices(const PrimitiveDesc& primitive) {
  return primitive.indices.empty() ? primitive.positions.size() / kPositionChannels : primitive.indices.size();
}

std::optional<Vec3> triangle_point(const PrimitiveDesc& primitive, std::size_t at, const Transform& world) {
  const std::size_t index = primitive.indices.empty() ? at : primitive.indices[at];
  const std::size_t offset = index * kPositionChannels;
  if (offset + kPositionChannels > primitive.positions.size()) return std::nullopt;
  const Vec3 point = world_point(vec3_from(primitive.positions.data() + offset), world);
  return finite_water_point(point) ? std::optional<Vec3>(point) : std::nullopt;
}

std::optional<WaterTriangle> triangle_of(const PrimitiveDesc& primitive, std::size_t first, const Transform& world) {
  WaterTriangle triangle;
  for (std::size_t corner = 0; corner < kWaterTriangleCorners; ++corner) {
    const auto point = triangle_point(primitive, first + corner, world);
    if (!point) return std::nullopt;
    triangle.points[corner] = *point;
  }
  const float area = cross_xz(triangle.points[1] - triangle.points[0], triangle.points[2] - triangle.points[0]);
  return std::fabs(area) > kFootprintEpsilon ? std::optional<WaterTriangle>(triangle) : std::nullopt;
}

void append_triangles(WaterSurface& surface, const PrimitiveDesc& primitive, const Transform& world, std::size_t room) {
  const std::size_t indices = triangle_indices(primitive);
  if (indices / kWaterTriangleCorners > room) return;
  for (std::size_t first = 0; first + kWaterTriangleCorners <= indices; first += kWaterTriangleCorners) {
    const auto triangle = triangle_of(primitive, first, world);
    if (!triangle) continue;
    surface.triangles.push_back(*triangle);
    for (const Vec3 point : triangle->points) surface.bounds.include(point);
  }
}

bool horizontal_surface(const WaterSurface& surface) {
  if (!surface.bounds.valid) return false;
  return surface.bounds.max.y - surface.bounds.min.y <= kWaterPlaneTolerance;
}

bool separated_on(Vec3 axis, const WaterTriangle& triangle, const Bounds& body) {
  const float center = dot(horizontal(body.center()), axis);
  const Vec3 half = body.half_extents();
  const float radius = std::fabs(axis.x) * half.x + std::fabs(axis.z) * half.z;
  const float a = dot(horizontal(triangle.points[0]), axis);
  const float b = dot(horizontal(triangle.points[1]), axis);
  const float c = dot(horizontal(triangle.points[2]), axis);
  return std::max({a, b, c}) < center - radius || std::min({a, b, c}) > center + radius;
}

bool triangle_overlaps(const WaterTriangle& triangle, const Bounds& body) {
  if (separated_on(Vec3::right(), triangle, body)) return false;
  if (separated_on(Vec3::forward(), triangle, body)) return false;
  for (std::size_t i = 0; i < kWaterTriangleCorners; ++i) {
    const Vec3 edge = triangle.points[(i + 1) % kWaterTriangleCorners] - triangle.points[i];
    if (separated_on(Vec3{-edge.z, 0.0f, edge.x}, triangle, body)) return false;
  }
  return true;
}

Vec3 closest_on_edge(Vec3 point, Vec3 a, Vec3 b) {
  const Vec3 edge = horizontal(b - a);
  const float squared = dot(edge, edge);
  if (squared <= kFootprintEpsilon) return a;
  const float along = std::clamp(dot(horizontal(point - a), edge) / squared, 0.0f, 1.0f);
  return a + (b - a) * along;
}

bool inside_triangle(Vec3 p, const WaterTriangle& triangle) {
  const auto& v = triangle.points;
  const float a = cross_xz(v[1] - v[0], p - v[0]);
  const float b = cross_xz(v[2] - v[1], p - v[1]);
  const float c = cross_xz(v[0] - v[2], p - v[2]);
  const bool positive = a >= 0.0f && b >= 0.0f && c >= 0.0f;
  const bool negative = a <= 0.0f && b <= 0.0f && c <= 0.0f;
  return positive || negative;
}

Vec3 closest_in_triangle(Vec3 point, const WaterTriangle& triangle) {
  point.y = triangle.points[0].y;
  if (inside_triangle(point, triangle)) return point;
  Vec3 closest = triangle.points[0];
  float distance = length(horizontal(point - closest));
  for (std::size_t i = 0; i < kWaterTriangleCorners; ++i) {
    const Vec3 next = closest_on_edge(point, triangle.points[i], triangle.points[(i + 1) % kWaterTriangleCorners]);
    const float candidate = length(horizontal(point - next));
    if (candidate < distance) {
      closest = next;
      distance = candidate;
    }
  }
  return closest;
}

bool overlapping_bounds(const Bounds& water, const Bounds& body) {
  const bool across = water.min.x <= body.max.x && water.max.x >= body.min.x;
  const bool along = water.min.z <= body.max.z && water.max.z >= body.min.z;
  return across && along;
}

Bounds triangle_bounds(const WaterTriangle& triangle) {
  Bounds bounds;
  for (const auto point : triangle.points) bounds.include(point);
  return bounds;
}

Bounds node_bounds(const WaterSurface& surface, std::size_t first, std::size_t count) {
  Bounds bounds;
  for (std::size_t i = first; i < first + count; ++i) {
    for (const auto point : surface.triangles[i].points) bounds.include(point);
  }
  return bounds;
}

void split_triangles(WaterSurface& surface, const WaterBvhNode& node, std::size_t middle) {
  const Vec3 half = node.bounds.half_extents();
  const bool across = half.x > half.z;
  auto begin = surface.triangles.begin();
  std::nth_element(begin + static_cast<std::ptrdiff_t>(node.first), begin + static_cast<std::ptrdiff_t>(middle),
                   begin + static_cast<std::ptrdiff_t>(node.first + node.count),
                   [across](const auto& a, const auto& b) {
                     const Vec3 from = triangle_bounds(a).center();
                     const Vec3 to = triangle_bounds(b).center();
                     return across ? from.x < to.x : from.z < to.z;
                   });
}

std::size_t build_bvh(WaterSurface& surface, std::size_t first, std::size_t count) {
  const std::size_t index = surface.nodes.size();
  WaterBvhNode node{node_bounds(surface, first, count), first, count, 0, 0};
  surface.nodes.push_back(node);
  if (count <= kWaterBvhLeafTriangles) return index;
  const std::size_t middle = first + count / 2;
  split_triangles(surface, node, middle);
  node.left = build_bvh(surface, first, middle - first);
  node.right = build_bvh(surface, middle, first + count - middle);
  node.count = 0;  // internal nodes reference children instead of triangles
  surface.nodes[index] = node;
  return index;
}

void append_surface(std::vector<WaterSurface>& out, const Scene& scene, const SceneDesc& desc, ObjectId id,
                    std::size_t index, std::size_t& triangles) {
  const auto& primitive = desc.meshes[static_cast<std::size_t>(scene.at(id).mesh)].primitives[index];
  if (!water_primitive(primitive, desc)) return;
  WaterSurface surface{scene.handle(id), index, {}, {}, {}};
  append_triangles(surface, primitive, scene.world_transform(id), kMaxWaterTriangles - triangles);
  if (!horizontal_surface(surface)) return;
  triangles += surface.triangles.size();
  build_bvh(surface, 0, surface.triangles.size());
  out.push_back(std::move(surface));
}

void append_surfaces(std::vector<WaterSurface>& out, const Scene& scene, const SceneDesc& desc, ObjectId id,
                     std::size_t& triangles) {
  const GameObject& object = scene.at(id);
  if (!eligible_surface(scene, object, desc)) return;
  const auto& primitives = desc.meshes[static_cast<std::size_t>(object.mesh)].primitives;
  for (std::size_t index = 0; index < primitives.size(); ++index) {
    if (out.size() >= kMaxWaterSurfaces) return;
    append_surface(out, scene, desc, id, index, triangles);
  }
}

struct SweepInterval {
  float enter = 0.0f;
  float leave = 1.0f;
};

bool clip_sweep(float start, float movement, float low, float high, SweepInterval& interval) {
  if (std::fabs(movement) <= kFootprintEpsilon) return start >= low && start <= high;
  const float a = (low - start) / movement;
  const float b = (high - start) / movement;
  interval.enter = std::max(interval.enter, std::min(a, b));
  interval.leave = std::min(interval.leave, std::max(a, b));
  return interval.enter <= interval.leave;
}

bool clip_axis(Vec3 axis, const WaterTriangle& triangle, const Bounds& before, const Bounds& after,
               SweepInterval& interval) {
  const Vec3 a = before.half_extents();
  const Vec3 b = after.half_extents();
  const Vec3 half{std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
  const float radius = dot(Vec3{std::fabs(axis.x), std::fabs(axis.y), std::fabs(axis.z)}, half);
  const float first = dot(triangle.points[0], axis);
  const float second = dot(triangle.points[1], axis);
  const float third = dot(triangle.points[2], axis);
  return clip_sweep(dot(before.center(), axis), dot(after.center() - before.center(), axis),
                    std::min({first, second, third}) - radius, std::max({first, second, third}) + radius, interval);
}

std::optional<float> triangle_sweep(const WaterTriangle& triangle, const Bounds& before, const Bounds& after) {
  SweepInterval interval;
  for (const Vec3 axis : {Vec3::right(), Vec3::up(), Vec3::forward()}) {
    if (!clip_axis(axis, triangle, before, after, interval)) return std::nullopt;
  }
  for (std::size_t i = 0; i < kWaterTriangleCorners; ++i) {
    const Vec3 edge = triangle.points[(i + 1) % kWaterTriangleCorners] - triangle.points[i];
    if (!clip_axis({-edge.z, 0.0f, edge.x}, triangle, before, after, interval)) return std::nullopt;
  }
  return interval.enter;
}

std::optional<Vec3> contact_in_node(const WaterSurface& surface, std::size_t index, const Bounds& body) {
  const auto& node = surface.nodes[index];
  if (!overlapping_bounds(node.bounds, body)) return std::nullopt;
  if (node.count == 0) {
    const auto left = contact_in_node(surface, node.left, body);
    return left ? left : contact_in_node(surface, node.right, body);
  }
  for (std::size_t i = node.first; i < node.first + node.count; ++i) {
    const auto& triangle = surface.triangles[i];
    if (triangle_overlaps(triangle, body)) return closest_in_triangle(body.center(), triangle);
  }
  return std::nullopt;
}

std::optional<WaterSweep> earliest_sweep(const std::optional<WaterSweep>& a, const std::optional<WaterSweep>& b) {
  if (!a) return b;
  if (!b) return a;
  return a->along < b->along ? a : b;
}

std::optional<WaterSweep> sweep_leaf(const WaterSurface& surface, const WaterBvhNode& node, const Bounds& before,
                                     const Bounds& after) {
  std::optional<WaterSweep> first;
  for (std::size_t i = node.first; i < node.first + node.count; ++i) {
    const auto& triangle = surface.triangles[i];
    const auto along = triangle_sweep(triangle, before, after);
    if (!along) continue;
    const Vec3 center = before.center() + (after.center() - before.center()) * *along;
    first = earliest_sweep(first, WaterSweep{closest_in_triangle(center, triangle), *along});
  }
  return first;
}

std::optional<WaterSweep> sweep_in_node(const WaterSurface& surface, std::size_t index, const Bounds& before,
                                        const Bounds& after, const Bounds& swept) {
  const auto& node = surface.nodes[index];
  if (!overlapping_bounds(node.bounds, swept)) return std::nullopt;
  if (node.count == 0)
    return earliest_sweep(sweep_in_node(surface, node.left, before, after, swept),
                          sweep_in_node(surface, node.right, before, after, swept));
  return sweep_leaf(surface, node, before, after);
}

bool same_transform(const Transform& a, const Transform& b) {
  const bool position = a.position.x == b.position.x && a.position.y == b.position.y && a.position.z == b.position.z;
  const bool scale = a.scale.x == b.scale.x && a.scale.y == b.scale.y && a.scale.z == b.scale.z;
  const bool rotation = a.rotation.x == b.rotation.x && a.rotation.y == b.rotation.y && a.rotation.z == b.rotation.z &&
                        a.rotation.w == b.rotation.w;
  return position && scale && rotation;
}

bool object_has_water(const Scene& scene, const SceneDesc& desc, const GameObject& object) {
  if (!eligible_surface(scene, object, desc)) return false;
  const auto& primitives = desc.meshes[static_cast<std::size_t>(object.mesh)].primitives;
  return std::any_of(primitives.begin(), primitives.end(), [&desc](const auto& p) { return water_primitive(p, desc); });
}

}  // namespace

bool finite_water_point(Vec3 point) {
  const bool finite = std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
  const float largest = std::max({std::fabs(point.x), std::fabs(point.y), std::fabs(point.z)});
  return finite && largest <= kWaterMaxCoordinate;
}

Bounds water_world_bounds(const Bounds& local, const Transform& world) {
  Bounds bounds;
  if (!local.valid) return bounds;
  for (const Vec3 direction : kBoundsCornerDirections) {
    const Vec3 point = world_point(local_corner(local, direction), world);
    if (!finite_water_point(point)) return Bounds{};
    bounds.include(point);
  }
  return bounds;
}

std::vector<WaterSurface> water_surfaces(const Scene& scene, const SceneDesc& desc) {
  std::vector<WaterSurface> surfaces;
  std::size_t triangles = 0;
  std::size_t objects = 0;
  for (const ObjectId id : scene.alive_ids()) {
    if (!object_has_water(scene, desc, scene.at(id))) continue;
    if (objects++ >= kMaxWaterSurfaceObjects) break;
    append_surfaces(surfaces, scene, desc, id, triangles);
  }
  return surfaces;
}

std::optional<Vec3> water_contact_point(const WaterSurface& surface, const Bounds& body) {
  if (!body.valid || surface.nodes.empty()) return std::nullopt;
  return contact_in_node(surface, 0, body);
}

std::optional<WaterSweep> water_swept_contact(const WaterSurface& surface, const Bounds& before, const Bounds& after) {
  if (surface.nodes.empty()) return std::nullopt;
  Bounds swept = before;
  swept.include(after.min);
  swept.include(after.max);
  if (swept.max.y < surface.bounds.min.y || swept.min.y > surface.bounds.max.y) return std::nullopt;
  return sweep_in_node(surface, 0, before, after, swept);
}

std::vector<WaterGeometryCache::Key> WaterGeometryCache::collect_keys(const Scene& scene, const SceneDesc& desc) {
  std::vector<Key> keys;
  for (const ObjectId id : scene.alive_ids()) {
    const auto& object = scene.at(id);
    if (keys.size() >= kMaxWaterSurfaceObjects) break;
    if (object_has_water(scene, desc, object))
      keys.push_back({scene.handle(id), object.mesh, scene.world_transform(id)});
  }
  return keys;
}

const std::vector<WaterSurface>& WaterGeometryCache::refresh(const Scene& scene, const SceneDesc& desc) {
  auto keys = collect_keys(scene, desc);
  const bool same = std::equal(keys.begin(), keys.end(), keys_.begin(), keys_.end(), [](const Key& a, const Key& b) {
    return a.object == b.object && a.mesh == b.mesh && same_transform(a.world, b.world);
  });
  if (!same) {
    keys_ = std::move(keys);
    surfaces_ = water_surfaces(scene, desc);
  }
  return surfaces_;
}

}  // namespace redgem
