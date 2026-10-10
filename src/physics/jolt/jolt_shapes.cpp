#include "physics/jolt/jolt_shapes.hpp"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <cmath>

#include "core/config.hpp"

namespace redgem::jolt {
namespace {

Vec3 abs(Vec3 v) { return {std::fabs(v.x), std::fabs(v.y), std::fabs(v.z)}; }

Vec3 clamp_half_extents(Vec3 half) {
  const float min_half = config::kMinColliderHalfExtent;
  return {std::max(half.x, min_half), std::max(half.y, min_half), std::max(half.z, min_half)};
}

std::variant<JPH::RefConst<JPH::Shape>, std::string> finish(const JPH::ShapeSettings& settings) {
  JPH::ShapeSettings::ShapeResult result = settings.Create();
  if (result.HasError()) return std::string(result.GetError().c_str());
  return JPH::RefConst<JPH::Shape>(result.Get());
}

std::variant<JPH::RefConst<JPH::Shape>, std::string> build_mesh(const BodyDesc& desc) {
  if (desc.mesh == nullptr) return std::string("mesh collider requires a mesh");
  JPH::VertexList vertices;
  JPH::IndexedTriangleList triangles;
  for (const PrimitiveDesc& primitive : desc.mesh->primitives) {
    const auto base = static_cast<JPH::uint32>(vertices.size());
    for (std::size_t i = 0; i + config::kFloatsPerPosition <= primitive.positions.size();
         i += config::kFloatsPerPosition) {
      const Vec3 p = hadamard({primitive.positions[i], primitive.positions[i + 1], primitive.positions[i + 2]},
                              desc.world.scale);
      vertices.push_back(JPH::Float3(p.x, p.y, p.z));
    }
    for (std::size_t i = 0; i + config::kVerticesPerTriangle <= primitive.indices.size();
         i += config::kVerticesPerTriangle) {
      triangles.push_back(JPH::IndexedTriangle(base + primitive.indices[i], base + primitive.indices[i + 1],
                                               base + primitive.indices[i + 2]));
    }
  }
  if (triangles.empty()) return std::string("mesh collider has no triangles");
  return finish(JPH::MeshShapeSettings(vertices, triangles));
}

}  // namespace

std::variant<ShapeInfo, std::string> build_shape(const BodyDesc& desc) {
  ShapeInfo info;
  if (desc.config.collider == ColliderShape::Mesh) {
    auto mesh = build_mesh(desc);
    if (auto* error = std::get_if<std::string>(&mesh)) return *error;
    info.shape = std::get<JPH::RefConst<JPH::Shape>>(mesh);
    return info;
  }

  const Vec3 scale = abs(desc.world.scale);
  const Vec3 half = clamp_half_extents(hadamard(desc.bounds.half_extents(), scale));
  info.center_offset = hadamard(desc.bounds.center(), desc.world.scale);
  info.half_extents = half;

  std::variant<JPH::RefConst<JPH::Shape>, std::string> inner;
  switch (desc.config.collider) {
    case ColliderShape::Box: {
      const float smallest = std::min({half.x, half.y, half.z});
      const float convex_radius = std::min(JPH::cDefaultConvexRadius, smallest * config::kConvexRadiusFraction);
      inner = finish(JPH::BoxShapeSettings(to_jolt(half), convex_radius));
      break;
    }
    case ColliderShape::Sphere:
      inner = finish(JPH::SphereShapeSettings(std::max({half.x, half.y, half.z})));
      break;
    case ColliderShape::Capsule:
    case ColliderShape::Mesh: {
      info.capsule_radius = std::max(half.x, half.z);
      info.capsule_half_height =
          std::max(half.y - info.capsule_radius, config::kCharacterMinCylinderHalfHeight);
      inner = finish(JPH::CapsuleShapeSettings(info.capsule_half_height, info.capsule_radius));
      break;
    }
  }
  if (auto* error = std::get_if<std::string>(&inner)) return *error;
  info.shape = std::get<JPH::RefConst<JPH::Shape>>(inner);

  if (length(info.center_offset) > kEpsilon) {
    auto offset = finish(JPH::RotatedTranslatedShapeSettings(to_jolt(info.center_offset), JPH::Quat::sIdentity(),
                                                             info.shape));
    if (auto* error = std::get_if<std::string>(&offset)) return *error;
    info.shape = std::get<JPH::RefConst<JPH::Shape>>(offset);
  }
  return info;
}

}  // namespace redgem::jolt
