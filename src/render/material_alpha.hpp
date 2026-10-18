#pragma once

// How the see-through parts of a material are drawn, pass by pass, and the order see-through
// surfaces are drawn in. No window and no graphics card needed, so it can be tested.
//   - Opaque materials are drawn whole.
//   - Cut-out ones (glTF MASK) leave out what is less opaque than their cutoff.
//   - Blended ones (glTF BLEND: glass, hair cards) are drawn twice. With the opaque surfaces, what
//     is nearly opaque (kBlendCore and above), which writes depth and so sorts itself, layer by
//     layer, however many cards overlap; then, after every opaque surface, the rest is mixed with
//     what is behind it, object by object, the farthest first, writing no depth. A dithered one
//     (MaterialDesc::dithered) is drawn once instead, with the opaque surfaces: its texels are
//     left out in a fine pattern that thins with their alpha, which needs no sorting.
// Textured cut-out and blended materials are foliage: drawn on both sides, and swaying in the wind
// (on objects that sway: see props::kSway).

#include <cstddef>
#include <vector>

#include "scene/scene_desc.hpp"

namespace redgem::render_math {

inline constexpr float kNoCutoff = 0.0f;   // an alpha test that lets everything through
inline constexpr float kNoCeiling = 2.0f;  // above any alpha: nothing is too opaque to be drawn
inline constexpr float kNoWind = 0.0f;
inline constexpr float kBlendCore = 0.9f;  // a blended texel this opaque is drawn with the opaque surfaces
inline constexpr float kInvisibleAlpha = 1.0f / 255.0f;  // a texel less opaque than this shows nothing
// Blended textures cast shadows as cut-outs, leaving out what is less opaque than this.
inline constexpr float kBlendShadowCutoff = 0.35f;

// The passes a material is drawn in: into the sun's shadow map, with the opaque surfaces, and
// mixed with what is behind it after them. For temporal anti-aliasing, Motion: the opaque surfaces
// again, for where they moved (what the opaque pass leaves out, it leaves out); Cover: the
// see-through ones again, for how much of each pixel they cover (as the see-through pass draws them).
enum class AlphaPass { Shadow, Opaque, SeeThrough, Motion, Cover };

struct AlphaTest {
  float cutoff = kNoCutoff;    // alpha below this is not drawn
  float ceiling = kNoCeiling;  // alpha at or above this is not drawn (it was, with the opaque surfaces)
  float sway = kNoWind;        // foliage sways in the wind; everything else stays still
  bool two_sided = false;      // drawn without backface culling
  bool dithered = false;       // left out by a fine pattern of its alpha, instead of the cutoff
  bool mixed = false;          // mixed with what is behind it by its alpha
};
AlphaTest alpha_test(const MaterialDesc& material, float wind, AlphaPass pass);
// Whether a material is drawn again after the opaque surfaces, mixed with what is behind them.
bool drawn_see_through(const MaterialDesc& material);

// See-through surfaces to draw: an object's copy of a mesh (`count` 1), or the scattered copies of
// one cell, which are not sorted among themselves. `first`: where its record numbers start among
// the pass's.
struct SeeThroughDraw {
  float distance = 0.0f;  // from the viewer, in meters (squared: only the order matters)
  int mesh = 0;
  std::size_t first = 0;
  std::size_t count = 0;
};
// The farthest first, so that each is mixed over what lies behind it; at the same distance, in
// the order they came.
void sort_back_to_front(std::vector<SeeThroughDraw>& draws);

}  // namespace redgem::render_math
