#pragma once

// Shape keys (glTF morph targets) as the vertex shader applies them, the way bones are: the
// deltas of every shape key of every mesh part in one texture of float texels (two a vertex: its
// position's delta, then its normal's), and for each drawn copy of a mesh the shape keys it shows
// most, kMaxActiveMorphs at most, as (key, weight) pairs in the frame's texels after its bones.
// No window and no graphics card needed, so it can be tested.

#include <array>
#include <cstddef>
#include <vector>

#include "core/math.hpp"
#include "render/texel_rows.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::render_math {

// The most shape keys one copy of a mesh shows at once: the heaviest ones. (A face with fifty keys
// rarely moves more than a handful at a time; each one costs every vertex two texel reads.)
inline constexpr std::size_t kMaxActiveMorphs = 8;
inline constexpr std::size_t kMorphsPerTexel = 2;  // (key, weight) pairs: two to a texel
inline constexpr std::size_t kMorphTexels = kMaxActiveMorphs / kMorphsPerTexel;
inline constexpr std::size_t kTexelsPerMorphVertex = 2;  // a vertex's position delta, then its normal's
inline constexpr int kPositionDeltaTexel = 0;            // which of a vertex's two texels is which
inline constexpr int kNormalDeltaTexel = 1;
// Where a copy's shape keys or a part's deltas are, for one that has none (texels go as floats).
inline constexpr float kNoMorphs = -1.0f;
// A weight closer to 0 than this shows nothing worth drawing.
inline constexpr float kSmallestWeight = 1e-4f;

// One shape key a copy shows.
struct ActiveMorph {
  std::size_t key = 0;
  float weight = 0.0f;
};

// The shape keys `weights` shows, the heaviest first (by how far from 0), kMaxActiveMorphs at most;
// the ones nearer 0 than kSmallestWeight are left out.
std::vector<ActiveMorph> active_morphs(const std::vector<float>& weights);
// Whether `weights` shows any shape key.
bool shows_morphs(const std::vector<float>& weights);

// The (key, weight) pairs of active_morphs as the texels a copy's record points at, the rest
// (0, 0): a key of no weight moves nothing.
using MorphTexels = std::array<float, kMorphTexels * kFloatsPerTexel>;
MorphTexels morph_texels(const std::vector<ActiveMorph>& active);

// Appends a part's shape-key deltas, key after key, vertex after vertex (kTexelsPerMorphVertex
// texels each; a key without normals turns none), and returns the texel they start at; kNoMorphs,
// with nothing appended, for a part without shape keys or when they would take `texels` past
// `max_texels`.
float append_morph_deltas(std::vector<float>& texels, const PrimitiveDesc& part, std::size_t max_texels);

// Where vertex `vertex` of a part is with the shape keys `active` applied, as the shader puts it.
Vec3 morphed_position(const PrimitiveDesc& part, std::size_t vertex, const std::vector<ActiveMorph>& active);

}  // namespace redgem::render_math
