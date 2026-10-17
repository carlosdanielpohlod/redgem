#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "net/animation_codec.hpp"
#include "net/pose_quantization.hpp"

// NetworkSync's poses on the wire: where an object is, sent by the machine that moves it twenty
// times a second, most of what a game sends. A pose travels as the bytes (the kData) of a message
// of its own kind (wire::kPose), little-endian:
//
//   flags       1 byte   which of the parts below follow (kAbsolutePosition, kMovedPosition,
//                        kRotation, kAnimation)
//   key size    1 byte   0 to kMaxPoseKeyBytes
//   key         the object's key on the network (UTF-8)
//   position    either where it is: x, y, z as 32-bit floats (12 bytes)
//               or how far it moved since the last pose its machine sent: x, y, z as 16-bit
//               whole numbers of kPoseStep meters (6 bytes, see pose_quantization.hpp)
//   rotation    4 bytes: "smallest three" (see pose_quantization.hpp)
//   animation   what the object's own animation is doing, to the end (see animation_codec.hpp)
//
// A part that did not change since the last pose sent is left out: an object standing still
// sends its key alone, which keeps the others' interpolation on time. A whole pose (where it is,
// and its rotation) starts each object, and comes again every so often and whenever someone
// joins (see pose_baselines.hpp). Positions are floats, as the engine keeps them: one sent whole
// is exactly the sender's, however far from the middle of the world; a move is counted from the
// last pose sent, so its precision does not depend on where the object is either.
namespace redgem::net {

// The longest key an object of a game goes by on the network (Network::Said::MAX_KEY_SIZE).
inline constexpr std::size_t kMaxPoseKeyBytes = 128;
// The most a pose's bytes may be: flags, key size, the longest key, where it is, its rotation, its
// animation.
inline constexpr std::size_t kMaxPoseBytes = 2 + kMaxPoseKeyBytes + 12 + 4 + kMaxAnimationBytes;

// What a pose says of an object: its key, and some of where it is.
struct PoseUpdate {
  std::string key;
  std::optional<Vec3> position;           // where it is, or ...
  std::optional<PoseSteps> moved;         // ... how far it moved since the last pose sent
  std::optional<std::uint32_t> rotation;  // packed_rotation
  std::optional<AnimationPart> animation;

  // Whether it says all of where the object is.
  bool whole() const { return position.has_value() && rotation.has_value(); }
};

std::vector<std::uint8_t> encoded(const PoseUpdate& update);
// What the bytes of a pose say; nothing when they are not a pose (they come from another machine).
std::optional<PoseUpdate> decoded(const std::vector<std::uint8_t>& bytes);

}  // namespace redgem::net
