#pragma once

// What one character does of its own (AnimationPlayer: game_object.animation in Ruby), as plain
// values: what its owner sends to the other machines of a game, and what they make theirs do.

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace redgem {

// One clip a character plays, and where it is in it.
struct ClipState {
  std::string clip;
  float time = 0.0f;   // seconds into the clip
  float speed = 1.0f;  // negative: backwards
  bool loop = true;
  bool playing = true;
};

inline constexpr std::size_t kMaxGraphSideClips = 2;  // neighboring clips of a one-dimensional blend

struct WeightedClipState {
  ClipState clip;
  float weight = 1.0f;
};

struct GraphAnimation {
  std::vector<WeightedClipState> from;
  std::vector<WeightedClipState> to;
  float blend = 1.0f;
  float remaining = 0.0f;
  bool playing = true;  // false when paused or when a non-looping state and its fade have ended
};

struct CharacterAnimation {
  // Changes whenever a script changes what the character does (another clip, a restart, a
  // pause...), not as its clips merely play on: what is worth telling the others about.
  std::uint32_t revision = 0;
  std::optional<ClipState> own;    // nothing: it follows the scene's clips
  float fade = 0.0f;               // seconds still to go of the change to `own`
  std::optional<ClipState> layer;  // a clip mixed over all that (mix)
  float layer_weight = 0.0f;
  std::optional<GraphAnimation> graph;
};

}  // namespace redgem
