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
#include <vector>

#include "net/pose_bytes.hpp"

namespace redgem::net {

// The longest clip name an animation carries: a character playing a clip of a longer one (or of
// an empty name, which a clip's size of 0 cannot tell from none) is sent without its animation.
inline constexpr std::size_t kMaxClipNameBytes = 64;

struct ClipPart {
  std::string clip;
  float time = 0.0f;
  float speed = 1.0f;
  bool loop = true;
  bool playing = true;
};

inline constexpr std::size_t kMaxBlendClips = 2;  // neighbors per side of a one-dimensional blend

struct WeightedClipPart {
  ClipPart clip;
  float weight = 1.0f;
};

struct GraphPart {
  std::vector<WeightedClipPart> from;
  std::vector<WeightedClipPart> to;
  float blend = 1.0f;
  float remaining = 0.0f;
  bool playing = true;
};

struct AnimationPart {
  float fade = 0.0f;
  float layer_weight = 0.0f;
  std::optional<ClipPart> own;  // nothing: it follows the scene's clips
  std::optional<ClipPart> layer;
  std::optional<GraphPart> graph;
};

// The largest mode has a layer, no manual base, and four graph clips with normalized weights.
inline constexpr std::size_t kMaxClipBytes = kByte + kMaxClipNameBytes + 2 * kFloatBytes + kByte;
inline constexpr std::size_t kGraphHeadBytes = 2 * kByte + 2 * kFloatBytes + kByte;
inline constexpr std::size_t kMaxAnimationBytes =
    2 * kFloatBytes + kByte + kMaxClipBytes + kGraphHeadBytes + 2 * kMaxBlendClips * (kMaxClipBytes + kFloatBytes);

bool valid_clip(const ClipPart& clip);
void write_animation_clip(PoseWriter& writer, const std::optional<ClipPart>& clip);
bool read_animation_clip(PoseReader& reader, std::optional<ClipPart>& clip);
bool valid_graph(const GraphPart& graph);
void write_graph(PoseWriter& writer, const GraphPart& graph);
std::optional<GraphPart> read_graph(PoseReader& reader);

// Whether every clip name of an animation is one it carries: 1 to kMaxClipNameBytes bytes.
bool carriable(const AnimationPart& part);
void write_animation(PoseWriter& writer, const AnimationPart& part);
// The animation in what is left of the bytes, which it must take up exactly; nothing otherwise.
std::optional<AnimationPart> read_animation(PoseReader& reader);

}  // namespace redgem::net
