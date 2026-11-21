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

// How tall (end to end) and how wide an upright capsule is.
struct CapsuleSize {
  float height = 0.0f;
  float radius = 0.0f;
};

// The size of a capsule collider (a character's: characters always have one, see body_config.hpp),
// as built: its cylinder is never shorter than kCharacterMinCylinderHalfHeight.
CapsuleSize capsule_size_of(const ShapeInfo& info);
// The lowest point of a capsule collider, from the object's origin.
Vec3 collider_bottom(const ShapeInfo& info);
// An upright capsule of that size, whose lowest point sits `bottom` away from the object's origin.
std::variant<ShapeInfo, std::string> capsule_standing_on(Vec3 bottom, CapsuleSize size);

inline JPH::Vec3 to_jolt(Vec3 v) { return JPH::Vec3(v.x, v.y, v.z); }
inline JPH::Quat to_jolt(Quat q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
inline Vec3 from_jolt(JPH::Vec3Arg v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
inline Quat from_jolt(JPH::QuatArg q) { return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()}; }
inline JPH::RMat44 matrix_of(Vec3 position, Quat rotation) {
  return JPH::RMat44::sRotationTranslation(to_jolt(rotation), JPH::RVec3(to_jolt(position)));
}

// Shapes the world builds for triggers and queries are made at their final size: unscaled.
inline constexpr float kUnitScale = 1.0f;

}  // namespace redgem::jolt
