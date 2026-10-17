#pragma once

// One copy of a mesh (an instance) as the vertex shader reads it from the frame's texels
// (texel_rows.hpp), kTexelsPerInstance of them from the instance's number times that on:
//   texels 0-3  where it is: its model matrix, a column each
//   texels 4-6  how its normals turn: the columns of its normal matrix; the first one's fourth float
//               is how much the wind sways it (1, or 0 for one that does not sway)
//   texel  7    its tint (red, green, blue), and where its skeleton's first bone is (kNoBones: none)
//   texel  8    how a scattered copy fades: the distances it is drawn from and to, the meters it
//               fades over at their ends (0: it does not), and how much it is lit as facing up
// Every copy of a mesh drawn in one call finds its own record by its number. No window and no
// graphics card needed, so it can be tested.

#include <raylib.h>

#include <cstddef>

#include "core/math.hpp"
#include "render/texel_rows.hpp"

namespace redgem::render_math {

inline constexpr int kTexelsPerInstance = 9;
inline constexpr int kNormalTexel = 4;  // where a record's normal matrix starts
inline constexpr int kLookTexel = 7;    // where its tint and first bone are
inline constexpr int kCopyTexel = 8;    // where how it fades is

inline constexpr float kSways = 1.0f;
inline constexpr float kStill = 0.0f;

// What a copy looks like besides where it is.
struct InstanceLook {
  Vec3 tint = Vec3::one();
  float first_bone = kNoBones;
  // Scattered copies only (see ScatterSet).
  float visible_from = 0.0f;
  float visible_to = 0.0f;
  float fade = 0.0f;
  float upward = 0.0f;
  float sway = kSways;  // how much the wind sways it (any copy)
};

// The first texel of instance `number`'s record.
inline std::size_t record_texel(std::size_t number) { return number * kTexelsPerInstance; }

// Writes instance `number`'s record. False, with nothing written, past what the texels can hold.
bool write_instance(TexelRows& texels, std::size_t number, const Matrix& model, const InstanceLook& look);

}  // namespace redgem::render_math
