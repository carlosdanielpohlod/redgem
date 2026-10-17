#pragma once

// Half floats (16 bits), the textures of light are kept in: every graphics card the engine runs
// on filters them, phones included (32-bit floats are not filtered there).

#include <cstdint>
#include <vector>

#include "core/math.hpp"

namespace redgem::render_math {

inline constexpr int kHalfsPerTexel = 4;  // red, green, blue, alpha (rows of four stay aligned)

// The nearest half float, for light: below zero and not-a-number become 0, and what is beyond
// the largest half float becomes it.
std::uint16_t to_half(float value);

// A texel of light (alpha 1) added to `texels`.
void add_texel(std::vector<std::uint16_t>& texels, Vec3 light);

}  // namespace redgem::render_math
