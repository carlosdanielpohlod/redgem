#pragma once

// The physics layer's numbers: the world, the bodies in it, characters and vehicles.

#include <algorithm>
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

// --- Threads --------------------------------------------------------------------
// The threads that solve a step. One solves it on the thread that steps, as RedGem always did;
// more run Jolt's thread pool (the stepping thread one of them), and a game's world then steps on
// a thread of its own while the frame before it is drawn (see app/engine.hpp).
inline constexpr int kDefaultPhysicsThreads = 1;
inline constexpr int kMaxPhysicsThreads = 32;
// The barriers Jolt's thread pool keeps (one for each step running at once: a world runs one).
inline constexpr std::uint32_t kMaxPhysicsBarriers = 8;

// What a setting asks for, within what a world can be given: 1 for anything below it.
constexpr int bounded_physics_threads(int requested) {
  return std::clamp(requested, kDefaultPhysicsThreads, kMaxPhysicsThreads);
}

// Whether a world given that many threads solves its steps on more than one.
constexpr bool solves_in_parallel(int threads) { return threads > kDefaultPhysicsThreads; }

// --- Colliders ------------------------------------------------------------------
// Smallest half extent accepted for a collider (degenerate meshes such as planes).
inline constexpr float kMinColliderHalfExtent = 0.01f;
// Convex radius is clamped to this fraction of the smallest half extent.
inline constexpr float kConvexRadiusFraction = 0.5f;

// How far apart two resting bodies may be and still touch (Jolt keeps a sliver between them).
inline constexpr float kRestingContactGap = 0.05f;

// --- Queries (rays, casts and overlaps) --------------------------------------------
// Most hits a query reports (the nearest ones): a query is not meant to list a crowd.
inline constexpr std::size_t kMaxQueryHits = 64;

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
