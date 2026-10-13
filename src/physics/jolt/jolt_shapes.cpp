#include "physics/jolt/jolt_shapes.hpp"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <cmath>

#include "physics/physics_config.hpp"
#include "scene/scene_config.hpp"

namespace redgem::jolt {
namespace {

Vec3 abs(Vec3 v) { return {std::fabs(v.x), std::fabs(v.y), std::fabs(v.z)}; }

Vec3 clamp_half_extents(Vec3 half) {
  const float min_half = config::kMinColliderHalfExtent;
  return {std::max(half.x, min_half), std::max(half.y, min_half), std::max(half.z, min_half)};
}

// A shape, or why it could not be made.
using ShapeResult = std::variant<JPH::RefConst<JPH::Shape>, std::string>;

// Where a triangle's corners are in a list of indices.
constexpr std::size_t kFirstCorner = 0;
constexpr std::size_t kSecondCorner = 1;
constexpr std::size_t kThirdCorner = 2;

ShapeResult finish(const JPH::ShapeSettings& settings) {
  JPH::ShapeSettings::ShapeResult result = settings.Create();
  if (result.HasError()) return std::string(result.GetError().c_str());
  return JPH::RefConst<JPH::Shape>(result.Get());
}

void add_scaled_vertices(const PrimitiveDesc& primitive, Vec3 scale, JPH::VertexList& vertices) {
  for (std::size_t i = 0; i + config::kFloatsPerPosition <= primitive.positions.size();
       i += config::kFloatsPerPosition) {
    const Vec3 p = hadamard(vec3_from(&primitive.positions[i]), scale);
    vertices.push_back(JPH::Float3(p.x, p.y, p.z));
  }
}

// `base` is where the primitive's vertices start in the collider's own list.
void add_triangles(const PrimitiveDesc& primitive, JPH::uint32 base, JPH::IndexedTriangleList& triangles) {
  for (std::size_t i = 0; i + config::kVerticesPerTriangle <= primitive.indices.size();
       i += config::kVerticesPerTriangle) {
    const std::uint32_t* corners = &primitive.indices[i];
    triangles.push_back(JPH::IndexedTriangle(base + corners[kFirstCorner], base + corners[kSecondCorner],
                                             base + corners[kThirdCorner]));
  }
}

ShapeResult build_mesh(const BodyDesc& desc) {
  if (desc.mesh == nullptr) return std::string("mesh collider requires a mesh");
  JPH::VertexList vertices;
  JPH::IndexedTriangleList triangles;
  for (const PrimitiveDesc& primitive : desc.mesh->primitives) {
    // (Indices that do not name the primitive's own vertices would make triangles out of nothing.)
    if (!primitive_shape_problem(primitive).empty()) continue;
    const auto base = static_cast<JPH::uint32>(vertices.size());
    add_scaled_vertices(primitive, desc.world.scale, vertices);
    add_triangles(primitive, base, triangles);
  }
  if (triangles.empty()) return std::string("mesh collider has no triangles");
  return finish(JPH::MeshShapeSettings(vertices, triangles));
}

ShapeResult box_shape(Vec3 half) {
  const float smallest = std::min({half.x, half.y, half.z});
  const float convex_radius = std::min(JPH::cDefaultConvexRadius, smallest * config::kConvexRadiusFraction);
  return finish(JPH::BoxShapeSettings(to_jolt(half), convex_radius));
}

ShapeResult sphere_shape(Vec3 half) { return finish(JPH::SphereShapeSettings(std::max({half.x, half.y, half.z}))); }

// An upright capsule; its measures are noted in `info`, for the character that may stand in it.
ShapeResult capsule_shape(Vec3 half, ShapeInfo& info) {
  info.capsule_radius = std::max(half.x, half.z);
  info.capsule_half_height = std::max(half.y - info.capsule_radius, config::kCharacterMinCylinderHalfHeight);
  return finish(JPH::CapsuleShapeSettings(info.capsule_half_height, info.capsule_radius));
}

// The collider around a box of that half size, centered on its own origin.
ShapeResult bounds_shape(ColliderShape collider, Vec3 half, ShapeInfo& info) {
  switch (collider) {
    case ColliderShape::Box:
      return box_shape(half);
    case ColliderShape::Sphere:
      return sphere_shape(half);
    case ColliderShape::Capsule:
    case ColliderShape::Mesh:
      break;
  }
  return capsule_shape(half, info);
}

// The same shape, moved so that its center sits `offset` away from the object's origin. (What
// could not be made, or already sits there, is handed back as it is.)
ShapeResult moved(ShapeResult shape, Vec3 offset) {
  const auto* centered = std::get_if<JPH::RefConst<JPH::Shape>>(&shape);
  const bool stays = centered == nullptr || length(offset) <= kEpsilon;
  if (stays) return shape;
  return finish(JPH::RotatedTranslatedShapeSettings(to_jolt(offset), JPH::Quat::sIdentity(), *centered));
}

std::variant<ShapeInfo, std::string> with_shape(ShapeInfo info, const ShapeResult& shape) {
  if (const auto* error = std::get_if<std::string>(&shape)) return *error;
  info.shape = std::get<JPH::RefConst<JPH::Shape>>(shape);
  return info;
}

}  // namespace

std::variant<ShapeInfo, std::string> build_shape(const BodyDesc& desc) {
  if (desc.config.collider == ColliderShape::Mesh) return with_shape(ShapeInfo{}, build_mesh(desc));
  ShapeInfo info;
  info.half_extents = clamp_half_extents(hadamard(desc.bounds.half_extents(), abs(desc.world.scale)));
  info.center_offset = hadamard(desc.bounds.center(), desc.world.scale);
  ShapeResult centered = bounds_shape(desc.config.collider, info.half_extents, info);
  return with_shape(info, moved(std::move(centered), info.center_offset));
}

}  // namespace redgem::jolt
