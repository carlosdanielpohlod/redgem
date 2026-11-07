// Optional editor view of the shapes the simulation actually uses, at their current poses.
#include "physics/jolt/jolt_physics_world.hpp"

#include <algorithm>
#include <array>

namespace redgem::jolt {
namespace {
constexpr int kBatchTriangles = JPH::Shape::cGetTrianglesMinTrianglesRequested;
constexpr std::size_t kBatchVertices = kBatchTriangles * kDebugTriangleCorners;
struct DebugShape {
  JPH::RefConst<JPH::Shape> shape;
  JPH::Vec3 center = JPH::Vec3::sZero();
  JPH::Quat rotation = JPH::Quat::sIdentity();
};
DebugShape trigger_shape(const Record& record) {
  const auto rotation = to_jolt(record.pose.rotation);
  // Trigger overlap queries use the stored pose as their center-of-mass transform.
  return {record.shape, to_jolt(record.pose.position), rotation};
}
std::optional<DebugShape> shape_of(const Record& record, const JPH::BodyInterface& bodies) {
  if (record.trigger) return trigger_shape(record);
  if (is_character(&record)) {
    const auto& character = *record.character;
    return DebugShape{character.GetShape(), JPH::Vec3(character.GetCenterOfMassPosition()), character.GetRotation()};
  }
  if (!has_body(&record)) return std::nullopt;
  return DebugShape{bodies.GetShape(record.body), JPH::Vec3(bodies.GetCenterOfMassPosition(record.body)),
                    bodies.GetRotation(record.body)};
}
Vec3 point(const JPH::Float3& vertex) { return {vertex.x, vertex.y, vertex.z}; }
void set_corners(PhysicsDebugTriangle& triangle, const JPH::Float3* vertices) {
  for (std::size_t corner = 0; corner < kDebugTriangleCorners; ++corner)
    triangle.corners[corner] = point(vertices[corner]);
}
bool append_batch(PhysicsDebugGeometry& geometry, const PhysicsDebugTriangle& metadata,
                  const std::array<JPH::Float3, kBatchVertices>& vertices, int count, std::size_t limit) {
  for (int index = 0; index < count; ++index) {
    if (geometry.triangles.size() == limit) {
      geometry.truncated = true;
      return false;
    }
    auto triangle = metadata;
    set_corners(triangle, &vertices[static_cast<std::size_t>(index) * kDebugTriangleCorners]);
    geometry.triangles.push_back(triangle);
  }
  return true;
}
void collect(const DebugShape& shape, const PhysicsDebugTriangle& metadata, std::size_t limit,
             PhysicsDebugGeometry& geometry) {
  JPH::Shape::GetTrianglesContext context;
  shape.shape->GetTrianglesStart(context, JPH::AABox::sBiggest(), shape.center, shape.rotation,
                                 JPH::Vec3::sReplicate(kUnitScale));
  std::array<JPH::Float3, kBatchVertices> vertices{};
  for (;;) {
    const int count = shape.shape->GetTrianglesNext(context, kBatchTriangles, vertices.data());
    if (count == 0) return;
    if (!append_batch(geometry, metadata, vertices, count, limit)) return;
  }
}
struct Limits {
  std::size_t total = 0;
  std::size_t per_collider = 0;
};
bool collect_record(ObjectId object, const Record& record, const JPH::BodyInterface& bodies, Limits limits,
                    PhysicsDebugGeometry& geometry) {
  if (geometry.triangles.size() == limits.total) {
    geometry.truncated = true;
    return false;
  }
  const auto shape = shape_of(record, bodies);
  if (!shape) return true;
  const auto end = std::min(limits.total, geometry.triangles.size() + limits.per_collider);
  collect(*shape, PhysicsDebugTriangle{object, record.type, record.trigger, {}}, end, geometry);
  return true;
}
}  // namespace
PhysicsDebugGeometry World::debug_geometry(std::size_t triangle_limit) const {
  PhysicsDebugGeometry geometry;
  if (records_.empty()) return geometry;
  const auto limit = std::min(triangle_limit, kMaxPhysicsDebugTriangles);
  const auto quota = std::min(kMaxDebugTrianglesPerCollider, std::max(std::size_t{1}, limit / records_.size()));
  for (const auto& [object, record] : records_) {
    if (!collect_record(object, record, system_.GetBodyInterface(), Limits{limit, quota}, geometry)) break;
  }
  return geometry;
}
}  // namespace redgem::jolt
