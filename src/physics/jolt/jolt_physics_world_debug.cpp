// Optional editor view of the shapes the simulation actually uses, at their current poses.
#include "physics/jolt/jolt_physics_world.hpp"
#include "physics/jolt/jolt_debug_geometry.hpp"

#include <algorithm>
#include <array>
#include <Jolt/Physics/Collision/TransformedShape.h>

namespace redgem::jolt {
namespace {
constexpr int kBatchTriangles = JPH::Shape::cGetTrianglesMinTrianglesRequested;
constexpr std::size_t kBatchVertices = kBatchTriangles * kDebugTriangleCorners;
std::optional<JPH::TransformedShape> shape_of(const Record& record, const JPH::BodyInterface& bodies) {
  if (record.trigger)
    return JPH::TransformedShape(JPH::RVec3(to_jolt(record.pose.position)), to_jolt(record.pose.rotation),
                                 record.shape.GetPtr(), JPH::BodyID());
  if (is_character(&record)) return record.character->GetTransformedShape();
  if (!has_body(&record)) return std::nullopt;
  return bodies.GetTransformedShape(record.body);
}
Vec3 point(const JPH::Float3& vertex) { return {vertex.x, vertex.y, vertex.z}; }
void set_corners(PhysicsDebugTriangle& triangle, const JPH::Float3* vertices) {
  for (std::size_t corner = 0; corner < kDebugTriangleCorners; ++corner)
    triangle.corners[corner] = point(vertices[corner]);
}
bool append_batch(PhysicsDebugGeometry& geometry, const PhysicsDebugTriangle& metadata, const JPH::Float3* vertices,
                  int count, std::size_t limit) {
  for (int index = 0; index < count; ++index) {
    if (geometry.triangles.size() == limit) {
      return false;
    }
    auto triangle = metadata;
    set_corners(triangle, &vertices[static_cast<std::size_t>(index) * kDebugTriangleCorners]);
    geometry.triangles.push_back(triangle);
  }
  return true;
}
bool append_after_skip(PhysicsDebugGeometry& geometry, const PhysicsDebugTriangle& metadata,
                       const std::array<JPH::Float3, kBatchVertices>& vertices, int count, std::size_t& skip,
                       std::size_t limit) {
  const auto skipped = std::min(skip, static_cast<std::size_t>(count));
  skip -= skipped;
  const auto remaining = count - static_cast<int>(skipped);
  return append_batch(geometry, metadata, vertices.data() + skipped * kDebugTriangleCorners, remaining, limit);
}
}  // namespace
bool append_debug_shape(const JPH::TransformedShape& shape, const PhysicsDebugTriangle& metadata, std::size_t skip,
                        std::size_t limit, PhysicsDebugGeometry& geometry) {
  JPH::TransformedShape::GetTrianglesContext context;
  shape.GetTrianglesStart(context, JPH::AABox::sBiggest(), JPH::RVec3::sZero());
  std::array<JPH::Float3, kBatchVertices> vertices{};
  for (;;) {
    const int count = shape.GetTrianglesNext(context, kBatchTriangles, vertices.data());
    if (count == 0) return true;
    if (!append_after_skip(geometry, metadata, vertices, count, skip, limit)) return false;
  }
}
namespace {
struct Candidate {
  JPH::TransformedShape shape;
  PhysicsDebugTriangle metadata;
  std::size_t sampled = 0;
  bool complete = false;
};
std::vector<Candidate> candidates(const std::map<ObjectId, Record>& records, const JPH::BodyInterface& bodies) {
  std::vector<Candidate> result;
  result.reserve(records.size());
  for (const auto& [object, record] : records) {
    const auto shape = shape_of(record, bodies);
    if (shape) result.push_back(Candidate{*shape, {object, record.type, record.trigger, {}}, 0, false});
  }
  return result;
}
void first_pass(std::vector<Candidate>& candidates, std::size_t limit, PhysicsDebugGeometry& geometry) {
  const auto quota = std::min(kMaxDebugTrianglesPerCollider, std::max(std::size_t{1}, limit / candidates.size()));
  for (auto& candidate : candidates) {
    const auto start = geometry.triangles.size();
    const auto end = std::min(limit, start + quota);
    candidate.complete = append_debug_shape(candidate.shape, candidate.metadata, 0, end, geometry);
    candidate.sampled = geometry.triangles.size() - start;
  }
}
void remaining_pass(std::vector<Candidate>& candidates, std::size_t limit, PhysicsDebugGeometry& geometry) {
  for (auto& candidate : candidates) {
    if (candidate.complete) continue;
    if (geometry.triangles.size() == limit) {
      geometry.truncated = true;
      continue;
    }
    candidate.complete = append_debug_shape(candidate.shape, candidate.metadata, candidate.sampled, limit, geometry);
    if (!candidate.complete) geometry.truncated = true;
  }
}
}  // namespace
PhysicsDebugGeometry World::debug_geometry(std::size_t triangle_limit) const {
  PhysicsDebugGeometry geometry;
  if (triangle_limit == 0) return geometry;
  auto found = candidates(records_, system_.GetBodyInterface());
  if (found.empty()) return geometry;
  const auto limit = std::min(triangle_limit, kMaxPhysicsDebugTriangles);
  geometry.triangles.reserve(limit);
  first_pass(found, limit, geometry);
  remaining_pass(found, limit, geometry);
  return geometry;
}
}  // namespace redgem::jolt
