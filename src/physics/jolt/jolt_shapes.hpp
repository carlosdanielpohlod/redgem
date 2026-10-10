#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <string>
#include <variant>

#include "physics/physics_world.hpp"

namespace redgem::jolt {

struct ShapeInfo {
  JPH::RefConst<JPH::Shape> shape;
  float capsule_radius = 0.0f;       // only for capsules
  float capsule_half_height = 0.0f;  // cylinder half height, only for capsules
  Vec3 center_offset;                // offset of the collider center from the object origin
  Vec3 half_extents;                 // scaled half size of the collider bounds
};

// Builds the collider for an object from its mesh bounds and world scale.
std::variant<ShapeInfo, std::string> build_shape(const BodyDesc& desc);

inline JPH::Vec3 to_jolt(Vec3 v) { return JPH::Vec3(v.x, v.y, v.z); }
inline JPH::Quat to_jolt(Quat q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
inline Vec3 from_jolt(JPH::Vec3Arg v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
inline Quat from_jolt(JPH::QuatArg q) { return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()}; }

}  // namespace redgem::jolt
