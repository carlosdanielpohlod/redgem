#pragma once

// What a character's own animation is doing, carried at the end of its pose (pose_codec.hpp) when
// its owner changed it, and with every whole pose (so that a machine that just arrived, or missed
// nothing but drifted, catches up). Little-endian:
//
//   fade          32-bit float   seconds still to go of the change to its own clip
//   layer weight  32-bit float   how much of the mixed clip is laid over (0 to 1)
//   own clip      a clip (below), or one byte 0: it follows the scene's clips
//   mixed clip    a clip, or one byte 0: none
//
// A clip is its name's size in bytes (1 to kMaxClipNameBytes), its name (UTF-8), where it is in
// it and how fast it plays (seconds, and 1 as written: two 32-bit floats), and one byte of flags
// (kLoops, kPlays).
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "net/pose_bytes.hpp"

namespace redgem::net {

// The longest clip name an animation carries: a character playing a clip of a longer one is sent
// without its animation.
inline constexpr std::size_t kMaxClipNameBytes = 64;

struct ClipPart {
  std::string clip;
  float time = 0.0f;
  float speed = 1.0f;
  bool loop = true;
  bool playing = true;
};

struct AnimationPart {
  float fade = 0.0f;
  float layer_weight = 0.0f;
  std::optional<ClipPart> own;  // nothing: it follows the scene's clips
  std::optional<ClipPart> layer;
};

// The most bytes an animation takes: two floats, and two clips of the longest names.
inline constexpr std::size_t kMaxAnimationBytes =
    2 * kFloatBytes + 2 * (kByte + kMaxClipNameBytes + 2 * kFloatBytes + kByte);

// Whether every clip name of an animation fits what it carries.
bool carriable(const AnimationPart& part);
void write_animation(PoseWriter& writer, const AnimationPart& part);
// The animation in what is left of the bytes, which it must take up exactly; nothing otherwise.
std::optional<AnimationPart> read_animation(PoseReader& reader);

}  // namespace redgem::net
