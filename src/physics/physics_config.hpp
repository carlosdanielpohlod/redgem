#pragma once

// The physics layer's numbers: the world, the bodies in it, characters and vehicles.

#include <cstddef>
#include <cstdint>

namespace redgem::config {

// --- Colliders without a mesh (e.g. Blender empties) -------------------------------
inline constexpr float kDefaultColliderHalfExtent = 0.5f;

// --- World and bodies -----------------------------------------------------------
inline constexpr float kGravityY = -20.0f;
inline constexpr float kDefaultBodyMass = 1.0f;
inline constexpr std::uint32_t kMaxBodies = 4096;
inline constexpr std::uint32_t kNumBodyMutexes = 0;  // 0 = Jolt picks a default
inline constexpr std::uint32_t kMaxBodyPairs = 4096;
inline constexpr std::uint32_t kMaxContactConstraints = 4096;
inline constexpr std::size_t kPhysicsTempAllocatorBytes = 10u * 1024u * 1024u;
inline constexpr std::uint32_t kMaxPhysicsJobs = 1024;
inline constexpr int kPhysicsCollisionSteps = 1;
// Smallest half extent accepted for a collider (degenerate meshes such as planes).
inline constexpr float kMinColliderHalfExtent = 0.01f;
// Convex radius is clamped to this fraction of the smallest half extent.
inline constexpr float kConvexRadiusFraction = 0.5f;

// --- Character controller -------------------------------------------------------
inline constexpr float kCharacterMaxSlopeDegrees = 50.0f;
inline constexpr float kCharacterMass = 70.0f;
inline constexpr float kCharacterMaxStrength = 100.0f;
inline constexpr float kCharacterPadding = 0.02f;
inline constexpr float kCharacterPenetrationRecoverySpeed = 1.0f;
inline constexpr float kCharacterPredictiveContactDistance = 0.1f;
inline constexpr float kCharacterStickToFloorDistance = 0.5f;
inline constexpr float kCharacterStairStepHeight = 0.4f;
// Shortest cylinder section allowed in a capsule built from flat or very round meshes.
inline constexpr float kCharacterMinCylinderHalfHeight = 0.05f;

// --- Vehicles -------------------------------------------------------------------
inline constexpr float kVehicleDefaultMass = 1200.0f;
inline constexpr float kVehicleMaxSteerDegrees = 30.0f;
inline constexpr float kVehicleMaxPitchRollDegrees = 60.0f;
inline constexpr float kVehicleSuspensionMinLength = 0.05f;
inline constexpr float kVehicleSuspensionMaxLength = 0.35f;
inline constexpr float kVehicleHandBrakeTorque = 4000.0f;
inline constexpr float kVehicleEngineMaxTorque = 600.0f;
// Lowers the chassis center of mass by this fraction of its half height (keeps cars upright).
inline constexpr float kVehicleCenterOfMassDrop = 0.6f;
inline constexpr float kVehicleWheelConvexRadiusFraction = 0.1f;

}  // namespace redgem::config
