#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "core/math.hpp"

// How a pose's numbers are made small for the wire (see pose_codec.hpp), and back.
namespace redgem::net {

// A rotation in 32 bits ("smallest three"): which of the quaternion's four parts is the largest
// (2 bits), and the other three, each between -1/sqrt(2) and 1/sqrt(2), in 10 bits each. The
// largest follows from them (the quaternion is of length 1, and turned so that it is positive).
// What comes back is within 0.3 degrees of what went.
std::uint32_t packed_rotation(Quat rotation);
Quat unpacked_rotation(std::uint32_t packed);

// A move along each axis in steps of kPoseStep meters (1/1024: a millimeter, near enough), which
// adds up the same on every machine: a power of two times a whole number is exact in a float, so
// the one rounding is that of the sum, which IEEE 754 makes the same everywhere.
inline constexpr float kPoseStep = 1.0f / 1024.0f;
inline constexpr std::size_t kPoseAxes = 3;  // x, y, z
using PoseSteps = std::array<std::int16_t, kPoseAxes>;

// The steps that take `from` closest to `to`; nothing when the move does not fit in 16 bits an
// axis (about 32 meters).
std::optional<PoseSteps> steps_between(Vec3 from, Vec3 to);
Vec3 moved_by(Vec3 from, const PoseSteps& steps);
// Whether it moves at all.
bool moves(const PoseSteps& steps);

}  // namespace redgem::net
