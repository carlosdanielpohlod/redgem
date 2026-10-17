#pragma once

// What one character does of its own (AnimationPlayer: game_object.animation in Ruby), as plain
// values: what its owner sends to the other machines of a game, and what they make theirs do.

#include <cstdint>
#include <optional>
#include <string>

namespace redgem {

// One clip a character plays, and where it is in it.
struct ClipState {
  std::string clip;
  float time = 0.0f;   // seconds into the clip
  float speed = 1.0f;  // negative: backwards
  bool loop = true;
  bool playing = true;
};

struct CharacterAnimation {
  // Changes whenever a script changes what the character does (another clip, a restart, a
  // pause...), not as its clips merely play on: what is worth telling the others about.
  std::uint32_t revision = 0;
  std::optional<ClipState> own;    // nothing: it follows the scene's clips
  float fade = 0.0f;               // seconds still to go of the change to `own`
  std::optional<ClipState> layer;  // a clip mixed over all that (mix)
  float layer_weight = 0.0f;
};

}  // namespace redgem
